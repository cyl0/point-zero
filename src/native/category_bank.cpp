#include "category_bank.hpp"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <climits>
#include <stdexcept>

namespace point_zero::native {
namespace {
using namespace category_layout;
constexpr std::uint64_t guard = 0xBACCAB1EBACCAB1EULL;
constexpr std::size_t count = 2 * CategoryBank::per_style;
bool pointer(std::uintptr_t at, std::size_t bytes = 0) noexcept {
    return at >= 0x10000 && !(at & 7) && bytes <= UINTPTR_MAX - at;
}
template<class T> bool read(MemoryView m, std::uintptr_t at, T& out) noexcept {
    __try {
        return read_field(m, at, 0, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool same(const CategorySummaryRequest& a, const CategorySummaryRequest& b) noexcept {
    return a.manager == b.manager && a.style == b.style && a.id == b.id;
}
bool same(const CategorySummaryBinding& a, const CategorySummaryBinding& b) noexcept {
    return a.category == b.category && a.summary == b.summary;
}
int index(int id, int style, int first) noexcept {
    return id >= first && id - first < CategoryBank::per_style &&
        style >= 0 && style < 2 ? style * CategoryBank::per_style + id - first : -1;
}
} // namespace

struct CategoryBank::Backing {
    static thread_local Backing* constructing;
    struct Summary {
        std::array<std::uint64_t, 2> before{guard, guard};
        std::array<std::byte, 168> bytes{};
        std::array<std::uint64_t, 2> after{guard, guard};
        bool intact() const noexcept { return before == std::array{guard, guard} && after == before; }
        std::uintptr_t address() const noexcept { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
    };
    struct Slot {
        CategoryStorage storage;
        Summary summary;
        Vector charts, bars;
        Slot(MemoryView m, CategoryNativeCalls calls) : storage(m, calls) {}
    };
    MemoryView memory;
    CategoryNativeCalls storage_calls;
    CategoryBankCalls calls;
    CategoryBadgeCalls badges;
    std::array<CategoryDefinition, per_style> definitions{};
    std::array<std::unique_ptr<Slot>, count> slots;
    std::array<CategoryAccessBinding, count> bindings{};
    std::array<std::array<int, 2>, 24> preferences{};
    std::array<std::size_t, count> member_counts{};
    std::uintptr_t manager{}, list{};
    int first{};
    CategoryBankState state{CategoryBankState::empty};
    bool native_started{};
    int calculating{-1}, active{-1};
    std::size_t receipts{};
    CategorySummaryRequest request{};
    CategorySummaryBinding binding{};

    Backing(MemoryView m, CategoryNativeCalls storage, CategoryBankCalls c, CategoryBadgeCalls b, int first)
        : memory(m), storage_calls(storage), calls(c), badges(b), first(first) {
        for (std::size_t n = 0; n < count; ++n) {
            bindings[n] = {first + static_cast<int>(n % per_style), static_cast<int>(n / per_style), 0, 0};
        }
    }
    const std::array<int, 2>& preference(std::size_t n) const noexcept {
        return preferences[12 * (n / per_style) + (n % per_style) / 12];
    }
    bool read_preferences() noexcept {
        for (unsigned style = 0; style < 2; ++style) for (int parent = 0; parent < 12; ++parent) {
            const auto at = manager + 16 + sizeof(Category) * (226ULL * style + 24 - parent);
            std::array<int, 2> value{};
            if (!read(memory, at + offsetof(Category, preferences), value) || value[0] < 0 || value[0] > 4 || value[1] < 0 || value[1] > 4)
                return false;
            preferences[12 * style + parent] = value;
        }
        return true;
    }
    bool identity(std::size_t n, Category& out) const noexcept {
        if (!slots[n]) return false;
        const auto& slot = *slots[n];
        const auto& b = bindings[n];
        if (!b.category || b.summary != slot.summary.address() || !slot.summary.intact() ||
            !slot.storage.complete() || b.category != slot.storage.address()) return false;
        out = *reinterpret_cast<const Category*>(b.category); // This slot owns the exact backing.
        return out.id == b.id && out.charts == slot.charts && out.bars == slot.bars &&
            out.insert_count == static_cast<int>(member_counts[n]) && out.chart_count >= 0 &&
            out.bar_count >= 0;
    }

};

thread_local CategoryBank::Backing* CategoryBank::Backing::constructing{};

CategoryBank::CategoryBank(MemoryView memory, CategoryNativeCalls storage, CategoryBankCalls calls,
                          CategoryBadgeCalls badges, std::span<const CategoryMetadataEntry> definitions) {
    if (!memory.read || !memory.query || !storage.construct || !storage.initialize || !storage.assign_string ||
        !storage.insert || !storage.destroy || storage.auxiliary_vtable < 0x10000 ||
        !calls.calculate || !calls.set_difficulty || !calls.sort ||
        !badges.music_owner || !badges.event_owner || !badges.chart_at || !badges.music_of ||
        !badges.weekly || !badges.recommend || !badges.shop || !badges.kac || definitions.size() != per_style)
        throw std::invalid_argument("incomplete native category bank dependencies");
    for (std::size_t n = 0; n < definitions.size(); ++n)
        if (definitions.front().id < 230 || definitions.front().id > INT_MAX - per_style - 230 ||
            definitions[n].id != definitions.front().id + static_cast<int>(n) || definitions[n].definition.difficulty_locked != 1)
            throw std::invalid_argument("invalid native category bank definitions");
    backing_ = std::make_unique<Backing>(memory, storage, calls, badges, definitions.front().id);
    for (std::size_t n = 0; n < definitions.size(); ++n) backing_->definitions[n] = definitions[n].definition;
}
CategoryBank::~CategoryBank() noexcept {
    // Summaries must survive along with category allocations. Retaining just
    // CategoryStorage would leave native badge/lamp consumers dangling.
    if (std::any_of(backing_->slots.begin(), backing_->slots.end(), [](const auto& slot) {
            return slot && (slot->storage.state() == CategoryStorageState::live ||
                            slot->storage.state() == CategoryStorageState::uncertain);
        }))
        (void)backing_.release();
}
bool CategoryBank::build_native(std::uintptr_t manager, std::uintptr_t list, LevelPopulation& population, Lookup lookup) {
    auto& b = *backing_;
    b.state = CategoryBankState::rejected;
    if (!pointer(manager, 0x29868) || !pointer(list) || !lookup) return false;
    int first = first_id;
    for (int n = 0; n < per_style;) {
        if (first > INT_MAX - per_style - 230) return false;
        const int id = first + n;
        bool occupied{};
        for (int style = 0; style < 2; ++style) {
            const auto category = lookup(manager, id, style);
            int actual{};
            if (category && !read(b.memory, category, actual)) return false;
            occupied = occupied || (category && actual == id);
        }
        if (occupied) { first = id + 1; n = 0; }
        else ++n;
    }
    b.first = first;
    for (std::size_t n = 0; n < b.bindings.size(); ++n)
        b.bindings[n].id = first + static_cast<int>(n % per_style);
    b.manager = manager; b.list = list;
    if (!population.capture(manager, list)) return false;
    const auto charts = population.charts();
    const auto assigned = std::count_if(charts.begin(), charts.end(), [](const LevelChart& c) { return c.assigned; });
    LevelMembership membership;
    membership.prepare(charts.size() + static_cast<std::size_t>(assigned));
    if (!membership.build(charts) || !b.read_preferences()) return false;
    for (std::size_t n = 0; n < count; ++n) {
        const auto local = static_cast<int>(n % per_style);
        const int level = 12 - local / 12, part = local % 12;
        const int style = static_cast<int>(n / per_style);
        const auto members = part < 2 ? membership.all(style, level) : membership.decimal(style, level, part - 2);
        b.member_counts[n] = members.size();
        if (members.empty()) continue;
        char label[16]{};
        if (part == 0) std::snprintf(label, sizeof(label), "LEVEL %d", level);
        else if (part == 1) std::snprintf(label, sizeof(label), "LEVEL %d ALL", level);
        else std::snprintf(label, sizeof(label), "%d.%d", level, part - 2);
        b.slots[n] = std::make_unique<Backing::Slot>(b.memory, b.storage_calls);
        auto& slot = *b.slots[n];
        auto& binding = b.bindings[n];
        b.native_started = true;
        if (!slot.storage.build(manager, binding.id, binding.style, b.definitions[local], label, members)) {
            if (slot.storage.state() == CategoryStorageState::uncertain) b.state = CategoryBankState::uncertain;
            return false;
        }
        Category c = *reinterpret_cast<const Category*>(slot.storage.address());
        slot.charts = c.charts; slot.bars = c.bars;
        binding.category = slot.storage.address(); binding.summary = slot.summary.address();
        b.calculating = static_cast<int>(n);
        const auto before = b.receipts;
        b.calls.calculate(manager, binding.style, binding.id);
        b.calculating = -1;
        if (b.state == CategoryBankState::uncertain || b.active != -1 || b.receipts != before + 1) {
            b.state = CategoryBankState::uncertain; return false;
        }
        for (int player = 0; player < 2; ++player)
            b.calls.set_difficulty(binding.category, b.preference(n)[player], player);
        // Parent folders expose headers; only ALL/decimal leaves need TITLE order.
        if (part != 0) b.calls.sort(binding.category, 1, 1); // Native TITLE, visible bars only.
        // Local descriptor checks cover these mutations. The bank-wide row,
        // permutation and allocation audit runs once after all native work,
        // including badges which can call providers and affect earlier slots.
        if (!slot.storage.descriptors() || !b.identity(n, c) || c.chart_count != c.insert_count) {
            b.state = CategoryBankState::uncertain; return false;
        }
    }
    // Event providers are optional decoration. A provider failure must not
    // discard otherwise usable categories or block future browsing.
    (void)refresh_category_badges(b.memory, b.badges, manager, b.bindings);
    b.state = CategoryBankState::ready;
    return true;
}
bool CategoryBank::build_checked(std::uintptr_t manager, std::uintptr_t list, LevelPopulation& population, Lookup lookup) noexcept {
    try { return build_native(manager, list, population, lookup); }
    catch (...) { backing_->state = backing_->native_started ? CategoryBankState::uncertain : CategoryBankState::rejected; return false; }
}
bool CategoryBank::build(std::uintptr_t manager, std::uintptr_t list, LevelPopulation& population, Lookup lookup) noexcept {
    if (attempted_ || busy_ || backing_->state != CategoryBankState::empty) return false;
    attempted_ = true; busy_ = true; thread_ = GetCurrentThreadId();
    auto* const previous = Backing::constructing;
    Backing::constructing = backing_.get();
    bool result{};
    __try {
        __try { result = build_checked(manager, list, population, lookup); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            backing_->state = backing_->native_started ? CategoryBankState::uncertain : CategoryBankState::rejected;
        }
    } __finally { Backing::constructing = previous; backing_->calculating = -1; backing_->active = -1; busy_ = false; }
    return result;
}
bool CategoryBank::reset_rejected() noexcept {
    if (busy_ || thread_ != GetCurrentThreadId() || backing_->native_started ||
        backing_->state != CategoryBankState::rejected) return false;
    try {
        // Replace private construction state as a unit, rather than changing
        // only the state enum or weakening the one-attempt build boundary.
        auto replacement = std::make_unique<Backing>(backing_->memory, backing_->storage_calls,
                                                     backing_->calls, backing_->badges, backing_->first);
        replacement->definitions = backing_->definitions;
        backing_ = std::move(replacement);
        attempted_ = false;
        return true;
    } catch (...) { return false; }
}
bool CategoryBank::destroy_checked() noexcept {
    auto& b = *backing_;
    try {
        for (const auto& slot : b.slots)
            if (slot && (!slot->summary.intact() || slot->storage.state() == CategoryStorageState::uncertain ||
                (slot->storage.state() == CategoryStorageState::live && !slot->storage.intact()))) {
                b.state = CategoryBankState::uncertain; return false;
            }
        b.state = CategoryBankState::uncertain;
        for (auto it = b.slots.rbegin(); it != b.slots.rend(); ++it)
            if (*it && !(*it)->storage.destroy()) return false;
        for (auto& binding : b.bindings) binding.category = binding.summary = 0;
        b.state = CategoryBankState::destroyed;
        return true;
    } catch (...) { b.state = CategoryBankState::uncertain; return false; }
}
bool CategoryBank::destroy() noexcept {
    auto& b = *backing_;
    if (busy_ || b.active != -1 || (thread_ && thread_ != GetCurrentThreadId())) return false;
    if (b.state == CategoryBankState::destroyed) return true;
    if (b.state == CategoryBankState::uncertain) return false;
    busy_ = true;
    bool result{};
    __try {
        __try { result = destroy_checked(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b.state = CategoryBankState::uncertain; }
    } __finally { busy_ = false; }
    return result;
}
CategoryBankState CategoryBank::state() const noexcept { return backing_->state; }
std::span<const CategoryAccessBinding> CategoryBank::bindings() const noexcept {
    return !busy_ && backing_->state == CategoryBankState::ready && thread_ == GetCurrentThreadId()
        ? std::span<const CategoryAccessBinding>(backing_->bindings) : std::span<const CategoryAccessBinding>{};
}
const CategoryDefinition* CategoryBank::active_definition(std::uintptr_t manager, int id) noexcept {
    const auto* b = Backing::constructing;
    if (!b || b->manager != manager || id < b->first || id - b->first >= per_style) return nullptr;
    const auto local = static_cast<std::size_t>(id - b->first);
    return b->bindings[local].category || b->bindings[per_style + local].category ? &b->definitions[local] : nullptr;
}
int CategoryBank::base_id() const noexcept { return backing_->first; }
bool CategoryBank::owned(int id, int style) const noexcept {
    const auto& b = *backing_;
    if (id < b.first || id - b.first >= per_style) return false;
    const auto local = static_cast<std::size_t>(id - b.first);
    if (style == -1) return b.bindings[local].category || b.bindings[per_style + local].category;
    return style >= 0 && style < 2 && b.bindings[style * per_style + local].category;
}
int CategoryBank::level(int id) const noexcept { return owned(id) ? 12 - (id - base_id()) / 12 : 0; }
int CategoryBank::part(int id) const noexcept { return owned(id) ? (id - base_id()) % 12 : -1; }
int CategoryBank::id(int level, int part) const noexcept {
    return level >= 1 && level <= 12 && part >= 0 && part < 12
        ? base_id() + (12 - level) * 12 + part : -1;
}
bool CategoryBank::owns(std::uintptr_t category, int id, int style, std::uintptr_t list) const noexcept {
    const auto& b = *backing_;
    const auto n = index(id, style, b.first);
    Category c;
    return !busy_ && b.state == CategoryBankState::ready && thread_ == GetCurrentThreadId() &&
        list == b.list && n >= 0 && b.bindings[n].category == category && b.identity(n, c);
}
bool CategoryBank::prepare_summary(const CategorySummaryRequest& request, CategorySummaryBinding& binding) noexcept {
    auto& b = *backing_;
    const auto n = index(request.id, request.style, b.first);
    Category c;
    if (thread_ != GetCurrentThreadId() || n < 0 || request.manager != b.manager || b.active != -1 ||
        (b.state != CategoryBankState::ready && !(busy_ && b.state == CategoryBankState::rejected && b.calculating == n)) ||
        !b.identity(n, c)) return false;
    b.request = request;
    b.binding = {b.bindings[n].category, b.bindings[n].summary};
    b.active = n; binding = b.binding;
    return true;
}
void CategoryBank::finish_summary(const CategorySummaryRequest& request, const CategorySummaryBinding& binding,
                                  bool completed) noexcept {
    auto& b = *backing_;
    if (thread_ != GetCurrentThreadId() || b.active == -1) return;
    Category c;
    if (!completed || !same(request, b.request) || !same(binding, b.binding) || !b.identity(b.active, c))
        b.state = CategoryBankState::uncertain;
    else ++b.receipts;
    b.active = -1;
}
} // namespace point_zero::native
