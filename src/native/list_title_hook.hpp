#pragma once

#include "memory.hpp"
#include <vector>

namespace point_zero::native {
struct ListTitleSite {
    std::uintptr_t address{};
    std::vector<std::byte> body; // Complete title renderer, including inner width site.
    std::uintptr_t caller{}; // Selected row renderer's native return address.
    std::size_t limit_offset{}; // COMISS XMM0,XMM1; JBE +10h inside body.
};

// Owns the title entry and its scoped text-width comparison. Bitmap transforms
// remain native. Only the admitted caller/thread with a successful permit gets
// the approved +24 x shift and 486-pixel native text limit (stock limit is510).
class ListTitleHook final {
public:
    using Title = std::uintptr_t (__fastcall*)(std::uintptr_t, int, int, float, int);
    using Permit = bool (*)(void*, std::uintptr_t owner) noexcept;
    ListTitleHook(MemoryView, const ListTitleSite&, void* context, Permit);
    ~ListTitleHook() noexcept;
    ListTitleHook(const ListTitleHook&) = delete;
    ListTitleHook& operator=(const ListTitleHook&) = delete;
    ListTitleHook(ListTitleHook&&) = delete;
    ListTitleHook& operator=(ListTitleHook&&) = delete;

    // One attempt, including rejected preflight. Both hooks start disabled and
    // must pass complete postchecks before callback admission opens.
    void arm();
    [[nodiscard]] bool intact() const noexcept;
    // Close permit admission and drain entered title calls. Existing custom
    // calls finish with their original width decision, even during retirement.
    // Never call from an entered title scope. No unhook/free after exposure.
    void retire_callbacks() noexcept;
private:
    struct State;
    State* state_{};
};
// Construction/arming/destruction are serialized; retirement may race calls.
// Context lives through retirement; MemoryView remains valid for arm/intact.
// Native target and this code stay loaded.
} // namespace point_zero::native
