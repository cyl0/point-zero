#pragma once

#include "played_contract.hpp"

namespace point_zero::native {
struct StepUpSites {
    PlayedFunction builder, fast_bind, scene_constructor, confirmation, committed_tuple;
    // Absolute instruction boundary and named (340390) return address.
    // Capture precedes the eight-byte indexed R8 load; RSI is the owner.
    // fast_bind tail-jumps to named after restoring RSP: named sees the
    // builder's return address and capture RSP - sizeof(std::uintptr_t).
    std::uintptr_t capture{}, named_return{};
};

// Finite LDJ-010-2026081900 code admission records retained body bytes and
// checks selected-image allocation/protection. Hook ownership is separate; throws on
// failure; no native calls, hooks, DLL loading or shared-state writes.
// Requires separate shared named admission (admit_played_sites), chart
// helper admission and runtime scene/tuple/layer/resource/label identity.
// No whole-module hash, native table/string or resource-content veto.
StepUpSites admit_stepup_sites(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
