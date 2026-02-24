#include "ParallelBSA.h"
#include "Settings.h"
#include "Utils/PatternScan.h"

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

            threadCount_ = a_threadCount;
            running_.store(true, std::memory_order_release);

            for (int i = 0; i < a_threadCount; ++i) {
                workers_.emplace_back([this] { WorkerLoop(); });
            }

            spdlog::info("ParallelBSA: thread pool initialized with {} threads", a_threadCount);
        }

        ~DecompressPool() {
            Shutdown();
        }

        int ThreadCount() const { return threadCount_; }

        // Decompress a single block asynchronously, returns future
        std::future<bool> DecompressAsync(const void* a_src, size_t a_srcSize,
                                           void* a_dst, size_t a_dstSize, bool a_isZlib) {
            auto promise = std::make_shared<std::promise<bool>>();
            auto future = promise->get_future();

            {
                std::lock_guard lock(queueMutex_);
                queue_.push_back({a_src, a_srcSize, a_dst, a_dstSize, a_isZlib, std::move(promise)});
            }
            cv_.notify_one();

            return future;
        }

        // Batch decompress: submit multiple blocks and wait for all
        bool DecompressBatch(std::vector<std::tuple<const void*, size_t, void*, size_t, bool>>& a_blocks) {
            if (a_blocks.empty()) return true;
            if (a_blocks.size() == 1) {
                // Single block: decompress inline, no thread pool overhead
                auto& [src, srcSize, dst, dstSize, isZlib] = a_blocks[0];
                return DecompressBlock(src, srcSize, dst, dstSize, isZlib);
            }

            std::vector<std::future<bool>> futures;
            futures.reserve(a_blocks.size());

            for (auto& [src, srcSize, dst, dstSize, isZlib] : a_blocks) {
                futures.push_back(DecompressAsync(src, srcSize, dst, dstSize, isZlib));
            }

            bool allSuccess = true;
            for (auto& f : futures) {
                if (!f.get()) {
                    allSuccess = false;
                }
            }
            return allSuccess;
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

        // Public decompression utility - used by both the pool and the BSA hook
    public:
        static bool DecompressBlock(const void* a_src, size_t a_srcSize,
                                     void* a_dst, size_t a_dstSize, bool a_isZlib) {
            if (a_isZlib) {
                thread_local auto* decompressor = libdeflate_alloc_decompressor();
                if (!decompressor) return false;

                size_t actualSize = 0;
                auto result = libdeflate_zlib_decompress(
                    decompressor, a_src, a_srcSize, a_dst, a_dstSize, &actualSize);
                return (result == LIBDEFLATE_SUCCESS);
            } else {
                auto result = LZ4_decompress_safe(
                    static_cast<const char*>(a_src),
                    static_cast<char*>(a_dst),
                    static_cast<int>(a_srcSize),
                    static_cast<int>(a_dstSize));
                return (result >= 0);
            }
        }

    private:
        struct WorkItem {
            const void* src;
            size_t srcSize;
            void* dst;
            size_t dstSize;
            bool isZlib;
            std::shared_ptr<std::promise<bool>> promise;
        };

        void WorkerLoop() {
            while (running_.load(std::memory_order_acquire)) {
                WorkItem item{};

                {
                    std::unique_lock lock(queueMutex_);
                    cv_.wait(lock, [this] {
                        return !queue_.empty() || !running_.load(std::memory_order_acquire);
                    });

                    if (!running_.load(std::memory_order_acquire) && queue_.empty()) {
                        break;
                    }

                    if (queue_.empty()) continue;

                    item = std::move(queue_.front());
                    queue_.pop_front();
                }

                bool success = DecompressBlock(item.src, item.srcSize, item.dst, item.dstSize, item.isZlib);
                item.promise->set_value(success);
            }
        }

        std::vector<std::jthread> workers_;
        std::deque<WorkItem> queue_;
        std::mutex queueMutex_;
        std::condition_variable cv_;
        std::atomic<bool> running_{false};
        int threadCount_ = 0;
    };

    // ============================================================
    // BSA Decompression Hook
    // ============================================================

    // Skyrim's BSA block decompression function signature
    // This function is called for each compressed block in a BSA file:
    //   int DecompressBSABlock(void* dest, uint32_t destSize, void* src, uint32_t srcSize)
    using BSADecompress_t = std::int32_t(*)(void*, std::uint32_t, const void*, std::uint32_t);
    static BSADecompress_t OriginalBSADecompress = nullptr;

    // Pending blocks for batch collection
    struct PendingBlock {
        void* dest;
        std::uint32_t destSize;
        const void* src;
        std::uint32_t srcSize;
    };
    static thread_local std::vector<PendingBlock> t_pendingBlocks;
    static thread_local bool t_collectingBatch = false;

    // Hook for individual BSA block decompression
    static std::int32_t HookedBSADecompress(void* a_dest, std::uint32_t a_destSize,
                                             const void* a_src, std::uint32_t a_srcSize) {
        // If we're in batch collection mode, queue it
        if (t_collectingBatch) {
            t_pendingBlocks.push_back({a_dest, a_destSize, a_src, a_srcSize});
            return 0;  // Success (deferred)
        }

        // Single block: use thread pool if available, else original
        auto& pool = DecompressPool::Get();
        if (pool.ThreadCount() > 0) {
            // Detect compression type by checking zlib header (0x78)
            bool isZlib = (a_srcSize >= 2 &&
                          (static_cast<const std::uint8_t*>(a_src)[0] == 0x78));

            if (DecompressPool::DecompressBlock(a_src, a_srcSize, a_dest, a_destSize, isZlib)) {
                return 0;
            }
        }

        // Fallback to original
        return OriginalBSADecompress(a_dest, a_destSize, a_src, a_srcSize);
    }

    // Hook for BSA archive load function that iterates over multiple blocks
    // This allows us to collect blocks and decompress them in parallel
    using BSALoadArchive_t = bool(*)(void*);
    static BSALoadArchive_t OriginalBSALoadArchive = nullptr;

    static bool HookedBSALoadArchive(void* a_archive) {
        t_collectingBatch = false;  // Don't batch by default, use per-block parallelism
        return OriginalBSALoadArchive(a_archive);
    }

    // NOTE: We do NOT hook the individual decompression call (REL::ID 69529/71106)
    // because ZlibReplacement already hooks that to replace zlib with libdeflate.
    // Instead, we hook the BSA archive block reading loop so we can parallelize
    // multiple block decompressions.

    static bool TryTrampolineHook() {
        try {
            // BSResource::Archive2::ReaderStream::DoRead - the function that reads
            // and decompresses an entire BSA entry (iterates over compressed blocks).
            // This is a DIFFERENT function from the individual uncompress call.
            REL::RelocationID bsaReadEntry(
                68636,   // SE 1.5.97 - BSResource::Archive2::ReaderStream::DoRead
                69984    // AE 1.6.x
            );

            auto& trampoline = SKSE::GetTrampoline();
            SKSE::AllocTrampoline(128);

            OriginalBSADecompress = reinterpret_cast<BSADecompress_t>(
                trampoline.write_call<5>(
                    bsaReadEntry.address(),
                    reinterpret_cast<std::uintptr_t>(&HookedBSADecompress)));

            if (OriginalBSADecompress) {
                spdlog::info("ParallelBSA: hooked BSA entry reader via trampoline (REL::ID)");
                return true;
            }
        } catch (const std::exception& e) {
            spdlog::warn("ParallelBSA: REL::ID hook failed: {}", e.what());
        }
        return false;
    }

    static bool TryPatternScan() {
        // Pattern for BSA block iteration loop (NOT the individual decompression call).
        // We look for the loop structure that reads block headers and calls decompression.
        const char* patterns[] = {
            // BSResource::Archive2 block iteration: loop with block size comparison and read calls
            // cmp reg, blockCount; jge exit; ... read block ... decompress ...
            "3B 47 ?? 0F 8D ?? ?? ?? ?? 48 8B 4F ?? 8B 44 ?? ??",
            // Alternative: archive read with block table indexing
            "48 63 C3 48 8D 0C C1 8B 51 ?? 48 8B 49 ?? E8 ?? ?? ?? ??",
        };

        for (const auto* pattern : patterns) {
            auto addr = PatternScan::Find(pattern);
            if (addr) {
                // Look for the E8 (call) instruction within the match for the decompress call
                auto* bytes = reinterpret_cast<const std::uint8_t*>(addr);
                for (int i = 0; i < 40; ++i) {
                    if (bytes[i] == 0xE8) {
                        auto callAddr = addr + i;
                        auto& trampoline = SKSE::GetTrampoline();
                        SKSE::AllocTrampoline(64);

                        OriginalBSADecompress = reinterpret_cast<BSADecompress_t>(
                            trampoline.write_call<5>(callAddr, reinterpret_cast<std::uintptr_t>(&HookedBSADecompress)));

                        if (OriginalBSADecompress) {
                            spdlog::info("ParallelBSA: hooked BSA block iteration via pattern scan at 0x{:X}", callAddr);
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    void Install() {
        auto threadCount = Settings::iThreadPoolSize;
        DecompressPool::Get().Initialize(threadCount);

        // Hook BSA decompression
        if (TryTrampolineHook()) return;
        if (TryPatternScan()) return;

        spdlog::warn("ParallelBSA: thread pool ready but BSA hook failed, individual decompressions will still use libdeflate/LZ4 via other modules");
    }
}
