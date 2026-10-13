#include "build_bindings.hpp"
#include "category_navigation.hpp"
#include "category_chart_match.hpp"
#include "inline_patch.hpp"
#include "played_mid_patch.hpp"

#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
bool site(const CategoryNavigationSite& s) noexcept {
    return s.address >= 0x10000 && !s.body.empty() && s.body.size() <= UINTPTR_MAX - s.address;
}
template<class T> bool read(MemoryView m, std::uintptr_t at, T& out) noexcept {
    __try {
        return at >= 0x10000 && sizeof(T) <= UINTPTR_MAX - at &&
            m.read(m.context, at, std::as_writable_bytes(std::span(&out, 1)));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int parent_level(int id) noexcept {
    const auto offset = id - CategoryBank::first_id;
    return id >= CategoryBank::first_id && offset < CategoryBank::per_style && offset % 12 == 0
        ? 12 - offset / 12 : 0;
}
bool leaf(int id) noexcept {
    return id >= CategoryBank::first_id && id < CategoryBank::first_id + CategoryBank::per_style && !parent_level(id);
}
} // namespace

struct CategoryNavigation::State {
    using Open = std::uintptr_t (__fastcall*)(std::uintptr_t, std::uint8_t);
    using Cycle = int (__fastcall*)(std::uintptr_t);
    using Exact = std::uint8_t (__fastcall*)(std::uintptr_t, int, int, int);
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    MemoryView memory;
    ImageRange game;
    FeatureState& feature;
    CategoryNavigationCalls calls;
    std::uintptr_t cycle_caller;
    std::uintptr_t back_continuation;
    std::array<std::unique_ptr<detail::InlinePatch>, 3> patches;
    std::unique_ptr<detail::PlayedMidPatch> back_patch;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{};
    DWORD thread{};
    bool retained{}, busy{};
    CategoryRoots* roots{};
    std::uintptr_t navigation{};
    CategoryRootOwner owner{};

    State(MemoryView m, ImageRange g, FeatureState& f, const CategoryNavigationSites& sites,
          CategoryNavigationCalls c) : memory(m), game(g), feature(f), calls(c), cycle_caller(sites.cycle_caller),
                                      back_continuation(sites.back_continuation) {
        patches[0] = std::make_unique<detail::InlinePatch>(m, sites.open.address, sites.open.body,
                                                        reinterpret_cast<std::uintptr_t>(&open));
        patches[1] = std::make_unique<detail::InlinePatch>(m, sites.cycle.address, sites.cycle.body,
                                                        reinterpret_cast<std::uintptr_t>(&cycle));
        patches[2] = std::make_unique<detail::InlinePatch>(m, sites.exact.address, sites.exact.body,
                                                        reinterpret_cast<std::uintptr_t>(&exact));
        back_patch = std::make_unique<detail::PlayedMidPatch>(m, sites.back.address, sites.back.body, back);
        require(before_intact(), "disabled category navigation changed during construction");
    }
    bool before_intact() const noexcept {
        return std::all_of(patches.begin(), patches.end(), [](const auto& p) { return p->before_intact(); }) &&
            back_patch->before_intact();
    }
    bool patches_intact() const noexcept {
        return std::all_of(patches.begin(), patches.end(), [](const auto& p) { return p->intact(); }) &&
            back_patch->intact();
    }
    bool intact() const noexcept { return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact(); }
    bool enter() noexcept {
        if (phase.load(std::memory_order_acquire) != Phase::armed || GetCurrentThreadId() != thread) return false;
        auto value = users.load(std::memory_order_relaxed);
        for (;;) {
            if ((value & 1) || value > UINT32_MAX - 2) return false;
            if (users.compare_exchange_weak(value, value + 2, std::memory_order_acquire,
                                           std::memory_order_relaxed)) return true;
        }
    }
    void leave() noexcept { if (users.fetch_sub(2, std::memory_order_release) == 3) users.notify_all(); }
    void retire() noexcept {
        auto value = users.fetch_or(1, std::memory_order_acq_rel) | 1U;
        while (value != 1) { users.wait(value, std::memory_order_acquire); value = users.load(std::memory_order_acquire); }
    }
    bool current(CategoryRootOwner expected) const noexcept {
        std::uintptr_t manager{}, list{}, vt{};
        return expected.manager >= 0x10000 && expected.list >= 0x10000 && expected.group >= 0x10000 &&
            read(memory, game.base + game_bindings(game).category_navigation.category_manager_rva, manager) && manager == expected.manager &&
            read(memory, manager + 0x175D0, list) && list == expected.list &&
            read(memory, list, vt) && vt == game.base + game_bindings(game).category_navigation.list_vtable_rva &&
            read(memory, expected.group, vt) && vt == game.base + game_bindings(game).category_navigation.level_group_vtable_rva;
    }
    bool enabled() const noexcept {
        return roots && feature.view().enabled() && roots->state() == CategoryRootsState::published;
    }
    bool closed(CategoryRootSelection& out) const noexcept {
        std::uint8_t open{};
        return current(owner) && roots->owner() == owner && roots->selection(out) && out.open == 0 &&
            read(memory, navigation + 5, open) && open == 0;
    }

    bool open_custom(std::uintptr_t nav, std::uint8_t sound) noexcept {
        if (busy || nav != navigation || !enabled()) return false;
        if (!current(owner)) return false; // The outer owner will bind the new native list.
        int group{};
        if (!read(memory, owner.list + 0x84, group)) { return true; }
        if (group != 1) return false;
        CategoryRootSelection before;
        std::uint8_t open{};
        if (!roots->selection(before) || !read(memory, nav + 5, open) || open > 1) { return true; }
        if (before.open || open || roots->page_level()) return false;
        const int level = parent_level(before.id);
        if (!level) return false;
        busy = true;
        __try {
            (void)open_checked(level, before.style, sound);
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        busy = false;
        return true; // Never run stock open after a partly completed private entry.
    }
    bool open_checked(int level, int style, std::uint8_t sound) noexcept {
        try {
            if (!roots->enter(level, style)) return false;
            CategoryRootSelection entered;
            if (!closed(entered) || entered.style != style ||
                entered.id <= CategoryBank::id(level, 0) || entered.id > CategoryBank::id(level, 11)) return false;
            if (sound) calls.sound(0x2C80);
            return true;
        } catch (...) { return false; }
    }
    bool back_custom(safetyhook::Context& registers) noexcept {
        if (busy || registers.r15 != navigation || registers.rsi || !enabled() || !roots->page_level()) return false;
        busy = true;
        bool handled{};
        int result{};
        __try { handled = back_checked(registers.r14, result); }
        __except (EXCEPTION_EXECUTE_HANDLER) { handled = true; }
        busy = false;
        if (handled) { registers.rsi = static_cast<unsigned>(result); registers.rip = back_continuation; }
        return handled;
    }
    bool back_checked(std::uintptr_t category, int& result) noexcept {
        try {
            // Native has already accepted a black-key release and rejected a
            // simultaneous white-key confirmation. Its +8 latch distinguishes
            // a short press from held category navigation. Do not poll inputs
            // again here: fresh press and native's held branch are disjoint.
            CategoryRootSelection before, after;
            std::uint8_t short_press{};
            int group{};
            if (!closed(before) || category != before.category ||
                !read(memory, owner.list + 0x84, group) || group != 1 ||
                !read(memory, navigation + 8, short_press) || short_press != 1) return false;
            *reinterpret_cast<std::uint8_t*>(navigation + 8) = 0;
            calls.close(navigation, 1); // Native cursor/filter/motion reset and Back sound.
            if (!closed(after) || after != before || !roots->leave()) { result = 0; return true; }
            result = 4; // The existing dispatcher performs category/graph presentation.
            return true;
        } catch (...) { result = 0; return true; }
    }
    static void back(safetyhook::Context& registers) noexcept {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter()) return;
        __try { (void)self.back_custom(registers); }
        __finally { self.leave(); }
    }
    void after_cycle(std::uintptr_t list, std::uintptr_t caller) noexcept {
        if (busy || list != owner.list || caller != cycle_caller || !enabled() || !roots->page_level()) return;
        busy = true;
        __try {
            int group{};
            if (read(memory, list + 0x84, group) && group != 1 && current(owner)) (void)roots->leave();
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        busy = false;
    }
    bool exact_context(const CategoryRootSelection& selected) const noexcept {
        CategoryRootSelection after;
        std::uint8_t open{};
        return enabled() && current(owner) && roots->owner() == owner &&
            roots->selection(after) && after == selected &&
            read(memory, navigation + 5, open) && open == selected.open;
    }
    bool exact_checked(const CategoryRootSelection& selected, int song, int p1, int p2) {
        using namespace category_layout;
        const auto category = selected.category;
        CategoryChartMatch matched;
        if (!exact_context(selected)) { return false; }
        const auto result = match_category_chart(memory, category, selected.id, selected.style, song, p1, p2, matched);
        if (result == CategoryChartResult::invalid || matched.category.open != selected.open) { return false; }
        if (result == CategoryChartResult::missing) return false; // Native chooses the next candidate/fallback.
        const auto& chosen = matched.bar;
        const int row = matched.row;
        // A VEFX working override is not a new folder member. Match the original
        // tuple, then use the same native reset as Back. Locked5FDBD0 changes
        // header/random preferences without forcing another chart's difficulty.
        calls.restore_chart(chosen.data);
        if (p1 != -1) calls.set_difficulty(category, p1, 0);
        if (p2 != -1) calls.set_difficulty(category, p2, 1);
        std::memcpy(reinterpret_cast<void*>(category + offsetof(Category, cursor)), &row, sizeof(row));
        return true;
    }
    bool exact_caught(const CategoryRootSelection& selected, int song, int p1, int p2) noexcept {
        try { return exact_checked(selected, song, p1, p2); }
        catch (...) { return false; }
    }
    bool native_category(std::uintptr_t category) const noexcept {
        // CategoryRoots' canonical native identity: two inline banks of 230
        // categories at manager+10h, stride D0h. Private bank allocations and
        // Playlister categories are outside this exact native-slot contract.
        constexpr std::size_t count = 230;
        constexpr std::size_t stride = sizeof(category_layout::Category);
        constexpr std::size_t bytes = 2 * count * stride;
        if (owner.manager > UINTPTR_MAX - 16 - bytes) return false;
        const auto first = owner.manager + 16;
        if (!contains(first, bytes, category, stride) || (category - first) % stride) return false;
        int id{};
        return read(memory, category, id) && id == static_cast<int>((category - first) / stride % count);
    }
    bool exact_custom(std::uintptr_t category, int song, int p1, int p2, std::uint8_t& result) noexcept {
        if (!enabled() || !current(owner)) return false;
        // Proven native inline objects cannot be owned private leaves. Forward
        // them even if the current LEVEL heading/selection cannot be read.
        if (native_category(category)) return false;
        int group{};
        CategoryRootSelection selected;
        if (!read(memory, owner.list + 0x84, group)) { return true; }
        if (group != 1) return false;
        if (!roots->selection(selected)) { return true; }
        if (selected.category != category || !leaf(selected.id)) return false;
        result = 0;
        if (busy || song <= 0 || p1 < -1 || p1 > 4 || p2 < -1 || p2 > 4) return true;
        busy = true;
        __try {
            result = exact_caught(selected, song, p1, p2) ? 1 : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        busy = false;
        return true; // Never replay song-only selection after private mutation.
    }
    static std::uintptr_t __fastcall open(std::uintptr_t nav, std::uint8_t sound) {
        auto& self = *live.load(std::memory_order_acquire);
        const bool entered = self.enter();
        __try {
            if (entered && self.open_custom(nav, sound)) return 0;
            const bool previous = entered ? self.busy : false;
            if (entered) self.busy = true;
            __try { return reinterpret_cast<Open>(self.patches[0]->original())(nav, sound); }
            __finally { if (entered) self.busy = previous; }
        } __finally { if (entered) self.leave(); }
    }
    static int __fastcall cycle(std::uintptr_t list) {
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        auto& self = *live.load(std::memory_order_acquire);
        const bool entered = self.enter();
        __try {
            int result{};
            const bool previous = entered ? self.busy : false;
            if (entered) self.busy = true;
            __try { result = reinterpret_cast<Cycle>(self.patches[1]->original())(list); }
            __finally { if (entered) self.busy = previous; }
            if (entered) self.after_cycle(list, caller);
            return result;
        } __finally { if (entered) self.leave(); }
    }
    static std::uint8_t __fastcall exact(std::uintptr_t category, int song, int p1, int p2) {
        auto& self = *live.load(std::memory_order_acquire);
        const bool entered = self.enter();
        __try {
            std::uint8_t result{};
            if (entered && self.exact_custom(category, song, p1, p2, result)) return result;
            return reinterpret_cast<Exact>(self.patches[2]->original())(category, song, p1, p2);
        } __finally { if (entered) self.leave(); }
    }
};
std::atomic<bool> CategoryNavigation::State::reserved{};
std::atomic<CategoryNavigation::State*> CategoryNavigation::State::live{};

CategoryNavigation::CategoryNavigation(MemoryView memory, ImageRange game, FeatureState& feature,
                                     const CategoryNavigationSites& sites, CategoryNavigationCalls calls) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category navigation process slot reserved");
    try {
        require(memory.read && memory.query && calls.close && calls.sound &&
                calls.restore_chart && calls.set_difficulty,
                "category navigation dependencies missing");
        require(game.base >= 0x10000 && contains(game.base, game.size, game.base, game.size) &&
                contains(game.base, game.size, game.base + game_bindings(game).category_navigation.category_manager_rva, 8), "category navigation image invalid");
        const std::array entries{&sites.open, &sites.cycle, &sites.exact, &sites.back};
        require(sites.cycle_caller >= 0x10000 && sites.back_continuation >= 0x10000,
                "category navigation caller/continuation invalid");
        constexpr std::array back_bytes{std::byte{0x41}, std::byte{0x80}, std::byte{0x7F}, std::byte{0x08}, std::byte{0}};
        require(sites.back.body.size() == back_bytes.size() &&
                std::equal(back_bytes.begin(), back_bytes.end(), sites.back.body.begin()),
                "category navigation release instruction changed");
        for (std::size_t i = 0; i < entries.size(); ++i) {
            require(site(*entries[i]), "category navigation site invalid");
            for (std::size_t j = 0; j < i; ++j)
                require(entries[i]->address + entries[i]->body.size() <= entries[j]->address ||
                        entries[j]->address + entries[j]->body.size() <= entries[i]->address,
                        "category navigation sites overlap");
        }
        state_ = new State(memory, game, feature, sites, calls);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategoryNavigation::~CategoryNavigation() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategoryNavigation::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled, "category navigation arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "category navigation already retired");
    require(self.before_intact(), "category navigation changed before arm");
    self.thread = GetCurrentThreadId(); self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    State::live.store(&self, std::memory_order_release);
    try {
        for (auto& patch : self.patches) patch->enable();
        self.back_patch->enable();
        require(self.patches_intact(), "category navigation changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) { self.retire(); self.phase.store(State::Phase::failed, std::memory_order_release); throw; }
}
bool CategoryNavigation::bind(CategoryRoots& roots, std::uintptr_t navigation) noexcept {
    auto& self = *state_;
    if (self.phase.load(std::memory_order_acquire) != State::Phase::armed || self.thread != GetCurrentThreadId() ||
        self.users.load(std::memory_order_acquire) || self.busy || roots.state() != CategoryRootsState::published ||
        navigation < 0x10000 || navigation > UINTPTR_MAX - 8) return false;
    const auto owner = roots.owner();
    std::uint8_t open{};
    int group{};
    if (!roots.current() || !self.current(owner) || !read(self.memory, navigation + 5, open) || open > 1 ||
        !read(self.memory, owner.list + 0x84, group)) return false;
    CategoryRootSelection selected;
    if (group == 1 && (!roots.selection(selected) || selected.open != open)) return false;
    self.owner = owner; self.roots = &roots; self.navigation = navigation;
    return true;
}
bool CategoryNavigation::unbind() noexcept {
    auto& self = *state_;
    if (self.thread == GetCurrentThreadId() && !self.users.load(std::memory_order_acquire) && !self.busy) {
        self.roots = nullptr; self.navigation = 0; self.owner = {};
        return true;
    }
    return false;
}
bool CategoryNavigation::intact() const noexcept { return state_->intact(); }
void CategoryNavigation::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
