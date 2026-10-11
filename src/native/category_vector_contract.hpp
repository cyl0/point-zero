#pragma once

#include "category_layout.hpp"
#include "image.hpp"

namespace point_zero::native {
struct CategoryVectorCalls {
    std::uintptr_t (__fastcall* grow)(category_layout::Vector*, std::uintptr_t position,
                                     const std::uintptr_t* value){};
    void (__fastcall* destroy)(category_layout::Vector*){};
};

// Finite, read-only LDJ-010-2026081900 admission of the two native standalone
// pointer-vector operations. Throws on refusal; installs no hook or allocation
// in the game and executes neither operation. The caller retains the inspected
// image, serializes native calls, and owns the vector/allocation lifetimes.
// Grow is used ONLY at size==capacity, with position==end for appending; other
// appends write within the owned buffer. Its return is the inserted SLOT's
// address, not the stored category pointer or a success flag. Destroy frees
// only the vector buffer, never its pointed-to categories.
// Both function ranges are read once with executable image checks.
// This is not an atomic snapshot or a guarantee against subsequent changes.
// Allocator/copy/error/exception helpers remain external (see .cpp).
CategoryVectorCalls admit_category_vector_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
