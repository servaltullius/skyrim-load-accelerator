#include "MaxStdio.h"
#include "Settings.h"

#include <cstdio>

namespace MaxStdio {
    void Install() {
        auto limit = Settings::iMaxStdioLimit;

        // Clamp to valid range
        if (limit < 512) limit = 512;
        if (limit > 8192) limit = 8192;

        auto previous = _getmaxstdio();
        if (previous >= limit) {
            spdlog::info("MaxStdio: current limit ({}) already >= requested ({}), skipping", previous, limit);
            return;
        }

        auto result = _setmaxstdio(limit);
        if (result == -1) {
            spdlog::error("MaxStdio: _setmaxstdio({}) failed", limit);
            return;
        }

        spdlog::info("MaxStdio: file handle limit increased from {} to {}", previous, result);
    }
}
