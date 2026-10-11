#pragma once
#include <array>

namespace point_zero::native::list_art {
struct Size { int width{}, height{}; };
struct Part { int glyph{}, x{}, y{}; }; // Incoming-origin offsets, including X-7; scale1.
using Placement = std::array<Part, 3>; // Native whole, point, final-size fraction.
// Approved B84 geometry only. Glyphs1..12 are native wholes,14..23 final-size
// fractions,24 the final-size point. Invalid requests leave output unchanged.
bool placement(bool compact, int tenths, Placement&) noexcept;
// Compatibility for palette-oriented artwork/layout oracle callers.
bool placement(int difficulty, bool compact, int tenths, Placement&) noexcept;
bool size(int difficulty, bool compact, int glyph, Size&) noexcept;
inline constexpr int window_width = 84, title_delta = 24;
} // namespace point_zero::native::list_art
