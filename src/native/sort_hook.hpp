#pragma once

#include "difficulty_sort.hpp"

namespace point_zero::native {
struct SortHookSite {
    std::uintptr_t caller{}; // Native dispatcher return; other calls forward.
};
class SortHook final {
public:
    using Callback = bool (*)(void*, const SortRequest&, NativeQSort original) noexcept;
    // Disabled; context/callback/module backing must survive callback drain.
    SortHook(MemoryView, const SortHookSite&, void* context, Callback);
    ~SortHook() noexcept;
    SortHook(const SortHook&) = delete;
    SortHook& operator=(const SortHook&) = delete;
    void arm(); // One attempt, on the admitted native callback thread.
    bool intact() const noexcept;
    void retire_callback() noexcept; // Never from an entered callback/original.
private:
    struct State;
    State* state_{};
};
// Construction/arming/destruction are externally serialized. Retirement drains
// callbacks while native calls may race. Exposed forwarding backing is retained
// for process lifetime; no shared feature publication or resource release here.
} // namespace point_zero::native
