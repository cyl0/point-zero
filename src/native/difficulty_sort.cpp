#include "build_bindings.hpp"
#include "difficulty_sort.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
using namespace category_layout;

template<class T> bool global(MemoryView memory, ImageRange game, std::size_t rva, T& out) noexcept {
    return rva <= game.size && sizeof(T) <= game.size - rva && rva <= UINTPTR_MAX - game.base &&
        read_field(memory, game.base, rva, out);
}
bool vector_has(const Vector& v, std::size_t count, std::size_t width, std::size_t maximum) noexcept {
    return v.begin >= 0x10000 && !(v.begin & 7) && v.begin <= v.end && v.end <= v.capacity &&
        v.capacity - v.begin <= maximum * width && (v.end - v.begin) % width == 0 &&
        (v.capacity - v.begin) % width == 0 && count <= (v.end - v.begin) / width;
}
template<class T> bool copy(MemoryView memory, std::uintptr_t at, std::span<T> out) noexcept {
    return memory.read && at >= 0x10000 && out.size_bytes() <= UINTPTR_MAX - at &&
        memory.read(memory.context, at, std::as_writable_bytes(out));
}
bool writable(MemoryView memory, std::uintptr_t at, std::size_t size) noexcept {
    if (!memory.query || at < 0x10000 || !size || size > UINTPTR_MAX - at) return false;
    const auto end = at + size;
    while (at < end) {
        MemoryRegion region{};
        if (!memory.query(memory.context, at, region) || !contains(region.begin, region.size, at, 1) ||
            !region.committed || !region.readable || !region.writable || region.guarded) return false;
        at = (std::min)(end, region.begin + region.size);
    }
    return true;
}
} // namespace

DifficultySort::DifficultySort(const AdapterContext& context, SortNativeCalls calls, SortOwnedCategory owned)
    : context_(context), calls_(calls), owned_(owned) {
    if (!context.memory.read || !context.memory.query || !calls.mode || !calls.level || !owned.verify)
        throw std::invalid_argument("incomplete admitted difficulty sort dependencies");
}
void DifficultySort::prepare() {
    if (attempted_) throw std::runtime_error("difficulty sort preparation already attempted");
    attempted_ = true;
    if (context_.state.view().phase != FeaturePhase::preparing)
        throw std::runtime_error("difficulty sort preparation outside native initialization");
    entries_.resize(limit); before_.resize(limit + 4); work_.resize(limit + 4);
    chart_entries_.resize(limit);
    thread_ = GetCurrentThreadId(); prepared_ = true;
}
void DifficultySort::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !context_.host.log) return;
    constexpr std::string_view message = "effective difficulty sorting skipped an adjustment; later callbacks remain enabled";
    try { context_.host.log(context_.host.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
bool DifficultySort::observe(Context& out) const {
    const auto m = context_.memory;
    const auto game = context_.modules.game.range;
    int list_style{}, native_style{};
    // Ordinary scope only: 930140 can force bank0 through the native BATTLE
    // option even when DP is set. Do not rely only on a previously shared flag.
    if (!global(m, game, game_bindings(context_.modules.game.range).difficulty_sort.store_rva, out.manager) || !read_field(m, out.manager, 0x16F50, out.list) ||
        !read_field(m, out.manager, 0x29864, out.style) || out.style < 0 || out.style > 1 ||
        !read_field(m, out.list, 0x78, list_style) || list_style != out.style ||
        !global(m, game, game_bindings(context_.modules.game.range).difficulty_sort.style_flag_rva, native_style) || native_style != out.style ||
        !global(m, game, game_bindings(context_.modules.game.range).difficulty_sort.visible_bars_rva, out.bars) || out.bars != 1 ||
        !global(m, game, game_bindings(context_.modules.game.range).difficulty_sort.active_flags_rva, out.active) || !global(m, game, game_bindings(context_.modules.game.range).difficulty_sort.dp_flag_rva, out.dp) ||
        out.dp < 0 || out.dp > 1 || out.dp != out.style) return false;
    for (const auto value : out.active) if (value < 0 || value > 1) return false;
    out.players = out.active[0] + out.active[1];
    if (out.dp && out.players == 2) out.players = 1;
    if (out.players < 1 || out.players > 2 || (out.players == 2 && out.style != 0)) return false;
    out.mode = calls_.mode();
    return out.mode == 0 || out.mode == 6;
}
bool DifficultySort::run(const SortRequest& request, NativeQSort original) {
    const auto game = context_.modules.game.range;
    if (!contains(game.base, game.size, request.caller, 1) ||
        request.caller - game.base != game_bindings(game).difficulty_sort.sort_return_rva || request.compare != calls_.level ||
        request.width != sizeof(Bar) || !request.visible || request.count < 2 || request.count > limit) return false;
    Context context;
    Category category;
    if (!observe(context) || !read_field(context_.memory, request.category, 0, category) ||
        category.chart_count != static_cast<int>(request.count) ||
        category.insert_count < category.chart_count || category.insert_count > static_cast<int>(limit) ||
        category.bar_count < category.chart_count + 1 || category.bar_count > category.chart_count + 3 ||
        !vector_has(category.bars, category.bar_count, sizeof(Bar), limit + 4) ||
        !vector_has(category.charts, category.insert_count, sizeof(Chart), limit) ||
        request.bars < category.bars.begin) return false;
    const bool owned = owned_.verify(owned_.context, request.category, category.id, context.style, context.list);
    const auto offset = request.bars - category.bars.begin;
    if (offset != sizeof(Bar) && offset != 2 * sizeof(Bar)) return false;
    const auto first = offset / sizeof(Bar);
    const auto total = static_cast<std::size_t>(category.bar_count);
    if (total < first + request.count || total > first + request.count + 1) return false;
    auto before = std::span(before_).first(total), work = std::span(work_).first(total);
    auto entries = std::span(entries_).first(request.count);
    // Bar data is an admitted offset into one contiguous native chart vector.
    // Map offsets directly; the table also rejects duplicates/foreign rows.
    auto chart_entries = std::span(chart_entries_).first(static_cast<std::size_t>(category.insert_count));
    std::fill(chart_entries.begin(), chart_entries.end(), static_cast<std::uint32_t>(entries.size()));
    if (!copy(context_.memory, category.bars.begin, before) || before[0].type != 0 ||
        (first == 2 && before[1].type != 2) || (total > first + request.count && before.back().type != 3)) return false;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto& e = entries[i]; e = {}; e.bar = before[first + i]; e.rank = entries.size();
        if (e.bar.type != 1 || e.bar.data < category.charts.begin ||
            e.bar.data - category.charts.begin >= static_cast<std::size_t>(category.insert_count) * sizeof(Chart) ||
            (e.bar.data - category.charts.begin) % sizeof(Chart) ||
            !read_field(context_.memory, e.bar.data, 0, e.chart) || e.chart.category != category.id ||
            e.chart.style != context.style || e.chart.p1 < 0 || e.chart.p1 > 4 ||
            e.chart.p2 < 0 || e.chart.p2 > 4 ||
            !read_field(context_.memory, e.chart.music, 0x67C, e.song) || e.song <= 0) return false;
        const auto chart_index = (e.bar.data - category.charts.begin) / sizeof(Chart);
        if (chart_entries[chart_index] != entries.size()) return false;
        chart_entries[chart_index] = static_cast<std::uint32_t>(i);
        // Native difficulty order follows the P1 working field even paired.
        const auto* row = context_.catalog.find({e.song, context.style, e.chart.p1});
        if (row) e.key = row->effective_tenths;
        else {
            std::int8_t level{};
            if (!read_field(context_.memory, e.chart.music, 0x3EC + 5 * context.style + e.chart.p1, level) ||
                level < 1 || level > 12) return false;
            e.key = 10 * level;
        }
        if (e.key < 10 || e.key > 129) return false;
    }
    native_started_ = true;
    original(reinterpret_cast<void*>(request.bars), request.count, request.width, request.compare);
    Context after;
    Category current;
    if (!observe(after) || after != context ||
        !read_field(context_.memory, request.category, 0, current) || current != category ||
        (owned && !owned_.verify(owned_.context, request.category, category.id, context.style, context.list)) ||
        !copy(context_.memory, category.bars.begin, work)) { fault(); return true; }
    // Preserve actual native tie behavior, including equality. Never invent a
    // pointer/song-ID tie rule, nor mix native and effective primary comparisons.
    for (std::size_t i = 0; i < request.count; ++i) {
        const auto& bar = work[first + i];
        if (bar.data < category.charts.begin ||
            (bar.data - category.charts.begin) % sizeof(Chart)) { fault(); return true; }
        const auto chart_index = (bar.data - category.charts.begin) / sizeof(Chart);
        if (chart_index >= chart_entries.size() || chart_entries[chart_index] >= entries.size()) {
            fault(); return true;
        }
        auto& entry = entries[chart_entries[chart_index]];
        Chart chart;
        int song{};
        if (entry.bar != bar || entry.rank != entries.size() ||
            !read_field(context_.memory, bar.data, 0, chart) || chart != entry.chart ||
            !read_field(context_.memory, chart.music, 0x67C, song) || song != entry.song) { fault(); return true; }
        entry.rank = i;
    }
    for (std::size_t i = 0; i < total; ++i)
        if ((i < first || i >= first + request.count) && work[i] != before[i]) { fault(); return true; }
    std::copy(work.begin(), work.end(), before.begin());
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.key != b.key ? a.key < b.key : a.rank < b.rank;
    });
    for (std::size_t i = 0; i < entries.size(); ++i) work[first + i] = entries[i].bar;
    if (std::equal(work.begin(), work.end(), before.begin())) return true;
    if (!writable(context_.memory, request.bars, request.count * sizeof(Bar))) {
        fault(); return true;
    }
    std::memcpy(reinterpret_cast<void*>(request.bars), work.data() + first, request.count * sizeof(Bar));
    return true;
}
bool DifficultySort::sort(const SortRequest& request, NativeQSort original, bool effective) noexcept {
    if (!prepared_ || thread_ != GetCurrentThreadId() || busy_ || !original) return false;
    const auto view = context_.state.view();
    if (!effective || view.phase != FeaturePhase::ready) return false;
    busy_ = true; native_started_ = false;
    __try {
        __try { return run(request, original); }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault(); return native_started_; }
    } __finally { busy_ = false; }
}
} // namespace point_zero::native
