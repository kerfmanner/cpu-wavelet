#include "lifting/cpu_transform.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>

#include "lifting/indexing.hpp"

#if defined(_MSC_VER)
#define TTWV_CPU_RESTRICT __restrict
#else
#define TTWV_CPU_RESTRICT __restrict__
#endif

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

[[nodiscard]] size_t ceil_div_nonnegative(const int64_t value, const int64_t divisor) {
    if (value <= 0) {
        return 0;
    }
    return static_cast<size_t>((value + divisor - 1) / divisor);
}

[[nodiscard]] IndexRange clamp_range(IndexRange range, const size_t length) {
    range.begin = std::min(range.begin, length);
    range.end = std::min(range.end, length);
    if (range.end < range.begin) {
        range.end = range.begin;
    }
    return range;
}

[[nodiscard]] IndexRange even_interior_range(
    const size_t even_len,
    const uint64_t input_len,
    const int64_t pad) {
    return clamp_range(
        IndexRange{
            ceil_div_nonnegative(pad, 2),
            ceil_div_nonnegative(static_cast<int64_t>(input_len) + pad, 2),
        },
        even_len);
}

[[nodiscard]] IndexRange odd_interior_range(
    const size_t odd_len,
    const uint64_t input_len,
    const int64_t pad) {
    return clamp_range(
        IndexRange{
            ceil_div_nonnegative(pad - 1, 2),
            ceil_div_nonnegative(static_cast<int64_t>(input_len) + pad - 1, 2),
        },
        odd_len);
}

template <class T>
void initialize_even_boundary_range(
    const T* const TTWV_CPU_RESTRICT input,
    T* const TTWV_CPU_RESTRICT even,
    const uint64_t input_len,
    const int64_t pad,
    const size_t begin,
    const size_t end) {
    for (size_t i = begin; i < end; ++i) {
        const int64_t padded_index = static_cast<int64_t>(2 * i) - pad;
        const uint64_t source_index = symmetric_index(padded_index, input_len);
        even[i] = input[static_cast<size_t>(source_index)];
    }
}

template <class T>
void initialize_odd_boundary_range(
    const T* const TTWV_CPU_RESTRICT input,
    T* const TTWV_CPU_RESTRICT odd,
    const uint64_t input_len,
    const int64_t pad,
    const size_t begin,
    const size_t end) {
    for (size_t i = begin; i < end; ++i) {
        const int64_t padded_index = static_cast<int64_t>(2 * i + 1) - pad;
        const uint64_t source_index = symmetric_index(padded_index, input_len);
        odd[i] = input[static_cast<size_t>(source_index)];
    }
}

template <class T>
void initialize_even_interior_range(
    const T* const TTWV_CPU_RESTRICT input,
    T* const TTWV_CPU_RESTRICT even,
    const int64_t pad,
    const size_t begin,
    const size_t end) {
    for (size_t i = begin; i < end; ++i) {
        even[i] = input[static_cast<size_t>(static_cast<int64_t>(2 * i) - pad)];
    }
}

template <class T>
void initialize_odd_interior_range(
    const T* const TTWV_CPU_RESTRICT input,
    T* const TTWV_CPU_RESTRICT odd,
    const int64_t pad,
    const size_t begin,
    const size_t end) {
    for (size_t i = begin; i < end; ++i) {
        odd[i] = input[static_cast<size_t>(static_cast<int64_t>(2 * i + 1) - pad)];
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

    const T* const TTWV_CPU_RESTRICT input_data = input.data();
    T* const TTWV_CPU_RESTRICT even_data = even.data.data();
    T* const TTWV_CPU_RESTRICT odd_data = odd.data.data();

    const IndexRange even_interior = even_interior_range(even.length, input_len, pad);
    initialize_even_boundary_range(input_data, even_data, input_len, pad, 0, even_interior.begin);
    executor.for_chunks_stencil(even_interior.begin, even_interior.end, [&](const size_t begin, const size_t end) {
        initialize_even_interior_range(input_data, even_data, pad, begin, end);
    });
    initialize_even_boundary_range(input_data, even_data, input_len, pad, even_interior.end, even.length);

    const IndexRange odd_interior = odd_interior_range(odd.length, input_len, pad);
    initialize_odd_boundary_range(input_data, odd_data, input_len, pad, 0, odd_interior.begin);
    executor.for_chunks_stencil(odd_interior.begin, odd_interior.end, [&](const size_t begin, const size_t end) {
        initialize_odd_interior_range(input_data, odd_data, pad, begin, end);
    });
    initialize_odd_boundary_range(input_data, odd_data, input_len, pad, odd_interior.end, odd.length);
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
void stencil_add_range(
    const StepPlan& plan,
    const float* const TTWV_CPU_RESTRICT coeffs,
    const size_t k,
    const T* const TTWV_CPU_RESTRICT source,
    const T* const TTWV_CPU_RESTRICT base,
    T* const TTWV_CPU_RESTRICT output,
    const size_t begin,
    const size_t end) {
    switch (k) {
        case 1: {
            const T c0 = static_cast<T>(coeffs[0]);
            for (size_t p = begin; p < end; ++p) {
                const T stencil = c0 * source[plan.source_offset + p];
                output[p] = base[plan.base_offset + p] + stencil;
            }
            return;
        }
        case 2: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            for (size_t p = begin; p < end; ++p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source[src + 1];
                const T term1 = c1 * source[src];
                const T stencil = term0 + term1;
                output[p] = base[plan.base_offset + p] + stencil;
            }
            return;
        }
        case 3: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            const T c2 = static_cast<T>(coeffs[2]);
            for (size_t p = begin; p < end; ++p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source[src + 2];
                const T term1 = c1 * source[src + 1];
                const T term2 = c2 * source[src];
                const T stencil = static_cast<T>(static_cast<T>(term0 + term1) + term2);
                output[p] = base[plan.base_offset + p] + stencil;
            }
            return;
        }
        case 4: {
            const T c0 = static_cast<T>(coeffs[0]);
            const T c1 = static_cast<T>(coeffs[1]);
            const T c2 = static_cast<T>(coeffs[2]);
            const T c3 = static_cast<T>(coeffs[3]);
            for (size_t p = begin; p < end; ++p) {
                const size_t src = plan.source_offset + p;
                const T term0 = c0 * source[src + 3];
                const T term1 = c1 * source[src + 2];
                const T term2 = c2 * source[src + 1];
                const T term3 = c3 * source[src];
                const T stencil = static_cast<T>(static_cast<T>(static_cast<T>(term0 + term1) + term2) + term3);
                output[p] = base[plan.base_offset + p] + stencil;
            }
            return;
        }
        default:
            for (size_t p = begin; p < end; ++p) {
                const size_t src = plan.source_offset + p;
                T stencil = T{0};
                for (size_t j = 0; j < k; ++j) {
                    const T term = static_cast<T>(coeffs[j]) * source[src + k - 1 - j];
                    stencil = static_cast<T>(stencil + term);
                }
                output[p] = base[plan.base_offset + p] + stencil;
            }
            return;
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

    const T* const TTWV_CPU_RESTRICT source_data = source.data.data();
    const T* const TTWV_CPU_RESTRICT base_data = base.data.data();
    T* const TTWV_CPU_RESTRICT output_data = output.data.data();
    const float* const TTWV_CPU_RESTRICT coeff_data = coeffs.data();

    executor.for_chunks_stencil(0, plan.out_len, [&](const size_t begin, const size_t end) {
        stencil_add_range(plan, coeff_data, k, source_data, base_data, output_data, begin, end);
    });
}

template <class T>
void scale_range(
    T* const TTWV_CPU_RESTRICT data,
    const T factor,
    const size_t begin,
    const size_t end) {
    for (size_t i = begin; i < end; ++i) {
        data[i] *= factor;
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

    T* const TTWV_CPU_RESTRICT signal_data = signal.data.data();
    executor.for_chunks_scale(0, signal.length, [&](const size_t begin, const size_t end) {
        scale_range(signal_data, factor, begin, end);
    });
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

#if defined(CPU_WAVELET_USE_OPENMP)
template <class Fn>
void run_openmp_region(const Executor& executor, Fn&& fn) {
    if (executor.thread_count() > 0) {
#pragma omp parallel num_threads(executor.thread_count())
        {
            fn(
                static_cast<size_t>(omp_get_thread_num()),
                static_cast<size_t>(omp_get_num_threads()));
        }
    } else {
#pragma omp parallel
        {
            fn(
                static_cast<size_t>(omp_get_thread_num()),
                static_cast<size_t>(omp_get_num_threads()));
        }
    }
}

[[nodiscard]] bool should_use_persistent_openmp(const LiftingPlan& plan, const Executor& executor) {
    if (executor.policy() != ExecutionPolicy::kOpenMP) {
        return false;
    }
    const size_t threshold = executor.stencil_parallel_threshold();
    return threshold == 0 || plan.input_len >= threshold;
}

template <class Fn>
void persistent_for_range(
    const size_t begin,
    const size_t end,
    const size_t threshold,
    const size_t thread_index,
    const size_t thread_count,
    Fn&& fn) {
    if (end <= begin) {
        return;
    }

    const size_t count = end - begin;
    if (count < threshold) {
        if (thread_index == 0) {
            fn(begin, end);
        }
        return;
    }

    const IndexRange range = static_chunk(begin, end, thread_index, thread_count);
    if (range.begin < range.end) {
        fn(range.begin, range.end);
    }
}

template <class T>
void initialize_even_odd_persistent_worker(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    SignalBuffer<T>& even,
    SignalBuffer<T>& odd,
    const Executor& executor,
    const size_t thread_index,
    const size_t thread_count) {
    const int64_t pad = static_cast<int64_t>(scheme.tap_size - 1);
    const uint64_t input_len = static_cast<uint64_t>(input.size());

    const T* const TTWV_CPU_RESTRICT input_data = input.data();
    T* const TTWV_CPU_RESTRICT even_data = even.data.data();
    T* const TTWV_CPU_RESTRICT odd_data = odd.data.data();

    const IndexRange even_interior = even_interior_range(even.length, input_len, pad);
    if (thread_index == 0) {
        initialize_even_boundary_range(input_data, even_data, input_len, pad, 0, even_interior.begin);
    }
    persistent_for_range(
        even_interior.begin,
        even_interior.end,
        executor.stencil_parallel_threshold(),
        thread_index,
        thread_count,
        [&](const size_t begin, const size_t end) {
            initialize_even_interior_range(input_data, even_data, pad, begin, end);
        });
    if (thread_index == 0) {
        initialize_even_boundary_range(input_data, even_data, input_len, pad, even_interior.end, even.length);
    }

    const IndexRange odd_interior = odd_interior_range(odd.length, input_len, pad);
    if (thread_index == 0) {
        initialize_odd_boundary_range(input_data, odd_data, input_len, pad, 0, odd_interior.begin);
    }
    persistent_for_range(
        odd_interior.begin,
        odd_interior.end,
        executor.stencil_parallel_threshold(),
        thread_index,
        thread_count,
        [&](const size_t begin, const size_t end) {
            initialize_odd_interior_range(input_data, odd_data, pad, begin, end);
        });
    if (thread_index == 0) {
        initialize_odd_boundary_range(input_data, odd_data, input_len, pad, odd_interior.end, odd.length);
    }
}

template <class T>
void apply_stencil_add_persistent_worker(
    const StepPlan& plan,
    const std::span<const float> coeffs,
    const SignalBuffer<T>& source,
    const SignalBuffer<T>& base,
    SignalBuffer<T>& output,
    const Executor& executor,
    const size_t thread_index,
    const size_t thread_count) {
    const T* const TTWV_CPU_RESTRICT source_data = source.data.data();
    const T* const TTWV_CPU_RESTRICT base_data = base.data.data();
    T* const TTWV_CPU_RESTRICT output_data = output.data.data();
    const float* const TTWV_CPU_RESTRICT coeff_data = coeffs.data();
    const size_t k = coeffs.size();

    persistent_for_range(
        0,
        plan.out_len,
        executor.stencil_parallel_threshold(),
        thread_index,
        thread_count,
        [&](const size_t begin, const size_t end) {
            stencil_add_range(plan, coeff_data, k, source_data, base_data, output_data, begin, end);
        });
}

template <class T>
void apply_scale_persistent_worker(
    const float raw_factor,
    SignalBuffer<T>& signal,
    const Executor& executor,
    const size_t thread_index,
    const size_t thread_count) {
    const T factor = static_cast<T>(raw_factor);
    if (factor == T{1}) {
        return;
    }

    T* const TTWV_CPU_RESTRICT signal_data = signal.data.data();
    persistent_for_range(
        0,
        signal.length,
        executor.scale_parallel_threshold(),
        thread_index,
        thread_count,
        [&](const size_t begin, const size_t end) {
            scale_range(signal_data, factor, begin, end);
        });
}

template <class T>
DwtResult<T> dwt_forward_one_level_persistent_openmp(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<T>& workspace,
    const Executor& executor) {
    validate_plan_for_input(input, scheme, plan);

    workspace.even_a.length = plan.initial_even_len;
    workspace.even_a.shift = scheme.delay_even;
    workspace.odd_a.length = plan.initial_odd_len;
    workspace.odd_a.shift = scheme.delay_odd;
    workspace.even_b.length = 0;
    workspace.even_b.shift = scheme.delay_even;
    workspace.odd_b.length = 0;
    workspace.odd_b.shift = scheme.delay_odd;

    SignalBuffer<T>* even_cur = &workspace.even_a;
    SignalBuffer<T>* even_tmp = &workspace.even_b;
    SignalBuffer<T>* odd_cur = &workspace.odd_a;
    SignalBuffer<T>* odd_tmp = &workspace.odd_b;

    run_openmp_region(executor, [&](const size_t thread_index, const size_t thread_count) {
        initialize_even_odd_persistent_worker(
            input,
            scheme,
            plan,
            workspace.even_a,
            workspace.odd_a,
            executor,
            thread_index,
            thread_count);

#pragma omp barrier

        for (const StepPlan& step_plan : plan.steps) {
            const LiftingStep& step = scheme.steps[step_plan.scheme_step_index];

            switch (step_plan.type) {
                case StepType::kPredict:
                    if (thread_index == 0) {
                        odd_tmp->length = step_plan.out_len;
                        odd_tmp->shift = step_plan.out_shift;
                    }
#pragma omp barrier
                    apply_stencil_add_persistent_worker(
                        step_plan,
                        std::span<const float>(step.coefficients.data(), step.coefficients.size()),
                        *even_cur,
                        *odd_cur,
                        *odd_tmp,
                        executor,
                        thread_index,
                        thread_count);
#pragma omp barrier
                    if (thread_index == 0) {
                        std::swap(odd_cur, odd_tmp);
                    }
#pragma omp barrier
                    break;

                case StepType::kUpdate:
                    if (thread_index == 0) {
                        even_tmp->length = step_plan.out_len;
                        even_tmp->shift = step_plan.out_shift;
                    }
#pragma omp barrier
                    apply_stencil_add_persistent_worker(
                        step_plan,
                        std::span<const float>(step.coefficients.data(), step.coefficients.size()),
                        *odd_cur,
                        *even_cur,
                        *even_tmp,
                        executor,
                        thread_index,
                        thread_count);
#pragma omp barrier
                    if (thread_index == 0) {
                        std::swap(even_cur, even_tmp);
                    }
#pragma omp barrier
                    break;

                case StepType::kScaleEven:
                    apply_scale_persistent_worker(
                        step.coefficients[0],
                        *even_cur,
                        executor,
                        thread_index,
                        thread_count);
#pragma omp barrier
                    break;

                case StepType::kScaleOdd:
                    apply_scale_persistent_worker(
                        step.coefficients[0],
                        *odd_cur,
                        executor,
                        thread_index,
                        thread_count);
#pragma omp barrier
                    break;

                case StepType::kSwap:
                    if (thread_index == 0) {
                        std::swap(even_cur, odd_cur);
                        std::swap(even_tmp, odd_tmp);
                    }
#pragma omp barrier
                    break;
            }
        }
    });

    const int direct_shift = scheme.tap_size / 2;
    return DwtResult<T>{
        .approximation = crop_stream(*even_cur, plan.output_len, direct_shift),
        .detail = crop_stream(*odd_cur, plan.output_len, direct_shift),
    };
}
#endif

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
BatchWorkspace<T> allocate_batch_workspace(
    const std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme) {
    BatchWorkspace<T> batch_workspace;
    batch_workspace.plans.reserve(inputs.size());
    batch_workspace.workspaces.reserve(inputs.size());

    for (const std::span<const T> input : inputs) {
        if (input.empty()) {
            throw std::invalid_argument("batch input signals must be non-empty");
        }
        LiftingPlan plan = build_forward_plan(scheme, input.size());
        batch_workspace.workspaces.push_back(allocate_workspace<T>(plan));
        batch_workspace.plans.push_back(std::move(plan));
    }

    return batch_workspace;
}

template <class T>
DwtResult<T> dwt_forward_one_level(
    const std::span<const T> input,
    const LiftingScheme& scheme,
    const LiftingPlan& plan,
    Workspace<T>& workspace,
    const Executor& executor) {
#if defined(CPU_WAVELET_USE_OPENMP)
    if (should_use_persistent_openmp(plan, executor)) {
        return dwt_forward_one_level_persistent_openmp(input, scheme, plan, workspace, executor);
    }
#endif

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

template <class T>
DwtBatchResult<T> dwt_forward_one_level_batch(
    const std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<T>& workspace,
    const Executor& batch_executor,
    const Executor& signal_executor) {
    if (workspace.plans.size() != inputs.size() || workspace.workspaces.size() != inputs.size()) {
        throw std::invalid_argument("batch workspace size does not match input batch size");
    }

    DwtBatchResult<T> result;
    result.signals.resize(inputs.size());

    batch_executor.for_each_batch(0, inputs.size(), [&](const size_t i) {
        result.signals[i] = dwt_forward_one_level<T>(
            inputs[i],
            scheme,
            workspace.plans[i],
            workspace.workspaces[i],
            signal_executor);
    });

    return result;
}

template <class T>
DwtBatchResult<T> dwt_forward_one_level_batch(
    const std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<T>& workspace,
    const Executor& batch_executor) {
    const Executor serial_signal_executor(ExecutionPolicy::kSerial);
    return dwt_forward_one_level_batch<T>(inputs, scheme, workspace, batch_executor, serial_signal_executor);
}

template <class T>
DwtBatchResult<T> dwt_forward_one_level_batch(
    const std::span<const std::span<const T>> inputs,
    const LiftingScheme& scheme,
    const Executor& batch_executor) {
    BatchWorkspace<T> workspace = allocate_batch_workspace<T>(inputs, scheme);
    return dwt_forward_one_level_batch<T>(inputs, scheme, workspace, batch_executor);
}

template Workspace<float> allocate_workspace<float>(const LiftingPlan& plan);
template Workspace<double> allocate_workspace<double>(const LiftingPlan& plan);

template BatchWorkspace<float> allocate_batch_workspace<float>(
    std::span<const std::span<const float>> inputs,
    const LiftingScheme& scheme);
template BatchWorkspace<double> allocate_batch_workspace<double>(
    std::span<const std::span<const double>> inputs,
    const LiftingScheme& scheme);

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

template DwtBatchResult<float> dwt_forward_one_level_batch<float>(
    std::span<const std::span<const float>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<float>& workspace,
    const Executor& batch_executor,
    const Executor& signal_executor);
template DwtBatchResult<double> dwt_forward_one_level_batch<double>(
    std::span<const std::span<const double>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<double>& workspace,
    const Executor& batch_executor,
    const Executor& signal_executor);
template DwtBatchResult<float> dwt_forward_one_level_batch<float>(
    std::span<const std::span<const float>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<float>& workspace,
    const Executor& batch_executor);
template DwtBatchResult<double> dwt_forward_one_level_batch<double>(
    std::span<const std::span<const double>> inputs,
    const LiftingScheme& scheme,
    BatchWorkspace<double>& workspace,
    const Executor& batch_executor);
template DwtBatchResult<float> dwt_forward_one_level_batch<float>(
    std::span<const std::span<const float>> inputs,
    const LiftingScheme& scheme,
    const Executor& batch_executor);
template DwtBatchResult<double> dwt_forward_one_level_batch<double>(
    std::span<const std::span<const double>> inputs,
    const LiftingScheme& scheme,
    const Executor& batch_executor);

}  // namespace ttwv::cpu
