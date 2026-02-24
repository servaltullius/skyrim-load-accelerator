#include "ZlibReplacement.h"
#include "Utils/IATHook.h"

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

    // Original function pointer
    using uncompress_t = int(__cdecl*)(Bytef*, uLongf*, const Bytef*, uLong);
    static uncompress_t OriginalUncompress = nullptr;

    // thread_local decompressor for thread safety
    static libdeflate_decompressor* GetDecompressor() {
        thread_local auto* decompressor = libdeflate_alloc_decompressor();
        return decompressor;
    }

    static int __cdecl HookedUncompress(Bytef* a_dest, uLongf* a_destLen, const Bytef* a_source, uLong a_sourceLen) {
        auto* decompressor = GetDecompressor();
        if (!decompressor) {
            spdlog::error("ZlibReplacement: failed to create libdeflate decompressor");
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
                spdlog::trace("ZlibReplacement: bad data, falling back to original zlib");
                if (OriginalUncompress) {
                    return OriginalUncompress(a_dest, a_destLen, a_source, a_sourceLen);
                }
                return Z_DATA_ERROR;

            case LIBDEFLATE_INSUFFICIENT_SPACE:
                spdlog::trace("ZlibReplacement: insufficient space, falling back to original zlib");
                if (OriginalUncompress) {
                    return OriginalUncompress(a_dest, a_destLen, a_source, a_sourceLen);
                }
                return Z_BUF_ERROR;

            default:
                if (OriginalUncompress) {
                    return OriginalUncompress(a_dest, a_destLen, a_source, a_sourceLen);
                }
                return Z_DATA_ERROR;
        }
    }

    void Install() {
        // Hook uncompress in the main executable
        auto original = IATHook::Apply("zlibx64.dll", "uncompress", reinterpret_cast<void*>(&HookedUncompress));
        if (original) {
            OriginalUncompress = reinterpret_cast<uncompress_t>(original);
            spdlog::info("ZlibReplacement: zlib uncompress replaced with libdeflate");
            return;
        }

        // Try alternative zlib DLL names
        for (const char* zlibName : {"zlib1.dll", "zlib.dll"}) {
            original = IATHook::Apply(zlibName, "uncompress", reinterpret_cast<void*>(&HookedUncompress));
            if (original) {
                OriginalUncompress = reinterpret_cast<uncompress_t>(original);
                spdlog::info("ZlibReplacement: hooked uncompress via {}", zlibName);
                return;
            }
        }

        // Try hooking inflate/inflateInit2_ for streaming decompression
        spdlog::warn("ZlibReplacement: could not find zlib uncompress import, decompression replacement inactive");
    }
}
