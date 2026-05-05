#pragma once

#include <cstdint>

namespace ttwv::cpu {

[[nodiscard]] constexpr uint64_t positive_mod(const int64_t value, const uint64_t modulus) noexcept {
    if (modulus == 0) {
        return 0;
    }

    const int64_t signed_modulus = static_cast<int64_t>(modulus);
    const int64_t remainder = value % signed_modulus;
    return static_cast<uint64_t>(remainder < 0 ? remainder + signed_modulus : remainder);
}

[[nodiscard]] constexpr uint64_t symmetric_index(const int64_t index, const uint64_t length) noexcept {
    if (length <= 1) {
        return 0;
    }

    const uint64_t period = length * 2;
    const uint64_t reflected = positive_mod(index, period);
    return reflected < length ? reflected : period - 1U - reflected;
}

}  // namespace ttwv::cpu
