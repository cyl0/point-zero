#pragma once

#include "category_roots.hpp"
#include "feature_state.hpp"

namespace point_zero::native {
struct CategoryNavigationSite { std::uintptr_t address{}; std::vector<std::byte> body; };
struct CategoryNavigationCalls {
    std::uintptr_t (__fastcall* close)(std::uintptr_t navigation, std::uint8_t sound){};
    std::uintptr_t (__fastcall* sound)(int id){};
    void (__fastcall* restore_chart)(std::uintptr_t chart){}; // Native5F7BE0, original working pair.
    void (__fastcall* set_difficulty)(std::uintptr_t category, int difficulty, int player){}; // Native5FDBD0.
};
struct CategoryNavigationSites {
    CategoryNavigationSite open, cycle, exact, back;
    std::uintptr_t cycle_caller{}; // Native closed-heading branch, after list vtable+8.
    std::uintptr_t back_continuation{}; // Native event/presentation completion, after short-key release.
};

// Native parent entry, Back and exact chart selection for one borrowed root/page owner.
// Open(603900), list cycle(63A940), exact selection(5FD520) and the short-black-key
// release branch(60451E) are intercepted. The existing input dispatcher supplies
// white-key priority/held-key policy and consumes native events 3/4/5.
// Private leaves match original chart tuples rather than native's song-only
// search, then use native setters to restore that row. Unknown/currently unowned
// categories forward unchanged. No new input loop, refresh, summary calculation
// or preview restart. Request page routing remains the outer owner's work.
class CategoryNavigation final {
public:
    CategoryNavigation(MemoryView, ImageRange game, FeatureState&,
                       const CategoryNavigationSites&, CategoryNavigationCalls);
    ~CategoryNavigation() noexcept;
    CategoryNavigation(const CategoryNavigation&) = delete;
    CategoryNavigation& operator=(const CategoryNavigation&) = delete;
    void arm(); // One attempt on the native browsing thread; initially unbound.
    // At an admitted serialized scene boundary. The owner proves navigation
    // belongs to that scene and keeps roots/navigation alive through callbacks.
    // Category backing must also survive the enclosing native input transaction.
    // Both native styles have already been published. Failed bind keeps old
    // binding; replacement/unbind does not withdraw or destroy either root bank.
    bool bind(CategoryRoots&, std::uintptr_t navigation) noexcept;
    bool unbind() noexcept; // Arming thread, outside an entered operation; false leaves binding intact.
    bool intact() const noexcept;
    void retire_callbacks() noexcept; // Not from a native operation/callback.
private:
    struct State;
    State* state_{};
};
// Exposed forwarding remains resident. Retirement drains originals and custom
// work, then no longer reads borrowed roots, feature state or memory provider.
// This does not prove retirement of native consumers or authorize freeing banks.
// Native exceptions propagate; custom operation failures close the shared gate
// with uncertain ownership. The outer browsing owner supplies restart reporting,
// toggle/filter/return, request routing and pre-style-refresh handling separately.
} // namespace point_zero::native
