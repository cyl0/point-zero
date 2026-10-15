#include "played_hooks.hpp"
#include "inline_patch.hpp"
#include "played_mid_patch.hpp"

#include <windows.h>
#include <intrin.h>

#pragma intrinsic(_AddressOfReturnAddress)
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <memory>
#include <stdexcept>

namespace point_zero::native {
namespace {
// Preserve the ordinary four/NOTES indices; Step Up is the last owned patch.
constexpr std::size_t notes_index = 4, stepup_index = 5;
constexpr std::array<std::size_t, 6> capture_bytes{8, 8, 6, 8, 5, 8};
constexpr std::array capture_kinds{
    PlayedCaptureKind::confirmation_first, PlayedCaptureKind::confirmation_second,
    PlayedCaptureKind::hud, PlayedCaptureKind::result, PlayedCaptureKind::unknown,
    PlayedCaptureKind::stepup};
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
int low_int(std::uintptr_t value) noexcept { return std::bit_cast<std::int32_t>(std::uint32_t(value)); }
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
bool stack_word(std::uintptr_t address, std::uintptr_t& out) noexcept {
    if (address < 0x10000 || sizeof(out) > UINTPTR_MAX - address) return false;
    __try { out = *reinterpret_cast<const std::uintptr_t*>(address); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::array<const PlayedFunction*, 7> functions(const PlayedSites& sites, const StepUpSites& stepup) noexcept {
    return {&sites.named, &sites.confirmation, &sites.hud, &sites.result, &sites.result_bind, &sites.notes_update,
            &stepup.builder};
}
const PlayedFunction& containing(const PlayedSites& sites, const StepUpSites& stepup, std::size_t index) noexcept {
    if (index == stepup_index) return stepup.builder;
    return index < 2 ? sites.confirmation : (index == 2 ? sites.hud : (index == 3 ? sites.result : sites.notes_update));
}
std::uintptr_t capture_at(const PlayedSites& sites, const StepUpSites& stepup, std::size_t index) noexcept {
    if (index == stepup_index) return stepup.capture;
    return index < 4 ? sites.captures[index] : sites.notes_capture;
}
std::span<const std::byte> capture_body(const PlayedSites& sites, const StepUpSites& stepup, std::size_t index) {
    const auto& fn = containing(sites, stepup, index);
    const auto address = capture_at(sites, stepup, index);
    require(contains(fn.address, fn.body.size(), address, capture_bytes[index]),
            "played capture outside its admitted body");
    return std::span(fn.body).subspan(address - fn.address, capture_bytes[index]);
}
} // namespace

struct PlayedHooks::State {
    enum class Phase { disabled, enabling, armed, failed };
    struct Pending { PlayedCapture chart{}; std::uintptr_t stack{}; unsigned kind{}; };
    struct Scope { State* state; bool protected_context; };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local Pending pending;
    static constinit thread_local Scope* scope;
    struct CaptureInstruction {
        std::uintptr_t address{}, named_return{};
        std::array<std::byte, 8> expected{};
        std::size_t size{};
        PlayedCaptureKind kind{PlayedCaptureKind::unknown};
    };
    // Only exact admitted capture instructions and caller identities survive
    // construction. MidPatch/InlinePatch retain their own displaced bytes.
    std::array<CaptureInstruction, capture_bytes.size()> instructions{};
    const std::array<std::uintptr_t, 2> result_returns;
    detail::InlinePatch named;
    std::array<std::unique_ptr<detail::PlayedMidPatch>, capture_bytes.size()> captures;
    const Named original;
    void* const context;
    const Callbacks callbacks;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<std::uint32_t> users{}; // Closed bit and entered scopes in increments of two.
    DWORD thread{};
    bool retained{};

    State(MemoryView view, const PlayedSites& selected, const StepUpSites& selected_stepup, void* opaque, Callbacks calls)
        : result_returns(selected.result_returns),
          named(view, selected.named.address, selected.named.body,
              reinterpret_cast<std::uintptr_t>(&named_entry)),
          original(reinterpret_cast<Named>(named.original())), context(opaque), callbacks(calls) {
        constexpr std::array<safetyhook::MidHookFn, capture_bytes.size()> entries{
            capture0, capture1, capture2, capture3, capture4, capture5};
        for (std::size_t i = 0; i < captures.size(); ++i) {
            auto& instruction = instructions[i];
            instruction.address = capture_at(selected, selected_stepup, i);
            instruction.named_return = i == stepup_index ? selected_stepup.named_return :
                (i < selected.named_returns.size() ? selected.named_returns[i] : 0);
            instruction.size = capture_bytes[i]; instruction.kind = capture_kinds[i];
            const auto before = capture_body(selected, selected_stepup, i);
            std::copy(before.begin(), before.end(), instruction.expected.begin());
            captures[i] = std::make_unique<detail::PlayedMidPatch>(view, instruction.address,
                std::span(instruction.expected).first(instruction.size), entries[i], *selected.bindings);
        }
        require(before_intact(), "disabled played hooks changed during construction");
    }
    bool before_intact() const noexcept {
        if (!named.before_intact()) return false;
        for (const auto& patch : captures) if (!patch->before_intact()) return false;
        return true;
    }
    bool patches_intact() const noexcept {
        if (!named.intact()) return false;
        for (const auto& patch : captures) if (!patch->intact()) return false;
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
    static void capture(std::size_t index, safetyhook::Context& registers) noexcept {
        auto& self = *live.load(std::memory_order_acquire);
        if (index != notes_index) pending = {};
        if (!self.enter()) return;
        __try {
            if (index == notes_index) { self.callbacks.notes(self.context, registers.rdi); return; }
            PlayedCapture chart;
            chart.site = self.instructions[index].address;
            chart.kind = self.instructions[index].kind;
            chart.owner = index == stepup_index ? registers.rsi : registers.r14;
            if (index == 2) {
                chart.music = registers.rbp; chart.player = low_int(registers.rbx);
                chart.style = low_int(registers.rsi); chart.difficulty = low_int(registers.rdi);
                chart.native_level = low_int(registers.rax);
            } else if (index == 3) {
                chart.music = registers.r13; chart.player = low_int(registers.rsi);
                chart.style = low_int(registers.rbx); chart.difficulty = low_int(registers.r15);
                chart.native_level = low_int(registers.rax);
            }
            const Pending next{chart, registers.rsp, static_cast<unsigned>(index + 1)};
            // The observer may enter native code itself. Publish this one-shot
            // token only after it returns so its nested work cannot inherit it.
            self.callbacks.capture(self.context, chart);
            pending = next;
        } __finally { self.leave(); }
    }
    static void capture0(safetyhook::Context& r) noexcept { capture(0, r); }
    static void capture1(safetyhook::Context& r) noexcept { capture(1, r); }
    static void capture2(safetyhook::Context& r) noexcept { capture(2, r); }
    static void capture3(safetyhook::Context& r) noexcept { capture(3, r); }
    static void capture4(safetyhook::Context& r) noexcept { capture(4, r); }
    static void capture5(safetyhook::Context& r) noexcept { capture(5, r); }

    bool matches(const Pending& token, std::uintptr_t entry_stack) const noexcept {
        if (!token.kind || token.kind > capture_bytes.size() || token.kind == notes_index + 1 ||
            entry_stack > UINTPTR_MAX - 0x38 ||
            entry_stack < 0x10000 || (entry_stack & 15) != 8) return false;
        const auto index = token.kind - 1;
        std::uintptr_t caller{};
        const auto named_return = instructions[index].named_return;
        if (!stack_word(entry_stack, caller) || caller != named_return) return false;
        // Step Up's fast binder restores its entire frame before tail-jumping
        // to named. It has the same exact stack relation as a direct caller.
        if (index != 3) return token.stack == entry_stack + 8;
        // The admitted result_bind pushes RBX and reserves20h before calling
        // named. Its outer return distinguishes the selected result from every
        // other user of that shared helper. No heuristic stack unwinding.
        std::uintptr_t outer{};
        return token.stack == entry_stack + 0x38 && stack_word(entry_stack + 0x30, outer) &&
            (outer == result_returns[0] || outer == result_returns[1]);
    }
    // This is the actual two-argument patch destination. Its own return-slot
    // address is the native incoming RSP even after the compiler's prologue.
    // Keep this noinline: querying a helper's return slot would add a frame.
    __declspec(noinline) static char __fastcall named_entry(std::uintptr_t wrapper, const char* name) {
        const auto entry_stack = reinterpret_cast<std::uintptr_t>(_AddressOfReturnAddress());
        return dispatch(wrapper, name, entry_stack);
    }
    static char __fastcall dispatch(std::uintptr_t wrapper, const char* name, std::uintptr_t entry_stack) {
        auto& self = *live.load(std::memory_order_acquire);
        const Pending next = pending; pending = {};
        auto* const previous = scope;
        const bool entered = self.enter();
        // A nested call after retirement still needs to shadow an outer active
        // renderer scope. That outer scope already protects this context until
        // its native call and matching end finish; no new lifetime is admitted.
        const bool borrowed = !entered && previous && previous->state == &self && previous->protected_context;
        Scope current{&self, entered || borrowed}; scope = &current;
        __try {
            if (!current.protected_context) return self.original(wrapper, name);
            char copied[96]{};
            const bool named_ok = bounded_name(name, copied);
            PlayedCapture selected;
            if (entered && !previous && named_ok && self.matches(next, entry_stack)) selected = next.chart;
            __try {
                self.callbacks.begin(self.context, selected, wrapper, named_ok ? copied : nullptr);
                return self.original(wrapper, name);
            } __finally { self.callbacks.end(self.context); }
        } __finally {
            scope = previous;
            if (entered) self.leave();
        }
    }
};
std::atomic<bool> PlayedHooks::State::reserved{};
std::atomic<PlayedHooks::State*> PlayedHooks::State::live{};
constinit thread_local PlayedHooks::State::Pending PlayedHooks::State::pending{};
constinit thread_local PlayedHooks::State::Scope* PlayedHooks::State::scope{};

PlayedHooks::PlayedHooks(MemoryView memory, const PlayedSites& sites, const StepUpSites& stepup,
                         void* context, Callbacks callbacks) {
    bool available{};
    require(State::reserved.compare_exchange_strong(available, true), "played hook process slot already reserved");
    try {
        require(memory.read && memory.query && callbacks.capture && callbacks.begin && callbacks.end && callbacks.notes,
                "played hook callbacks or memory missing");
        const auto bodies = functions(sites, stepup);
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const auto& a = *bodies[i];
            require(a.address >= 0x10000 && !a.body.empty() && a.body.size() <= UINTPTR_MAX - a.address,
                    "played hook function range invalid");
            for (std::size_t j = 0; j < i; ++j) {
                const auto& b = *bodies[j];
                require(a.address + a.body.size() <= b.address || b.address + b.body.size() <= a.address,
                        "played hook function bodies overlap");
            }
        }
        for (std::size_t i = 0; i < capture_bytes.size(); ++i) (void)capture_body(sites, stepup, i);
        require(sites.captures[0] + capture_bytes[0] <= sites.captures[1], "played confirmation captures overlap");
        for (std::size_t i = 0; i < sites.named_returns.size(); ++i) {
            const auto& fn = i == 3 ? sites.result_bind : containing(sites, stepup, i);
            require(contains(fn.address, fn.body.size(), sites.named_returns[i], 1), "played named return outside its body");
        }
        require(sites.result_returns[0] != sites.result_returns[1], "played result returns are duplicated");
        for (const auto caller : sites.result_returns)
            require(contains(sites.result.address, sites.result.body.size(), caller, 1), "played result return outside its body");
        require(contains(stepup.builder.address, stepup.builder.body.size(), stepup.named_return, 1),
                "Step Up named return outside builder body");
        state_ = new State(memory, sites, stepup, context, callbacks);
    } catch (...) { State::reserved.store(false, std::memory_order_release); throw; }
}
PlayedHooks::~PlayedHooks() noexcept {
    state_->retire();
    if (!state_->retained) { delete state_; State::reserved.store(false, std::memory_order_release); }
}
void PlayedHooks::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled, "played hooks arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "played callbacks already retired");
    require(self.before_intact(), "played hooks changed before arm");
    self.thread = GetCurrentThreadId(); self.retained = true;
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    State::live.store(&self, std::memory_order_release);
    try {
        self.named.enable();
        for (auto& patch : self.captures) patch->enable();
        require(self.patches_intact(), "played hooks changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire(); self.phase.store(State::Phase::failed, std::memory_order_release); throw;
    }
}
bool PlayedHooks::intact() const noexcept { return state_->intact(); }
void PlayedHooks::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
