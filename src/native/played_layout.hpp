#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace point_zero::native::played_art {
enum class Surface { hud = 2, result = 3, stepup = 4 };

struct Image {
    const char* name;
    int width, height, x, y; // Retained drawable extent and native crop origin.
};
struct Part {
    int image{-1}; // Index into images(); indices are local, not package IDs.
    float scale{1}, x{}, y{};
};
struct Plan {
    std::array<Part, 3> parts{}; // Whole, point, fraction, at unit scale.
    int count{};
    float width{};
    bool extends_result_plate{};
};

// Immutable, process-lifetime table of exactly the required private-package
// images. The renderer must preflight their names/extents before publication.
inline constexpr std::size_t image_count = 244;
std::span<const Image> images() noexcept;

// Difficulty 0..4 = Beginner/Normal/Hyper/Another/
// Leggendaria; HUD shares one family. Tenths 10..129 include explicit .0.
// Invalid surface/difficulty/value returns an empty Plan (all image indices -1).
// Offsets are relative to the native numeral host; do not add Image::x/y again.
// For wide results,
// the caller must extend the native plate/adjacent layout by width-200 pixels,
// preserving its right edge and animation. This function never mutates owners.
// Allocation-free; identity, availability, BATTLE policy and native admission
// belong to the caller. Step Up uses its separate glowing family and optical
// cell; its inherited native confirmation animation is not changed here.
Plan make(Surface surface, int difficulty, int tenths) noexcept;

struct ResultPlate {
    int depth{}, shape{};
    std::uint64_t anonymous{};
    const char* name{};
    bool mirrored{};
    explicit operator bool() const noexcept { return name != nullptr; }
};
// Exact final result.ifs owner/leaf match. sole is relevant only for one player.
// A mismatch returns an empty descriptor; these are resource contracts, not
// native handles. Paired P2's native plate host supplies the horizontal mirror.
ResultPlate select_result_plate(int players, int sole, int player, const char* leaf) noexcept;

struct NotesRole {
    int player, depth, shape;
    std::uint64_t anonymous;
    const char* name;
    float x, y, scale_x, pivot_x, pivot_y;
};
// Six original musicinfo_2pp roles/poses: plate, count, label for P1 then P2.
std::span<const NotesRole> notes_roles() noexcept;
// Adjustment only for finite widths in (200, 264]; zero otherwise. The caller
// applies these only for an admitted custom result, preserving all native poses
// and animation. number_shift cancels make()'s left extension for mirrored P2;
// NOTES move outward (P1 negative, P2 positive). Invalid players return zero.
float number_shift(const ResultPlate&, float width) noexcept;
float notes_offset(int player, float width) noexcept;
} // namespace point_zero::native::played_art
