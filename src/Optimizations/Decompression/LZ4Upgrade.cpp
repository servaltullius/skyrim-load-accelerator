#include "LZ4Upgrade.h"
#include "Utils/IATHook.h"

#include <lz4.h>

namespace LZ4Upgrade {
    // Original function pointer
    using LZ4_decompress_safe_t = int(__cdecl*)(const char*, char*, int, int);
    static LZ4_decompress_safe_t OriginalDecompressSafe = nullptr;

    static int __cdecl HookedDecompressSafe(const char* a_src, char* a_dst, int a_compressedSize, int a_dstCapacity) {
        // Use the latest LZ4 library's decompress function (with AVX2/SSE4.2 optimizations)
        int result = LZ4_decompress_safe(a_src, a_dst, a_compressedSize, a_dstCapacity);

        if (result < 0 && OriginalDecompressSafe) {
            // Fallback to original if our version fails
            spdlog::trace("LZ4Upgrade: decompress failed with {}, trying original", result);
            return OriginalDecompressSafe(a_src, a_dst, a_compressedSize, a_dstCapacity);
        }

        return result;
    }

    void Install() {
        // Skyrim AE links against an internal LZ4 or a bundled DLL
        // Try to hook the IAT entry
        auto original = IATHook::Apply("lz4.dll", "LZ4_decompress_safe", reinterpret_cast<void*>(&HookedDecompressSafe));
        if (original) {
            OriginalDecompressSafe = reinterpret_cast<LZ4_decompress_safe_t>(original);
            spdlog::info("LZ4Upgrade: LZ4_decompress_safe upgraded to v{}", LZ4_versionString());
            return;
        }

        // Skyrim might statically link LZ4 - try pattern scanning
        // The LZ4 decompress function has a recognizable pattern
        // For now, log that IAT hooking failed and suggest alternative approaches
        spdlog::warn("LZ4Upgrade: could not find LZ4 import in IAT");
        spdlog::info("LZ4Upgrade: Skyrim may statically link LZ4; pattern-based hooking may be needed");

        spdlog::info("LZ4Upgrade: linked against LZ4 v{}, ready for pattern-based hooking if needed", LZ4_versionString());
    }
}
