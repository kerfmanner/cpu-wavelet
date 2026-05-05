#include "lifting/cpu_transform.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

#include "lifting/indexing.hpp"

namespace ttwv::cpu {

namespace {

template <class T>
void validate_plan_for_input(const std::span<const T> input, const LiftingScheme& scheme, const LiftingPlan& plan) {
    if (input.empty()) {
        throw std::invalid_argument("input signal must be non-empty");
    }
    if (plan.input_len != input.size()) {
        throw std::invalid_argument("lifting plan input length does not match input signal length");
    }
    if (plan.steps.size() != scheme.steps.size()) {
        throw std::invalid_argument("lifting plan step count does not match lifting scheme step count");
    }
}

template <class T>
void initialize_even_odd(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    SignalBuffer<T>& even,
    SignalBuffer<T>& odd,
    const Executor& executor) {
    const int64_t pad = static_cast<int64_t>(scheme.tap_size - 1);
    const uint64_t input_len = static_cast<uint64_t>(input.size());

    even.length = plan.initial_even_len;
    even.shift = scheme.delay_even;
    odd.length = plan.initial_odd_len;
    odd.shift = scheme.delay_odd;

    executor.for_each(0, even.length, [&](const size_t i) {
        const int64_t padded_index = static_cast<int64_t>(2 * i) - pad;
        const uint64_t source_index = symmetric_index(padded_index, input_len);
        even.data[i] = input[static_cast<size_t>(source_index)];
    });

    executor.for_each(0, odd.length, [&](const size_t i) {
        const int64_t padded_index = static_cast<int64_t>(2 * i + 1) - pad;
        const uint64_t source_index = symmetric_index(padded_index, input_len);
        odd.data[i] = input[static_cast<size_t>(source_index)];
    });
}

template <class T>
void validate_stencil_bounds(
    const StepPlan& plan,
    const SignalBuffer<T>& source,
    const SignalBuffer<T>& base,
    const size_t k) {
    if (k == 0) {
        throw std::invalid_argument("predict/update step must contain coefficients");
    }
    if (plan.base_offset + plan.out_len > base.length) {
        throw std::runtime_error("planned base range exceeds base signal length");
    }
    if (plan.out_len > 0 && plan.source_offset + plan.out_len + k - 1 > source.length) {
        throw std::runtime_error("planned source range exceeds source signal length");
    }
}

template <class T>
void apply_stencil_add(
    const StepPlan& plan,
    const std::span<const float> coeffs,
    const SignalBuffer<T>& source,
    const SignalBuffer<T>& base,
    SignalBuffer<T>& output,
    const Executor& executor) {
    const size_t k = coeffs.size();
    validate_stencil_bounds(plan, source, base, k);

    if (output.data.data() == source.data.data() || output.data.data() == base.data.data()) {
        throw std::runtime_error("predict/update output buffer aliases an input buffer");
    }

    output.length = plan.out_len;
    output.shift = plan.out_shift;

    switch (k) {
        case 1: {
            const T c0 = static_cast<T>(coeffs[0]);
            executor.for_each(0, plan.out_len, [&](const size_t p) {
                const T stencil = c0 * source.data[plan.source_offset + p];
                output.data[p] = base.data[plan.base_offset + p] + stencil;
            });
            return;
        }
        case 2: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            executor.for_each(0, plan.out_len, [&](const size_t p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source.data[src + 1];
                const T term1 = c1 * source.data[src];
                const T stencil = term0 + term1;
                output.data[p] = base.data[plan.base_offset + p] + stencil;
            });
            return;
        }
        case 3: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            const T c2 = static_cast<T>(coeffs[2]);
            executor.for_each(0, plan.out_len, [&](const size_t p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source.data[src + 2];
                const T term1 = c1 * source.data[src + 1];
                const T term2 = c2 * source.data[src];
                const T stencil = static_cast<T>(static_cast<T>(term0 + term1) + term2);
                output.data[p] = base.data[plan.base_offset + p] + stencil;
            });
            return;
        }
        case 4: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            const T c2 = static_cast<T>(coeffs[2]);
            const T c3 = static_cast<T>(coeffs[3]);
            executor.for_each(0, plan.out_len, [&](const size_t p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source.data[src + 3];
                const T term1 = c1 * source.data[src + 2];
                const T term2 = c2 * source.data[src + 1];
                const T term3 = c3 * source.data[src];
                const T stencil = static_cast<T>(static_cast<T>(static_cast<T>(term0 + term1) + term2) + term3);
                output.data[p] = base.data[plan.base_offset + p] + stencil;
            });
            return;
        }
        default:
            executor.for_each(0, plan.out_len, [&](const size_t p) {
                const size_t src = plan.source_offset + p;
                T stencil = T{0};
                for (size_t j = 0; j < k; ++j) {
                    const T term = static_cast<T>(coeffs[j]) * source.data[src + k - 1 - j];
                    stencil = static_cast<T>(stencil + term);
                }
                output.data[p] = base.data[plan.base_offset + p] + stencil;
            });
            return;
    }
}

template <class T>
void apply_scale(const StepPlan& plan, const float raw_factor, SignalBuffer<T>& signal, const Executor& executor) {
    if (plan.out_len != signal.length || plan.out_shift != signal.shift) {
        throw std::runtime_error("scale plan does not match active signal metadata");
    }

    const T factor = static_cast<T>(raw_factor);
    if (factor == T{1}) {
        return;
    }

    executor.for_each(0, signal.length, [&](const size_t i) { signal.data[i] *= factor; });
}

template <class T>
[[nodiscard]] std::vector<T> crop_stream(
    const SignalBuffer<T>& signal,
    const size_t output_len,
    const int direct_shift) {
    const int relative_shift = signal.shift - direct_shift;
    if (relative_shift > 0) {
        throw std::runtime_error("coefficients have positive shift after forward lifting");
    }

    const size_t start = static_cast<size_t>(-relative_shift);
    if (start + output_len > signal.length) {
        throw std::runtime_error("final crop exceeds signal length");
    }

    return std::vector<T>(
        signal.data.begin() + static_cast<std::ptrdiff_t>(start),
        signal.data.begin() + static_cast<std::ptrdiff_t>(start + output_len));
}

}  // namespace

template <class T>
Workspace<T> allocate_workspace(const LiftingPlan& plan) {
    const size_t max_stream_len = std::max(plan.max_even_len, plan.max_odd_len);
    Workspace<T> workspace;
    workspace.even_a.reserve_length(max_stream_len);
    workspace.even_b.reserve_length(max_stream_len);
    workspace.odd_a.reserve_length(max_stream_len);
    workspace.odd_b.reserve_length(max_stream_len);
    return workspace;
}

template <class T>
DwtResult<T> dwt_forward_one_level(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<T>& workspace,
    const Executor& executor) {
    validate_plan_for_input(input, scheme, plan);
    initialize_even_odd(input, scheme, plan, workspace.even_a, workspace.odd_a, executor);

    workspace.even_b.length = 0;
    workspace.even_b.shift = scheme.delay_even;
    workspace.odd_b.length = 0;
    workspace.odd_b.shift = scheme.delay_odd;

    SignalBuffer<T>* even_cur = &workspace.even_a;
    SignalBuffer<T>* even_tmp = &workspace.even_b;
    SignalBuffer<T>* odd_cur = &workspace.odd_a;
    SignalBuffer<T>* odd_tmp = &workspace.odd_b;

    for (const StepPlan& step_plan : plan.steps) {
        const LiftingStep& step = scheme.steps[step_plan.scheme_step_index];

        switch (step_plan.type) {
            case StepType::kPredict:
                apply_stencil_add(
                    step_plan,
                    std::span<const float>(step.coefficients.data(), step.coefficients.size()),
                    *even_cur,
                    *odd_cur,
                    *odd_tmp,
                    executor);
                std::swap(odd_cur, odd_tmp);
                break;

            case StepType::kUpdate:
                apply_stencil_add(
                    step_plan,
                    std::span<const float>(step.coefficients.data(), step.coefficients.size()),
                    *odd_cur,
                    *even_cur,
                    *even_tmp,
                    executor);
                std::swap(even_cur, even_tmp);
                break;

            case StepType::kScaleEven:
                apply_scale(step_plan, step.coefficients[0], *even_cur, executor);
                break;

            case StepType::kScaleOdd:
                apply_scale(step_plan, step.coefficients[0], *odd_cur, executor);
                break;

            case StepType::kSwap:
                std::swap(even_cur, odd_cur);
                std::swap(even_tmp, odd_tmp);
                break;
        }
    }

    const int direct_shift = scheme.tap_size / 2;
    return DwtResult<T>{
        .approximation = crop_stream(*even_cur, plan.output_len, direct_shift),
        .detail = crop_stream(*odd_cur, plan.output_len, direct_shift),
    };
}

template <class T>
DwtResult<T> dwt_forward_one_level(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const Executor& executor) {
    LiftingPlan plan = build_forward_plan(scheme, input.size());
    Workspace<T> workspace = allocate_workspace<T>(plan);
    return dwt_forward_one_level(input, scheme, plan, workspace, executor);
}

template Workspace<float> allocate_workspace<float>(const LiftingPlan& plan);
template Workspace<double> allocate_workspace<double>(const LiftingPlan& plan);

template DwtResult<float> dwt_forward_one_level<float>(
    std::span<const float> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<float>& workspace,
    const Executor& executor);
template DwtResult<double> dwt_forward_one_level<double>(
    std::span<const double> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<double>& workspace,
    const Executor& executor);
template DwtResult<float> dwt_forward_one_level<float>(
    std::span<const float> input,
    const LiftingScheme& scheme,
    const Executor& executor);
template DwtResult<double> dwt_forward_one_level<double>(
    std::span<const double> input,
    const LiftingScheme& scheme,
    const Executor& executor);

}  // namespace ttwv::cpu
