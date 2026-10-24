#include "build_bindings.hpp"
#include "category_roots.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using category_layout::Vector;
constexpr std::size_t limit = 4096, capacity_limit = 6144;
struct Entry { std::uintptr_t pointer{}; int id{}; bool operator==(const Entry&) const = default; };
struct Allocation { Vector vector; std::uintptr_t base{}; bool operator==(const Allocation&) const = default; };
struct Visible {
    std::array<Vector, 2> vectors{};
    std::array<int, 2> cursors{};
    bool operator==(const Visible&) const = default;
};
static_assert(sizeof(Visible) == 56 && offsetof(Visible, cursors) == 48);
bool pointer(std::uintptr_t at, std::size_t bytes = 0) noexcept {
    return at >= 0x10000 && !(at & 7) && bytes <= UINTPTR_MAX - at;
}
template<class T> bool read(MemoryView m, std::uintptr_t at, T& out) noexcept {
    __try {
        return read_field(m, at, 0, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::size_t size(const Vector& v) noexcept { return (v.end - v.begin) / 8; }
bool overlap(const Allocation& a, const Allocation& b) noexcept {
    return a.base && b.base && a.base < b.vector.capacity && b.base < a.vector.capacity;
}
bool cursor(const Vector& v, int value) noexcept {
    return size(v) ? value >= 0 && static_cast<std::size_t>(value) < size(v) : value == 0 || value == -1;
}
int select(std::span<const Entry> values, Entry wanted, int fallback = 0) noexcept {
    if (values.empty()) return fallback == -1 ? -1 : 0;
    const auto found = std::find(values.begin(), values.end(), wanted);
    return found != values.end() ? static_cast<int>(found - values.begin())
        : (std::clamp)(fallback, 0, static_cast<int>(values.size()) - 1);
}
} // namespace
bool find_level_group(MemoryView memory, ImageRange, std::uintptr_t list,
                      std::uintptr_t& group, std::uintptr_t expected) noexcept {
    std::uintptr_t head{}, node{};
    if (!pointer(list, 0x88) ||
        !read(memory, list + 16, head) || !pointer(head, 8) || !read(memory, head, node)) return false;
    group = 0;
    for (unsigned n = 0; node != head; ++n) {
        std::uintptr_t next{}, value{};
        int id{};
        if (n >= 64 || !pointer(node, 32) || !read(memory, node, next) ||
            !read(memory, node + 16, id) || !read(memory, node + 24, value)) return false;
        if (id == 1) {
            if (group || (expected && value != expected) || !pointer(value, 0xB0) ||
                !read(memory, value + 8, id) || id != 1) return false;
            group = value;
        } else if (expected && value == expected) return false;
        node = next;
    }
    return group != 0;
}

bool selected_group_id(MemoryView memory, std::uintptr_t list, int& id) noexcept {
    id = -1;
    int selected{}, index{}, wanted{};
    Vector ids{};
    if (!pointer(list, 0x88) || !read(memory, list + 0x78, selected) || selected < 0 || selected > 1 ||
        !read(memory, list + 0x7C, index) || index < 0 ||
        !read(memory, list + 0x48 + 0x18ULL * selected, ids) || !pointer(ids.begin) ||
        ids.begin > ids.end || ids.end > ids.capacity ||
        (ids.end - ids.begin) % 4 || (ids.capacity - ids.begin) % 4 ||
        (ids.capacity - ids.begin) / 4 > limit ||
        static_cast<std::size_t>(index) >= (ids.end - ids.begin) / 4 ||
        !read(memory, ids.begin + 4ULL * index, wanted)) return false;
    std::uintptr_t head{}, node{}, found{};
    if (!read(memory, list + 0x10, head) || !pointer(head, 8) || !read(memory, head, node)) return false;
    for (unsigned n = 0; node != head; ++n) {
        std::uintptr_t next{}, value{};
        int key{}, actual{};
        if (n >= 64 || !pointer(node, 32) || !read(memory, node, next) ||
            !read(memory, node + 16, key) || !read(memory, node + 24, value)) return false;
        if (key == wanted) {
            if (found || !pointer(value, 16) || !read(memory, value + 8, actual) || actual != wanted) return false;
            found = value;
        }
        node = next;
    }
    if (!found) return false;
    id = wanted;
    return true;
}

struct CategoryRoots::Backing {
    MemoryView memory;
    ImageRange game;
    CategoryVectorCalls calls;
    std::uintptr_t manager{}, list{}, group{};
    CategoryRootsState state{CategoryRootsState::empty};
    Visible before{}, root_page{};
    std::array<Allocation, 2> native{}, projected{}, replacement{}, children{};
    // merged retains the hidden roots while a child page is visible.
    std::array<std::vector<Entry>, 2> originals, values, merged;
    std::array<std::vector<Entry>, 2> child_rows;
    std::array<std::array<std::uintptr_t, CategoryBank::per_style>, 2> categories{};
    std::array<std::array<int, 12>, 2> child_parts{};
    int first_id{-1}, page{};

    Backing(MemoryView m, ImageRange g, CategoryVectorCalls c) : memory(m), game(g), calls(c) {
        for (auto& parts : child_parts) parts.fill(1); // ALL initially.
    }
    int local(int id, int style) const noexcept {
        if (first_id < 0 || style < 0 || style > 1 || id < first_id || id - first_id >= CategoryBank::per_style)
            return -1;
        return categories[style][id - first_id] ? id - first_id : -1;
    }
    int id(int level, int part) const noexcept {
        return first_id >= 0 && level >= 1 && level <= 12 && part >= 0 && part < 12
            ? first_id + (12 - level) * 12 + part : -1;
    }
    Entry binding(int level, int part, int style) const noexcept {
        const int value = id(level, part), index = local(value, style);
        return {index >= 0 ? categories[style][index] : 0, value};
    }
    bool owns(Entry value, int style) const noexcept {
        const int index = local(value.id, style);
        return index >= 0 && categories[style][index] == value.pointer;
    }
    int native_level(Entry value, int style) const noexcept {
        return value.id >= 13 && value.id <= 24 &&
            value.pointer == manager + 16 + sizeof(category_layout::Category) * (226ULL * style + value.id)
            ? value.id - 12 : 0;
    }
    Entry parent(Entry value, int style) const noexcept {
        const int level = native_level(value, style);
        const auto target = binding(level, 0, style);
        return target.pointer ? target : value;
    }
    Entry original(Entry value, int style) const noexcept {
        if (!owns(value, style)) return value;
        const int index = local(value.id, style);
        if (index % 12) return {};
        const int level = 12 - index / 12;
        const auto& rows = originals[style];
        const auto found = std::find_if(rows.begin(), rows.end(), [&](Entry e) { return native_level(e, style) == level; });
        return found != rows.end() ? *found : Entry{};
    }
    bool inspect(Vector v, Allocation& out) const noexcept {
        out = {v, v.begin};
        if (v == Vector{}) return true;
        if (!pointer(v.begin) || v.begin > v.end || v.end > v.capacity ||
            (v.end - v.begin) % 8 || (v.capacity - v.begin) % 8 || v.capacity == v.begin ||
            size(v) > limit || (v.capacity - v.begin) / 8 > capacity_limit ||
            !readable_range(memory, v.begin, v.capacity - v.begin, RangeUse::private_writable)) return false;
        if (v.capacity - v.begin >= 4096) {
            if (!read(memory, v.begin - 8, out.base) || !pointer(out.base) || out.base > v.begin - 8 ||
                v.begin - out.base - 8 > 31 ||
                !readable_range(memory, out.base, v.capacity - out.base, RangeUse::private_writable)) return false;
        }
        return !(out.base < group + 0xB0 && group < v.capacity);
    }
    bool intact(const Allocation& a) const noexcept {
        Allocation current;
        return inspect(a.vector, current) && current == a;
    }
    bool entry(std::uintptr_t at, Entry& out) const noexcept {
        return read(memory, at, out.pointer) && pointer(out.pointer, sizeof(category_layout::Category)) &&
            read(memory, out.pointer, out.id);
    }
    bool agrees(const Vector& v, std::span<const Entry> expected) const noexcept {
        if (size(v) != expected.size()) return false;
        for (std::size_t n = 0; n < expected.size(); ++n) {
            std::uintptr_t current{};
            if (!read(memory, v.begin + n * 8, current) || current != expected[n].pointer) return false;
        }
        return true;
    }
    bool populated(Entry value) const noexcept {
        int id{}, count{};
        return read(memory, value.pointer, id) && id == value.id &&
            read(memory, value.pointer + 0x10, count) && count > 0;
    }
    bool capture(const Vector& v, std::vector<Entry>& rows, int style) const {
        rows.clear();
        rows.reserve(size(v));
        for (std::size_t n = 0; n < size(v); ++n) {
            Entry value;
            if (!entry(v.begin + n * 8, value) || (owns(value, style) && !populated(value))) return false;
            rows.push_back(value);
        }
        return true;
    }
    bool project(std::span<const Entry> source, std::span<const Entry> owned,
                 std::vector<Entry>& dest, int style) const {
        const auto foreign = std::count_if(source.begin(), source.end(), [&](Entry e) { return !owns(e, style); });
        if (static_cast<std::size_t>(foreign) + owned.size() > limit) return false;
        dest.clear();
        dest.reserve(static_cast<std::size_t>(foreign) + owned.size());
        bool added{};
        for (const auto value : source) {
            if (!owns(value, style)) dest.push_back(value);
            else if (!added) {
                dest.insert(dest.end(), owned.begin(), owned.end());
                added = true;
            }
        }
        if (!added) dest.insert(dest.begin(), owned.begin(), owned.end());
        return true;
    }
    bool current() const noexcept {
        std::uintptr_t m{}, l{}, found{};
        return read(memory, game.base + game_bindings(game).category_roots.category_manager_rva, m) && m == manager &&
            read(memory, manager + 0x16F50, l) && l == list &&
            find_level_group(memory, game, list, found, group);
    }
    bool visible(Visible& out) const noexcept { return read(memory, group + 0x78, out); }
    bool selected(int style) const noexcept {
        int manager_style{}, list_style{}, group_id{};
        return read(memory, manager + 0x29864, manager_style) && manager_style == style &&
            read(memory, list + 0x78, list_style) && list_style == style &&
            selected_group_id(memory, list, group_id) && group_id == 1;
    }
    bool write(const Visible& value) noexcept {
        if (!readable_range(memory, group + 0x78, sizeof(value), RangeUse::private_writable)) return false;
        std::memcpy(reinterpret_cast<void*>(group + 0x78), &value, sizeof(value));
        return true;
    }
    bool build(Allocation& out, std::span<const Entry> entries) {
        if (out.vector != Vector{} || entries.size() > limit) return false;
        for (std::size_t n = 0; n < entries.size(); ++n) {
            auto& v = out.vector;
            if (v.end == v.capacity) {
                const auto result = calls.grow(&v, v.end, &entries[n].pointer);
                Allocation grown;
                if (!inspect(v, grown) || size(v) != n + 1 || result != v.begin + n * 8) return false;
                out = grown;
            } else {
                // Growth admitted this allocation; only end advances between
                // native calls. Check the spare slot without rescanning rows.
                if (v.end > v.capacity || v.capacity - v.end < 8 || size(v) != n) return false;
                std::memcpy(reinterpret_cast<void*>(v.end), &entries[n].pointer, 8);
                v.end += 8;
            }
        }
        return intact(out);
    }
    bool release(Allocation& allocation) {
        if (!intact(allocation)) return false;
        if (allocation.vector != Vector{}) calls.destroy(&allocation.vector);
        if (allocation.vector != Vector{}) return false;
        allocation.base = 0;
        return true;
    }
    bool separate(std::span<const Allocation> a) const noexcept {
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = 0; j < i; ++j) if (overlap(a[i], a[j])) return false;
        return true;
    }
    bool snapshot(std::span<const CategoryAccessBinding> bank) {
        if (bank.size() != 2 * CategoryBank::per_style || !current() || !visible(before)) return false;
        const int base = bank[0].id;
        if (base < 0 || base > (std::numeric_limits<int>::max)() - (CategoryBank::per_style - 1)) return false;
        first_id = base;
        for (std::size_t n = 0; n < bank.size(); ++n) {
            const auto& b = bank[n];
            const int style = static_cast<int>(n / CategoryBank::per_style), local = static_cast<int>(n % CategoryBank::per_style);
            if (b.style != style || b.id != first_id + local || (!b.category != !b.summary)) return false;
            if (!b.category) continue;
            int actual{}, inserted{};
            if (!pointer(b.category, 208) || !pointer(b.summary, 168) || !read(memory, b.category, actual) || actual != b.id ||
                !read(memory, b.category + 0x10, inserted) || inserted <= 0) return false;
            categories[style][local] = b.category;
        }
        for (int style = 0; style < 2; ++style) {
            if (!inspect(before.vectors[style], native[style]) || !cursor(before.vectors[style], before.cursors[style])) return false;
            auto& source = originals[style];
            if (!capture(before.vectors[style], source, style)) return false;
            auto& dest = values[style];
            dest.reserve(source.size());
            for (const auto value : source) dest.push_back(parent(value, style));
            std::vector<Entry> added;
            for (int l = 1; l <= 12; ++l) {
                const auto value = binding(l, 0, style);
                if (value.pointer && std::find(dest.begin(), dest.end(), value) == dest.end()) added.push_back(value);
            }
            if (dest.size() + added.size() > limit) return false;
            for (const auto value : added) {
                auto position = dest.begin();
                const int level = 12 - local(value.id, style) / 12;
                for (auto at = dest.begin(); at != dest.end(); ++at) {
                    if (!owns(*at, style)) continue;
                    if (12 - local(at->id, style) / 12 > level) { position = at; break; }
                    position = at + 1;
                }
                dest.insert(position, value);
            }
        }
        return separate(native);
    }
    // Observe the actual current allocation first. The previously published
    // address may already have been freed by native growth; never read it.
    bool observe(Visible& out, std::array<Allocation, 2>& actual) {
        if (!visible(out)) return false;
        for (int s = 0; s < 2; ++s) {
            if (!inspect(out.vectors[s], actual[s]) || !cursor(out.vectors[s], out.cursors[s]) ||
                !capture(out.vectors[s], values[s], s) || !intact(native[s]) || !agrees(native[s].vector, originals[s])) return false;
        }
        const std::array all{native[0], native[1], actual[0], actual[1]};
        return separate(all);
    }
    bool observe_page(Visible& out, std::array<Allocation, 2>& actual) {
        if (!page || !visible(out)) return false;
        for (int s = 0; s < 2; ++s) {
            if (!inspect(out.vectors[s], actual[s]) || !cursor(out.vectors[s], out.cursors[s]) ||
                !capture(out.vectors[s], child_rows[s], s) ||
                projected[s].vector != root_page.vectors[s] || !intact(projected[s]) ||
                !agrees(root_page.vectors[s], merged[s]) || !intact(native[s]) ||
                !agrees(native[s].vector, originals[s])) return false;
        }
        const std::array all{native[0], native[1], projected[0], projected[1], actual[0], actual[1]};
        return separate(all);
    }
};

CategoryRoots::CategoryRoots(MemoryView memory, ImageRange game, CategoryVectorCalls calls) {
    if (!memory.read || !memory.query || !calls.grow || !calls.destroy ||
        !pointer(game.base, game.size) || game.size < game_bindings(game).category_roots.category_storage_rva)
        throw std::invalid_argument("incomplete native LEVEL root dependencies");
    backing_ = std::make_unique<Backing>(memory, game, calls);
}
CategoryRoots::~CategoryRoots() noexcept {
    const auto& b = *backing_;
    if (b.state == CategoryRootsState::published || b.state == CategoryRootsState::uncertain ||
        b.projected[0].vector.begin || b.projected[1].vector.begin || b.replacement[0].vector.begin || b.replacement[1].vector.begin ||
        b.children[0].vector.begin || b.children[1].vector.begin)
        (void)backing_.release();
}
bool CategoryRoots::prepare_checked(std::uintptr_t manager, std::uintptr_t list, std::uintptr_t group,
                                    std::span<const CategoryAccessBinding> bank) noexcept {
    auto& b = *backing_;
    b.state = CategoryRootsState::rejected;
    try {
        if (!pointer(manager, 0x29868) || !pointer(list, 0x88) || !pointer(group, 0xB0)) return false;
        b.manager = manager; b.list = list; b.group = group;
        if (!b.snapshot(bank)) return false;
        for (int s = 0; s < 2; ++s) if (!b.build(b.projected[s], b.values[s])) {
            b.state = CategoryRootsState::uncertain; return false;
        }
        const std::array all{b.native[0], b.native[1], b.projected[0], b.projected[1]};
        if (!b.separate(all)) return false;
        b.state = CategoryRootsState::prepared; return true;
    } catch (...) { b.state = CategoryRootsState::uncertain; return false; }
}
bool CategoryRoots::prepare(std::uintptr_t manager, std::uintptr_t list, std::uintptr_t group,
                           std::span<const CategoryAccessBinding> bank) noexcept {
    if (attempted_ || busy_ || backing_->state != CategoryRootsState::empty) return false;
    attempted_ = true; busy_ = true; thread_ = GetCurrentThreadId();
    bool result{};
    __try {
        __try { result = prepare_checked(manager, list, group, bank); }
        __except (EXCEPTION_EXECUTE_HANDLER) { backing_->state = CategoryRootsState::uncertain; }
    } __finally { busy_ = false; }
    return result;
}
bool CategoryRoots::publish_checked() noexcept {
    auto& b = *backing_;
    if (b.state != CategoryRootsState::prepared) return false;
    b.state = CategoryRootsState::rejected;
    try {
        Visible now, next;
        if (!b.current() || !b.visible(now) || now != b.before) { b.state = CategoryRootsState::rejected; return false; }
        for (int s = 0; s < 2; ++s) {
            if (!b.intact(b.native[s]) || !b.agrees(b.native[s].vector, b.originals[s]) ||
                !b.intact(b.projected[s]) || !b.agrees(b.projected[s].vector, b.values[s])) return false;
            next.vectors[s] = b.projected[s].vector;
            const auto old = b.originals[s].empty() ? Entry{} : b.originals[s][now.cursors[s]];
            next.cursors[s] = select(b.values[s], b.parent(old, s), now.cursors[s]);
        }
        b.state = CategoryRootsState::uncertain;
        if (!b.write(next)) return false;
        b.state = CategoryRootsState::published; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::select_parent_checked(int wanted, int style) noexcept {
    auto& b = *backing_;
    if (wanted < 1 || wanted > 12 || style < 0 || style > 1 ||
        b.state != CategoryRootsState::published || b.page) return false;
    try {
        Visible now;
        std::array<Allocation, 2> actual;
        if (!b.current() || !b.selected(style) || !b.observe(now, actual)) return false;
        const auto target = b.binding(wanted, 0, style);
        const auto& rows = b.values[style];
        const auto found = std::find(rows.begin(), rows.end(), target);
        Entry selected;
        if (!target.pointer || found == rows.end() || !size(now.vectors[style]) ||
            !b.entry(now.vectors[style].begin + 8ULL * now.cursors[style], selected)) return false;
        const int next = static_cast<int>(found - rows.begin());
        const auto at = b.group + 0xA8 + 4ULL * style;
        if (next != now.cursors[style] &&
            !readable_range(b.memory, at, sizeof(next), RangeUse::private_writable)) return false;
        std::uint8_t current_open{}, target_open{};
        if (!read(b.memory, selected.pointer + 4, current_open) || current_open ||
            !read(b.memory, target.pointer + 4, target_open) || target_open) return false;
        if (next == now.cursors[style]) return true;

        // Only the selected cursor is ours to write. Keep the observed native
        // descriptors (including reallocation/appends) and the other cursor.
        std::memcpy(reinterpret_cast<void*>(at), &next, sizeof(next));
        return true;
    } catch (...) { return false; }
}
bool CategoryRoots::select_parent(int level, int style) noexcept {
    if (busy_ || (thread_ && thread_ != GetCurrentThreadId())) return false;
    busy_ = true;
    bool result{};
    __try {
        __try { result = select_parent_checked(level, style); }
        // A cursor store does not transfer or release allocation ownership.
        __except (EXCEPTION_EXECUTE_HANDLER) { result = false; }
    } __finally { busy_ = false; }
    return result;
}
bool CategoryRoots::enter_checked(int wanted, int style) noexcept {
    auto& b = *backing_;
    if (wanted < 1 || wanted > 12 || style < 0 || style > 1 ||
        b.state != CategoryRootsState::published || b.page) return false;
    try {
        Visible now, next;
        std::array<Allocation, 2> actual;
        if (!b.current() || !b.selected(style) || !b.observe(now, actual)) return false;
        const int parent = b.id(wanted, 0);
        Entry selected;
        if (!size(now.vectors[style]) ||
            !b.entry(now.vectors[style].begin + 8ULL * now.cursors[style], selected) ||
            selected != b.binding(wanted, 0, style)) return false;

        for (int s = 0; s < 2; ++s) {
            b.merged[s] = b.values[s];
            std::vector<Entry> children;
            children.reserve(11);
            for (int part = 1; part < 12; ++part) {
                const auto child = b.binding(wanted, part, s);
                if (!child.pointer) continue;
                if (!b.populated(child)) return false;
                children.push_back(child);
            }
            if (bool(b.binding(wanted, 0, s).pointer) != !children.empty() ||
                (!children.empty() && children.front().id != parent + 1) ||
                !b.project(b.values[s], children, b.child_rows[s], s)) return false;
            const auto old = b.values[s].empty() ? Entry{} : b.values[s][now.cursors[s]];
            next.cursors[s] = children.empty() ? select(b.child_rows[s], old, now.cursors[s])
                : select(b.child_rows[s], b.binding(wanted, b.child_parts[s][wanted - 1], s),
                         select(b.child_rows[s], children.front()));
        }
        b.state = CategoryRootsState::uncertain;
        b.projected = actual;
        for (int s = 0; s < 2; ++s) {
            if (!b.build(b.children[s], b.child_rows[s])) return false;
            next.vectors[s] = b.children[s].vector;
        }
        const std::array all{b.native[0], b.native[1], actual[0], actual[1], b.children[0], b.children[1]};
        if (!b.separate(all)) return false;
        b.root_page = now;
        for (int s = 0; s < 2; ++s)
            b.root_page.cursors[s] = select(b.merged[s], b.binding(wanted, 0, s), now.cursors[s]);
        b.page = wanted;
        if (!b.write(next)) return false;
        b.state = CategoryRootsState::published; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::enter(int level, int style) noexcept {
    if (busy_ || (thread_ && thread_ != GetCurrentThreadId())) return false;
    busy_ = true;
    bool result{};
    __try {
        __try { result = enter_checked(level, style); }
        __except (EXCEPTION_EXECUTE_HANDLER) { backing_->state = CategoryRootsState::uncertain; }
    } __finally { busy_ = false; }
    return result;
}
bool CategoryRoots::leave_checked() noexcept {
    auto& b = *backing_;
    if (b.state != CategoryRootsState::published) return false;
    if (!b.page) return true;
    try {
        Visible now, next;
        std::array<Allocation, 2> actual;
        std::array<bool, 2> changed{};
        if (!b.current() || !b.observe_page(now, actual)) return false;
        for (int s = 0; s < 2; ++s) {
            std::vector<Entry> parents;
            for (const auto value : b.merged[s]) if (b.owns(value, s)) parents.push_back(value);
            if (!b.project(b.child_rows[s], parents, b.values[s], s)) return false;
            auto selected = b.child_rows[s].empty() ? Entry{} : b.child_rows[s][now.cursors[s]];
            if (b.owns(selected, s)) {
                const int local = b.local(selected.id, s);
                if (12 - local / 12 == b.page && local % 12)
                    b.child_parts[s][b.page - 1] = local % 12;
                selected = b.binding(b.page, 0, s);
            }
            next.cursors[s] = select(b.values[s], selected, b.root_page.cursors[s]);
            changed[s] = b.values[s] != b.merged[s];
        }
        b.state = CategoryRootsState::uncertain;
        b.children = actual;
        for (int s = 0; s < 2; ++s) {
            if (changed[s] && !b.build(b.replacement[s], b.values[s])) return false;
            next.vectors[s] = changed[s] ? b.replacement[s].vector : b.projected[s].vector;
        }
        const std::array all{b.native[0], b.native[1], b.projected[0], b.projected[1],
                             actual[0], actual[1], b.replacement[0], b.replacement[1]};
        if (!b.separate(all) || !b.write(next)) return false;
        b.page = 0;
        for (int s = 0; s < 2; ++s) if (changed[s]) {
            std::swap(b.projected[s], b.replacement[s]);
            if (!b.release(b.replacement[s])) return false;
        }
        for (auto& allocation : b.children) if (!b.release(allocation)) return false;
        b.state = CategoryRootsState::published; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::withdraw_checked() noexcept {
    auto& b = *backing_;
    if (b.state != CategoryRootsState::published) return false;
    if (b.page && !leave_checked()) return false;
    try {
        Visible now, next;
        std::array<Allocation, 2> actual;
        std::array<bool, 2> changed{};
        if (!b.current() || !b.observe(now, actual)) return false;
        // Stage C++ scratch before native allocation or visible writes. A
        // reserve failure keeps published ownership and navigation retryable.
        for (int s = 0; s < 2; ++s) {
            auto& rows = b.merged[s];
            rows.clear();
            rows.reserve(b.values[s].size());
            Entry selected;
            int fallback{};
            for (std::size_t n = 0; n < b.values[s].size(); ++n) {
                const auto value = b.original(b.values[s][n], s);
                if (static_cast<int>(n) == now.cursors[s]) {
                    selected = value;
                    fallback = static_cast<int>(rows.size());
                }
                if (value.pointer) rows.push_back(value);
            }
            changed[s] = rows != b.originals[s];
            next.cursors[s] = select(rows, selected, rows.empty() ? b.before.cursors[s] : fallback);
        }
        b.state = CategoryRootsState::uncertain;
        b.projected = actual;
        for (int s = 0; s < 2; ++s) {
            const auto& merged = b.merged[s];
            if (changed[s]) {
                if (!b.build(b.replacement[s], merged)) return false;
                next.vectors[s] = b.replacement[s].vector;
            } else next.vectors[s] = b.native[s].vector;
        }
        const std::array all{b.native[0], b.native[1], actual[0], actual[1], b.replacement[0], b.replacement[1]};
        if (!b.separate(all) || !b.write(next)) return false;
        // Native owns next now. Free only the replaced hidden originals and
        // detached custom allocations, never a saved pre-growth address.
        for (int s = 0; s < 2; ++s) {
            if (changed[s]) {
                b.replacement[s] = {};
                if (!b.release(b.native[s])) return false;
            } else b.native[s] = {};
            if (!b.release(b.projected[s])) return false;
        }
        b.state = CategoryRootsState::withdrawn; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::handoff_checked() noexcept {
    auto& b = *backing_;
    if (b.state != CategoryRootsState::published) return false;
    b.state = CategoryRootsState::uncertain;
    try {
        // The old list may already have been replaced in the manager, and the
        // base destructor may have a different vtable. The caller proves this
        // exact tracked group's actual destruction entry, not current selection.
        Visible now;
        std::array<Allocation, 2> actual;
        if (b.page ? !b.observe_page(now, actual) : !b.observe(now, actual)) return false;
        for (int s = 0; s < 2; ++s) if (!b.release(b.native[s])) return false;
        if (b.page) {
            for (int s = 0; s < 2; ++s) if (!b.release(b.projected[s])) return false;
            b.children = {}; // Current child vectors stay in the native group.
            b.page = 0;
        } else b.projected = {}; // Current roots stay in the native group.
        b.state = CategoryRootsState::handed_off; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::discard_checked() noexcept {
    auto& b = *backing_;
    if (b.state != CategoryRootsState::empty && b.state != CategoryRootsState::prepared && b.state != CategoryRootsState::rejected)
        return false;
    b.state = CategoryRootsState::uncertain;
    try {
        for (const auto& a : b.projected) if (!b.intact(a)) return false;
        for (auto& a : b.projected) if (!b.release(a)) return false;
        b.state = CategoryRootsState::withdrawn; return true;
    } catch (...) { return false; }
}
bool CategoryRoots::invoke(bool (CategoryRoots::*operation)() noexcept) noexcept {
    if (busy_ || (thread_ && thread_ != GetCurrentThreadId())) return false;
    busy_ = true;
    bool result{};
    __try {
        __try { result = (this->*operation)(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { backing_->state = CategoryRootsState::uncertain; }
    } __finally { busy_ = false; }
    return result;
}
bool CategoryRoots::publish() noexcept { return invoke(&CategoryRoots::publish_checked); }
bool CategoryRoots::leave() noexcept { return invoke(&CategoryRoots::leave_checked); }
bool CategoryRoots::withdraw() noexcept { return invoke(&CategoryRoots::withdraw_checked); }
bool CategoryRoots::handoff() noexcept { return invoke(&CategoryRoots::handoff_checked); }
bool CategoryRoots::discard() noexcept { return invoke(&CategoryRoots::discard_checked); }
CategoryRootsState CategoryRoots::state() const noexcept { return backing_->state; }
int CategoryRoots::page_level() const noexcept {
    return backing_->state == CategoryRootsState::published ? backing_->page : 0;
}
bool CategoryRoots::page_available(int style) const noexcept {
    return page_level() && backing_->binding(backing_->page, 1, style).pointer != 0;
}
int CategoryRoots::id(int level, int part) const noexcept {
    return backing_->id(level, part);
}
int CategoryRoots::level(int id, int style) const noexcept {
    const int local = backing_->local(id, style);
    return local >= 0 ? 12 - local / 12 : 0;
}
int CategoryRoots::part(int id, int style) const noexcept {
    const int local = backing_->local(id, style);
    return local >= 0 ? local % 12 : -1;
}
bool CategoryRoots::owns(std::uintptr_t category, int id, int style) const noexcept {
    return backing_->owns({category, id}, style);
}
CategoryRootOwner CategoryRoots::owner() const noexcept {
    if (!thread_ || thread_ != GetCurrentThreadId()) return {};
    const auto& b = *backing_;
    return {b.manager, b.list, b.group};
}
bool CategoryRoots::current() const noexcept {
    return !busy_ && thread_ && thread_ == GetCurrentThreadId() &&
        backing_->state == CategoryRootsState::published && backing_->current();
}
bool CategoryRoots::selection(CategoryRootSelection& out) const noexcept {
    out = {};
    if (busy_ || !thread_ || thread_ != GetCurrentThreadId()) return false;
    const auto& b = *backing_;
    if (b.state != CategoryRootsState::published) return false;
    __try {
        CategoryRootSelection value;
        Visible visible;
        Entry entry;
        if (!b.current() || !read(b.memory, b.manager + 0x29864, value.style) ||
            value.style < 0 || value.style > 1 || !b.selected(value.style) || !b.visible(visible)) return false;
        const auto& vector = visible.vectors[value.style];
        value.cursor = visible.cursors[value.style];
        if (!pointer(vector.begin) || vector.end < vector.begin || (vector.end - vector.begin) % 8 ||
            size(vector) > limit || value.cursor < 0 || static_cast<std::size_t>(value.cursor) >= size(vector) ||
            !b.entry(vector.begin + 8ULL * value.cursor, entry) ||
            !read(b.memory, entry.pointer + 4, value.open) || value.open > 1) return false;
        value.category = entry.pointer; value.id = entry.id;
        out = value; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace point_zero::native
