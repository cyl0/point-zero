#pragma once

#include "category_badges.hpp"
#include "image.hpp"

namespace point_zero::native {
// Finite, read-only LDJ-010-2026081900 code admission. Eight typed calls plus
// their selected row/Recommend/KAC and stock suppression helpers; no mutable
// event state or refresh lifecycle scan. Requires an inspected, retained game
// image. The caller owns native-thread serialization and category/event/
// allocation lifetimes.
// Function ranges are read once; ordinary bodies need no digest match. Native data,
// external gate/retrieval helpers and the clock remain explicit dependencies
// (see .cpp). This installs no hook and performs no native call.
CategoryBadgeCalls admit_category_badges_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
