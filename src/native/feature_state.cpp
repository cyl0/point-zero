#include "feature_state.hpp"

namespace point_zero::native {
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

FeatureView FeatureState::view() const noexcept {
    const auto word = word_.load(std::memory_order_acquire);
    return {static_cast<FeaturePhase>(word & phase_mask),
            (word & preference_bit) != 0, (word & battle_bit) != 0};
}

bool FeatureState::publish(bool battle) noexcept {
    auto word = word_.load(std::memory_order_relaxed);
    while (static_cast<FeaturePhase>(word & phase_mask) == FeaturePhase::preparing) {
        const auto next = static_cast<std::uint32_t>(FeaturePhase::ready) |
                          (word & preference_bit) | (battle ? battle_bit : 0U);
        if (word_.compare_exchange_weak(word, next, std::memory_order_release,
                                       std::memory_order_relaxed)) return true;
    }
    return false;
}

bool FeatureState::commit_view(bool preference, bool battle) noexcept {
    auto word = word_.load(std::memory_order_relaxed);
    const auto next = static_cast<std::uint32_t>(FeaturePhase::ready) |
                      (preference ? preference_bit : 0U) | (battle ? battle_bit : 0U);
    while (static_cast<FeaturePhase>(word & phase_mask) == FeaturePhase::ready) {
        if (word_.compare_exchange_weak(word, next, std::memory_order_release,
                                       std::memory_order_relaxed)) return true;
    }
    return false;
}

void FeatureState::fail(bool verified_native_ownership) noexcept {
    auto word = word_.load(std::memory_order_relaxed);
    for (;;) {
        const auto phase = static_cast<FeaturePhase>(word & phase_mask);
        if (phase == FeaturePhase::stopped || phase == FeaturePhase::uncertain) return;
        const auto next = (word & ~phase_mask) | static_cast<std::uint32_t>(
            verified_native_ownership ? FeaturePhase::unavailable : FeaturePhase::uncertain);
        if (word_.compare_exchange_weak(word, next, std::memory_order_release,
                                       std::memory_order_relaxed)) return;
    }
}

void FeatureState::stop() noexcept {
    auto word = word_.load(std::memory_order_relaxed);
    while (static_cast<FeaturePhase>(word & phase_mask) != FeaturePhase::stopped) {
        const auto next = (word & ~phase_mask) | static_cast<std::uint32_t>(FeaturePhase::stopped);
        if (word_.compare_exchange_weak(word, next, std::memory_order_release,
                                       std::memory_order_relaxed)) return;
    }
}
} // namespace point_zero::native
