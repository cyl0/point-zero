#include "category_lifecycle_hooks.hpp"
#include "inline_patch.hpp"

#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <memory>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
constexpr std::size_t count = 13;
bool valid(const CategoryLifeSite& site) noexcept {
    return site.address >= 0x10000 && !site.body.empty() && site.body.size() <= UINTPTR_MAX - site.address;
}
bool overlap(const CategoryLifeSite& a, const CategoryLifeSite& b) noexcept {
    return a.address < b.address + b.body.size() && b.address < a.address + a.body.size();
}
template<class T> bool read(MemoryView memory, std::uintptr_t at, T& value) noexcept {
    __try {
        return at >= 0x10000 && sizeof(T) <= UINTPTR_MAX - at &&
            memory.read(memory.context, at, std::as_writable_bytes(std::span(&value, 1)));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace

struct CategoryLifecycleHooks::State {
    enum class Phase { disabled, enabling, armed, failed };
    // The game callers use these operations for their effects. Keep even the
    // original unspecified RAX value when forwarding; it is not a success bool.
    using NativeOne = std::uintptr_t (__fastcall*)(std::uintptr_t);
    using NativePair = std::uintptr_t (__fastcall*)(std::uintptr_t, std::uintptr_t);
    using NativeInitialize = std::uintptr_t (__fastcall*)(std::uintptr_t, std::int32_t);
    using NativePreview = std::uintptr_t (__fastcall*)(std::uintptr_t, int);
    using NativeRestore = std::uintptr_t (__fastcall*)(std::uintptr_t, int, int, int, int, std::uint8_t, int);
    using NativeTick = std::uintptr_t (__fastcall*)(std::uintptr_t, std::uint8_t, std::uintptr_t);
    struct RestoreArguments {
        CategoryRestoreTarget target;
        const int song, p1, p2, extra; // Never forward the observer's snapshot fields.
    };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local State* scope;
    MemoryView memory;
    std::array<std::unique_ptr<detail::InlinePatch>, count> patches;
    void* const context;
    const Callbacks callbacks;
    const std::uintptr_t initialize_caller;
    const std::uintptr_t scene_entry_caller, scene_tick_caller, scene_vtable, idle_callback;
    const std::uintptr_t scene_refresh_caller;
    const std::uintptr_t preview_caller;
    const std::uintptr_t restore_caller;
    const std::uintptr_t style_apply_caller, style_restore_caller;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit + outer transactions in increments of two.
    DWORD thread{};
    bool retained{};

    State(MemoryView m, const CategoryLifeSites& sites, void* opaque, Callbacks cb)
        : memory(m), context(opaque), callbacks(cb), initialize_caller(sites.initialize_caller),
          scene_entry_caller(sites.scene_entry_caller), scene_tick_caller(sites.scene_tick_caller),
          scene_vtable(sites.scene_vtable), idle_callback(sites.idle_callback),
          scene_refresh_caller(sites.scene_refresh_caller), preview_caller(sites.preview_caller),
          restore_caller(sites.restore_caller), style_apply_caller(sites.style_apply_caller),
          style_restore_caller(sites.style_restore_caller) {
        const std::array targets{
            reinterpret_cast<std::uintptr_t>(&adopt), reinterpret_cast<std::uintptr_t>(&populate),
            reinterpret_cast<std::uintptr_t>(&list_destroy), reinterpret_cast<std::uintptr_t>(&group_destroy),
            reinterpret_cast<std::uintptr_t>(&initialize), reinterpret_cast<std::uintptr_t>(&graph_bind),
            reinterpret_cast<std::uintptr_t>(&selection_destroy), reinterpret_cast<std::uintptr_t>(&scene_entry),
            reinterpret_cast<std::uintptr_t>(&scene_tick), reinterpret_cast<std::uintptr_t>(&scene_refresh),
            reinterpret_cast<std::uintptr_t>(&preview_assign), reinterpret_cast<std::uintptr_t>(&selection_restore),
            reinterpret_cast<std::uintptr_t>(&style_apply)};
        for (std::size_t n = 0; n < count; ++n)
            patches[n] = std::make_unique<detail::InlinePatch>(m, sites.entries[n].address, sites.entries[n].body, targets[n]);
        require(before_intact(), "disabled category lifecycle hooks changed during construction");
    }
    bool before_intact() const noexcept {
        for (const auto& patch : patches) if (!patch->before_intact()) return false;
        return true;
    }
    bool patches_intact() const noexcept {
        for (const auto& patch : patches) if (!patch->intact()) return false;
        return true;
    }
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact();
    }
    bool enter() noexcept {
        if (phase.load(std::memory_order_acquire) != Phase::armed || GetCurrentThreadId() != thread) return false;
        auto value = users.load(std::memory_order_relaxed);
        for (;;) {
            if ((value & 1) || value > UINT32_MAX - 2) return false;
            if (users.compare_exchange_weak(value, value + 2, std::memory_order_acquire,
                                           std::memory_order_relaxed)) return true;
        }
    }
    void leave() noexcept {
        if (users.fetch_sub(2, std::memory_order_release) == 3) users.notify_all();
    }
    void retire() noexcept {
        auto value = users.fetch_or(1, std::memory_order_acq_rel) | 1U;
        while (value != 1) {
            users.wait(value, std::memory_order_acquire);
            value = users.load(std::memory_order_acquire);
        }
    }
    std::uintptr_t original(CategoryLifeEvent event, std::uintptr_t owner, std::uintptr_t argument,
                            std::uintptr_t payload = 0, const RestoreArguments* restore = nullptr) const {
        const auto entry = patches[static_cast<std::size_t>(event)]->original();
        if (event == CategoryLifeEvent::adopt || event == CategoryLifeEvent::graph_bind)
            return reinterpret_cast<NativePair>(entry)(owner, argument);
        if (event == CategoryLifeEvent::initialize)
            return reinterpret_cast<NativeInitialize>(entry)(owner, static_cast<std::int32_t>(argument));
        if (event == CategoryLifeEvent::preview_assign)
            return reinterpret_cast<NativePreview>(entry)(owner, static_cast<std::int32_t>(argument));
        if (event == CategoryLifeEvent::selection_restore)
            return reinterpret_cast<NativeRestore>(entry)(owner, restore->target.category, restore->song,
                restore->p1, restore->p2, restore->target.force_open, restore->extra);
        if (event == CategoryLifeEvent::scene_tick)
            return reinterpret_cast<NativeTick>(entry)(owner, static_cast<std::uint8_t>(argument), payload);
        return reinterpret_cast<NativeOne>(entry)(owner);
    }
    bool boundary(const CategoryLifeRequest& request, std::uintptr_t caller) const noexcept {
        const bool entry = request.event == CategoryLifeEvent::scene_entry;
        if (!entry && request.event != CategoryLifeEvent::scene_tick) return false;
        if (caller != (entry ? scene_entry_caller : scene_tick_caller) ||
            (!entry && (request.argument || request.payload)) || request.owner > UINTPTR_MAX - 0x98) return false;
        std::uintptr_t vtable{}, controller{}, target{}, callback{};
        std::int32_t adjustment{};
        if (!native::read(memory, request.owner, vtable) || vtable != scene_vtable) return false;
        if (entry) return true;
        // Ordinary idle route only. Other native phases/messages remain
        // observed transactions but cannot authorize collection of old borrows.
        return native::read(memory, request.owner + 0x90, controller) && controller >= 0x10000 &&
            controller <= UINTPTR_MAX - 0x14 && native::read(memory, controller, target) && target == request.owner &&
            native::read(memory, controller + 8, callback) && callback == idle_callback &&
            native::read(memory, controller + 0x10, adjustment) && adjustment == 0;
    }
    static std::uintptr_t dispatch(CategoryLifeEvent event, std::uintptr_t owner, std::uintptr_t argument,
                                   std::uintptr_t payload = 0, std::uintptr_t caller = 0,
                                   RestoreArguments* restore = nullptr) {
        auto& self = *live.load(std::memory_order_acquire);
        auto* const previous = scope;
        const bool nested = previous == &self && GetCurrentThreadId() == self.thread;
        const bool entered = !nested && self.enter();
        scope = nested || entered ? &self : nullptr;
        CategoryLifeRequest request{event, owner, argument, payload};
        auto outcome = CategoryLifeOutcome::unwound;
        __try {
            if (!scope) return self.original(event, owner, argument, payload, restore);
            // Acquire the observer lease BEFORE reading borrowed scene state.
            // Refused scene routes still hold scope around nested native work.
            request.collection_boundary = self.boundary(request, caller);
            if (event == CategoryLifeEvent::scene_refresh && caller == self.scene_refresh_caller) {
                std::uintptr_t vtable{};
                request.refresh_boundary = native::read(self.memory, owner, vtable) && vtable == self.scene_vtable;
            }
            request.preview_boundary = event == CategoryLifeEvent::preview_assign && caller == self.preview_caller;
            request.style_boundary = event == CategoryLifeEvent::style_apply && caller == self.style_apply_caller;
            request.refresh_restore = event == CategoryLifeEvent::selection_restore && caller == self.style_restore_caller;
            if (event == CategoryLifeEvent::selection_restore && (caller == self.restore_caller || request.refresh_restore))
                request.restore = &restore->target;
            if (!self.callbacks.begin(self.context, request) &&
                (event == CategoryLifeEvent::populate || request.refresh_boundary || request.style_boundary ||
                 request.preview_boundary || request.restore)) {
                outcome = CategoryLifeOutcome::cancelled;
                return 0;
            }
            const auto result = self.original(event, owner, argument, payload, restore);
            outcome = event == CategoryLifeEvent::scene_entry && !(result & 0xFF)
                ? CategoryLifeOutcome::incomplete : CategoryLifeOutcome::completed;
            return result;
        } __finally {
            // A parent native call can destroy its children after retirement
            // closes new entries. The parent lease keeps that complete nested
            // lifetime transaction observable until every paired finish returns.
            __try {
                if (nested || entered) self.callbacks.finish(self.context, request, outcome);
            } __finally {
                scope = previous;
                if (entered) self.leave();
            }
        }
    }
    static std::uintptr_t __fastcall adopt(std::uintptr_t owner, std::uintptr_t argument) {
        return dispatch(CategoryLifeEvent::adopt, owner, argument);
    }
    static std::uintptr_t __fastcall populate(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::populate, owner, 0);
    }
    static std::uintptr_t __fastcall list_destroy(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::list_destroy, owner, 0);
    }
    static std::uintptr_t __fastcall group_destroy(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::group_destroy, owner, 0);
    }
    static std::uintptr_t __fastcall graph_bind(std::uintptr_t owner, std::uintptr_t category) {
        return dispatch(CategoryLifeEvent::graph_bind, owner, category);
    }
    static std::uintptr_t __fastcall selection_destroy(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::selection_destroy, owner, 0);
    }
    static std::uintptr_t __fastcall scene_entry(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::scene_entry, owner, 0, 0,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    }
    static std::uintptr_t __fastcall scene_tick(std::uintptr_t owner, std::uint8_t flag, std::uintptr_t payload) {
        return dispatch(CategoryLifeEvent::scene_tick, owner, flag, payload,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    }
    static std::uintptr_t __fastcall scene_refresh(std::uintptr_t owner) {
        return dispatch(CategoryLifeEvent::scene_refresh, owner, 0, 0,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    }
    static std::uintptr_t __fastcall preview_assign(std::uintptr_t preview, int song) {
        return dispatch(CategoryLifeEvent::preview_assign, preview, static_cast<std::uint32_t>(song), 0,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    }
    static std::uintptr_t __fastcall style_apply(std::uintptr_t selection) {
        return dispatch(CategoryLifeEvent::style_apply, selection, 0, 0,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    }
    static std::uintptr_t __fastcall selection_restore(std::uintptr_t selection, int category, int song,
                                                      int p1, int p2, std::uint8_t force_open, int extra) {
        RestoreArguments restore{{category, force_open, song, p1, p2, extra}, song, p1, p2, extra};
        return dispatch(CategoryLifeEvent::selection_restore, selection, static_cast<std::uint32_t>(category), 0,
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), &restore);
    }
    static std::uintptr_t __fastcall initialize(std::uintptr_t owner, std::int32_t selector) {
        const auto& self = *live.load(std::memory_order_acquire);
        const auto argument = static_cast<std::uint32_t>(selector);
        // This entry is shared with another selection layout. Do not infer its
        // navigation owner from an offset or treat it as our ready boundary.
        if (reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != self.initialize_caller)
            return self.original(CategoryLifeEvent::initialize, owner, argument);
        return dispatch(CategoryLifeEvent::initialize, owner, argument);
    }
};
std::atomic<bool> CategoryLifecycleHooks::State::reserved{};
std::atomic<CategoryLifecycleHooks::State*> CategoryLifecycleHooks::State::live{};
constinit thread_local CategoryLifecycleHooks::State* CategoryLifecycleHooks::State::scope{};

CategoryLifecycleHooks::CategoryLifecycleHooks(MemoryView memory, const CategoryLifeSites& sites,
                                             void* context, Callbacks callbacks) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category lifecycle hook process slot reserved");
    try {
        require(memory.read && memory.query && callbacks.begin && callbacks.finish,
                "category lifecycle hook dependencies missing");
        require(sites.initialize_caller >= 0x10000, "category lifecycle initializer caller missing");
        require(sites.preview_caller >= 0x10000, "category lifecycle preview caller missing");
        require(sites.restore_caller >= 0x10000, "category lifecycle restore caller missing");
        require(sites.style_apply_caller >= 0x10000 && sites.style_restore_caller >= 0x10000,
                "category lifecycle style callers missing");
        require(sites.scene_entry_caller >= 0x10000 && sites.scene_tick_caller >= 0x10000 &&
                sites.scene_vtable >= 0x10000 && sites.idle_callback >= 0x10000 &&
                sites.scene_refresh_caller >= 0x10000,
                "category lifecycle ordinary scene boundary missing");
        for (std::size_t n = 0; n < count; ++n) {
            require(valid(sites.entries[n]), "category lifecycle hook site invalid");
            require(sites.initialize_caller < sites.entries[n].address ||
                    sites.initialize_caller >= sites.entries[n].address + sites.entries[n].body.size(),
                    "category lifecycle initializer caller overlaps hook body");
            for (const auto at : {sites.scene_entry_caller, sites.scene_tick_caller, sites.idle_callback, sites.scene_vtable})
                require(at < sites.entries[n].address || at >= sites.entries[n].address + sites.entries[n].body.size(),
                        "category lifecycle scene evidence overlaps hook body");
            for (std::size_t prior = 0; prior < n; ++prior)
                require(!overlap(sites.entries[n], sites.entries[prior]), "category lifecycle hook sites overlap");
        }
        state_ = new State(memory, sites, context, callbacks);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategoryLifecycleHooks::~CategoryLifecycleHooks() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategoryLifecycleHooks::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "category lifecycle hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "category lifecycle callbacks already retired");
    require(self.before_intact(), "category lifecycle hooks changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    State::live.store(&self, std::memory_order_release);
    try {
        for (auto& patch : self.patches) patch->enable();
        require(self.patches_intact(), "category lifecycle hooks changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool CategoryLifecycleHooks::intact() const noexcept { return state_->intact(); }
void CategoryLifecycleHooks::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
