#pragma once

#include <atomic>
#include <cstdint>

namespace point_zero::native {
enum class FeaturePhase { preparing, ready, unavailable, uncertain, stopped };

struct FeatureView {
    FeaturePhase phase;
    bool preference;
    bool battle;

    bool enabled() const noexcept {
        return phase == FeaturePhase::ready && preference && !battle;
    }
};

// One shared state for display, bank and filter decisions; no separate display
// latch. The caller serializes native transitions. This object neither changes
// native banks nor proves their coherency. Startup failure is terminal;
// per-operation native failures do not call fail().
class FeatureState {
public:
    FeatureState() noexcept = default;
    FeatureState(const FeatureState&) = delete;
    FeatureState& operator=(const FeatureState&) = delete;

    // One acquire snapshot of phase and both flags.
    FeatureView view() const noexcept;

    // Publish only after coherent native initialization; preparing -> ready once,
    // preserving preference.
    bool publish(bool battle = false) noexcept;

    // Call after completing the native root exchange. Publish both
    // flags together, only while ready. Retain the preference across BATTLE,
    // style and credit changes. A false result leaves retirement to the caller.
    bool commit_view(bool preference, bool battle) noexcept;

    // Verified ownership -> unavailable; otherwise -> uncertain. Uncertainty
    // survives later verification. Neither failure path revives stopped state.
    void fail(bool verified_native_ownership) noexcept;
    // Terminal from every phase, preserving the last flags.
    void stop() noexcept;

private:
    static constexpr std::uint32_t phase_mask = 0x7;
    static constexpr std::uint32_t preference_bit = 0x8;
    static constexpr std::uint32_t battle_bit = 0x10;
    std::atomic<std::uint32_t> word_{
        static_cast<std::uint32_t>(FeaturePhase::preparing) | preference_bit};
};
} // namespace point_zero::native
