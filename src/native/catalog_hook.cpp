#include "catalog_hook.hpp"
#include "inline_patch.hpp"

#include <atomic>
#include <mutex>
#include <stdexcept>

namespace point_zero::native {
namespace {
using Original = int (__fastcall*)();
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
} // namespace

struct CatalogHook::State {
    enum class Phase { disabled, enabling, armed, failed };
    static std::atomic<bool> reserved;
    static std::atomic<State*> live;
    detail::InlinePatch patch;
    Original original;
    std::atomic<Phase> phase{Phase::disabled};
    std::atomic<bool> claimed{};
    std::mutex observer_mutex;
    void* context;
    Observer observer;
    bool retired{}, retained{};

    State(MemoryView view, std::uintptr_t address, std::span<const std::byte> expected,
          void* opaque, Observer callback)
        : patch(view, address, expected, reinterpret_cast<std::uintptr_t>(&thunk)),
          original(reinterpret_cast<Original>(patch.original())), context(opaque), observer(callback) {}
    bool intact() const noexcept {
        return phase.load(std::memory_order_acquire) == Phase::armed && patch.intact();
    }
    void retire() noexcept {
        std::lock_guard lock(observer_mutex);
        retired = true;
        observer = nullptr;
        context = nullptr;
    }
    static int __fastcall thunk() {
        auto& self = *live.load(std::memory_order_acquire);
        const int result = self.original();
        // Claim AFTER the original returns. Later originals never acquire the
        // observer lock, so a blocked callback cannot serialize native calls.
        if (!self.claimed.exchange(true, std::memory_order_acq_rel)) {
            auto phase = self.phase.load(std::memory_order_acquire);
            while (phase == Phase::enabling) {
                self.phase.wait(phase, std::memory_order_acquire);
                phase = self.phase.load(std::memory_order_acquire);
            }
            if (phase == Phase::armed) {
                std::lock_guard lock(self.observer_mutex);
                if (!self.retired && self.observer) self.observer(self.context, result);
            }
        }
        return result;
    }
};
std::atomic<bool> CatalogHook::State::reserved{};
std::atomic<CatalogHook::State*> CatalogHook::State::live{};

CatalogHook::CatalogHook(MemoryView memory, std::uintptr_t target,
                         std::span<const std::byte> expected_body,
                         void* context, Observer after_original) {
    bool available = false;
    require(State::reserved.compare_exchange_strong(available, true),
            "catalogue hook process slot already reserved");
    try {
        require(memory.read && memory.query && after_original && !expected_body.empty(),
                "catalogue hook arguments missing");
        state_ = new State(memory, target, expected_body, context, after_original);
    } catch (...) {
        State::reserved.store(false, std::memory_order_release);
        throw;
    }
}
CatalogHook::~CatalogHook() noexcept {
    state_->retire();
    if (!state_->retained) {
        delete state_;
        State::reserved.store(false, std::memory_order_release);
    }
}
void CatalogHook::arm() {
    auto& self = *state_;
    require(self.phase.load(std::memory_order_acquire) == State::Phase::disabled,
            "catalogue hook arm already attempted");
    self.phase.store(State::Phase::failed, std::memory_order_release);
    {
        std::lock_guard lock(self.observer_mutex);
        require(!self.retired, "catalogue hook observer already retired");
    }
    require(self.patch.before_intact(), "catalogue hook changed before arm");
    self.phase.store(State::Phase::enabling, std::memory_order_release);
    self.retained = true;
    State::live.store(&self, std::memory_order_release);
    try {
        self.patch.enable();
        self.phase.store(State::Phase::armed, std::memory_order_release);
        self.phase.notify_all();
    } catch (...) {
        self.retire();
        self.phase.store(State::Phase::failed, std::memory_order_release);
        self.phase.notify_all();
        throw; // Exposed code is retained even if enabling had an unknown result.
    }
}
bool CatalogHook::intact() const noexcept { return state_->intact(); }
void CatalogHook::retire_observer() noexcept { state_->retire(); }
} // namespace point_zero::native
