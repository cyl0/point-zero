#include "category_chart_match.hpp"
#include <windows.h>

namespace point_zero::native {
namespace {
template<class T> bool read(MemoryView m, std::uintptr_t at, T& out) noexcept {
    __try {
        return m.read && at >= 0x10000 && sizeof(T) <= UINTPTR_MAX - at &&
            m.read(m.context, at, std::as_writable_bytes(std::span(&out, 1)));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool fits(const category_layout::Vector& v, int count, std::size_t width, std::size_t maximum) noexcept {
    if (count < 0 || static_cast<std::size_t>(count) > maximum) return false;
    if (v == category_layout::Vector{}) return count == 0;
    return v.begin >= 0x10000 && !(v.begin & 7) &&
        v.begin <= v.end && v.end <= v.capacity && (v.end - v.begin) % width == 0 &&
        (v.capacity - v.begin) % width == 0 && v.capacity - v.begin <= maximum * width &&
        static_cast<std::size_t>(count) <= (v.end - v.begin) / width;
}
} // namespace
CategoryChartResult match_category_chart(MemoryView memory, std::uintptr_t category, int id, int style,
        int song, int p1, int p2, CategoryChartMatch& out) noexcept {
    using namespace category_layout;
    out = {};
    auto& c = out.category;
    if (style < 0 || style > 1 || song <= 0 || p1 < -1 || p1 > 4 || p2 < -1 || p2 > 4 ||
        !read(memory, category, c) || c.id != id ||
        !fits(c.bars, c.bar_count, sizeof(Bar), 20004) || !fits(c.charts, c.insert_count, sizeof(Chart), 20000))
        return CategoryChartResult::invalid;
    for (int n = 0; n < c.bar_count; ++n) {
        Bar bar;
        if (!read(memory, c.bars.begin + sizeof(Bar) * n, bar))
            return CategoryChartResult::invalid;
        if (bar.type != 1) continue;
        Chart chart;
        int actual{};
        if (bar.data < c.charts.begin || (bar.data - c.charts.begin) % sizeof(Chart) ||
            (bar.data - c.charts.begin) / sizeof(Chart) >= static_cast<std::size_t>(c.insert_count) ||
            !read(memory, bar.data, chart) || chart.style != style || chart.category != id ||
            chart.original_p1 < 0 || chart.original_p1 > 4 || chart.original_p2 < 0 || chart.original_p2 > 4 ||
            chart.music < 0x10000 || (chart.music & 7) || chart.music > UINTPTR_MAX - 0x680 ||
            !read(memory, chart.music + 0x67C, actual) || actual <= 0) return CategoryChartResult::invalid;
        if (actual == song && (p1 == -1 || p1 == chart.original_p1) && (p2 == -1 || p2 == chart.original_p2)) {
            out.row = n; out.bar = bar; out.chart = chart;
            return CategoryChartResult::found;
        }
    }
    return CategoryChartResult::missing;
}
} // namespace point_zero::native
