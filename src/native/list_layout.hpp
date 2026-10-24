#pragma once
#include <array>

namespace point_zero::native::list_art {
struct Part { int glyph{}, x{}, y{}; }; // Incoming-origin offsets, including X-7; scale1.
using Placement = std::array<Part, 3>; // Native whole, point, final-size fraction.
// Approved B84 geometry only. Glyphs1..12 are native wholes,14..23 final-size
// fractions,24 the final-size point. Invalid requests leave output unchanged.
bool placement(bool compact, int tenths, Placement&) noexcept;
inline constexpr int window_width = 84, title_delta = 24;
} // namespace point_zero::native::list_art
