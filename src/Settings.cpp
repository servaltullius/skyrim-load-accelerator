#include "Settings.h"

#include <SimpleIni.h>

namespace Settings {
    void Load() {
        const auto path = std::filesystem::path("Data/SKSE/Plugins/SkyrimLoadAccelerator.ini");

        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(path.string().c_str()) < 0) {
            spdlog::warn("Failed to load settings from {}, using defaults", path.string());
            return;
        }

        // General
        bEnableLogging = ini.GetBoolValue("General", "bEnableLogging", bEnableLogging);
        bEnableBenchmark = ini.GetBoolValue("General", "bEnableBenchmark", bEnableBenchmark);

        // Decompression
        bReplaceZlib = ini.GetBoolValue("Decompression", "bReplaceZlib", bReplaceZlib);
        bUpgradeLZ4 = ini.GetBoolValue("Decompression", "bUpgradeLZ4", bUpgradeLZ4);

        // Memory
        bReplaceMalloc = ini.GetBoolValue("Memory", "bReplaceMalloc", bReplaceMalloc);

        // FileIO
        bCacheINI = ini.GetBoolValue("FileIO", "bCacheINI", bCacheINI);
        bMaxStdio = ini.GetBoolValue("FileIO", "bMaxStdio", bMaxStdio);
        iMaxStdioLimit = static_cast<std::int32_t>(ini.GetLongValue("FileIO", "iMaxStdioLimit", iMaxStdioLimit));
        bSequentialScan = ini.GetBoolValue("FileIO", "bSequentialScan", bSequentialScan);

        // Caching
        bFormCache = ini.GetBoolValue("Caching", "bFormCache", bFormCache);

        // Threading
        bParallelBSA = ini.GetBoolValue("Threading", "bParallelBSA", bParallelBSA);
        iThreadPoolSize = static_cast<std::int32_t>(ini.GetLongValue("Threading", "iThreadPoolSize", iThreadPoolSize));

        spdlog::info("Settings loaded successfully");
        spdlog::info("  Decompression: zlib={}, LZ4={}", bReplaceZlib, bUpgradeLZ4);
        spdlog::info("  Memory: malloc={}", bReplaceMalloc);
        spdlog::info("  FileIO: INI={}, MaxStdio={} ({}), SeqScan={}", bCacheINI, bMaxStdio, iMaxStdioLimit, bSequentialScan);
        spdlog::info("  Caching: Form={}", bFormCache);
        spdlog::info("  Threading: BSA={}, Threads={}", bParallelBSA, iThreadPoolSize);
    }
}
