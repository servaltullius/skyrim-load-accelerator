#include "FormCache.h"

#include <tbb/concurrent_hash_map.h>

namespace FormCache {
    // Cache: FormID -> TESForm*
    static tbb::concurrent_hash_map<RE::FormID, RE::TESForm*> g_formCache;
    static std::atomic<bool> g_cacheEnabled{false};

    // Statistics
    static std::atomic<uint64_t> g_cacheHits{0};
    static std::atomic<uint64_t> g_cacheMisses{0};

    // Original function
    using LookupByID_t = RE::TESForm*(*)(RE::FormID);
    static LookupByID_t OriginalLookupByID = nullptr;

    static RE::TESForm* HookedLookupByID(RE::FormID a_formID) {
        if (!g_cacheEnabled.load(std::memory_order_relaxed)) {
            return OriginalLookupByID(a_formID);
        }

        // Try cache first
        {
            tbb::concurrent_hash_map<RE::FormID, RE::TESForm*>::const_accessor accessor;
            if (g_formCache.find(accessor, a_formID)) {
                g_cacheHits.fetch_add(1, std::memory_order_relaxed);
                return accessor->second;
            }
        }

        // Cache miss - call original
        g_cacheMisses.fetch_add(1, std::memory_order_relaxed);
        auto* form = OriginalLookupByID(a_formID);

        if (form) {
            tbb::concurrent_hash_map<RE::FormID, RE::TESForm*>::accessor accessor;
            g_formCache.insert(accessor, a_formID);
            accessor->second = form;
        }

        return form;
    }

    static void ClearCache() {
        g_formCache.clear();
        spdlog::info("FormCache: cache cleared (hits={}, misses={})",
            g_cacheHits.load(), g_cacheMisses.load());
        g_cacheHits = 0;
        g_cacheMisses = 0;
    }

    // SKSE message handler to clear cache on data loaded
    static void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
            ClearCache();
        }
    }

    void Install() {
        // Hook TESForm::LookupByID
        auto& trampoline = SKSE::GetTrampoline();
        SKSE::AllocTrampoline(64);

        // TESForm::LookupByID - REL::RelocationID for SE/AE compatibility
        REL::RelocationID lookupByID(
            14461,  // SE 1.5.x
            14602   // AE 1.6.x
        );

        OriginalLookupByID = reinterpret_cast<LookupByID_t>(
            trampoline.write_call<5>(lookupByID.address(), reinterpret_cast<std::uintptr_t>(&HookedLookupByID)));

        if (OriginalLookupByID) {
            g_cacheEnabled.store(true, std::memory_order_release);

            // Register SKSE message handler to clear cache on data reload
            auto* messaging = SKSE::GetMessagingInterface();
            if (messaging) {
                messaging->RegisterListener(MessageHandler);
            }

            spdlog::info("FormCache: TESForm::LookupByID hooked with concurrent cache");
        } else {
            spdlog::error("FormCache: failed to hook TESForm::LookupByID");
        }

        spdlog::info("FormCache: installed (cache stats will be logged on clear)");
    }
}
