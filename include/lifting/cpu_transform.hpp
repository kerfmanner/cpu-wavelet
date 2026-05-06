#pragma once

#include <span>
#include <vector>

#include "lifting/executor.hpp"
#include "lifting/plan.hpp"
#include "lifting/scheme.hpp"
#include "lifting/shifted_signal.hpp"

namespace ttwv::cpu {

template <class T>
struct DwtResult {
    std::vector<T> approximation;
    std::vector<T> detail;
};

template <class T>
struct Workspace {
    SignalBuffer<T> even_a;
    SignalBuffer<T> even_b;
    SignalBuffer<T> odd_a;
    SignalBuffer<T> odd_b;
};

template <class T>
struct BatchWorkspace {
    std::vector<LiftingPlan> plans;
    std::vector<Workspace<T>> workspaces;
};

template <class T>
struct DwtBatchResult {
    std::vector<DwtResult<T>> signals;
};

template <class T>
[[nodiscard]] Workspace<T> allocate_workspace(const LiftingPlan& plan);

template <class T>
[[nodiscard]] BatchWorkspace<T> allocate_batch_workspace(
    std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme);

template <class T>
[[nodiscard]] DwtResult<T> dwt_forward_one_level(
    std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<T>& workspace,
    const Executor& executor);

template <class T>
[[nodiscard]] DwtResult<T> dwt_forward_one_level(
    std::span<const T> input,
    const LiftingScheme& scheme,
    const Executor& executor);

template <class T>
[[nodiscard]] DwtBatchResult<T> dwt_forward_one_level_batch(
    std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<T>& workspace,
    const Executor& batch_executor,
    const Executor& signal_executor);

template <class T>
[[nodiscard]] DwtBatchResult<T> dwt_forward_one_level_batch(
    std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<T>& workspace,
    const Executor& batch_executor);

template <class T>
[[nodiscard]] DwtBatchResult<T> dwt_forward_one_level_batch(
    std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    const Executor& batch_executor);

}  // namespace ttwv::cpu
