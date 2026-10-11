#pragma once

#include "catalog.hpp"
#include "memory.hpp"

#include <array>

namespace point_zero::native {
// Ephemeral observation on the native song-select thread. These pointers must
// not be retained across native calls, input, or a scene transition.
struct Selection {
    std::uintptr_t data{}, list{}, group{}, category{}, chart{}, music{};
    int group_id{}, category_cursor{}, bar_cursor{}, song_id{}, style{}, difficulty{};
    bool operator==(const Selection&) const = default;
};

// LDJ2026081900 only. Call after the selected core has admitted the menu code
// and layouts. No scan, allocation, native calls or writes. False is ordinary
// native fallback (folder/header/transition/unknown owner), not feature failure.
bool read_selection(MemoryView, ImageRange game, Selection&) noexcept;

struct CupValues {
    int song_id{}, style{};
    // Borrowed from the process-retained immutable catalogue. A missing chart
    // or native/raw disagreement leaves that cup null. Assignment presence is
    // still distinct from an integer effective value; consumers use FeatureState.
    std::array<const CatalogRow*, 5> charts{};
};
// Resolves all five cups together, traversing the selected native list once.
// The two native music caches, working difficulty and cup values must agree.
bool read_cup_values(MemoryView, ImageRange game, const Catalog&,
                     std::uintptr_t cup_owner, CupValues&) noexcept;
} // namespace point_zero::native
