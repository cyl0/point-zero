#pragma once
namespace point_zero::native::list_art::detail {
// Native whole glyphs1..12, ordinary/compact. All five retained palettes have
// identical geometry; keep it once. Source: list_glyph_bank.hpp in
// 09_outputs/research_v1_playlister51_20261001/candidate/source/src/.
inline constexpr int whole_widths[2][12] = {
    {13,34,34,34,34,34,34,34,34,46,42,46},
    {9,22,22,22,22,22,22,22,22,29,27,29},
};
} // namespace point_zero::native::list_art::detail
