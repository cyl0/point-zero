#include "category_accessors.hpp"
#include "inline_patch.hpp"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr auto count = static_cast<unsigned>(CategoryAccessor::count);
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
bool buffer(std::uintptr_t at, std::size_t size) noexcept {
    return at >= 0x10000 && !(at & 7) && size <= UINTPTR_MAX - at;
}
bool valid(const CategoryAccessBinding& b) noexcept {
    if (!b.category && !b.summary) return true;
    return buffer(b.category, 208) && buffer(b.summary, 168) &&
        (b.category + 208 <= b.summary || b.summary + 168 <= b.category);
}
// These reads use no caller-owned MemoryView: the retained accessor may outlive
// its wrapper. Native bank ownership and serialized publication are preconditions.
template<class T> bool load(std::uintptr_t at, T& value) noexcept {
    __try { value = *reinterpret_cast<const T*>(at); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int native_style(std::uintptr_t function, std::uintptr_t manager) noexcept {
    __try { return reinterpret_cast<int (__fastcall*)(std::uintptr_t)>(function)(manager); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
} // namespace

struct CategoryAccessors::State {
    using Lookup = std::uintptr_t (__fastcall*)(std::uintptr_t, int, int);
    using Active = std::uintptr_t (__fastcall*)(std::uintptr_t);
    using Lamp = int (__fastcall*)(std::uintptr_t, int, int, int);
    using Badge = int (__fastcall*)(std::uintptr_t, int);
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    const std::uintptr_t style;
    const std::vector<int> ids;
    const bool contiguous;
    std::vector<CategoryAccessBinding> bindings;
    std::array<std::unique_ptr<detail::InlinePatch>, count> patches;
    std::atomic<Phase> phase{Phase::disabled};
    DWORD thread{};
    std::uintptr_t manager{};
    bool retained{};

    State(MemoryView m, const CategoryAccessorSites& sites, std::span<const int> selected)
        : style(sites.style.address), ids(selected.begin(), selected.end()),
          contiguous(static_cast<std::size_t>(ids.back() - ids.front() + 1) == ids.size()), bindings(2 * ids.size()) {
        const std::array destinations{
            reinterpret_cast<std::uintptr_t>(&lookup), reinterpret_cast<std::uintptr_t>(&active),
            reinterpret_cast<std::uintptr_t>(&clear), reinterpret_cast<std::uintptr_t>(&rival),
            reinterpret_cast<std::uintptr_t>(&badge<CategoryAccessor::kac, 3>),
            reinterpret_cast<std::uintptr_t>(&badge<CategoryAccessor::recommend, 1>),
            reinterpret_cast<std::uintptr_t>(&badge<CategoryAccessor::shop, 2>),
            reinterpret_cast<std::uintptr_t>(&badge<CategoryAccessor::weekly, 0>)};
        for (unsigned i = 0; i < count; ++i)
            patches[i] = std::make_unique<detail::InlinePatch>(m, sites.entries[i].address,
                sites.entries[i].body, destinations[i]);
        require(before_intact(), "category accessors changed during construction");
    }
    template<class F> F original(CategoryAccessor which) const noexcept {
        return reinterpret_cast<F>(patches[static_cast<unsigned>(which)]->original());
    }
    std::size_t index(int id) const noexcept {
        // Stock IDs and unrelated custom folders forward without a search.
        if (id < ids.front() || id > ids.back()) return ids.size();
        if (contiguous) return static_cast<std::size_t>(id - ids.front());
        const auto at = std::lower_bound(ids.begin(), ids.end(), id);
        return at != ids.end() && *at == id ? static_cast<std::size_t>(at - ids.begin()) : ids.size();
    }
    bool available(std::uintptr_t owner) const noexcept {
        // The thread check precedes the mutable bank fields. Foreign callers
        // never participate in native-thread publication or read its table.
        return phase.load(std::memory_order_acquire) == Phase::armed && GetCurrentThreadId() == thread &&
            owner && owner == manager;
    }
    // Caller must pass the availability gate before resolving native style.
    const CategoryAccessBinding* resolve(std::uintptr_t owner, std::size_t i, int selected) const noexcept {
        if (selected == 3) selected = native_style(style, owner);
        if (selected < 0 || selected > 1) return nullptr;
        return &bindings[static_cast<std::size_t>(selected) * ids.size() + i];
    }
    const CategoryAccessBinding* find(std::uintptr_t owner, std::size_t i, int selected) const noexcept {
        return available(owner) ? resolve(owner, i, selected) : nullptr;
    }
    static std::uintptr_t __fastcall lookup(std::uintptr_t owner, int id, int selected) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto i = self.index(id);
        if (i == self.ids.size()) return self.original<Lookup>(CategoryAccessor::lookup)(owner, id, selected);
        const auto* b = self.find(owner, i, selected);
        return b ? b->category : 0;
    }
    static std::uintptr_t __fastcall active(std::uintptr_t owner) {
        auto& self = *live.load(std::memory_order_acquire);
        int id{};
        if (buffer(owner, 0x2A428) && load(owner + 0x2A420, id)) {
            const auto i = self.index(id);
            if (i != self.ids.size()) {
                const auto* b = self.find(owner, i, 3);
                return b ? b->category : 0;
            }
        }
        return self.original<Active>(CategoryAccessor::active)(owner);
    }
    int lamp(std::uintptr_t owner, std::size_t i, int difficulty, int player, bool is_rival) const noexcept {
        const int players = is_rival ? 6 : 2;
        if (!available(owner) || difficulty < 0 || difficulty > 4 || player < 0 || player >= players) return 0;
        int selector{};
        if (!load(owner + 0x2A424, selector) || selector == 2) return 0;
        const auto* b = resolve(owner, i, 3);
        if (!b || !b->summary) return 0;
        int value{};
        const auto offset = static_cast<unsigned>((is_rival ? 48 : 8) + 4 * (players * difficulty + player));
        return load(b->summary + offset, value) ? value : 0;
    }
    static int __fastcall clear(std::uintptr_t owner, int id, int difficulty, int player) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto i = self.index(id);
        if (i == self.ids.size()) return self.original<Lamp>(CategoryAccessor::clear)(owner, id, difficulty, player);
        return self.lamp(owner, i, difficulty, player, false);
    }
    static int __fastcall rival(std::uintptr_t owner, int id, int difficulty, int player) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto i = self.index(id);
        if (i == self.ids.size()) return self.original<Lamp>(CategoryAccessor::rival)(owner, id, difficulty, player);
        return self.lamp(owner, i, difficulty, player, true);
    }
    template<CategoryAccessor Which, unsigned Offset>
    static int __fastcall badge(std::uintptr_t owner, int id) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto i = self.index(id);
        if (i == self.ids.size()) return self.original<Badge>(Which)(owner, id);
        const auto* b = self.find(owner, i, 3);
        unsigned char value{};
        return b && b->summary && load(b->summary + Offset, value) ? value : 0;
    }
    bool before_intact() const noexcept {
        return std::all_of(patches.begin(), patches.end(), [](const auto& p) { return p->before_intact(); });
    }
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed &&
            std::all_of(patches.begin(), patches.end(), [](const auto& p) { return p->intact(); });
    }
};
std::atomic<bool> CategoryAccessors::State::reserved{};
std::atomic<CategoryAccessors::State*> CategoryAccessors::State::live{};

CategoryAccessors::CategoryAccessors(MemoryView memory, const CategoryAccessorSites& sites, std::span<const int> ids) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category accessor process slot already reserved");
    try {
        require(memory.read && memory.query && !ids.empty() && ids.size() <= 144,
                "category accessor memory or IDs missing/excessive");
        for (std::size_t i = 0; i < ids.size(); ++i)
            require(ids[i] >= 230 && ids[i] < 1000 && (!i || ids[i - 1] < ids[i]), "category accessor ID/order invalid");
        std::array<const CategoryAccessorSite*, count + 1> all{};
        for (unsigned i = 0; i < count; ++i) all[i] = &sites.entries[i];
        all.back() = &sites.style;
        for (std::size_t i = 0; i < all.size(); ++i) {
            const auto& a = *all[i];
            require(a.address >= 0x10000 && !a.body.empty() && a.body.size() <= UINTPTR_MAX - a.address,
                    "category accessor site missing/invalid");
            for (std::size_t j = 0; j < i; ++j) {
                const auto& b = *all[j];
                require(a.address + a.body.size() <= b.address || b.address + b.body.size() <= a.address,
                        "category accessor sites overlap");
            }
        }
        state_ = new State(memory, sites, ids);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategoryAccessors::~CategoryAccessors() noexcept {
    // Native return values, not wrapper callbacks. Neither code nor the copied
    // pointer table may disappear while native consumers can still use them.
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategoryAccessors::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_relaxed) == State::Phase::disabled, "category accessor arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(self.before_intact(), "category accessors changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true;
    State::live.store(&self, std::memory_order_release);
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    try {
        for (auto& patch : self.patches) patch->enable();
        require(std::all_of(self.patches.begin(), self.patches.end(),
            [](const auto& p) { return p->intact(); }), "category accessors changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) { self.phase.store(State::Phase::failed, std::memory_order_release); throw; }
}
bool CategoryAccessors::bind(std::uintptr_t manager, std::span<const CategoryAccessBinding> input) noexcept {
    auto& self = *state_;
    if (self.phase.load(std::memory_order_acquire) != State::Phase::armed || GetCurrentThreadId() != self.thread ||
        !buffer(manager, 0x2A428) || input.size() != self.bindings.size()) return false;
    for (std::size_t i = 0; i < input.size(); ++i)
        if (input[i].id != self.ids[i % self.ids.size()] || input[i].style != static_cast<int>(i / self.ids.size()) ||
            !valid(input[i])) return false;
    std::copy(input.begin(), input.end(), self.bindings.begin());
    self.manager = manager;
    return true;
}
bool CategoryAccessors::intact() const noexcept { return state_->intact(); }
} // namespace point_zero::native
