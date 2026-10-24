#pragma once

#include "catalog.hpp"
#include "memory.hpp"

#include <array>

namespace point_zero::native {
struct CupValues {
    int song_id{}, style{};
    // Borrowed from the process-retained immutable catalogue. A missing chart
    // or native/raw disagreement leaves that cup null. Assignment presence is
    // still distinct from an integer effective value; consumers use FeatureState.
    std::array<const CatalogRow*, 5> charts{};
};
// Actual34's setter supplies the music identity; old scene-relative caches
// are no longer the primary owner's contract. Store values, never native views.
struct CupBinding {
    std::uintptr_t owner{}, music{};
    int song_id{}, style{-1};
    std::array<int, 5> levels{};
    std::array<std::uintptr_t, 5> layers{};
};
bool read_cup_binding(MemoryView, std::uintptr_t owner, std::uintptr_t music,
                      int style, CupBinding&) noexcept;
// Revalidate the setter tuple and all five levels against native music data.
bool read_cup_values(MemoryView, const Catalog&, const CupBinding&,
                     int style, CupValues&) noexcept;
} // namespace point_zero::native
