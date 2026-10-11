#pragma once

#include "category_metadata.hpp"
#include "image.hpp"
#include <optional>

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 / official Playlister 5.1.0 admission.
// Returns the complete stock body or the real PL implementation (never a
// relay). Proves the selected native body and PL's saved original continuation
// before the caller chains at that implementation's nonrelative prologue.
//
// Images must already be inspected and retained. A range observation is not an
// atomic snapshot or a lifetime lease: the caller owns serialization, module
// retention and every hook operation. No code is executed or discovered by
// scanning. Mutable category tables, CRT/mutex internals and native range-test
// helper 9771E0 are outside this finite code/chain slice.
CategoryMetadataSite admit_category_metadata_contract(
    MemoryView, const ImageInfo& game, const std::optional<ImageInfo>& playlister);
} // namespace point_zero::native
