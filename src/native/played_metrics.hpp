#pragma once
#include "played_layout.hpp"

#include <iterator>

namespace point_zero::native::played_art::detail {
// Provenance: 09_outputs/research_v1_playlister51_20261001/candidate/source/src/
// played_slot_bank.hpp selects point_zero_played_bank.hpp with
// IIDX_POINT_ZERO_INPUTS. Only HUD/result records are retained here;
// private package names and every live bitmap metric are unchanged.
// Extents describe native drawable quads, excluding atlas extrusion. The saved
// native PNG exports include one extra pixel on every edge; final SVG exports
// declare their own bitmap extent directly. Do not add that atlas gutter here.
// Each result family: wholes 1..12, fractions 0..9, point.
// HUD: native numerals 0..12, point. No zero whole, unused point or two-digit
// fraction from the other families is needed. All 129 entries are reachable.
// Completed missing native-family art retains its reconstructed provenance:
// 09_outputs/research_v1_data_assets_20260922/README.md.
// point_zero_played_bank.hpp SHA-256:
// 4d867029afd34d5df669712a923d76b899ece1559acc48702a7ddd96bc54c2a8
inline constexpr int family_images = 23;
inline constexpr int hud_begin = 0;
inline constexpr int result_begin = hud_begin + 14;
inline constexpr float result_left_bearing[5][2]{{27, 27}, {34, 34}, {45, 45}, {28, 25}, {4, 0}};
inline constexpr Image image_table[] = {
    // HUD, family 0.
    {"ps_lvlnum_00", 28, 19, 0, 0},
    {"ps_lvlnum_01", 10, 19, 13, 0},
    {"ps_lvlnum_02", 28, 19, 4, 0},
    {"ps_lvlnum_03", 28, 19, 4, 0},
    {"ps_lvlnum_04", 28, 19, 3, 0},
    {"ps_lvlnum_05", 28, 19, 4, 0},
    {"ps_lvlnum_06", 30, 19, 3, 0},
    {"ps_lvlnum_07", 29, 19, 4, 0},
    {"ps_lvlnum_08", 30, 19, 3, 0},
    {"ps_lvlnum_09", 30, 19, 3, 0},
    {"ps_lvlnum_10", 36, 19, 0, 0},
    {"ps_lvlnum_11", 31, 19, 2, 0},
    {"ps_lvlnum_12", 36, 19, 0, 0},
    {"ps_point_hud", 8, 8, 36, 12},
    // Result, family 0.
    {"ps_diff_b01", 169, 22, 0, 0},
    {"ps_diff_b02", 174, 22, 0, 0},
    {"ps_diff_b03", 173, 22, 0, 0},
    {"ps_diff_b04", 175, 22, 0, 0},
    {"ps_diff_b05", 174, 22, 0, 0},
    {"ps_diff_b06", 175, 22, 0, 0},
    {"ps_diff_b07", 174, 22, 0, 0},
    {"ps_diff_b08", 175, 22, 0, 0},
    {"ps_diff_b09", 175, 22, 0, 0},
    {"ps_diff_b10", 178, 22, 0, 0},
    {"ps_diff_b11", 177, 22, 0, 0},
    {"ps_diff_b12", 178, 22, 0, 0},
    {"ps_frac_diff_b00", 19, 20, 0, 1},
    {"ps_frac_diff_b01", 8, 20, 161, 1},
    {"ps_frac_diff_b02", 18, 19, 156, 1},
    {"ps_frac_diff_b03", 17, 19, 156, 1},
    {"ps_frac_diff_b04", 20, 20, 0, 1},
    {"ps_frac_diff_b05", 18, 19, 0, 1},
    {"ps_frac_diff_b06", 19, 20, 0, 1},
    {"ps_frac_diff_b07", 18, 20, 0, 1},
    {"ps_frac_diff_b08", 20, 19, 0, 1},
    {"ps_frac_diff_b09", 19, 19, 0, 1},
    {"ps_result_point_b", 8, 7, 0, 0},
    // Result, family 1.
    {"ps_diff_n01", 157, 22, 0, 0},
    {"ps_diff_n02", 162, 22, 0, 0},
    {"ps_diff_n03", 161, 22, 0, 0},
    {"ps_diff_n04", 163, 22, 0, 0},
    {"ps_diff_n05", 162, 22, 0, 0},
    {"ps_diff_n06", 163, 22, 0, 0},
    {"ps_diff_n07", 162, 22, 0, 0},
    {"ps_diff_n08", 163, 22, 0, 0},
    {"ps_diff_n09", 163, 22, 0, 0},
    {"ps_diff_n10", 166, 22, 0, 0},
    {"ps_diff_n11", 165, 22, 0, 0},
    {"ps_diff_n12", 166, 22, 0, 0},
    {"ps_frac_diff_n00", 19, 20, 0, 1},
    {"ps_frac_diff_n01", 8, 20, 149, 1},
    {"ps_frac_diff_n02", 18, 19, 144, 1},
    {"ps_frac_diff_n03", 17, 19, 144, 1},
    {"ps_frac_diff_n04", 20, 20, 143, 1},
    {"ps_frac_diff_n05", 18, 19, 144, 1},
    {"ps_frac_diff_n06", 19, 20, 144, 1},
    {"ps_frac_diff_n07", 18, 20, 144, 1},
    {"ps_frac_diff_n08", 20, 19, 143, 1},
    {"ps_frac_diff_n09", 19, 19, 144, 1},
    {"ps_result_point_n", 8, 7, 0, 0},
    // Result, family 2.
    {"ps_diff_h01", 147, 22, 0, 0},
    {"ps_diff_h02", 152, 22, 0, 0},
    {"ps_diff_h03", 151, 22, 0, 0},
    {"ps_diff_h04", 152, 22, 0, 0},
    {"ps_diff_h05", 152, 22, 0, 0},
    {"ps_diff_h06", 152, 22, 0, 0},
    {"ps_diff_h07", 151, 22, 0, 0},
    {"ps_diff_h08", 152, 22, 0, 0},
    {"ps_diff_h09", 152, 22, 0, 0},
    {"ps_diff_h10", 155, 22, 0, 0},
    {"ps_diff_h11", 155, 22, 0, 0},
    {"ps_diff_h12", 156, 22, 0, 0},
    {"ps_frac_diff_h00", 19, 20, 0, 1},
    {"ps_frac_diff_h01", 9, 20, 138, 1},
    {"ps_frac_diff_h02", 19, 19, 133, 1},
    {"ps_frac_diff_h03", 17, 19, 134, 1},
    {"ps_frac_diff_h04", 19, 20, 133, 1},
    {"ps_frac_diff_h05", 18, 19, 134, 1},
    {"ps_frac_diff_h06", 19, 20, 133, 1},
    {"ps_frac_diff_h07", 18, 20, 133, 1},
    {"ps_frac_diff_h08", 19, 19, 133, 1},
    {"ps_frac_diff_h09", 19, 19, 133, 1},
    {"ps_result_point_h", 9, 7, 0, 0},
    // Result, family 3.
    {"ps_diff_a01", 164, 22, 0, 0},
    {"ps_diff_a02", 169, 22, 0, 0},
    {"ps_diff_a03", 168, 22, 0, 0},
    {"ps_diff_a04", 170, 22, 0, 0},
    {"ps_diff_a05", 169, 22, 0, 0},
    {"ps_diff_a06", 170, 22, 0, 0},
    {"ps_diff_a07", 169, 22, 0, 0},
    {"ps_diff_a08", 170, 22, 0, 0},
    {"ps_diff_a09", 170, 22, 0, 0},
    {"ps_diff_a10", 174, 22, 0, 0},
    {"ps_diff_a11", 172, 22, 0, 0},
    {"ps_diff_a12", 174, 22, 0, 0},
    {"ps_frac_diff_a00", 19, 20, 0, 1},
    {"ps_frac_diff_a01", 8, 20, 156, 1},
    {"ps_frac_diff_a02", 18, 19, 151, 1},
    {"ps_frac_diff_a03", 17, 19, 151, 1},
    {"ps_frac_diff_a04", 20, 20, 150, 1},
    {"ps_frac_diff_a05", 18, 19, 151, 1},
    {"ps_frac_diff_a06", 19, 20, 151, 1},
    {"ps_frac_diff_a07", 18, 20, 151, 1},
    {"ps_frac_diff_a08", 20, 19, 150, 1},
    {"ps_frac_diff_a09", 19, 19, 151, 1},
    {"ps_result_point_a", 8, 7, 0, 0},
    // Result, family 4.
    {"ps_diff_l01", 192, 22, 0, 0},
    {"ps_diff_l02", 197, 22, 0, 0},
    {"ps_diff_l03", 196, 22, 0, 0},
    {"ps_diff_l04", 197, 22, 0, 0},
    {"ps_diff_l05", 197, 22, 0, 0},
    {"ps_diff_l06", 197, 22, 0, 0},
    {"ps_diff_l07", 196, 22, 0, 0},
    {"ps_diff_l08", 197, 22, 0, 0},
    {"ps_diff_l09", 197, 22, 0, 0},
    {"ps_diff_l10", 200, 22, 0, 0},
    {"ps_diff_l11", 198, 22, 0, 0},
    {"ps_diff_l12", 199, 22, 0, 0},
    {"ps_frac_diff_l00", 19, 20, 0, 1},
    {"ps_frac_diff_l01", 9, 20, 0, 1},
    {"ps_frac_diff_l02", 19, 19, 0, 1},
    {"ps_frac_diff_l03", 17, 19, 0, 0},
    {"ps_frac_diff_l04", 19, 20, 0, 1},
    {"ps_frac_diff_l05", 18, 19, 0, 1},
    {"ps_frac_diff_l06", 19, 20, 0, 1},
    {"ps_frac_diff_l07", 18, 20, 0, 1},
    {"ps_frac_diff_l08", 19, 19, 0, 1},
    {"ps_frac_diff_l09", 19, 19, 178, 1},
    {"ps_result_point_l", 8, 7, 0, 0},
};
static_assert(std::size(image_table) == result_begin + 5 * family_images);

// Same retained source directory: played_result_contract.hpp and
// played_notes_contract.hpp. Final LDJ2026081900 result.ifs, musicinfo_2pp.
// Plate shapes 33/38 share pixels; paired P2's host supplies the mirror.
// Original NOTES plate/count/label geometry, with no mutation or fade override.
inline constexpr NotesRole notes[] = {
    {0, 3, 26, 3, nullptr, 595, 1047, 1, 125, 16},
    {0, 4, 23, 12, nullptr, 600, 1047, 1, 0, 10},
    {0, 5, 20, 0, "notes_1p", 497, 1037, 1, 0, 0},
    {1, 2, 26, 4, nullptr, 1325, 1047, -1, 125, 16},
    {1, 6, 23, 9, nullptr, 1332, 1047, 1, 0, 10},
    {1, 7, 20, 0, "notes_2p", 1226, 1037, 1, 0, 0},
};
} // namespace point_zero::native::played_art::detail
