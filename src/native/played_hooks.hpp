#pragma once

#include "played_chart.hpp"
#include "played_contract.hpp"
#include "stepup_contract.hpp"

namespace point_zero::native {
// Ordinary/Step Up confirmation, HUD and result captures, shared named binder and
// paired NOTES update. The AFP bitmap hook remains owned by SurfaceHooks.
class PlayedHooks final {
public:
    using Named = char (__fastcall*)(std::uintptr_t, const char*);
    struct Callbacks {
        void (*capture)(void*, const PlayedCapture&) noexcept{};
        // Every entered named call shadows the outer renderer context. Unknown,
        // nested or unmatched calls receive an empty capture; invalid names
        // receive nullptr. A valid name is a local copy of at most 95 bytes.
        void (*begin)(void*, const PlayedCapture&, std::uintptr_t, const char*) noexcept{};
        void (*end)(void*) noexcept{};
        void (*notes)(void*, std::uintptr_t) noexcept{};
    };
    PlayedHooks(MemoryView, const PlayedSites&, const StepUpSites&, void* context, Callbacks);
    ~PlayedHooks() noexcept;
    PlayedHooks(const PlayedHooks&) = delete;
    PlayedHooks& operator=(const PlayedHooks&) = delete;
    PlayedHooks(PlayedHooks&&) = delete;
    PlayedHooks& operator=(PlayedHooks&&) = delete;

    // One attempt. All seven patches start disabled and must pass complete code
    // and backing checks before callbacks open on the arming native thread.
    void arm();
    bool intact() const noexcept;
    // Close new callbacks and drain existing callbacks/native named scopes.
    // Never invoke from an entered callback/original named scope. Native code
    // keeps forwarding after retirement; no exposed backing is freed/unhooked.
    void retire_callbacks() noexcept;
private:
    struct State;
    State* state_{};
};
// Construction/arming/destruction serialized. Callback context outlives drain;
// targets and this core remain loaded. Mid-hook generated stubs have the same
// Windows exception-unwind limitation as the existing title comparison hook.
} // namespace point_zero::native
