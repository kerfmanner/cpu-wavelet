#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace ttwv::cpu {

template <class T>
struct SignalView {
    T* data{nullptr};
    size_t length{0};
    int shift{0};

    [[nodiscard]] constexpr T& operator[](const size_t index) noexcept { return data[index]; }
    [[nodiscard]] constexpr const T& operator[](const size_t index) const noexcept { return data[index]; }
    [[nodiscard]] constexpr int end_shift() const noexcept { return shift + static_cast<int>(length); }
    [[nodiscard]] constexpr std::span<T> span() noexcept { return {data, length}; }
    [[nodiscard]] constexpr std::span<const T> span() const noexcept { return {data, length}; }
};

template <class T>
struct SignalBuffer {
    std::vector<T> data;
    size_t length{0};
    int shift{0};

    void reserve_length(const size_t capacity) { data.resize(capacity); }

    [[nodiscard]] SignalView<T> view() noexcept {
        return SignalView<T>{.data = data.data(), .length = length, .shift = shift};
    }

    [[nodiscard]] SignalView<const T> view() const noexcept {
        return SignalView<const T>{.data = data.data(), .length = length, .shift = shift};
    }
};

}  // namespace ttwv::cpu
