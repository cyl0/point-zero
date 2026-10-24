#include "category_summary.hpp"
#include "inline_patch.hpp"
#include "played_mid_patch.hpp"

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <stdexcept>

namespace point_zero::native {
namespace {
using NativeCalculate = std::intptr_t (__fastcall*)(std::uintptr_t, int, int);
struct Invocation {
    NativeCalculate original;
    std::uintptr_t manager;
    int style, id;
    std::uintptr_t entry_rsp{};
};
static_assert(offsetof(Invocation, manager) == 8 && offsetof(Invocation, style) == 16 &&
              offsetof(Invocation, id) == 20 && offsetof(Invocation, entry_rsp) == 24);
extern "C" std::intptr_t pz_category_summary_invoke(Invocation*) noexcept(false);
constexpr std::array bind{std::byte{0x4c}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x38}};
constexpr std::array cancel{std::byte{0x48}, std::byte{0x8b}, std::byte{0x4c}, std::byte{0x24}, std::byte{0x68}};
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
bool buffer(std::uintptr_t at, std::size_t size) noexcept {
    return at >= 0x10000 && !(at & 7) && size <= UINTPTR_MAX - at;
}
bool valid(const CategorySummaryBinding& b) noexcept {
    return buffer(b.category, 208) && buffer(b.summary, 168) &&
        (b.category + 208 <= b.summary || b.summary + 168 <= b.category);
}
bool valid(const CategorySummaryRequest& r) noexcept {
    return r.style >= 0 && r.style <= 1 &&
        r.id >= 0 && buffer(r.manager, 0x29868);
}
template<std::size_t N> bool bytes_at(const CategorySummarySite& s, std::uintptr_t at,
                                     const std::array<std::byte, N>& expected) noexcept {
    return contains(s.address, s.body.size(), at, N) &&
        std::equal(expected.begin(), expected.end(), s.body.begin() + (at - s.address));
}
// The exact native frame is established before touching its spill. Do not query
// pages or trust a stale capture from a different invocation.
bool replace_spill(std::uintptr_t at, std::uintptr_t expected, std::uintptr_t replacement) noexcept {
    __try {
        auto* slot = reinterpret_cast<std::uintptr_t*>(at);
        if (*slot != expected) return false;
        *slot = replacement;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace

struct CategorySummary::State {
    enum class Phase { disabled, enabling, armed, failed };
    struct Scope {
        State* state{};
        Invocation invocation{};
        CategorySummaryBinding binding{};
        unsigned captures{};
        bool rejected{};
    };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local Scope* scope;
    const std::uintptr_t cancel_address;
    detail::InlinePatch entry;
    detail::PlayedMidPatch capture;
    const NativeCalculate original;
    void* const context;
    const Callbacks callbacks;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit; complete observed calls in increments of two.
    DWORD thread{};
    bool retained{};

    State(MemoryView m, const CategorySummarySite& s, void* c, Callbacks calls)
        : cancel_address(s.cancel),
          entry(m, s.address, s.body, reinterpret_cast<std::uintptr_t>(&dispatch)),
          capture(m, s.capture, std::span(s.body).subspan(s.capture - s.address, 5), on_capture),
          original(reinterpret_cast<NativeCalculate>(entry.original())), context(c), callbacks(calls) {
        require(before_intact(), "category summary changed during construction");
    }
    bool before_intact() const noexcept {
        return entry.before_intact() && capture.before_intact();
    }
    bool patches_intact() const noexcept {
        return entry.intact() && capture.intact();
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
        while (value != 1) { users.wait(value, std::memory_order_acquire); value = users.load(std::memory_order_acquire); }
    }
    static void on_capture(safetyhook::Context& registers) noexcept {
        auto* const current = scope;
        if (!current || !current->state) return;
        auto& self = *current->state;
        const auto& call = current->invocation;
        const auto index = static_cast<std::uintptr_t>(226 * call.style + call.id);
        const auto category = call.manager + 16 + 208 * index;
        // Only this admitted function's real invocation may bind. On failure
        // resume its native cookie/epilogue before any invalid indexed access.
        const bool matched = current->captures++ == 0 && call.entry_rsp >= 0x10000 + 0xA8 &&
            registers.rsp == call.entry_rsp - 0xA8 && !(registers.rsp & 15) &&
            registers.rcx == call.manager && registers.r12 == static_cast<std::uintptr_t>(call.style) &&
            registers.r8 == index && registers.rbx == category &&
            registers.r14 == call.manager + 0x16F68 + 168 * index &&
            registers.rdi == call.manager + 0x16F94 + 168 * index &&
            replace_spill(registers.rsp + 0x48, category, current->binding.category);
        if (!matched) {
            current->rejected = true;
            registers.rip = self.cancel_address;
            return;
        }
        registers.rbx = current->binding.category;
        registers.r14 = current->binding.summary + 8;
        registers.rdi = current->binding.summary + 52;
    }
    static std::intptr_t __fastcall dispatch(std::uintptr_t manager, int style, int id) {
        auto& self = *live.load(std::memory_order_acquire);
        Scope current{};
        auto* const previous = scope;
        scope = &current; // Unowned/rejected nested calls must not inherit an outer bind.
        const CategorySummaryRequest request{manager, style, id};
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        bool entered{}, prepared{}, completed{}, observed{};
        __try {
            entered = valid(request) && self.enter();
            if (entered) prepared = self.callbacks.prepare(self.context, request, current.binding);
            if (!prepared) {
                observed = entered && id < 226 && self.callbacks.native_finish;
                const auto result = self.original(manager, style, id);
                completed = true;
                return result;
            }
            if (!valid(current.binding)) return 0;
            current.invocation = {self.original, manager, style, id};
            current.state = &self;
            const auto result = pz_category_summary_invoke(&current.invocation);
            completed = current.captures == 1 && !current.rejected;
            return completed ? result : 0;
        } __finally {
            // No active native frame remains while the owner's postcheck runs.
            current.state = nullptr;
            if (prepared) self.callbacks.finish(self.context, request, current.binding, completed);
            if (observed) self.callbacks.native_finish(self.context, request, caller, completed);
            scope = previous;
            if (entered) self.leave();
        }
    }
};
std::atomic<bool> CategorySummary::State::reserved{};
std::atomic<CategorySummary::State*> CategorySummary::State::live{};
constinit thread_local CategorySummary::State::Scope* CategorySummary::State::scope{};

CategorySummary::CategorySummary(MemoryView memory, const CategorySummarySite& site,
                                 void* context, Callbacks callbacks) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "category summary process slot already reserved");
    try {
        require(memory.read && memory.query && context && callbacks.prepare && callbacks.finish &&
                site.address >= 0x10000 && site.body.size() <= UINTPTR_MAX - site.address &&
                !site.body.empty() && bytes_at(site, site.capture, bind) &&
                bytes_at(site, site.cancel, cancel) && site.capture >= site.address + 5 && site.cancel >= site.capture + 5,
                "category summary dependencies/body sites invalid");
        state_ = new State(memory, site, context, callbacks);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
CategorySummary::~CategorySummary() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void CategorySummary::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled, "category summary arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1) && self.before_intact(), "category summary retired/changed before arm");
    self.thread = GetCurrentThreadId();
    self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    State::live.store(&self, std::memory_order_release);
    try {
        self.capture.enable();
        self.entry.enable();
        require(self.patches_intact(), "category summary changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw;
    }
}
bool CategorySummary::intact() const noexcept { return state_->intact(); }
void CategorySummary::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
