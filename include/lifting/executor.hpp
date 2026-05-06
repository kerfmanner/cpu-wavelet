#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(CPU_WAVELET_USE_OPENMP)
#include <omp.h>
#endif

namespace ttwv::cpu {

enum class ExecutionPolicy {
    kSerial,
    kOpenMP,
};

inline constexpr size_t kDefaultStencilParallelThreshold = 32768;
inline constexpr size_t kDefaultScaleParallelThreshold = 131072;
inline constexpr size_t kDefaultBatchParallelThreshold = 1;

struct IndexRange {
    size_t begin = 0;
    size_t end = 0;
};

[[nodiscard]] inline IndexRange static_chunk(
    const size_t begin,
    const size_t end,
    const size_t worker_index,
    const size_t worker_count) noexcept {
    if (end <= begin || worker_count == 0 || worker_index >= worker_count) {
        return IndexRange{begin, begin};
    }

    const size_t count = end - begin;
    const size_t chunk = (count + worker_count - 1) / worker_count;
    const size_t local_begin_offset = std::min(worker_index * chunk, count);
    const size_t local_end_offset = std::min(local_begin_offset + chunk, count);
    return IndexRange{begin + local_begin_offset, begin + local_end_offset};
}

class Executor {
   public:
    explicit Executor(
        ExecutionPolicy policy = ExecutionPolicy::kSerial,
        size_t stencil_parallel_threshold = kDefaultStencilParallelThreshold,
        int thread_count = 0,
        size_t scale_parallel_threshold = kDefaultScaleParallelThreshold,
        size_t batch_parallel_threshold = kDefaultBatchParallelThreshold) noexcept;

    [[nodiscard]] ExecutionPolicy policy() const noexcept { return policy_; }
    [[nodiscard]] size_t parallel_threshold() const noexcept { return stencil_parallel_threshold_; }
    [[nodiscard]] size_t stencil_parallel_threshold() const noexcept { return stencil_parallel_threshold_; }
    [[nodiscard]] size_t scale_parallel_threshold() const noexcept { return scale_parallel_threshold_; }
    [[nodiscard]] size_t batch_parallel_threshold() const noexcept { return batch_parallel_threshold_; }
    [[nodiscard]] int thread_count() const noexcept { return thread_count_; }

    template <class Fn>
    void for_chunks_with_threshold(const size_t begin, const size_t end, const size_t threshold, Fn&& fn) const {
        if (end <= begin) {
            return;
        }

        const size_t count = end - begin;
        if (policy_ == ExecutionPolicy::kSerial || count < threshold) {
            fn(begin, end);
            return;
        }

#if defined(CPU_WAVELET_USE_OPENMP)
        if (policy_ == ExecutionPolicy::kOpenMP) {
            if (thread_count_ > 0) {
#pragma omp parallel num_threads(thread_count_)
                {
                    const auto range = static_chunk(
                        begin,
                        end,
                        static_cast<size_t>(omp_get_thread_num()),
                        static_cast<size_t>(omp_get_num_threads()));
                    if (range.begin < range.end) {
                        fn(range.begin, range.end);
                    }
                }
            } else {
#pragma omp parallel
                {
                    const auto range = static_chunk(
                        begin,
                        end,
                        static_cast<size_t>(omp_get_thread_num()),
                        static_cast<size_t>(omp_get_num_threads()));
                    if (range.begin < range.end) {
                        fn(range.begin, range.end);
                    }
                }
            }
            return;
        }
#endif

        fn(begin, end);
    }

    template <class Fn>
    void for_each_with_threshold(const size_t begin, const size_t end, const size_t threshold, Fn&& fn) const {
        for_chunks_with_threshold(begin, end, threshold, [&](const size_t chunk_begin, const size_t chunk_end) {
            for (size_t i = chunk_begin; i < chunk_end; ++i) {
                fn(i);
            }
        });
    }

    template <class Fn>
    void for_each(const size_t begin, const size_t end, Fn&& fn) const {
        for_each_stencil(begin, end, std::forward<Fn>(fn));
    }

    template <class Fn>
    void for_each_stencil(const size_t begin, const size_t end, Fn&& fn) const {
        for_each_with_threshold(begin, end, stencil_parallel_threshold_, std::forward<Fn>(fn));
    }

    template <class Fn>
    void for_each_scale(const size_t begin, const size_t end, Fn&& fn) const {
        for_each_with_threshold(begin, end, scale_parallel_threshold_, std::forward<Fn>(fn));
    }

    template <class Fn>
    void for_each_batch(const size_t begin, const size_t end, Fn&& fn) const {
        for_each_with_threshold(begin, end, batch_parallel_threshold_, std::forward<Fn>(fn));
    }

    template <class Fn>
    void for_chunks_stencil(const size_t begin, const size_t end, Fn&& fn) const {
        for_chunks_with_threshold(begin, end, stencil_parallel_threshold_, std::forward<Fn>(fn));
    }

    template <class Fn>
    void for_chunks_scale(const size_t begin, const size_t end, Fn&& fn) const {
        for_chunks_with_threshold(begin, end, scale_parallel_threshold_, std::forward<Fn>(fn));
    }

   private:
    ExecutionPolicy policy_{ExecutionPolicy::kSerial};
    size_t stencil_parallel_threshold_{kDefaultStencilParallelThreshold};
    size_t scale_parallel_threshold_{kDefaultScaleParallelThreshold};
    size_t batch_parallel_threshold_{kDefaultBatchParallelThreshold};
    int thread_count_{0};
};

}  // namespace ttwv::cpu
