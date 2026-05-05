#pragma once

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

class Executor {
   public:
    explicit Executor(
        ExecutionPolicy policy = ExecutionPolicy::kSerial,
        size_t parallel_threshold = 4096,
        int thread_count = 0) noexcept;

    [[nodiscard]] ExecutionPolicy policy() const noexcept { return policy_; }
    [[nodiscard]] size_t parallel_threshold() const noexcept { return parallel_threshold_; }
    [[nodiscard]] int thread_count() const noexcept { return thread_count_; }

    template <class Fn>
    void for_each(const size_t begin, const size_t end, Fn&& fn) const {
        if (end <= begin) {
            return;
        }

        const size_t count = end - begin;
        if (policy_ == ExecutionPolicy::kSerial || count < parallel_threshold_) {
            for (size_t i = begin; i < end; ++i) {
                fn(i);
            }
            return;
        }

#if defined(CPU_WAVELET_USE_OPENMP)
        if (policy_ == ExecutionPolicy::kOpenMP) {
            const int64_t signed_begin = static_cast<int64_t>(begin);
            const int64_t signed_end = static_cast<int64_t>(end);
            if (thread_count_ > 0) {
#pragma omp parallel for schedule(static) num_threads(thread_count_)
                for (int64_t i = signed_begin; i < signed_end; ++i) {
                    fn(static_cast<size_t>(i));
                }
            } else {
#pragma omp parallel for schedule(static)
                for (int64_t i = signed_begin; i < signed_end; ++i) {
                    fn(static_cast<size_t>(i));
                }
            }
            return;
        }
#endif

        for (size_t i = begin; i < end; ++i) {
            fn(i);
        }
    }

   private:
    ExecutionPolicy policy_{ExecutionPolicy::kSerial};
    size_t parallel_threshold_{4096};
    int thread_count_{0};
};

}  // namespace ttwv::cpu
