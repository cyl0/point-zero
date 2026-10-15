#pragma once

#include "category_layout.hpp"
#include "memory.hpp"

namespace point_zero::native {
enum class CategoryChartResult { invalid, missing, found };
struct CategoryChartMatch {
    category_layout::Category category;
    category_layout::Bar bar;
    category_layout::Chart chart;
    int row{-1};
};
// Caller proves current category lifetime and serializes native work.
// Validate the visible original tuples, allowing -1 request wildcards. No native
// calls, writes, allocation or protection queries. This is shared by request
// preflight (before group capture) and the subsequent exact native selector.
CategoryChartResult match_category_chart(MemoryView, std::uintptr_t category, int id, int style,
    int song, int p1, int p2, CategoryChartMatch&) noexcept;
} // namespace point_zero::native
