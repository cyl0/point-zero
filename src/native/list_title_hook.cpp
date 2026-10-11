#include "list_title_hook.hpp"
#include "inline_patch.hpp"
#include "list_layout.hpp"
#include "title_limit_patch.hpp"

#include <windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <stdexcept>

#pragma intrinsic(_ReturnAddress)

namespace point_zero::native {
namespace {
constexpr std::array title_entry{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57}};
constexpr std::array title_limit{
    std::byte{0x0f}, std::byte{0x2f}, std::byte{0xc1}, std::byte{0x76}, std::byte{0x10}};
static_assert(sizeof(std::uintptr_t) == 8 && list_art::title_delta == 24);

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

} // namespace

struct ListTitleHook::State {
    enum class Phase { disabled, enabling, armed, failed };
    struct Scope {
        std::uintptr_t owner;
        bool custom;
    };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    static constinit thread_local Scope* scope;
    detail::InlinePatch entry;
    detail::TitleLimitPatch limit;
    const Title original;
    void* const context;
    const Permit permit;
    const std::uintptr_t caller;
    std::atomic<Phase> phase{Phase::disabled};
    // Low bit permanently closes admission; remaining bits count complete
    // permit-admitted title scopes, including their native original calls.
    std::atomic<std::uint32_t> users{};
    DWORD thread{};
    bool retained{};

    State(MemoryView view, const ListTitleSite& site, void* opaque, Permit callback)
        : entry(view, site.address, std::span(site.body).first(title_entry.size()),
                reinterpret_cast<std::uintptr_t>(&title_thunk)),
          limit(view, site.address + site.limit_offset, &width),
          original(reinterpret_cast<Title>(entry.original())), context(opaque),
          permit(callback), caller(site.caller) {
        require(before_intact(), "disabled title hooks changed during construction");
    }
    bool before_intact() const noexcept {
        return entry.before_intact() && limit.before_intact();
    }
    bool patches_intact() const noexcept {
        return entry.intact() && limit.intact();
    }
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patches_intact();
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
    static __declspec(noinline) std::uintptr_t __fastcall title_thunk(
        std::uintptr_t owner, int x, int y, float alpha, int layer) {
        const auto return_address = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        auto& self = *live.load(std::memory_order_acquire);
        Scope entered_scope{owner, false};
        auto* const previous = scope;
        scope = &entered_scope;
        bool entered{};
        // Every title call shadows the outer decision, even an unknown caller,
        // a wrong thread or a call after retirement. Permit also sees false
        // until it returns successfully, so its nested calls cannot inherit it.
        __try {
            entered = self.enter(return_address);
            if (entered && self.permit(self.context, owner) &&
                x <= (std::numeric_limits<int>::max)() - list_art::title_delta) {
                entered_scope.custom = true;
                x += list_art::title_delta;
            }
            return self.original(owner, x, y, alpha, layer);
        } __finally {
            scope = previous;
            if (entered) self.leave();
        }
    }
    static void width(safetyhook::Context& ctx) noexcept {
        // This decision belongs to the entered native title, not to the current
        // admission gate. No callback, native query or new policy check here.
        const auto* const current = scope;
        if (current && current->custom && ctx.rsi == current->owner)
            ctx.xmm1.f32[0] = 486.0f;
    }
};
std::atomic<bool> ListTitleHook::State::reserved{};
std::atomic<ListTitleHook::State*> ListTitleHook::State::live{};
constinit thread_local ListTitleHook::State::Scope* ListTitleHook::State::scope{};

ListTitleHook::ListTitleHook(MemoryView memory, const ListTitleSite& site, void* context, Permit permit) {
    bool available = false;
    require(State::reserved.compare_exchange_strong(available, true), "title hook process slot already reserved");
    try {
        require(memory.read && memory.query && permit && site.address >= 0x10000 &&
                site.body.size() >= title_entry.size() + title_limit.size() &&
                site.body.size() <= UINTPTR_MAX - site.address &&
                site.limit_offset >= title_entry.size() && site.limit_offset <= site.body.size() &&
                // Both the fallthrough and JBE target must remain in the
                // complete admitted body, not just the five patched bytes.
                21 < site.body.size() - site.limit_offset,
                "title hook arguments missing or invalid");
        require(site.caller >= 0x10000, "title hook caller is absent");
        require(std::equal(title_entry.begin(), title_entry.end(), site.body.begin()) &&
                std::equal(title_limit.begin(), title_limit.end(), site.body.begin() + site.limit_offset),
                "title hook entry or width instructions mismatch");
        // Validate the complete original before constructing either disabled
        // hook. An injected view cannot vouch for different live native code.
        state_ = new State(memory, site, context, permit);
    } catch (...) {
        State::reserved.store(false, std::memory_order_release);
        throw;
    }
}
ListTitleHook::~ListTitleHook() noexcept {
    state_->retire();
    if (!state_->retained) {
        delete state_;
        State::reserved.store(false, std::memory_order_release);
    }
}
void ListTitleHook::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "title hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    require(!(self.users.load(std::memory_order_acquire) & 1), "title callbacks already retired");
    require(self.before_intact(), "title hooks changed before arm");
    self.thread = GetCurrentThreadId();
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    self.retained = true;
    State::live.store(&self, std::memory_order_release);
    try {
        self.limit.enable();
        self.entry.enable();
        require(self.patches_intact(), "title hooks changed during arm");
        self.phase.store(State::Phase::armed, std::memory_order_release);
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        throw; // Both patches and forwarding backing survive every enable attempt.
    }
}
bool ListTitleHook::intact() const noexcept { return state_->intact(); }
void ListTitleHook::retire_callbacks() noexcept { state_->retire(); }
} // namespace point_zero::native
