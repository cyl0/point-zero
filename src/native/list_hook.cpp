#include "list_hook.hpp"
#include "call_patch.hpp"
#include <memory>

#include <windows.h>
#include <atomic>
#include <stdexcept>

// The compiled Win64 shim copies RSI/return address into two extra arguments.
// This private dispatch pointer is set once before exposing any entry patch.
// It is never cleared: later native calls still require retained forwarding.
using ListDispatch = std::uintptr_t (__fastcall*)(std::uintptr_t, const char*, int, int,
    std::uint32_t, std::uint32_t, std::uint32_t, std::uintptr_t, std::uintptr_t);
extern "C" {
ListDispatch pz_list_draw_dispatch{};
std::uintptr_t pz_list_draw_entry(std::uintptr_t, const char*, int, int,
    std::uint32_t, std::uint32_t, std::uint32_t);
}

namespace point_zero::native {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool bounded_name(const char* from, char (&to)[96]) noexcept {
    if (!from) return false;
    __try {
        for (std::size_t i = 0; i < sizeof(to); ++i) {
            to[i] = from[i];
            if (!to[i]) return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}
} // namespace

struct ListHook::State {
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    std::array<std::unique_ptr<detail::CallPatch>, 4> patches;
    void* const context;
    const Callback callback;
    const std::array<std::uintptr_t, 4> callers;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit plus entered scopes in steps of2.
    DWORD thread{};
    bool retained{};

    State(MemoryView view, const ListHookSite& site, void* opaque, Callback call)
        : context(opaque), callback(call), callers(site.callers) {
        for (std::size_t i = 0; i < patches.size(); ++i)
            patches[i] = std::make_unique<detail::CallPatch>(view, callers[i] - 5,
                reinterpret_cast<std::uintptr_t>(&pz_list_draw_entry));
    }
    Draw original(std::uintptr_t caller) const noexcept {
        for (std::size_t i = 0; i < callers.size(); ++i)
            if (callers[i] == caller) return reinterpret_cast<Draw>(patches[i]->original());
        return reinterpret_cast<Draw>(patches.front()->original());
    }
    bool intact() const noexcept {
        if (phase.load(std::memory_order_acquire) != Phase::armed) return false;
        for (const auto& patch : patches) if (!patch->intact()) return false;
        return true;
    }
    bool enter(std::uintptr_t return_address) noexcept {
        if (return_address != callers[0] && return_address != callers[1] &&
            return_address != callers[2] && return_address != callers[3]) return false;
        if (phase.load(std::memory_order_acquire) != Phase::armed || GetCurrentThreadId() != thread)
            return false;
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
    static std::uintptr_t __fastcall draw(std::uintptr_t manager, const char* name, int x, int y,
        std::uint32_t alpha, std::uint32_t layer, std::uint32_t flags,
        std::uintptr_t owner, std::uintptr_t return_address) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto original = self.original(return_address);
        if (!self.enter(return_address)) return original(manager, name, x, y, alpha, layer, flags);
        __try {
            char copied[96]{};
            const char* callback_name = name;
            if (return_address == self.callers[0]) {
                if (!bounded_name(name, copied))
                    return original(manager, name, x, y, alpha, layer, flags);
                callback_name = copied;
            }
            std::uintptr_t result{};
            if (self.callback(self.context, owner, return_address,
                manager, callback_name, x, y, alpha, layer, flags, result)) return result;
            return original(manager, name, x, y, alpha, layer, flags);
        } __finally {
            self.leave();
        }
    }
};
std::atomic<bool> ListHook::State::reserved{};
std::atomic<ListHook::State*> ListHook::State::live{};

ListHook::ListHook(MemoryView memory, const ListHookSite& site, void* context, Callback callback) {
    bool available = false;
    require(State::reserved.compare_exchange_strong(available, true), "list hook process slot already reserved");
    try {
        require(memory.read && memory.query && callback, "list hook arguments missing or invalid");
        for (std::size_t i = 0; i < site.callers.size(); ++i) {
            require(site.callers[i] >= 0x10005, "list hook caller is absent");
            for (std::size_t j = 0; j < i; ++j)
                require(site.callers[i] + 5 <= site.callers[j] || site.callers[j] + 5 <= site.callers[i],
                        "list hook callers overlap");
        }
        state_ = new State(memory, site, context, callback);
    } catch (...) {
        State::reserved.store(false, std::memory_order_release);
        throw;
    }
}
ListHook::~ListHook() noexcept {
    state_->retire();
    if (!state_->retained) {
        delete state_;
        State::reserved.store(false, std::memory_order_release);
    }
}
void ListHook::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "list hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "list callback already retired");
    for (const auto& patch : self.patches)
        require(patch->before_intact(), "list hook changed before arm");
    self.thread = GetCurrentThreadId();
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    self.retained = true;
    pz_list_draw_dispatch = &State::draw;
    State::live.store(&self, std::memory_order_release);
    try {
        for (auto& patch : self.patches) patch->enable();
        for (const auto& patch : self.patches)
            require(patch->intact(), "list hook changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool ListHook::intact() const noexcept { return state_->intact(); }
void ListHook::retire_callback() noexcept { state_->retire(); }
} // namespace point_zero::native
