#include "build_bindings.hpp"
#include "category_roots.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
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
bool integer(int id) noexcept { return id >= 6 && id <= 17; }
int level(int id, bool custom) noexcept {
    if (!custom) return integer(id) ? id - 5 : 0;
    return CategoryBank::part(id) == 0 ? CategoryBank::level(id) : 0;
}
bool overlap(const Allocation& a, const Allocation& b) noexcept {
    return a.base && b.base && a.base < b.vector.capacity && b.base < a.vector.capacity;
}
bool cursor(const Vector& v, int value) noexcept {
    return size(v) ? value >= 0 && static_cast<std::size_t>(value) < size(v) : value == 0 || value == -1;
}
int select(std::span<const Entry> values, int wanted, bool custom) noexcept {
    if (values.empty()) return 0;
    const int target = level(wanted, custom);
    int best{}, distance = 1000;
    for (std::size_t n = 0; n < values.size(); ++n) {
        if (values[n].id == wanted) return static_cast<int>(n);
        const int candidate = level(values[n].id, custom);
        if (!target || !candidate) continue;
        const int d = candidate <= target ? target - candidate : 100 + candidate - target;
        if (d < distance) { distance = d; best = static_cast<int>(n); }
    }
    return best;
}
} // namespace
bool find_level_group(MemoryView memory, ImageRange game, std::uintptr_t list,
                      std::uintptr_t& group, std::uintptr_t expected) noexcept {
    std::uintptr_t head{}, node{}, vtable{};
    if (!pointer(list, 0x88) || !read(memory, list, vtable) || vtable != game.base + game_bindings(game).category_roots.list_vtable_rva ||
        !read(memory, list + 16, head) || !pointer(head, 8) || !read(memory, head, node)) return false;
    group = 0;
    for (unsigned n = 0; node != head; ++n) {
        std::uintptr_t next{}, value{};
        int id{};
        if (n >= 64 || !pointer(node, 32) || !read(memory, node, next) ||
            !read(memory, node + 16, id) || !read(memory, node + 24, value)) return false;
        if (id == 1) {
            if (group || (expected && value != expected) || !pointer(value, 0xB0) ||
                !read(memory, value, vtable) || vtable != game.base + game_bindings(game).category_roots.level_group_vtable_rva ||
                !read(memory, value + 8, id) || id != 1) return false;
            group = value;
        } else if (expected && value == expected) return false;
        node = next;
    }
    return group != 0;
}

struct CategoryRoots::Backing {
    MemoryView memory;
    ImageRange game;
    CategoryVectorCalls calls;
    Lookup lookup;
    std::uintptr_t manager{}, list{}, group{};
    CategoryRootsState state{CategoryRootsState::empty};
    Visible before{}, root_page{};
    std::array<Allocation, 2> native{}, projected{}, replacement{}, children{};
    // merged is scratch for OFF merging, or the complete retained root rows
    // while a child page is visible. Those operations cannot overlap.
    std::array<std::vector<Entry>, 2> originals, values, merged;
    std::array<std::vector<Entry>, 2> child_rows;
    std::array<std::array<std::uintptr_t, CategoryBank::per_style>, 2> categories{};
    std::array<std::array<int, 12>, 2> child_parts{};
    int page{};

    Backing(MemoryView m, ImageRange g, CategoryVectorCalls c, Lookup l) : memory(m), game(g), calls(c), lookup(l) {
        for (auto& parts : child_parts) parts.fill(1); // ALL initially.
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
    bool agrees(const Vector& v, std::span<const Entry> expected, bool exact = true) const noexcept {
        if (size(v) < expected.size() || (exact && size(v) != expected.size())) return false;
        for (std::size_t n = 0; n < expected.size(); ++n) {
            Entry current;
            if (!entry(v.begin + n * 8, current) || current != expected[n]) return false;
        }
        return true;
    }
    bool populated(Entry value) const noexcept {
        int id{}, count{};
        return read(memory, value.pointer, id) && id == value.id &&
            read(memory, value.pointer + 0x10, count) && count > 0;
    }
    bool canonical(Entry value, int style) const {
        if (value.id < 0 || (value.id >= 230 && value.id < 1000)) return false;
        if (value.id < 230 && value.pointer != manager + 16 + sizeof(category_layout::Category) * (230ULL * style + value.id))
            return false;
        return lookup(manager, value.id, style) == value.pointer;
    }
    bool current() const noexcept {
        std::uintptr_t m{}, l{}, found{};
        return read(memory, game.base + game_bindings(game).category_roots.category_manager_rva, m) && m == manager &&
            read(memory, manager + 0x175D0, l) && l == list &&
            find_level_group(memory, game, list, found, group);
    }
    bool visible(Visible& out) const noexcept { return read(memory, group + 0x78, out); }
    bool selected(int style) const noexcept {
        int manager_style{}, list_style{}, group_id{};
        return read(memory, manager + 0x2A424, manager_style) && manager_style == style &&
            read(memory, list + 0x78, list_style) && list_style == style &&
            read(memory, list + 0x84, group_id) && group_id == 1;
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
        for (std::size_t n = 0; n < bank.size(); ++n) {
            const auto& b = bank[n];
            const int style = static_cast<int>(n / CategoryBank::per_style), local = static_cast<int>(n % CategoryBank::per_style);
            if (b.style != style || b.id != CategoryBank::first_id + local || (!b.category != !b.summary)) return false;
            if (!b.category) continue;
            int actual{}, inserted{};
            if (!pointer(b.category, 208) || !pointer(b.summary, 168) || !read(memory, b.category, actual) || actual != b.id ||
                !read(memory, b.category + 0x10, inserted) || inserted <= 0) return false;
            categories[style][local] = b.category;
        }
        for (int style = 0; style < 2; ++style) {
            if (!inspect(before.vectors[style], native[style]) || !cursor(before.vectors[style], before.cursors[style])) return false;
            auto& source = originals[style];
            source.reserve(size(before.vectors[style]));
            int previous{}; bool block{}, ended{};
            for (std::size_t n = 0; n < size(before.vectors[style]); ++n) {
                Entry value;
                if (!entry(before.vectors[style].begin + n * 8, value) || !canonical(value, style) ||
                    std::find(source.begin(), source.end(), value) != source.end()) return false;
                if (integer(value.id)) {
                    if (ended || value.id <= previous) return false;
                    previous = value.id; block = true;
                } else if (block) ended = true;
                source.push_back(value);
            }
            auto& dest = values[style];
            const auto integers = std::count_if(source.begin(), source.end(), [](Entry e) { return integer(e.id); });
            std::size_t parents{};
            for (int l = 1; l <= 12; ++l) parents += categories[style][CategoryBank::id(l, 0) - CategoryBank::first_id] != 0;
            const auto projected_size = source.size() - static_cast<std::size_t>(integers) + parents;
            if (projected_size > limit) return false;
            dest.reserve(projected_size);
            const auto append_parents = [&] {
                for (int l = 1; l <= 12; ++l) {
                    const int id = CategoryBank::id(l, 0);
                    if (categories[style][id - CategoryBank::first_id]) dest.push_back({categories[style][id - CategoryBank::first_id], id});
                }
            };
            bool added{};
            if (!block) { append_parents(); added = true; }
            for (const auto value : source) {
                if (integer(value.id)) { if (!added) { append_parents(); added = true; } }
                else dest.push_back(value);
            }
            if (dest.size() > limit) return false;
        }
        return separate(native);
    }
    // Observe the actual current allocation first. The previously published
    // address may already have been freed by native growth; never read it.
    bool observe(Visible& out, std::array<Allocation, 2>& actual) {
        if (!visible(out)) return false;
        for (int s = 0; s < 2; ++s) {
            if (!inspect(out.vectors[s], actual[s]) || !cursor(out.vectors[s], out.cursors[s]) ||
                !agrees(out.vectors[s], values[s], false) || !intact(native[s]) || !agrees(native[s].vector, originals[s])) return false;
            for (std::size_t n = values[s].size(); n < size(out.vectors[s]); ++n) {
                Entry added;
                if (!entry(out.vectors[s].begin + n * 8, added) || integer(added.id) || !canonical(added, s)) return false;
                for (std::size_t prior = 0; prior < n; ++prior) {
                    Entry value;
                    if (!entry(out.vectors[s].begin + prior * 8, value) || value.pointer == added.pointer || value.id == added.id) return false;
                }
            }
        }
        const std::array all{native[0], native[1], actual[0], actual[1]};
        return separate(all);
    }
    bool observe_page(Visible& out) const noexcept {
        if (!page || !visible(out)) return false;
        for (int s = 0; s < 2; ++s) {
            if (out.vectors[s] != children[s].vector || !cursor(out.vectors[s], out.cursors[s]) ||
                !intact(children[s]) || !agrees(out.vectors[s], child_rows[s]) ||
                projected[s].vector != root_page.vectors[s] || !intact(projected[s]) ||
                !agrees(root_page.vectors[s], merged[s]) || !intact(native[s]) ||
                !agrees(native[s].vector, originals[s])) return false;
            for (const auto child : child_rows[s]) if (!populated(child)) return false;
        }
        const std::array all{native[0], native[1], projected[0], projected[1], children[0], children[1]};
        return separate(all);
    }
};

CategoryRoots::CategoryRoots(MemoryView memory, ImageRange game, CategoryVectorCalls calls, Lookup lookup) {
    if (!memory.read || !memory.query || !calls.grow || !calls.destroy || !lookup ||
        !pointer(game.base, game.size) || game.size < game_bindings(game).category_roots.category_storage_rva)
        throw std::invalid_argument("incomplete native LEVEL root dependencies");
    backing_ = std::make_unique<Backing>(memory, game, calls, lookup);
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
        if (!pointer(manager, 0x2A428) || !pointer(list, 0x88) || !pointer(group, 0xB0)) return false;
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
            const int old = b.originals[s].empty() ? -1 : b.originals[s][now.cursors[s]].id;
            next.cursors[s] = b.values[s].empty() ? (b.originals[s].empty() ? b.before.cursors[s] : 0)
                : select(b.values[s], integer(old) ? CategoryBank::id(old - 5, 0) : old, true);
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
        const int parent = CategoryBank::id(wanted, 0);
        const Entry target{b.categories[style][parent - CategoryBank::first_id], parent};
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
        const int parent = CategoryBank::id(wanted, 0);
        Entry selected;
        if (!size(now.vectors[style]) ||
            !b.entry(now.vectors[style].begin + 8ULL * now.cursors[style], selected) ||
            selected != Entry{b.categories[style][parent - CategoryBank::first_id], parent}) return false;

        b.state = CategoryRootsState::uncertain;
        for (int s = 0; s < 2; ++s) {
            auto& roots = b.merged[s]; roots.clear();
            roots.reserve(size(now.vectors[s]));
            for (std::size_t n = 0; n < size(now.vectors[s]); ++n) {
                Entry value;
                if (!b.entry(now.vectors[s].begin + n * 8, value)) return false;
                roots.push_back(value);
            }
            auto& rows = b.child_rows[s]; rows.clear();
            rows.reserve(11);
            for (int part = 1; part < 12; ++part) {
                const int id = parent + part;
                const auto at = b.categories[s][id - CategoryBank::first_id];
                if (!at) continue;
                const Entry child{at, id};
                if (!b.populated(child)) return false;
                rows.push_back(child);
            }
            // ALL and the parent share membership but own distinct categories.
            // No empty synthetic category is introduced for a missing style.
            if (bool(b.categories[s][parent - CategoryBank::first_id]) != !rows.empty() ||
                (!rows.empty() && rows.front().id != parent + 1) ||
                !b.build(b.children[s], rows)) return false;
            next.vectors[s] = b.children[s].vector;
            next.cursors[s] = select(rows, parent + b.child_parts[s][wanted - 1], false);
        }
        const std::array all{b.native[0], b.native[1], actual[0], actual[1], b.children[0], b.children[1]};
        if (!b.separate(all)) return false;
        b.root_page = now;
        for (int s = 0; s < 2; ++s)
            if (!b.merged[s].empty()) b.root_page.cursors[s] = select(b.merged[s], parent, true);
        b.projected = actual;
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
        Visible now;
        if (!b.current() || !b.observe_page(now)) return false;
        b.state = CategoryRootsState::uncertain;
        for (int s = 0; s < 2; ++s)
            if (!b.child_rows[s].empty())
                b.child_parts[s][b.page - 1] = b.child_rows[s][now.cursors[s]].id - CategoryBank::id(b.page, 0);
        if (!b.write(b.root_page)) return false;
        b.page = 0;
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
        if (!b.current() || !b.observe(now, actual)) return false;
        // Stage C++ scratch before native allocation or visible writes. A
        // reserve failure keeps published ownership and navigation retryable.
        for (int s = 0; s < 2; ++s) {
            auto& merged = b.merged[s];
            const auto appended = size(now.vectors[s]) - b.values[s].size();
            if (b.originals[s].size() + appended > limit) return false;
            merged.reserve(b.originals[s].size() + appended);
            merged = b.originals[s];
            for (std::size_t n = b.values[s].size(); n < size(now.vectors[s]); ++n) {
                Entry value;
                if (!b.entry(now.vectors[s].begin + n * 8, value)) return false;
                if (std::find(merged.begin(), merged.end(), value) == merged.end()) {
                    if (merged.size() == limit) return false;
                    merged.push_back(value);
                }
            }
        }
        b.state = CategoryRootsState::uncertain;
        for (int s = 0; s < 2; ++s) {
            const auto& merged = b.merged[s];
            if (merged != b.originals[s]) {
                if (!b.build(b.replacement[s], merged)) return false;
                next.vectors[s] = b.replacement[s].vector;
            } else next.vectors[s] = b.native[s].vector;
            Entry selected;
            if (!b.values[s].empty() || size(now.vectors[s])) {
                if (!b.entry(now.vectors[s].begin + 8ULL * now.cursors[s], selected)) return false;
            } else selected.id = -1;
            const int l = level(selected.id, true);
            next.cursors[s] = merged.empty() ? b.before.cursors[s] : select(merged, l ? l + 5 : selected.id, false);
        }
        const std::array all{b.native[0], b.native[1], actual[0], actual[1], b.replacement[0], b.replacement[1]};
        if (!b.separate(all) || !b.write(next)) return false;
        // Native owns next now. Free only the replaced hidden originals and
        // detached custom allocations, never a saved pre-growth address.
        for (int s = 0; s < 2; ++s) {
            if (b.replacement[s].vector != Vector{}) {
                b.replacement[s] = {};
                if (!b.release(b.native[s])) return false;
            } else b.native[s] = {};
            b.projected[s] = actual[s];
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
        if (b.page ? !b.observe_page(now) : !b.observe(now, actual)) return false;
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
    return style >= 0 && style < 2 && page_level() && !backing_->child_rows[style].empty();
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
        if (!b.current() || !read(b.memory, b.manager + 0x2A424, value.style) ||
            value.style < 0 || value.style > 1 || !b.selected(value.style) || !b.visible(visible)) return false;
        const auto& vector = visible.vectors[value.style];
        value.cursor = visible.cursors[value.style];
        if (!pointer(vector.begin) || vector.end < vector.begin || (vector.end - vector.begin) % 8 ||
            size(vector) > limit || value.cursor < 0 || static_cast<std::size_t>(value.cursor) >= size(vector) ||
            !b.entry(vector.begin + 8ULL * value.cursor, entry) || entry.id < 0 ||
            !read(b.memory, entry.pointer + 4, value.open) || value.open > 1) return false;
        if (entry.id >= CategoryBank::first_id && entry.id < CategoryBank::first_id + CategoryBank::per_style &&
            b.categories[value.style][entry.id - CategoryBank::first_id] != entry.pointer) return false;
        value.category = entry.pointer; value.id = entry.id;
        out = value; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace point_zero::native
