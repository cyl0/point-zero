#pragma once

#include "memory.hpp"

namespace point_zero::native {
// One process-lifetime x64 catalogue startup hook, int __fastcall(). The parent
// admits the target and supplies its expected displaced instructions (including any
// admitted foreign edits). This helper does not perform admission or ACK.
class CatalogHook final {
public:
    using Observer = void (*)(void*, int) noexcept;

    // Verifies displaced instructions and creates SafetyHook disabled. Throws on mismatch,
    // an occupied process slot, or an unsupported prologue. Supports the near
    // E9 layout in installed SafetyHook 0.7 with a non-relative stolen prologue.
    CatalogHook(MemoryView memory, std::uintptr_t target,
                std::span<const std::byte> expected_body,
                void* context, Observer after_original);
    ~CatalogHook() noexcept;
    CatalogHook(const CatalogHook&) = delete;
    CatalogHook& operator=(const CatalogHook&) = delete;
    CatalogHook(CatalogHook&&) = delete;
    CatalogHook& operator=(CatalogHook&&) = delete;

    // One attempt per object, including rejected preflight; throws on failure.
    // Rechecks displaced instructions before
    // enabling, then proves the exact owned entry/relay/trampoline. Once enable
    // is attempted, even failure retains backing state and consumes the slot.
    void arm();
    [[nodiscard]] bool intact() const noexcept;

    // Waits for an active observer; after return native calls never touch the
    // observer/context. Does not wait for originals, disable, or free code.
    // Do not call from inside the observer. The observer may use intact.
    void retire_observer() noexcept;

private:
    struct State;
    State* state_{};
};
// Construction, arm, and destruction are externally serialized. Retirement may
// race native calls. First original to return claims the only observation; later
// originals do not wait for it. The target and this core's code remain loaded
// for process lifetime. Destroying an armed wrapper retires only its observer;
// the thunk, trampoline, and reservation deliberately have no static destructor.
} // namespace point_zero::native
