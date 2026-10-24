#include "surface_hooks.hpp"
#include "inline_patch.hpp"

#include <windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <stdexcept>

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

struct SurfaceHooks::State {
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    detail::InlinePatch cup, bitmap, cup_setter, confirmation_setter, confirmation;
    Cup original_cup;
    Bitmap original_bitmap;
    CupSetter original_cup_setter;
    ConfirmationSetter original_confirmation_setter;
    Cup original_confirmation;
    void* const context;
    const Callbacks callbacks;
    std::atomic<Phase> phase{Phase::disabled};
    // Low bit permanently closes admission; remaining bits count entered
    // scopes. No render mutex, allocation, code hash or native memory query.
    std::atomic<std::uint32_t> users{};
    DWORD thread{};
    bool retained{};

    State(MemoryView memory_view, const SurfaceHookSites& sites, void* opaque, Callbacks calls)
        : cup(memory_view, sites.cup.address, sites.cup.body, reinterpret_cast<std::uintptr_t>(&cup_thunk)),
          bitmap(memory_view, sites.bitmap.address, sites.bitmap.body, reinterpret_cast<std::uintptr_t>(&bitmap_thunk)),
          cup_setter(memory_view, sites.cup_setter.address, sites.cup_setter.body, reinterpret_cast<std::uintptr_t>(&cup_setter_thunk)),
          confirmation_setter(memory_view, sites.confirmation_setter.address, sites.confirmation_setter.body, reinterpret_cast<std::uintptr_t>(&confirmation_setter_thunk)),
          confirmation(memory_view, sites.confirmation.address, sites.confirmation.body, reinterpret_cast<std::uintptr_t>(&confirmation_thunk)),
          original_cup(reinterpret_cast<Cup>(cup.original())),
          original_bitmap(reinterpret_cast<Bitmap>(bitmap.original())),
          original_cup_setter(reinterpret_cast<CupSetter>(cup_setter.original())),
          original_confirmation_setter(reinterpret_cast<ConfirmationSetter>(confirmation_setter.original())),
          original_confirmation(reinterpret_cast<Cup>(confirmation.original())), context(opaque), callbacks(calls) {}
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact();
    }
    bool patches_intact() const noexcept {
        return cup.intact() && bitmap.intact() && cup_setter.intact() &&
            confirmation_setter.intact() && confirmation.intact();
    }
    bool before_intact() const noexcept {
        return cup.before_intact() && bitmap.before_intact() && cup_setter.before_intact() &&
            confirmation_setter.before_intact() && confirmation.before_intact();
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
        // Context is immutable. After the gate closes, no new reader can
        // acquire it; after draining, even the final end_cup has finished.
    }
    static std::int64_t __fastcall cup_thunk(std::uintptr_t owner) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter()) return self.original_cup(owner);
        // Native faults retain their native behavior, but always retire the
        // entered scope and run its matching end, including nested exceptions.
        __try {
            __try {
                self.callbacks.begin_cup(self.context, owner);
                return self.original_cup(owner);
            } __finally {
                self.callbacks.end_cup(self.context);
            }
        } __finally {
            self.leave();
        }
    }
    static int __fastcall bitmap_thunk(int mc, const char* name, int mode) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter()) return self.original_bitmap(mc, name, mode);
        __try {
            char copied[96]{};
            int result{};
            if (bounded_name(name, copied) &&
                self.callbacks.bitmap(self.context, mc, copied, mode, result, self.original_bitmap)) return result;
            // Declined/invalid names preserve the exact caller arguments.
            return self.original_bitmap(mc, name, mode);
        } __finally {
            self.leave();
        }
    }
    static std::int64_t __fastcall cup_setter_thunk(std::uintptr_t owner, std::uintptr_t music,
                                                   int value, std::uint8_t mode) {
        auto& self = *live.load(std::memory_order_acquire);
        if (!self.enter()) return self.original_cup_setter(owner, music, value, mode);
        __try {
            self.callbacks.begin_cup_set(self.context);
            const auto result = self.original_cup_setter(owner, music, value, mode);
            self.callbacks.bind_cup(self.context, owner, music);
            return result;
        } __finally { self.leave(); }
    }
    static std::int64_t __fastcall confirmation_setter_thunk(std::uintptr_t owner, std::uintptr_t music,
                                                            int first, int second) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        if (!self.enter()) return self.original_confirmation_setter(owner, music, first, second);
        __try {
            self.callbacks.begin_confirmation_set(self.context);
            const auto result = self.original_confirmation_setter(owner, music, first, second);
            self.callbacks.bind_confirmation(self.context, owner, music, caller);
            return result;
        } __finally { self.leave(); }
    }
    static std::int64_t __fastcall confirmation_thunk(std::uintptr_t owner) {
        auto& self = *live.load(std::memory_order_acquire);
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        if (!self.enter()) return self.original_confirmation(owner);
        __try {
            __try {
                self.callbacks.begin_confirmation(self.context, owner, caller);
                return self.original_confirmation(owner);
            } __finally { self.callbacks.end_confirmation(self.context); }
        } __finally { self.leave(); }
    }
};
std::atomic<bool> SurfaceHooks::State::reserved{};
std::atomic<SurfaceHooks::State*> SurfaceHooks::State::live{};

SurfaceHooks::SurfaceHooks(MemoryView memory, const SurfaceHookSites& sites,
                           void* context, Callbacks callbacks) {
    bool available = false;
    require(State::reserved.compare_exchange_strong(available, true),
            "surface hook process slot already reserved");
    try {
        require(memory.read && memory.query && callbacks.begin_cup && callbacks.end_cup && callbacks.bitmap &&
                callbacks.begin_cup_set && callbacks.bind_cup && callbacks.begin_confirmation_set &&
                callbacks.bind_confirmation && callbacks.begin_confirmation && callbacks.end_confirmation,
                "surface hook arguments missing or invalid");
        const std::array entries{&sites.cup, &sites.bitmap, &sites.cup_setter,
                                 &sites.confirmation_setter, &sites.confirmation};
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& site = *entries[i];
            require(!site.body.empty() && site.body.size() <= UINTPTR_MAX - site.address,
                    "surface hook body missing or invalid");
            for (std::size_t j = 0; j < i; ++j)
                require(site.address + site.body.size() <= entries[j]->address ||
                        entries[j]->address + entries[j]->body.size() <= site.address,
                        "surface hook bodies overlap");
        }
        state_ = new State(memory, sites, context, callbacks);
    } catch (...) {
        State::reserved.store(false, std::memory_order_release);
        throw;
    }
}
SurfaceHooks::~SurfaceHooks() noexcept {
    state_->retire();
    if (!state_->retained) {
        delete state_;
        State::reserved.store(false, std::memory_order_release);
    }
}
void SurfaceHooks::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "surface hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "surface callbacks already retired");
    require(self.before_intact(), "surface hooks changed before arm");
    self.thread = GetCurrentThreadId();
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    self.retained = true;
    State::live.store(&self, std::memory_order_release);
    try {
        self.bitmap.enable();
        self.cup_setter.enable();
        self.cup.enable();
        self.confirmation_setter.enable();
        self.confirmation.enable();
        require(self.patches_intact(), "surface hooks changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool SurfaceHooks::intact() const noexcept { return state_->intact(); }
SurfaceHooks::Bitmap SurfaceHooks::stock_bitmap() const noexcept { return state_->original_bitmap; }
void SurfaceHooks::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
