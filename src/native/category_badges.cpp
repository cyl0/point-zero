#include "category_badges.hpp"
#include "category_layout.hpp"
#include "level_population.hpp"

#include <windows.h>
#include <array>
#include <cstring>
#include <vector>

namespace point_zero::native {
namespace {
using namespace category_layout;
constexpr std::size_t row_limit = 6 * LevelPopulation::style_limit;
constexpr std::size_t summary_size = 168;
struct Prefix {
    std::uintptr_t music{};
    int working_p1{}, working_p2{}, original_p1{}, original_p2{}, style{}, id{};
};
static_assert(sizeof(Prefix) == 32);
struct Row {
    Bar bar;
    Prefix chart;
    int song{};
};
struct Entry {
    CategoryAccessBinding binding;
    Category category;
    std::array<std::byte, 4> badges{};
    std::size_t first{}, count{}, prefix{};
};
bool buffer(std::uintptr_t at, std::size_t size) noexcept {
    return at >= 0x10000 && !(at & 7) && size <= UINTPTR_MAX - at;
}
bool overlap(std::uintptr_t a, std::size_t n, std::uintptr_t b, std::size_t m) noexcept {
    return n && m && a < b + m && b < a + n;
}
template<class T> bool read(MemoryView memory, std::uintptr_t at, T& out) noexcept {
    __try {
        return at >= 0x10000 && sizeof(T) <= UINTPTR_MAX - at &&
            memory.read(memory.context, at, std::as_writable_bytes(std::span(&out, 1)));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool writable(MemoryView memory, std::uintptr_t at) noexcept {
    __try { return readable_range(memory, at, 168, RangeUse::private_writable); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool publish(std::uintptr_t at, const std::array<std::byte, 4>& bytes) noexcept {
    __try { std::memcpy(reinterpret_cast<void*>(at), bytes.data(), bytes.size()); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool vector(const Vector& v, std::size_t width, std::size_t count, std::size_t maximum) noexcept {
    if (!v.begin && !v.end && !v.capacity) return count == 0;
    return buffer(v.begin, 0) && v.begin <= v.end && v.end <= v.capacity &&
        (v.end - v.begin) % width == 0 && (v.capacity - v.begin) % width == 0 &&
        v.capacity - v.begin <= maximum * width && count <= (v.end - v.begin) / width;
}
bool shape(MemoryView memory, Entry& e) noexcept {
    const auto& b = e.binding;
    if (!b.category && !b.summary) return true;
    if (!buffer(b.category, sizeof(Category)) || !buffer(b.summary, summary_size) ||
        overlap(b.category, sizeof(Category), b.summary, summary_size) ||
        !read(memory, b.category, e.category) ||
        !writable(memory, b.summary)) return false;
    const auto& c = e.category;
    if (c.id != b.id || c.insert_count < 0 || c.insert_count > static_cast<int>(LevelPopulation::style_limit) ||
        c.chart_count < 0 || c.chart_count > c.insert_count || c.bar_count < 0 ||
        !vector(c.charts, sizeof(Chart), c.insert_count, LevelPopulation::style_limit) ||
        !vector(c.bars, sizeof(Bar), c.bar_count, LevelPopulation::style_limit + 3)) return false;
    e.count = static_cast<std::size_t>(c.chart_count);
    if (!c.bar_count) return !e.count;
    Bar bar;
    if (!read(memory, c.bars.begin, bar) || bar.type != 0 || bar.data != b.category + 0x38) return false;
    e.prefix = 1;
    if (c.bar_count > 1) {
        if (!read(memory, c.bars.begin + sizeof(Bar), bar)) return false;
        if (bar.type == 2) {
            if (bar.data != b.category + 0x68) return false;
            e.prefix = 2;
        }
    }
    const auto end = e.prefix + e.count;
    if (static_cast<std::size_t>(c.bar_count) == end) return true;
    return static_cast<std::size_t>(c.bar_count) == end + 1 &&
        read(memory, c.bars.begin + end * sizeof(Bar), bar) && bar.type == 3 && bar.data == b.category + 0x90;
}
bool row(MemoryView memory, const Entry& e, std::size_t index, Row& out) noexcept {
    const auto& c = e.category;
    if (!read(memory, c.bars.begin + (e.prefix + index) * sizeof(Bar), out.bar) || out.bar.type != 1 ||
        out.bar.data < c.charts.begin || (out.bar.data - c.charts.begin) % sizeof(Chart) ||
        !contains(c.charts.begin, static_cast<std::size_t>(c.insert_count) * sizeof(Chart), out.bar.data, sizeof(Chart)) ||
        !read(memory, out.bar.data, out.chart)) return false;
    const auto& r = out.chart;
    return buffer(r.music, 0x680) && r.id == e.binding.id && r.style == e.binding.style &&
        r.original_p1 >= 0 && r.original_p1 <= 4 && r.original_p2 == r.original_p1 &&
        read(memory, r.music + 0x67C, out.song) && out.song > 0;
}
// Contain native SEH before returning through the snapshot vectors. Let C++
// exceptions unwind normally to checked(), including their exception objects.
bool evaluate(const CategoryBadgeCalls& calls, unsigned badge, std::uintptr_t manager,
              const Entry& e, std::size_t index, const Row& expected, std::uint8_t& result) {
    __try {
        const auto chart = calls.chart_at(e.binding.category, static_cast<int>(index));
        if (chart != expected.bar.data) return false;
        const auto owner = badge < 2 ? calls.music_owner() : badge == 3 ? calls.event_owner() : manager;
        if (calls.music_of(chart) != expected.chart.music) return false;
        const auto song = static_cast<unsigned>(expected.song);
        result = badge == 0 ? calls.weekly(owner, song) : badge == 1 ? calls.recommend(owner, song) :
            badge == 2 ? calls.shop(owner, song) : calls.kac(owner, song, static_cast<unsigned>(e.binding.style));
        return true;
    } __except (GetExceptionCode() == 0xE06D7363U ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool run(MemoryView memory, const CategoryBadgeCalls& calls, std::uintptr_t manager,
         std::span<const CategoryAccessBinding> input, bool& wrote) {
    if (!memory.read || !memory.query || !calls.music_owner || !calls.event_owner || !calls.chart_at ||
        !calls.music_of || !calls.weekly || !calls.recommend || !calls.shop || !calls.kac ||
        !buffer(manager, 0x29868) || input.size() > 288) return false;
    std::vector<Entry> entries(input.size());
    std::array<unsigned, 2> counts{};
    std::size_t total{};
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto& e = entries[i];
        e.binding = input[i];
        const auto& b = e.binding;
        if (b.id < 230 || b.style < 0 || b.style > 1 || ++counts[b.style] > 144 ||
            (i && (input[i - 1].style > b.style || (input[i - 1].style == b.style && input[i - 1].id >= b.id))) ||
            !shape(memory, e) || e.count > row_limit - total) return false;
        e.first = total; total += e.count;
    }
    std::vector<Row> rows(total);
    for (const auto& e : entries)
        for (std::size_t i = 0; i < e.count; ++i) if (!row(memory, e, i, rows[e.first + i])) return false;
    for (unsigned badge = 0; badge < 4; ++badge) {
        for (auto& e : entries) {
            for (std::size_t i = 0; i < e.count; ++i) {
                const auto& expected = rows[e.first + i];
                // Match the native call order; owner getters run per row too.
                std::uint8_t result{};
                bool evaluated{};
                try { evaluated = evaluate(calls, badge, manager, e, i, expected, result); }
                catch (...) {}
                if (!evaluated) continue;
                if (result) e.badges[badge] = std::byte{1};
            }
        }
    }
    for (const auto& e : entries) {
        if (!e.binding.summary) continue;
        wrote = true;
        if (!publish(e.binding.summary, e.badges)) return false;
    }
    return true;
}
bool checked(MemoryView memory, const CategoryBadgeCalls& calls, std::uintptr_t manager,
             std::span<const CategoryAccessBinding> input, bool& wrote) noexcept {
    try { return run(memory, calls, manager, input, wrote); }
    catch (...) { return false; }
}
} // namespace

CategoryBadgeResult refresh_category_badges(MemoryView memory, const CategoryBadgeCalls& calls,
    std::uintptr_t manager, std::span<const CategoryAccessBinding> entries) noexcept {
    bool wrote{}, complete{};
    __try { complete = checked(memory, calls, manager, entries, wrote); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return complete ? CategoryBadgeResult::complete : wrote ? CategoryBadgeResult::uncertain : CategoryBadgeResult::rejected;
}
} // namespace point_zero::native
