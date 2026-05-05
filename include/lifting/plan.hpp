#pragma once

#include <cstddef>
#include <vector>

#include "lifting/scheme.hpp"

namespace ttwv::cpu {

struct StepPlan {
    StepType type{StepType::kPredict};
    size_t scheme_step_index{0};

    int out_shift{0};
    size_t out_len{0};

    size_t base_offset{0};
    size_t source_offset{0};

    int source_shift{0};
    int base_shift{0};
    size_t source_len{0};
    size_t base_len{0};
};

struct LiftingPlan {
    size_t input_len{0};
    size_t padded_len{0};
    size_t output_len{0};

    size_t initial_even_len{0};
    size_t initial_odd_len{0};

    size_t max_even_len{0};
    size_t max_odd_len{0};

    int final_even_shift{0};
    int final_odd_shift{0};
    size_t final_even_len{0};
    size_t final_odd_len{0};

    std::vector<StepPlan> steps;
};

[[nodiscard]] LiftingPlan build_forward_plan(const LiftingScheme& scheme, size_t input_len);

}  // namespace ttwv::cpu
