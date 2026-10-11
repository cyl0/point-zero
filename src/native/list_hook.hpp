#pragma once

#include "memory.hpp"
#include <array>
#include <vector>

namespace point_zero::native {
struct ListHookSite {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Complete expected displaced emitter instructions.
    // Exact return addresses for background, paired P1/P2 and solo badge calls.
    // Other shared-image calls forward without copying a name or using context.
    std::array<std::uintptr_t, 4> callers{};
};

// The shared named-image entry. The selected row owner is carried in the
// caller's RSI, outside the Win64 argument list. A compiled ABI shim captures
// that register and the native return address before a C++ prologue can use it.
// Native callers still use the unchanged seven-argument Draw signature.
class ListHook final {
public:
    using Draw = std::uintptr_t (__fastcall*)(std::uintptr_t, const char*, int, int,
        std::uint32_t, std::uint32_t, std::uint32_t);
    using Callback = bool (*)(void*, std::uintptr_t owner, std::uintptr_t return_address,
        std::uintptr_t manager, const char* name, int x, int y,
        std::uint32_t alpha, std::uint32_t layer, std::uint32_t flags,
        std::uintptr_t& result) noexcept;
    // Background names are bounded local copies. Badge names preserve the
    // original native pointer for live table identity; consumers validate it.

    // Creates disabled and checks displaced instructions and the owned bridge.
    ListHook(MemoryView, const ListHookSite&, void* context, Callback);
    ~ListHook() noexcept;
    ListHook(const ListHook&) = delete;
    ListHook& operator=(const ListHook&) = delete;
    ListHook(ListHook&&) = delete;
    ListHook& operator=(ListHook&&) = delete;

    // One attempt, including failed preflight. Only the admitted native thread
    // invokes the callback after successful postcheck. Other calls forward.
    void arm();
    [[nodiscard]] bool intact() const noexcept;
    // Close and drain callback scopes. Never call from within an entered draw.
    // Forwarding backing remains retained forever after any enable attempt.
    void retire_callback() noexcept;

private:
    struct State;
    State* state_{};
};
// Construct/arm/destroy are externally serialized. Retirement may race draws.
// Callback context lives through retirement. Native target and core code stay
// loaded for process lifetime. Does not own assets, FeatureState or title hooks.
} // namespace point_zero::native
