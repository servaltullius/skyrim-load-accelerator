#include "ParallelBSA.h"
#include "Settings.h"

#include <latch>
#include <semaphore>
#include <libdeflate.h>
#include <lz4.h>

namespace ParallelBSA {
    // Thread pool for decompression work
    class DecompressPool {
    public:
        static DecompressPool& Get() {
            static DecompressPool instance;
            return instance;
        }

        void Initialize(int a_threadCount) {
            if (a_threadCount <= 0) {
                a_threadCount = static_cast<int>(std::thread::hardware_concurrency()) - 1;
                if (a_threadCount < 1) a_threadCount = 1;
            }

            running_.store(true, std::memory_order_release);

            for (int i = 0; i < a_threadCount; ++i) {
                workers_.emplace_back([this] { WorkerLoop(); });
            }

            spdlog::info("ParallelBSA: thread pool initialized with {} threads", a_threadCount);
        }

        ~DecompressPool() {
            Shutdown();
        }

        struct Task {
            const void* compressedData;
            size_t compressedSize;
            void* outputData;
            size_t outputSize;
            bool isZlib;  // true = zlib, false = LZ4
            std::atomic<bool> success{false};
        };

        void Submit(std::vector<Task*>& a_tasks) {
            if (a_tasks.empty()) return;

            std::latch completionLatch(static_cast<std::ptrdiff_t>(a_tasks.size()));

            {
                std::lock_guard lock(queueMutex_);
                for (auto* task : a_tasks) {
                    queue_.push_back({task, &completionLatch});
                }
            }
            cv_.notify_all();

            completionLatch.wait();
        }

        void Shutdown() {
            running_.store(false, std::memory_order_release);
            cv_.notify_all();

            for (auto& worker : workers_) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            workers_.clear();
        }

    private:
        struct QueueItem {
            Task* task;
            std::latch* latch;
        };

        void WorkerLoop() {
            // Thread-local decompressors
            thread_local auto* zlibDecompressor = libdeflate_alloc_decompressor();

            while (running_.load(std::memory_order_acquire)) {
                QueueItem item{};

                {
                    std::unique_lock lock(queueMutex_);
                    cv_.wait(lock, [this] {
                        return !queue_.empty() || !running_.load(std::memory_order_acquire);
                    });

                    if (!running_.load(std::memory_order_acquire) && queue_.empty()) {
                        break;
                    }

                    if (queue_.empty()) continue;

                    item = queue_.front();
                    queue_.pop_front();
                }

                // Decompress
                bool success = false;
                if (item.task->isZlib && zlibDecompressor) {
                    size_t actualSize = 0;
                    auto result = libdeflate_zlib_decompress(
                        zlibDecompressor,
                        item.task->compressedData, item.task->compressedSize,
                        item.task->outputData, item.task->outputSize,
                        &actualSize);
                    success = (result == LIBDEFLATE_SUCCESS);
                } else if (!item.task->isZlib) {
                    auto result = LZ4_decompress_safe(
                        static_cast<const char*>(item.task->compressedData),
                        static_cast<char*>(item.task->outputData),
                        static_cast<int>(item.task->compressedSize),
                        static_cast<int>(item.task->outputSize));
                    success = (result >= 0);
                }

                item.task->success.store(success, std::memory_order_release);
                item.latch->count_down();
            }
        }

        std::vector<std::jthread> workers_;
        std::deque<QueueItem> queue_;
        std::mutex queueMutex_;
        std::condition_variable cv_;
        std::atomic<bool> running_{false};
    };

    void Install() {
        auto threadCount = Settings::iThreadPoolSize;
        DecompressPool::Get().Initialize(threadCount);

        // Note: The actual BSA decompression hooking requires finding the BSA loading
        // functions in Skyrim's code. This varies by version.
        // The pool is ready for use; integration with BSA loading hooks would be done
        // by intercepting the BSA block decompression loop.

        // For CommonLibSSE-NG, we can hook BSResource::ArchiveStream functions
        // REL::RelocationID would be used to find the correct addresses

        spdlog::info("ParallelBSA: decompression thread pool ready");
        spdlog::info("ParallelBSA: BSA loading hook integration requires version-specific addresses");
    }
}
