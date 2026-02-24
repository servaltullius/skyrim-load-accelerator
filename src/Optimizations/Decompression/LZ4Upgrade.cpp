#include "LZ4Upgrade.h"
#include "Utils/IATHook.h"
#include "Utils/PatternScan.h"

#include <lz4.h>

namespace LZ4Upgrade {
    // Original function pointer
    using LZ4_decompress_safe_t = int(__cdecl*)(const char*, char*, int, int);
    static LZ4_decompress_safe_t OriginalDecompressSafe = nullptr;

    static int __cdecl HookedDecompressSafe(const char* a_src, char* a_dst, int a_compressedSize, int a_dstCapacity) {
        // Use the latest LZ4 library's decompress function
        int result = LZ4_decompress_safe(a_src, a_dst, a_compressedSize, a_dstCapacity);

        if (result < 0 && OriginalDecompressSafe) {
            spdlog::trace("LZ4Upgrade: decompress failed with {}, trying original", result);
            return OriginalDecompressSafe(a_src, a_dst, a_compressedSize, a_dstCapacity);
        }

        return result;
    }

    static bool TryIATHook() {
        auto original = IATHook::Apply("lz4.dll", "LZ4_decompress_safe", reinterpret_cast<void*>(&HookedDecompressSafe));
        if (original) {
            OriginalDecompressSafe = reinterpret_cast<LZ4_decompress_safe_t>(original);
            spdlog::info("LZ4Upgrade: hooked via IAT, upgraded to v{}", LZ4_versionString());
            return true;
        }
        return false;
    }

    static bool TryTrampolineHook() {
        // BSA v105 (SE/AE default) uses LZ4 for block compression
        // The LZ4 decompression call is inside the BSA block reader
        try {
            REL::RelocationID lz4DecompressCall(
                69557,   // SE 1.5.97 - BSA LZ4 block decompression site
                71135    // AE 1.6.x
            );

            auto& trampoline = SKSE::GetTrampoline();
            SKSE::AllocTrampoline(64);

            OriginalDecompressSafe = reinterpret_cast<LZ4_decompress_safe_t>(
                trampoline.write_call<5>(
                    lz4DecompressCall.address(),
                    reinterpret_cast<std::uintptr_t>(&HookedDecompressSafe)));

            if (OriginalDecompressSafe) {
                spdlog::info("LZ4Upgrade: hooked via trampoline (REL::ID), upgraded to v{}", LZ4_versionString());
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("LZ4Upgrade: REL::ID trampoline failed: {}", e.what());
        }
        return false;
    }

    static bool TryPatternScan() {
        // LZ4_decompress_safe has a distinctive prologue pattern
        // The function starts with specific register setup for the fast path
        const char* patterns[] = {
            // LZ4_decompress_safe prologue pattern (LZ4 1.9.x embedded in Skyrim)
            // push rbp; push rbx; push rsi; push rdi; push r12...
            "40 55 53 56 57 41 54 41 55 41 56 48 8D 6C 24",
            // Alternative: shorter pattern for LZ4 entry
            "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 56 41 57 48 83 EC ?? 45 8B F1 41 8B F8 48 8B F2 48 8B E9",
            // Call site pattern: call to LZ4 within BSA decompression
            "41 8B D6 48 8B CE 44 8B C7 48 8B D5 E8 ?? ?? ?? ??",
        };

        for (const auto* pattern : patterns) {
            auto addr = PatternScan::Find(pattern);
            if (addr) {
                // Check if this is a call site (E8 pattern) vs function start
                auto* bytes = reinterpret_cast<const std::uint8_t*>(addr);

                // If we found a call site, resolve the target
                if (std::string_view(pattern).find("E8 ??") != std::string_view::npos) {
                    // Find the E8 byte in the match
                    auto callOffset = std::string_view(pattern).find("E8") / 3;  // approximate byte offset
                    auto callAddr = addr + callOffset;

                    auto& trampoline = SKSE::GetTrampoline();
                    SKSE::AllocTrampoline(64);

                    OriginalDecompressSafe = reinterpret_cast<LZ4_decompress_safe_t>(
                        trampoline.write_call<5>(callAddr, reinterpret_cast<std::uintptr_t>(&HookedDecompressSafe)));

                    spdlog::info("LZ4Upgrade: hooked call site via pattern scan at 0x{:X}", callAddr);
                    return true;
                }

                // Function start - use branch hook
                auto& trampoline = SKSE::GetTrampoline();
                SKSE::AllocTrampoline(64);

                OriginalDecompressSafe = reinterpret_cast<LZ4_decompress_safe_t>(
                    trampoline.write_branch<5>(addr, reinterpret_cast<std::uintptr_t>(&HookedDecompressSafe)));

                spdlog::info("LZ4Upgrade: hooked function via pattern scan at 0x{:X}, upgraded to v{}", addr, LZ4_versionString());
                return true;
            }
        }

        return false;
    }

    void Install() {
        // Strategy 1: IAT hook (works when LZ4 is dynamically linked)
        if (TryIATHook()) return;

        // Strategy 2: Trampoline via REL::ID
        if (TryTrampolineHook()) return;

        // Strategy 3: Pattern scan
        if (TryPatternScan()) return;

        spdlog::warn("LZ4Upgrade: all hooking strategies failed, module inactive");
        spdlog::info("LZ4Upgrade: linked against LZ4 v{} but could not find hook target", LZ4_versionString());
    }
}
