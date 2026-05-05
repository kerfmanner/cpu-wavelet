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
[[nodiscard]] Workspace<T> allocate_workspace(const LiftingPlan& plan);

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

}  // namespace ttwv::cpu
