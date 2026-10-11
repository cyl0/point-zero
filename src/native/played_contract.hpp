#pragma once

#include "image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace point_zero::native {
struct PlayedFunction {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Retained native bytes; patch ownership is checked by the hook.
};
struct PlayedSites {
    PlayedFunction named, confirmation, hud, result, result_bind;
    // Absolute addresses, ordered confirmation copy 0, copy 1, HUD, result.
    // Captures are instruction boundaries, not necessarily CALL returns.
    std::array<std::uintptr_t, 4> captures{};
    // Returns from named (340390). Result's return is inside result_bind,
    // shared with other callers, and does not itself establish result scope.
    std::array<std::uintptr_t, 4> named_returns{};
    PlayedFunction notes_update;
    std::uintptr_t notes_capture{};
    std::array<std::uintptr_t, 2> result_returns{}; // Outer result -> shared layer helper.
};

// Finite LDJ-010-2026081900 capture/name-routing admission records native
// bytes and checks selected-image allocation/protection. Patch owners consume
// the required instruction bytes; admission does not authenticate full bodies.
// Throws on failure; no native calls, hooks, DLL loading or shared-state writes.
// Caller scope/retirement, chart helpers, AFP exports/binding, stock tables
// and Step Up need separate admission. No whole-module hash veto.
PlayedSites admit_played_sites(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
