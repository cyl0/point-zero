#include "sort_hook.hpp"
#include "call_patch.hpp"

#include <windows.h>
#include <atomic>
#include <stdexcept>

using SortDispatch = void (__fastcall*)(void*, std::size_t, std::size_t,
    point_zero::native::SortCompare, std::uintptr_t, std::uintptr_t, std::uintptr_t);
extern "C" {
SortDispatch pz_sort_dispatch{};
void pz_sort_entry(void*, std::size_t, std::size_t, point_zero::native::SortCompare);
}
namespace point_zero::native {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
}
struct SortHook::State {
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    detail::CallPatch patch;
    NativeQSort original;
    void* const context;
    const Callback callback;
    const std::uintptr_t caller;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{};
    DWORD thread{};
    bool retained{};
    State(MemoryView m, const SortHookSite& site, void* opaque, Callback fn)
        : patch(m, site.caller - 5, reinterpret_cast<std::uintptr_t>(&pz_sort_entry)),
          original(reinterpret_cast<NativeQSort>(patch.original())), context(opaque), callback(fn), caller(site.caller) {}
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patch.intact();
    }
    bool enter(std::uintptr_t return_address) noexcept {
        if (return_address != caller || phase.load(std::memory_order_acquire) != Phase::armed ||
            GetCurrentThreadId() != thread) return false;
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
    static void __fastcall dispatch(void* bars, std::size_t count, std::size_t width, SortCompare compare,
                                    std::uintptr_t category, std::uintptr_t visible, std::uintptr_t caller) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter(caller)) { self.original(bars, count, width, compare); return; }
        __try {
            const SortRequest request{category, caller, reinterpret_cast<std::uintptr_t>(bars),
                                      count, width, compare, static_cast<std::uint8_t>(visible)};
            if (!self.callback(self.context, request, self.original)) self.original(bars, count, width, compare);
        } __finally { self.leave(); }
    }
};
std::atomic<bool> SortHook::State::reserved{};
std::atomic<SortHook::State*> SortHook::State::live{};

SortHook::SortHook(MemoryView memory, const SortHookSite& site, void* context, Callback callback) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "sort hook process slot already reserved");
    try {
        require(memory.read && memory.query && callback && site.caller >= 0x10005,
                "sort hook arguments missing or invalid");
        state_ = new State(memory, site, context, callback);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
SortHook::~SortHook() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void SortHook::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled, "sort hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "sort callback already retired");
    require(self.patch.before_intact(), "sort hook changed before arm");
    self.thread = GetCurrentThreadId(); self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    pz_sort_dispatch = &State::dispatch;
    State::live.store(&self, std::memory_order_release);
    try {
        self.patch.enable();
        require(self.patch.intact(), "sort hook changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) { self.retire(); self.phase.store(State::Phase::failed, std::memory_order_release); throw; }
}
bool SortHook::intact() const noexcept { return state_->intact(); }
void SortHook::retire_callback() noexcept { state_->retire(); }
} // namespace point_zero::native
