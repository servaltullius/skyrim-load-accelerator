#include "AllocatorReplacement.h"
#include "Utils/IATHook.h"

#include <mimalloc.h>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

namespace AllocatorReplacement {
    // Original CRT function pointers
    static decltype(&malloc) OriginalMalloc = nullptr;
    static decltype(&calloc) OriginalCalloc = nullptr;
    static decltype(&realloc) OriginalRealloc = nullptr;
    static decltype(&free) OriginalFree = nullptr;
    static decltype(&_aligned_malloc) OriginalAlignedMalloc = nullptr;
    static decltype(&_aligned_free) OriginalAlignedFree = nullptr;
    static decltype(&_msize) OriginalMsize = nullptr;

    static void* __cdecl HookedMalloc(size_t a_size) {
        return mi_malloc(a_size);
    }

    static void* __cdecl HookedCalloc(size_t a_count, size_t a_size) {
        return mi_calloc(a_count, a_size);
    }

    static void* __cdecl HookedRealloc(void* a_block, size_t a_size) {
        return mi_realloc(a_block, a_size);
    }

    static void __cdecl HookedFree(void* a_block) {
        mi_free(a_block);
    }

    static void* __cdecl HookedAlignedMalloc(size_t a_size, size_t a_alignment) {
        return mi_aligned_alloc(a_alignment, a_size);
    }

    static void __cdecl HookedAlignedFree(void* a_block) {
        mi_free(a_block);
    }

    static size_t __cdecl HookedMsize(void* a_block) {
        return mi_usable_size(a_block);
    }

    // Try multiple CRT DLL names that Skyrim might use
    static const char* CRT_DLLS[] = {
        "api-ms-win-crt-heap-l1-1-0.dll",
        "ucrtbase.dll",
        "msvcrt.dll",
    };

    static bool TryHookCRT(const char* a_crtDll) {
        bool anySuccess = false;

        auto orig = IATHook::Apply(a_crtDll, "malloc", reinterpret_cast<void*>(&HookedMalloc));
        if (orig) { OriginalMalloc = reinterpret_cast<decltype(OriginalMalloc)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "calloc", reinterpret_cast<void*>(&HookedCalloc));
        if (orig) { OriginalCalloc = reinterpret_cast<decltype(OriginalCalloc)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "realloc", reinterpret_cast<void*>(&HookedRealloc));
        if (orig) { OriginalRealloc = reinterpret_cast<decltype(OriginalRealloc)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "free", reinterpret_cast<void*>(&HookedFree));
        if (orig) { OriginalFree = reinterpret_cast<decltype(OriginalFree)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "_aligned_malloc", reinterpret_cast<void*>(&HookedAlignedMalloc));
        if (orig) { OriginalAlignedMalloc = reinterpret_cast<decltype(OriginalAlignedMalloc)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "_aligned_free", reinterpret_cast<void*>(&HookedAlignedFree));
        if (orig) { OriginalAlignedFree = reinterpret_cast<decltype(OriginalAlignedFree)>(orig); anySuccess = true; }

        orig = IATHook::Apply(a_crtDll, "_msize", reinterpret_cast<void*>(&HookedMsize));
        if (orig) { OriginalMsize = reinterpret_cast<decltype(OriginalMsize)>(orig); anySuccess = true; }

        return anySuccess;
    }

    void Install() {
        for (const auto* crtDll : CRT_DLLS) {
            if (TryHookCRT(crtDll)) {
                spdlog::info("AllocatorReplacement: CRT heap functions replaced with mimalloc via {}", crtDll);
                return;
            }
        }

        spdlog::warn("AllocatorReplacement: could not find CRT heap imports to replace");
    }
}
