#include "Benchmark.h"
#include "Settings.h"

namespace Benchmark {
    Timer::Timer(std::string_view a_name)
        : name_(a_name)
        , start_(std::chrono::high_resolution_clock::now()) {}

    Timer::~Timer() {
        if (Settings::bEnableBenchmark) {
            auto elapsed = ElapsedMs();
            spdlog::info("[Benchmark] {} took {:.3f} ms", name_, elapsed);
        }
    }

    double Timer::ElapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }
}
