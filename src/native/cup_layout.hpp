#pragma once

#include <array>

namespace point_zero::native::cup_art {
struct Transform {
    std::array<float, 2> scale{1, 1}, position{};
};
using Placement = std::array<Transform, 3>;

// Approved palette families [0, 5], count-up tenths [0, 129].
// Output order: whole, fraction, point. Fixed maximum 129 and layout scale 1;
// whole transforms restore native art size, dedicated fraction/point scale is 1.
// Returns false for invalid input and leaves out unchanged.
bool placement(int family, int tenths, Placement& out) noexcept;
} // namespace point_zero::native::cup_art
