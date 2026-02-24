#pragma once

namespace Benchmark {
    class Timer {
    public:
        explicit Timer(std::string_view a_name);
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        double ElapsedMs() const;

    private:
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;
    };
}
