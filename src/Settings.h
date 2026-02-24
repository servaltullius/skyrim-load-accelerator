#pragma once

namespace Settings {
    // General
    inline bool bEnableLogging = true;
    inline bool bEnableBenchmark = false;

    // Decompression
    inline bool bReplaceZlib = true;
    inline bool bUpgradeLZ4 = true;

    // Memory
    inline bool bReplaceMalloc = false;

    // FileIO
    inline bool bCacheINI = true;
    inline bool bMaxStdio = true;
    inline std::int32_t iMaxStdioLimit = 8192;
    inline bool bSequentialScan = true;

    // Caching
    inline bool bFormCache = false;

    // Threading
    inline bool bParallelBSA = true;
    inline std::int32_t iThreadPoolSize = 0;

    void Load();
}
