#pragma once
#include "list_layout.hpp"

namespace point_zero::native::list_art::detail {
// Native whole glyphs1..12, ordinary/compact. All five retained palettes have
// identical geometry; keep it once. Source: list_glyph_bank.hpp in
// 09_outputs/research_v1_playlister51_20261001/candidate/source/src/.
inline constexpr Size whole_sizes[2][12] = {
    {{13,28},{34,28},{34,28},{34,28},{34,28},{34,28},
     {34,28},{34,28},{34,28},{46,28},{42,28},{46,28}},
    {{9,19},{22,19},{22,19},{22,19},{22,19},{22,19},
     {22,19},{22,19},{22,19},{29,19},{27,19},{29,19}},
};
inline constexpr Size fraction_sizes[2] = {{21,18}, {16,14}};
inline constexpr Size point_size{12,12};
} // namespace point_zero::native::list_art::detail
