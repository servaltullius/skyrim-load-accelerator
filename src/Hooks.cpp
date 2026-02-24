#include "Hooks.h"
#include "Settings.h"
#include "Utils/Benchmark.h"

#include "Optimizations/Decompression/ZlibReplacement.h"
#include "Optimizations/Decompression/LZ4Upgrade.h"
#include "Optimizations/Memory/AllocatorReplacement.h"
#include "Optimizations/FileIO/INICaching.h"
#include "Optimizations/FileIO/MaxStdio.h"
#include "Optimizations/FileIO/SequentialScan.h"
#include "Optimizations/Caching/FormCache.h"
#include "Optimizations/Threading/ParallelBSA.h"

namespace Hooks {
    static bool IsModuleLoaded(const char* a_name) {
        return GetModuleHandleA(a_name) != nullptr;
    }

    void Install() {
        Benchmark::Timer totalTimer("Total hook installation");

        // MaxStdio - always safe, no conflicts
        if (Settings::bMaxStdio) {
            Benchmark::Timer timer("MaxStdio");
            MaxStdio::Install();
        }

        // zlib -> libdeflate replacement
        if (Settings::bReplaceZlib) {
            Benchmark::Timer timer("ZlibReplacement");
            ZlibReplacement::Install();
        }

        // LZ4 upgrade
        if (Settings::bUpgradeLZ4) {
            Benchmark::Timer timer("LZ4Upgrade");
            LZ4Upgrade::Install();
        }

        // mimalloc - skip if SSE Engine Fixes is handling allocation
        if (Settings::bReplaceMalloc) {
            if (IsModuleLoaded("EngineFixes.dll")) {
                spdlog::warn("EngineFixes.dll detected, skipping malloc replacement to avoid conflict");
            } else {
                Benchmark::Timer timer("AllocatorReplacement");
                AllocatorReplacement::Install();
            }
        }

        // INI caching - skip if PrivateProfileRedirector is present
        if (Settings::bCacheINI) {
            if (IsModuleLoaded("PrivateProfileRedirector.dll")) {
                spdlog::warn("PrivateProfileRedirector detected, skipping INI caching");
            } else {
                Benchmark::Timer timer("INICaching");
                INICaching::Install();
            }
        }

        // Sequential scan hint for BSA/BA2 files
        if (Settings::bSequentialScan) {
            Benchmark::Timer timer("SequentialScan");
            SequentialScan::Install();
        }

        // Form cache - skip if SSE Engine Fixes handles it
        if (Settings::bFormCache) {
            if (IsModuleLoaded("EngineFixes.dll")) {
                spdlog::warn("EngineFixes.dll detected, skipping form caching to avoid conflict");
            } else {
                Benchmark::Timer timer("FormCache");
                FormCache::Install();
            }
        }

        // Parallel BSA decompression
        if (Settings::bParallelBSA) {
            Benchmark::Timer timer("ParallelBSA");
            ParallelBSA::Install();
        }
    }
}
