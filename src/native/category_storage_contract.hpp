#pragma once

#include "category_storage.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 storage-call admission. Throws on a
// invalid selected auxiliary binding or image range/protection.
// Requires an inspected/retained game image. Executes no native code or hooks.
// Range observations do not certify future bytes or lifetimes.
//
// Metadata-hook admission/routing at 86EF00 is a SEPARATE caller prerequisite:
// Playlister may hook it, so this contract deliberately never reads its body.
// Native candidate/filter policy, chart lookup/style internals, mutable manager
// state, CRT/OS and exception handlers are outside this finite storage slice.
// Caller must exclude special IDs 210..214, validate native allocation identity
// and postconditions, keep the auxiliary vector empty and prove no consumers
// before destruction. This is not an admission of publication or shared refs.
CategoryNativeCalls admit_category_storage_contract(MemoryView, const ImageInfo&);
} // namespace point_zero::native
