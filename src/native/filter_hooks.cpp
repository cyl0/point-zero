#include "filter_hooks.hpp"
#include "inline_patch.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <stdexcept>

using FilterApplyDispatch = bool (__fastcall*)(std::uintptr_t, int, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t);
using FilterLevelDispatch = bool (__fastcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
extern "C" {
FilterApplyDispatch pz_filter_apply_dispatch{};
FilterLevelDispatch pz_filter_level_dispatch{};
bool pz_filter_apply_entry(std::uintptr_t, int, std::uintptr_t);
bool pz_filter_level_entry(std::uintptr_t, std::uintptr_t);
}

namespace point_zero::native {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool valid(const FilterSite& site) noexcept {
    return site.address >= 0x10000 && !site.body.empty() && site.body.size() <= UINTPTR_MAX - site.address;
}
bool intersects(const FilterSite& site, std::uintptr_t at, std::size_t size) noexcept {
    return size && at < site.address + site.body.size() && at + size > site.address;
}
bool category_at(MemoryView memory, std::uintptr_t entry_rsp, int& category) noexcept {
    constexpr std::uintptr_t offset = 0x78;
    if (entry_rsp < 0x10000 || (entry_rsp & 15) != 8 ||
        entry_rsp > UINTPTR_MAX - offset - sizeof(std::int32_t)) return false;
    std::int32_t value{};
    // This exact native caller owns the slot. Bound the address and guard the
    // provider itself, including injected readers which can fault before copy.
    __try {
        if (!memory.read || !memory.read(memory.context, entry_rsp + offset,
                                        std::as_writable_bytes(std::span(&value, 1)))) return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    category = value;
    return true;
}
} // namespace

struct FilterHooks::State {
    enum class Phase { disabled, enabling, armed, failed };
    struct Scope { State* state{}; std::uintptr_t candidate{}; bool in_level{}; };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local Scope* scope;
    MemoryView memory;
    const FilterSites sites;
    detail::InlinePatch apply, level;
    const NativeFilterApply original_apply;
    using NativeLevel = bool (__fastcall*)(std::uintptr_t, std::uintptr_t);
    const NativeLevel original_level;
    void* const context;
    const Callbacks callbacks;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit plus complete applications in increments of two.
    DWORD thread{};
    bool retained{};

    State(MemoryView view, const FilterSites& selected, void* opaque, Callbacks calls)
        : memory(view), sites(selected),
          apply(view, sites.apply.address, sites.apply.body, reinterpret_cast<std::uintptr_t>(&pz_filter_apply_entry)),
          level(view, sites.level.address, sites.level.body, reinterpret_cast<std::uintptr_t>(&pz_filter_level_entry)),
          original_apply(reinterpret_cast<NativeFilterApply>(apply.original())),
          original_level(reinterpret_cast<NativeLevel>(level.original())), context(opaque), callbacks(calls) {
        require(before_intact(), "disabled filter hooks changed during construction");
    }
    bool before_intact() const noexcept {
        return apply.before_intact() && level.before_intact();
    }
    bool patches_intact() const noexcept {
        return apply.intact() && level.intact();
    }
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact();
    }
    bool enter(std::uintptr_t caller, int set) noexcept {
        if (caller != sites.apply_caller || set != 1 ||
            phase.load(std::memory_order_acquire) != Phase::armed || GetCurrentThreadId() != thread) return false;
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
    static bool __fastcall dispatch_apply(std::uintptr_t owner, int set, std::uintptr_t candidate,
        std::uintptr_t entry_rsp, std::uintptr_t style, std::uintptr_t caller) {
        auto& self = *live.load(std::memory_order_acquire);
        Scope current{};
        auto* const previous = scope;
        scope = &current; // Unknown, wrong-set and retired applications also shadow the outer lease.
        bool entered{}, result{};
        __try {
            entered = self.enter(caller, set);
            int category{};
            // Never inspect the caller's outer stack until caller/thread/set and
            // admission are established. A rejected read forwards under a shadow.
            if (!entered || !category_at(self.memory, entry_rsp, category)) {
                result = self.original_apply(owner, set, candidate);
            } else {
                current.state = &self;
                current.candidate = candidate;
                const FilterApplyRequest request{owner, candidate, caller, set, category,
                    std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(style))};
                // The application callback owns exactly one native original call;
                // its returned bool is the native result, not a handled flag.
                result = self.callbacks.apply(self.context, request, self.original_apply);
            }
        } __finally {
            scope = previous;
            if (entered) self.leave();
        }
        // Fall through the finally on success. Returning inside the protected
        // block makes MSVC perform a local unwind for every native candidate.
        return result;
    }
    static bool __fastcall dispatch_level(std::uintptr_t filter, std::uintptr_t candidate, std::uintptr_t caller) {
        auto& self = *live.load(std::memory_order_acquire);
        auto* const current = scope;
        if (!current || current->state != &self || current->candidate != candidate ||
            caller != self.sites.level_caller || current->in_level)
            return self.original_level(filter, candidate);
        // This application's held lease admits its remaining leaves even after
        // retirement closes new applications. Recursion is local to this scope;
        // a nested admitted application has its own independent leaf guard.
        current->in_level = true;
        bool result{};
        __try {
            bool reject{};
            result = self.callbacks.level(self.context, filter, candidate, reject)
                ? reject : self.original_level(filter, candidate);
        } __finally { current->in_level = false; }
        return result;
    }
};
std::atomic<bool> FilterHooks::State::reserved{};
std::atomic<FilterHooks::State*> FilterHooks::State::live{};
constinit thread_local FilterHooks::State::Scope* FilterHooks::State::scope{};

FilterHooks::FilterHooks(MemoryView memory, const FilterSites& sites, void* context, Callbacks callbacks) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "filter hook process slot already reserved");
    try {
        require(memory.read && memory.query && callbacks.apply && callbacks.level &&
                valid(sites.apply) && valid(sites.level) && valid(sites.candidate) &&
                sites.apply_caller >= 0x10000 && sites.level_caller >= 0x10000,
                "filter hook arguments missing or invalid");
        require(contains(sites.candidate.address, sites.candidate.body.size(), sites.apply_caller, 1) &&
                contains(sites.apply.address, sites.apply.body.size(), sites.level_caller, 1),
                "filter callers outside their admitted owning bodies");
        require(!intersects(sites.apply, sites.level.address, sites.level.body.size()) &&
                !intersects(sites.apply, sites.candidate.address, sites.candidate.body.size()) &&
                !intersects(sites.level, sites.candidate.address, sites.candidate.body.size()),
                "filter hook bodies overlap");
        state_ = new State(memory, sites, context, callbacks);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
FilterHooks::~FilterHooks() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void FilterHooks::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled, "filter hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "filter callbacks already retired");
    require(self.before_intact(), "filter hooks changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    pz_filter_apply_dispatch = &State::dispatch_apply;
    pz_filter_level_dispatch = &State::dispatch_level;
    State::live.store(&self, std::memory_order_release);
    try {
        self.level.enable();
        self.apply.enable();
        require(self.patches_intact(), "filter hooks changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool FilterHooks::intact() const noexcept { return state_->intact(); }
void FilterHooks::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
