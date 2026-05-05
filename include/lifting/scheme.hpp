#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ttwv::cpu {

enum class StepType {
    kPredict,
    kUpdate,
    kScaleEven,
    kScaleOdd,
    kSwap,
};

struct LiftingStep {
    StepType type{StepType::kPredict};
    int shift{0};
    std::vector<float> coefficients;
};

struct LiftingScheme {
    std::string mode{"symmetric"};
    int tap_size{0};
    int delay_even{0};
    int delay_odd{0};
    std::vector<LiftingStep> steps;
};

[[nodiscard]] LiftingScheme load_lifting_scheme_json(const std::filesystem::path& path);

}  // namespace ttwv::cpu
