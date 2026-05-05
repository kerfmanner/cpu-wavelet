#include "lifting/executor.hpp"

namespace ttwv::cpu {

Executor::Executor(const ExecutionPolicy policy, const size_t parallel_threshold, const int thread_count) noexcept :
    policy_(policy), parallel_threshold_(parallel_threshold), thread_count_(thread_count) {}

}  // namespace ttwv::cpu
