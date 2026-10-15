#pragma once

#include "category_summary.hpp"
#include "image.hpp"

namespace point_zero::native {
// Capture the current entry through the cancel instruction. CategorySummary
// checks the required bind/cancel instructions before installing its hooks.
CategorySummarySite admit_category_summary_contract(MemoryView, const ImageInfo& game);
} // namespace point_zero::native
