#pragma once

#include "memory.hpp"

namespace point_zero::native {
struct AssistantHeaderDraw {
    std::uintptr_t owner{}, layout{};
    int song{}, style{-1}, difficulty{-1}, native_level{};
    bool selected{};
    char texture[32]{};
};

// Absolute addresses supplied by the admitted October 13 game bindings.
struct AssistantHeaderSites {
    std::uintptr_t update{}, header{}, style{}, player{};
};

class AssistantHeaderHook final {
public:
    struct Callbacks {
        void (*begin)(void*, std::uintptr_t) noexcept{};
        // completed=false abandons the scope without native restoration calls.
        void (*end)(void*, std::uintptr_t, bool completed) noexcept{};
        // true replaces the named setter and supplies its bool result.
        bool (*draw)(void*, const AssistantHeaderDraw&, bool&) noexcept{};
    };
    AssistantHeaderHook(MemoryView, ImageRange, const AssistantHeaderSites&, void*, Callbacks);
    ~AssistantHeaderHook() noexcept;
    AssistantHeaderHook(const AssistantHeaderHook&) = delete;
    AssistantHeaderHook& operator=(const AssistantHeaderHook&) = delete;
    // One attempt on the native menu thread. Both call sites must be intact
    // before callbacks run. Exposed forwarding code is retained for the process.
    void arm();
    bool intact() const noexcept;
    // Drain before destroying the renderer or retiring shared bitmap forwarding.
    // No native edits or unhooking; never call from an entered native scope.
    void retire_callbacks() noexcept;

private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
