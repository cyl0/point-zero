#pragma once

#include <array>

namespace point_zero::native::cup_art {
struct Transform {
    std::array<float, 2> scale{1, 1}, position{};
};
using Placement = std::array<Transform, 3>;

// Plain B/N/H/A/L families [0,4], selected families [5,9], tenths [0,129].
// Output order: whole, fraction, point. Native 94x66 whole cells stay at scale1;
// fraction/point transforms are the accepted archive's matching placements.
// Returns false for invalid input and leaves out unchanged.
bool placement(int family, int tenths, Placement& out) noexcept;
} // namespace point_zero::native::cup_art
