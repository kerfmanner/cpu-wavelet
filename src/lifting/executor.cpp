#include "lifting/executor.hpp"

namespace ttwv::cpu {

Executor::Executor(
    const ExecutionPolicy policy,
    const size_t stencil_parallel_threshold,
    const int thread_count,
    const size_t scale_parallel_threshold,
    const size_t batch_parallel_threshold) noexcept :
    policy_(policy),
    stencil_parallel_threshold_(stencil_parallel_threshold),
    scale_parallel_threshold_(scale_parallel_threshold),
    batch_parallel_threshold_(batch_parallel_threshold),
    thread_count_(thread_count) {}

}  // namespace ttwv::cpu
