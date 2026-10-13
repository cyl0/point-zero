#include "build_bindings.hpp"
#include "level_population.hpp"

#include <windows.h>
#include <algorithm>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr std::size_t stride = sizeof(category_layout::Chart);
bool pointer(std::uintptr_t value) noexcept { return value >= 0x10000 && (value & 7) == 0; }
bool vector_has(const category_layout::Vector& v, std::size_t count) noexcept {
    if (!v.begin && !v.end && !v.capacity) return count == 0;
    return pointer(v.begin) && pointer(v.end) && pointer(v.capacity) &&
        v.begin <= v.end && v.end <= v.capacity &&
        v.capacity - v.begin <= LevelPopulation::style_limit * stride &&
        (v.end - v.begin) % stride == 0 && (v.capacity - v.begin) % stride == 0 &&
        count <= (v.end - v.begin) / stride;
}
} // namespace

LevelPopulation::LevelPopulation(MemoryView memory, ImageRange game, const Catalog& catalog)
    : memory_(memory), game_(game), catalog_(catalog) {
    if (!memory.read) throw std::invalid_argument("missing native LEVEL population reader");
}
void LevelPopulation::prepare() {
    if (attempted_) throw std::runtime_error("LEVEL population preparation already attempted");
    attempted_ = true;
    charts_.resize(2 * style_limit);
    thread_ = GetCurrentThreadId(); prepared_ = true;
}
bool LevelPopulation::owner() const noexcept {
    std::uintptr_t manager{}, list{}, vt{};
    return pointer(game_.base) && contains(game_.base, game_.size, game_.base, game_.size) &&
        game_.size >= game_bindings(game_).level_population.category_manager_rva + 8 && pointer(manager_) && pointer(list_) &&
        read_field(memory_, game_.base, game_bindings(game_).level_population.category_manager_rva, manager) && manager == manager_ &&
        read_field(memory_, manager_, 0x175D0, list) && list == list_ &&
        read_field(memory_, list_, 0, vt) && vt == game_.base + game_bindings(game_).level_population.list_vtable_rva;
}
bool LevelPopulation::source(unsigned index, Source& out) const noexcept {
    const auto style = index / 12;
    const int id = 6 + static_cast<int>(index % 12);
    const auto offset = 16 + sizeof(category_layout::Category) * (230ULL * style + id);
    if (index >= 24 || offset > UINTPTR_MAX - manager_) return false;
    const auto category = manager_ + offset;
    return read_field(memory_, category, 0, out.id) && out.id == id &&
        read_field(memory_, category, 0xC, out.count) && out.count >= 0 &&
        out.count <= static_cast<int>(style_limit) &&
        read_field(memory_, category, 0x10, out.inserted) && out.inserted == out.count &&
        read_field(memory_, category, 0x78, out.charts) && vector_has(out.charts, out.count);
}
bool LevelPopulation::collect() noexcept {
    if (!owner()) return false;
    std::array<Source, 24> sources{};
    std::array<std::size_t, 2> totals{};
    for (unsigned s = 0; s < sources.size(); ++s) {
        auto& src = sources[s];
        if (!source(s, src)) return false;
        auto& total = totals[s / 12];
        total += src.count;
        if (total > style_limit) return false;
    }
    for (unsigned s = 0; s < sources.size(); ++s) {
        const auto& src = sources[s];
        const int style = static_cast<int>(s / 12), level = src.id - 5;
        for (int i = 0; i < src.count; ++i) {
            const auto address = src.charts.begin + static_cast<std::size_t>(i) * stride;
            category_layout::Chart n{};
            // Share the native layout, while reading only original identity.
            // Working difficulties, scores and unused tail bytes are not inputs.
            auto original = std::as_writable_bytes(std::span(&n, 1)).subspan(
                offsetof(category_layout::Chart, original_p1), 4 * sizeof(std::int32_t));
            int song{};
            std::int8_t raw{};
            if (!read_field(memory_, address, 0, n.music) || !pointer(n.music) ||
                !memory_.read(memory_.context, address + offsetof(category_layout::Chart, original_p1), original) ||
                n.style != style || n.category != src.id ||
                n.original_p1 < 0 || n.original_p1 > 4 ||
                !read_field(memory_, n.music, 0x67C, song) || song <= 0 ||
                !read_field(memory_, n.music, 0x3EC + 5 * style + n.original_p1, raw) || raw != level) return false;
            const ratings::Key key{song, style, n.original_p1};
            const auto* bound = catalog_.find(key);
            if (!bound || bound->native_level != raw || bound->effective_tenths < 10 ||
                bound->effective_tenths > 129 || (!bound->assigned && bound->effective_tenths != 10 * raw))
                return false;
            charts_[count_++] = {n.music, key, raw, bound->effective_tenths, bound->assigned};
        }
    }
    auto rows = std::span(charts_).first(count_);
    std::sort(rows.begin(), rows.end(), [](const LevelChart& a, const LevelChart& b) { return a.key < b.key; });
    for (std::size_t i = 1; i < rows.size(); ++i) if (rows[i - 1].key == rows[i].key) return false;
    return true;
}
bool LevelPopulation::capture(std::uintptr_t manager, std::uintptr_t list) noexcept {
    if (!prepared_ || thread_ != GetCurrentThreadId() || busy_) return false;
    busy_ = true; valid_ = false; count_ = 0; manager_ = manager; list_ = list;
    __try {
        __try { valid_ = collect(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { valid_ = false; }
    } __finally { busy_ = false; }
    return valid_;
}
std::span<const LevelChart> LevelPopulation::charts() const noexcept {
    return valid_ ? std::span(charts_).first(count_) : std::span<const LevelChart>{};
}
} // namespace point_zero::native
