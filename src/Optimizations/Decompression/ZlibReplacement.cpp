#include "ZlibReplacement.h"
#include "Utils/IATHook.h"
#include "Utils/PatternScan.h"

#include <libdeflate.h>

namespace ZlibReplacement {
    // zlib types and constants
    using Bytef = unsigned char;
    using uLongf = unsigned long;
    using uLong = unsigned long;

    constexpr int Z_OK = 0;
    constexpr int Z_DATA_ERROR = -3;
    constexpr int Z_MEM_ERROR = -4;
    constexpr int Z_BUF_ERROR = -5;

    // zlib stream-based types
    struct z_stream {
        const Bytef* next_in;
        uLong avail_in;
        uLong total_in;
        Bytef* next_out;
        uLong avail_out;
        uLong total_out;
        const char* msg;
        void* state;
        void* zalloc;
        void* zfree;
        void* opaque;
        int data_type;
        uLong adler;
        uLong reserved;
    };

    // Original function pointers
    using uncompress_t = int(__cdecl*)(Bytef*, uLongf*, const Bytef*, uLong);
    using inflate_t = int(__cdecl*)(z_stream*, int);
    using inflateInit2_t = int(__cdecl*)(z_stream*, int, const char*, int);
    using inflateEnd_t = int(__cdecl*)(z_stream*);

    static uncompress_t OriginalUncompress = nullptr;
    static std::uintptr_t TrampolineTarget = 0;

    // thread_local decompressor for thread safety
    static libdeflate_decompressor* GetDecompressor() {
        thread_local auto* decompressor = libdeflate_alloc_decompressor();
        return decompressor;
    }

    static int __cdecl HookedUncompress(Bytef* a_dest, uLongf* a_destLen, const Bytef* a_source, uLong a_sourceLen) {
        auto* decompressor = GetDecompressor();
        if (!decompressor) {
            if (OriginalUncompress) {
                return OriginalUncompress(a_dest, a_destLen, a_source, a_sourceLen);
            }
            return Z_MEM_ERROR;
        }

        size_t actualSize = 0;
        auto result = libdeflate_zlib_decompress(
            decompressor,
            a_source, static_cast<size_t>(a_sourceLen),
            a_dest, static_cast<size_t>(*a_destLen),
            &actualSize);

        switch (result) {
            case LIBDEFLATE_SUCCESS:
                *a_destLen = static_cast<uLongf>(actualSize);
                return Z_OK;

            case LIBDEFLATE_BAD_DATA:
            case LIBDEFLATE_INSUFFICIENT_SPACE:
            default:
                if (OriginalUncompress) {
                    return OriginalUncompress(a_dest, a_destLen, a_source, a_sourceLen);
                }
                return (result == LIBDEFLATE_BAD_DATA) ? Z_DATA_ERROR : Z_BUF_ERROR;
        }
    }

    static bool TryIATHook() {
        for (const char* zlibName : {"zlibx64.dll", "zlib1.dll", "zlib.dll"}) {
            auto original = IATHook::Apply(zlibName, "uncompress", reinterpret_cast<void*>(&HookedUncompress));
            if (original) {
                OriginalUncompress = reinterpret_cast<uncompress_t>(original);
                spdlog::info("ZlibReplacement: hooked uncompress via IAT ({})", zlibName);
                return true;
            }
        }
        return false;
    }

    static bool TryTrampolineHook() {
        // Skyrim's internal BSA decompression wrapper that calls zlib uncompress
        // These REL IDs point to the function that decompresses BSA zlib blocks
        // BSResource::CompressedArchiveStream::DoRead or similar
        try {
            REL::RelocationID decompressFunc(
                69529,   // SE 1.5.97 - BSResource archive decompression
                71106    // AE 1.6.x
            );

            auto& trampoline = SKSE::GetTrampoline();
            SKSE::AllocTrampoline(64);

            OriginalUncompress = reinterpret_cast<uncompress_t>(
                trampoline.write_call<5>(
                    decompressFunc.address(),
                    reinterpret_cast<std::uintptr_t>(&HookedUncompress)));

            if (OriginalUncompress) {
                spdlog::info("ZlibReplacement: hooked decompression via trampoline (REL::ID)");
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("ZlibReplacement: REL::ID trampoline failed: {}", e.what());
        }
        return false;
    }

    static bool TryPatternScan() {
        // Pattern for zlib uncompress function prologue
        // This is a common pattern for the start of zlib's uncompress implementation
        // mov [rsp+...], rbx; push rdi; sub rsp, ... pattern
        const char* patterns[] = {
            // uncompress2 prologue (zlib 1.2.11+)
            "48 89 5C 24 ?? 57 48 83 EC 40 48 8B DA 48 8B F9 48 8D 4C 24 ?? 41 8B D0",
            // Alternative: call to inflateInit2_ followed by inflate
            "E8 ?? ?? ?? ?? 85 C0 75 ?? 48 8D 4C 24 ?? 6A 04 E8 ?? ?? ?? ??",
        };

        for (const auto* pattern : patterns) {
            auto addr = PatternScan::Find(pattern);
            if (addr) {
                auto& trampoline = SKSE::GetTrampoline();
                SKSE::AllocTrampoline(64);

                OriginalUncompress = reinterpret_cast<uncompress_t>(
                    trampoline.write_branch<5>(addr, reinterpret_cast<std::uintptr_t>(&HookedUncompress)));

                spdlog::info("ZlibReplacement: hooked uncompress via pattern scan at 0x{:X}", addr);
                return true;
            }
        }

        return false;
    }

    void Install() {
        // Strategy 1: IAT hook (works when zlib is dynamically linked)
        if (TryIATHook()) return;

        // Strategy 2: Trampoline via REL::ID (version-specific, most reliable)
        if (TryTrampolineHook()) return;

        // Strategy 3: Pattern scan (version-independent fallback)
        if (TryPatternScan()) return;

        spdlog::warn("ZlibReplacement: all hooking strategies failed, module inactive");
    }
}
