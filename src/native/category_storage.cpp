#include "category_storage.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
using namespace category_layout;
constexpr std::uint64_t guard = 0xA55AA55AA55AA55AULL;
struct Allocation {
    std::uintptr_t begin{}, end{}, capacity{}, allocation{};
    bool operator==(const Allocation&) const = default;
};
template<class T> bool read(MemoryView memory, std::uintptr_t at, T& out) noexcept {
    return read_field(memory, at, 0, out);
}
bool vector_allocation(MemoryView memory, const Vector& v, std::size_t width,
                       std::size_t maximum, Allocation& out) noexcept {
    out = {v.begin, v.end, v.capacity, v.begin};
    if (!v.begin && !v.end && !v.capacity) return true;
    if (v.begin < 0x10000 || (v.begin & 7) || v.begin > v.end || v.end > v.capacity ||
        v.capacity - v.begin > maximum * width || (v.end - v.begin) % width ||
        (v.capacity - v.begin) % width || v.capacity == v.begin) return false;
    if (v.capacity - v.begin >= 4096) {
        if (!readable_range(memory, v.begin - 8, 8, RangeUse::private_writable) ||
            !read(memory, v.begin - 8, out.allocation) || out.allocation < 0x10000 ||
            out.allocation > v.begin - 8 || v.begin - out.allocation - 8 > 31) return false;
    }
    // The prefix is checked before dereference; this fresh whole-allocation
    // sweep also covers the data range, including every interior region.
    return readable_range(memory, out.allocation, v.capacity - out.allocation,
                          RangeUse::private_writable);
}
bool intersects(std::uintptr_t a, std::size_t size, std::uintptr_t b, std::size_t count) noexcept {
    return size && count && a < b + count && b < a + size;
}
} // namespace

struct CategoryStorage::Backing {
    struct alignas(16) Slot {
        std::array<std::uint64_t, 2> before{guard, guard};
        Category category;
        std::array<std::uint64_t, 2> after{guard, guard};
    } slot;
    MemoryView memory;
    CategoryNativeCalls calls;
    CategoryDefinition definition;
    CategoryStorageState state{CategoryStorageState::empty};
    std::uintptr_t manager{};
    int id{-1};
    std::size_t metadata_calls{};
    bool completed{}, recorded{};
    std::array<Allocation, 3> allocations{}; // charts, bars, heap string (if any)
    String label;
    static thread_local Backing* active;

    std::uintptr_t address() const noexcept { return reinterpret_cast<std::uintptr_t>(&slot.category); }
    bool guarded() const noexcept { return slot.before == std::array{guard, guard} && slot.after == slot.before; }
    bool inspect(std::array<Allocation, 3>& result) const noexcept {
        const auto& c = slot.category;
        const auto& text = c.label;
        if (!guarded() || c.auxiliary_vtable != calls.auxiliary_vtable || c.auxiliary != Vector{} ||
            !vector_allocation(memory, c.charts, sizeof(Chart), LevelPopulation::style_limit, result[0]) ||
            !vector_allocation(memory, c.bars, sizeof(Bar), LevelPopulation::style_limit + 3, result[1]) ||
            text.size > text.capacity || text.capacity > 4095) return false;
        result[2] = {};
        if (text.capacity < 16) {
            if (text.capacity != 15 || text.storage[text.size] != '\0') return false;
        } else {
            std::uintptr_t data{};
            std::memcpy(&data, text.storage.data(), sizeof(data));
            if (text.capacity + 1 > UINTPTR_MAX - data ||
                !vector_allocation(memory, {data, data + text.size + 1, data + text.capacity + 1}, 1, 4096, result[2])) return false;
        }
        for (std::size_t i = 0; i < result.size(); ++i) {
            const auto& a = result[i];
            // Include the native aligned-allocation prefix: the destructor
            // frees allocation, not necessarily begin.
            if (intersects(a.allocation, a.capacity - a.allocation,
                           reinterpret_cast<std::uintptr_t>(&slot), sizeof(slot))) return false;
            for (std::size_t j = 0; j < i; ++j)
                if (intersects(a.allocation, a.capacity - a.allocation,
                               result[j].allocation, result[j].capacity - result[j].allocation)) return false;
        }
        return true;
    }
    bool record() noexcept {
        recorded = inspect(allocations);
        if (!recorded) state = CategoryStorageState::uncertain;
        return recorded;
    }
    bool intact() const noexcept {
        std::array<Allocation, 3> current;
        return recorded && inspect(current) && current == allocations;
    }
    bool descriptors() const noexcept {
        const auto& c = slot.category;
        return recorded && guarded() &&
            c.charts == Vector{allocations[0].begin, allocations[0].end, allocations[0].capacity} &&
            c.bars == Vector{allocations[1].begin, allocations[1].end, allocations[1].capacity} &&
            c.auxiliary_vtable == calls.auxiliary_vtable && c.auxiliary == Vector{} && c.label == label;
    }
    bool identified() const noexcept {
        const auto& c = slot.category;
        return c.id == id && c.header_id == id && c.random_id == id;
    }
};
thread_local CategoryStorage::Backing* CategoryStorage::Backing::active{};

CategoryStorage::CategoryStorage(MemoryView memory, CategoryNativeCalls calls) {
    if (!memory.read || !memory.query || !calls.construct || !calls.initialize || !calls.assign_string ||
        !calls.insert || !calls.destroy || calls.auxiliary_vtable < 0x10000)
        throw std::invalid_argument("incomplete native category construction dependencies");
    backing_ = std::make_unique<Backing>();
    backing_->memory = memory; backing_->calls = calls;
}
CategoryStorage::~CategoryStorage() noexcept {
    // Native fields/allocations must remain addressable after uncertain effects
    // or premature caller teardown. Never invoke a game destructor at shutdown.
    if (backing_->state == CategoryStorageState::live || backing_->state == CategoryStorageState::uncertain)
        (void)backing_.release();
}
bool CategoryStorage::build_native(std::uintptr_t manager, int id, int style, CategoryDefinition definition,
                                  std::string_view label, std::span<const LevelChart> members) {
    auto& b = *backing_;
    if (manager < 0x10000 || (manager & 7) || id < 230 || style < 0 || style > 1 ||
        members.size() > LevelPopulation::style_limit || label.empty() || label.size() > 127 ||
        label.find('\0') != label.npos || definition.difficulty_locked != 1) return false;
    for (std::size_t i = 0; i < members.size(); ++i) {
        const auto& m = members[i];
        if (m.music < 0x10000 || (m.music & 7) || m.music > UINTPTR_MAX - 0x680 || m.key.song <= 0 ||
            m.key.style != style || m.key.difficulty < 0 || m.key.difficulty > 4 ||
            m.native_level < 1 || m.native_level > 12 || (i && !(members[i - 1].key < m.key))) return false;
    }
    b.manager = manager; b.id = id;
    b.definition = definition; b.definition.capacity = static_cast<int>(members.size());
    auto& c = b.slot.category;
    // Until return plus postcheck, a native constructor might own partial state.
    b.state = CategoryStorageState::uncertain;
    b.calls.construct(b.address());
    if (!b.record() || c.charts != Vector{} || c.bars != Vector{} ||
        c.label.size != 0 || c.label.capacity != 15) return false;
    b.state = CategoryStorageState::live;
    b.calls.initialize(b.address(), id);
    if (!b.record() || !b.metadata_calls || !b.identified() || c.insert_count != 0 ||
        c.charts.end - c.charts.begin < members.size() * sizeof(Chart) ||
        c.bars.end - c.bars.begin < (members.size() + 3) * sizeof(Bar)) {
        b.state = CategoryStorageState::uncertain; return false;
    }
    b.calls.assign_string(reinterpret_cast<std::uintptr_t>(&c.label), label.data(), label.size());
    if (!b.record()) return false;
    b.label = c.label;
    // Retained 86E1DB..86E1FA compares initialized chart length with
    // insert_count and skips 5FB500 while N > insert_count. The admitted
    // setters write this one existing row; bars/string/auxiliary descriptors
    // do not grow here. Native eligibility/provider callbacks still run, so
    // check descriptors/canaries and the inserted tuple after each call.
    for (std::size_t i = 0; i < members.size(); ++i) {
        const auto& member = members[i];
        auto music = member.music, category = b.address();
        const int difficulty = member.key.difficulty, flag = 0;
        const void* params[]{&music, &category, &difficulty, &flag, &flag};
        if (!b.calls.insert(params, difficulty, style, id)) return false;
    }
    if (!b.intact() || !b.identified() || c.insert_count != static_cast<int>(members.size())) {
        b.state = CategoryStorageState::uncertain; return false;
    }
    c.chart_count = c.insert_count;
    auto* bars = reinterpret_cast<Bar*>(c.bars.begin);
    int count{};
    bars[count++] = {0, 0, reinterpret_cast<std::uintptr_t>(&c.header_id)};
    if (members.size() > 1) bars[count++] = {2, 0, reinterpret_cast<std::uintptr_t>(&c.random_id)};
    for (std::size_t i = 0; i < members.size(); ++i) bars[count++] = {1, 0, c.charts.begin + i * sizeof(Chart)};
    c.bar_count = count;
    b.completed = true;
    return true;
}
bool CategoryStorage::build_checked(std::uintptr_t manager, int id, int style, CategoryDefinition definition,
                                   std::string_view label, std::span<const LevelChart> members) noexcept {
    // C++ exceptions get ordinary exception-object destruction. The outer SEH
    // boundary contains native access faults. Neither path retries a mutator.
    try { return build_native(manager, id, style, definition, label, members); }
    catch (...) {
        if (backing_->state != CategoryStorageState::empty) backing_->state = CategoryStorageState::uncertain;
        return false;
    }
}
bool CategoryStorage::build(std::uintptr_t manager, int id, int style, CategoryDefinition definition,
                           std::string_view label, std::span<const LevelChart> members) noexcept {
    if (busy_ || attempted_ || (thread_ && thread_ != GetCurrentThreadId())) return false;
    attempted_ = true; busy_ = true; thread_ = GetCurrentThreadId();
    auto* const previous = Backing::active;
    Backing::active = backing_.get();
    bool result{};
    __try {
        __try { result = build_checked(manager, id, style, definition, label, members); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            if (backing_->state != CategoryStorageState::empty) backing_->state = CategoryStorageState::uncertain;
        }
    } __finally { Backing::active = previous; busy_ = false; }
    return result;
}
bool CategoryStorage::destroy_native() {
    auto& b = *backing_;
    if (!b.intact()) { b.state = CategoryStorageState::uncertain; return false; }
    b.state = CategoryStorageState::uncertain; b.completed = false;
    b.calls.destroy(b.address());
    b.state = CategoryStorageState::destroyed;
    return true;
}
bool CategoryStorage::destroy_checked() noexcept {
    try { return destroy_native(); }
    catch (...) { backing_->state = CategoryStorageState::uncertain; return false; }
}
bool CategoryStorage::destroy() noexcept {
    if (busy_ || (thread_ && thread_ != GetCurrentThreadId())) return false;
    auto& b = *backing_;
    if (b.state == CategoryStorageState::empty || b.state == CategoryStorageState::destroyed) return true;
    if (b.state != CategoryStorageState::live) return false;
    busy_ = true;
    bool result{};
    __try {
        __try { result = destroy_checked(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { b.state = CategoryStorageState::uncertain; }
    } __finally { busy_ = false; }
    return result;
}
CategoryStorageState CategoryStorage::state() const noexcept { return backing_->state; }
bool CategoryStorage::complete() const noexcept { return backing_->state == CategoryStorageState::live && backing_->completed; }
bool CategoryStorage::intact() const noexcept {
    __try { return backing_->state == CategoryStorageState::live && backing_->intact(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool CategoryStorage::descriptors() const noexcept {
    __try { return backing_->state == CategoryStorageState::live && backing_->descriptors(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::uintptr_t CategoryStorage::address() const noexcept { return backing_->address(); }
const CategoryDefinition* CategoryStorage::definition() const noexcept { return backing_->id >= 230 ? &backing_->definition : nullptr; }
const CategoryDefinition* CategoryStorage::active_definition(std::uintptr_t manager, int id) noexcept {
    auto* b = Backing::active;
    if (!b || b->id != id || b->manager != manager || b->state == CategoryStorageState::empty) return nullptr;
    ++b->metadata_calls;
    return &b->definition;
}
} // namespace point_zero::native
