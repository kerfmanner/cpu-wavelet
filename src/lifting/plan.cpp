#include "lifting/plan.hpp"

#include <algorithm>
#include <stdexcept>

namespace ttwv::cpu {

LiftingPlan build_forward_plan(const LiftingScheme& scheme, const size_t input_len) {
    if (input_len == 0) {
        throw std::invalid_argument("input_len must be positive");
    }
    if (scheme.tap_size <= 0) {
        throw std::invalid_argument("scheme.tap_size must be positive");
    }
    if (scheme.mode != "symmetric") {
        throw std::invalid_argument("only symmetric mode is supported");
    }

    const size_t pad = static_cast<size_t>(scheme.tap_size - 1);
    const size_t padded_len = input_len + 2 * pad;
    const size_t initial_even_len = (padded_len + 1) / 2;
    const size_t initial_odd_len = padded_len / 2;

    LiftingPlan plan{
        .input_len = input_len,
        .padded_len = padded_len,
        .output_len = (input_len + static_cast<size_t>(scheme.tap_size) - 1) / 2,
        .initial_even_len = initial_even_len,
        .initial_odd_len = initial_odd_len,
        .max_even_len = initial_even_len,
        .max_odd_len = initial_odd_len,
        .final_even_shift = scheme.delay_even,
        .final_odd_shift = scheme.delay_odd,
        .final_even_len = initial_even_len,
        .final_odd_len = initial_odd_len,
        .steps = {},
    };

    struct StreamState {
        int shift{0};
        size_t length{0};
    };

    StreamState even{.shift = scheme.delay_even, .length = initial_even_len};
    StreamState odd{.shift = scheme.delay_odd, .length = initial_odd_len};

    const auto compute_geometry = [](const StreamState& source,
                                     const int kernel_shift,
                                     const size_t k,
                                     const StreamState& base) {
        struct Geometry {
            int out_shift{0};
            size_t out_len{0};
            size_t source_offset{0};
            size_t base_offset{0};
        };

        const int conv_shift = source.shift + kernel_shift + static_cast<int>(std::min(source.length, k)) - 1;
        const size_t conv_len = source.length >= k ? source.length - k + 1 : 0;
        const int out_shift = std::max(base.shift, conv_shift);
        const int out_end =
            std::min(base.shift + static_cast<int>(base.length), conv_shift + static_cast<int>(conv_len));
        const size_t out_len = out_end > out_shift ? static_cast<size_t>(out_end - out_shift) : 0;

        return Geometry{
            .out_shift = out_shift,
            .out_len = out_len,
            .source_offset = static_cast<size_t>(out_shift - conv_shift),
            .base_offset = static_cast<size_t>(out_shift - base.shift),
        };
    };

    plan.steps.reserve(scheme.steps.size());
    for (size_t i = 0; i < scheme.steps.size(); ++i) {
        const auto& step = scheme.steps[i];

        if (step.type == StepType::kPredict) {
            if (step.coefficients.empty()) {
                throw std::invalid_argument("predict steps must contain coefficients");
            }
            const auto geometry = compute_geometry(even, step.shift, step.coefficients.size(), odd);
            plan.steps.push_back(StepPlan{
                .type = step.type,
                .scheme_step_index = i,
                .out_shift = geometry.out_shift,
                .out_len = geometry.out_len,
                .base_offset = geometry.base_offset,
                .source_offset = geometry.source_offset,
                .source_shift = even.shift,
                .base_shift = odd.shift,
                .source_len = even.length,
                .base_len = odd.length,
            });
            odd = StreamState{.shift = geometry.out_shift, .length = geometry.out_len};
            plan.max_odd_len = std::max(plan.max_odd_len, odd.length);
            continue;
        }

        if (step.type == StepType::kUpdate) {
            if (step.coefficients.empty()) {
                throw std::invalid_argument("update steps must contain coefficients");
            }
            const auto geometry = compute_geometry(odd, step.shift, step.coefficients.size(), even);
            plan.steps.push_back(StepPlan{
                .type = step.type,
                .scheme_step_index = i,
                .out_shift = geometry.out_shift,
                .out_len = geometry.out_len,
                .base_offset = geometry.base_offset,
                .source_offset = geometry.source_offset,
                .source_shift = odd.shift,
                .base_shift = even.shift,
                .source_len = odd.length,
                .base_len = even.length,
            });
            even = StreamState{.shift = geometry.out_shift, .length = geometry.out_len};
            plan.max_even_len = std::max(plan.max_even_len, even.length);
            continue;
        }

        if (step.type == StepType::kScaleEven) {
            if (step.shift != 0 || step.coefficients.size() != 1) {
                throw std::invalid_argument("scale-even steps must have zero shift and one coefficient");
            }
            plan.steps.push_back(StepPlan{
                .type = step.type,
                .scheme_step_index = i,
                .out_shift = even.shift,
                .out_len = even.length,
                .base_offset = 0,
                .source_offset = 0,
                .source_shift = even.shift,
                .base_shift = even.shift,
                .source_len = even.length,
                .base_len = even.length,
            });
            continue;
        }

        if (step.type == StepType::kScaleOdd) {
            if (step.shift != 0 || step.coefficients.size() != 1) {
                throw std::invalid_argument("scale-odd steps must have zero shift and one coefficient");
            }
            plan.steps.push_back(StepPlan{
                .type = step.type,
                .scheme_step_index = i,
                .out_shift = odd.shift,
                .out_len = odd.length,
                .base_offset = 0,
                .source_offset = 0,
                .source_shift = odd.shift,
                .base_shift = odd.shift,
                .source_len = odd.length,
                .base_len = odd.length,
            });
            continue;
        }

        plan.steps.push_back(StepPlan{
            .type = step.type,
            .scheme_step_index = i,
            .out_shift = 0,
            .out_len = 0,
            .base_offset = 0,
            .source_offset = 0,
            .source_shift = even.shift,
            .base_shift = odd.shift,
            .source_len = even.length,
            .base_len = odd.length,
        });
        std::swap(even, odd);
        plan.max_even_len = std::max(plan.max_even_len, even.length);
        plan.max_odd_len = std::max(plan.max_odd_len, odd.length);
    }

    plan.final_even_shift = even.shift;
    plan.final_odd_shift = odd.shift;
    plan.final_even_len = even.length;
    plan.final_odd_len = odd.length;
    plan.max_even_len = std::max(plan.max_even_len, even.length);
    plan.max_odd_len = std::max(plan.max_odd_len, odd.length);

    return plan;
}

}  // namespace ttwv::cpu
