#pragma once

#include "category_summary.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 admission. Returns the complete native
// calculation and its pointer-bind/cookie-epilogue sites. Reads selected code
// bounds/permissions of support code and the rival seed literal; no execution,
// scanning, hooks or mutable
// category/summary inspection.
// Requires an inspected, retained image and separately admitted metadata and
// candidate-policy routing. Exact external dependencies are listed in the .cpp.
// Caller owns serialization, native state, allocation identity and lifetimes.
// Admission is not atomic and does not certify future bytes.
CategorySummarySite admit_category_summary_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
