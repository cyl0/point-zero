#pragma once

#include "category_generations.hpp"
#include "sort_hook.hpp"

namespace point_zero::native {
// Fixed browsing composition for the selected datecode. The final adapter
// supplies the qualified definitions/art and publishes the shared feature only
// after every required component is active. No scene, input or native function
// is executed by construction, prepare or activate.
class NativeBrowsing final {
public:
    explicit NativeBrowsing(const AdapterContext&) noexcept;
    ~NativeBrowsing() noexcept;
    NativeBrowsing(const NativeBrowsing&) = delete;
    NativeBrowsing& operator=(const NativeBrowsing&) = delete;
    NativeBrowsing(NativeBrowsing&&) = delete;
    NativeBrowsing& operator=(NativeBrowsing&&) = delete;

    // One attempt on the future native browsing thread, while preparing.
    // Exactly the144 ascending private IDs. Metadata copies all strings;
    // neither the supplied entries nor their strings are borrowed afterward.
    void prepare(std::span<const CategoryMetadataEntry>);
    // Arm the prepared prerequisites after package loading:
    // the fixed hooks from dependencies to outer lifecycle. No publication.
    void activate();
    // Serialized prepublication integrity check; does not publish readiness.
    bool intact() const noexcept;
    // Live keypad8 eligibility for guide callbacks on the admitted native thread.
    bool toggle_available() const noexcept;
    // Prepublication only. Drain first; an uncertain enable retains backing.
    // Does not undo native categories, unhook code, or destroy resident mappings.
    bool rollback() noexcept;
    // After startup has drained, never from a callback/native transaction.
    // Drains outer work before retiring its inner dependencies. Idempotent.
    void retire_callbacks() noexcept;

private:
    AdapterContext context_;
    std::unique_ptr<CategoryMetadata> metadata_;
    std::unique_ptr<CategoryAccessors> accessors_;
    std::unique_ptr<CategoryNavigation> navigation_;
    std::unique_ptr<OriginalFilter> filter_;
    std::unique_ptr<CategoryGenerations> generations_;
    std::unique_ptr<DifficultySort> sorter_;
    std::unique_ptr<FilterHooks> filter_hooks_;
    std::unique_ptr<SortHook> sort_hook_;
    std::unique_ptr<CategorySummary> summary_;
    std::unique_ptr<CategoryRequestHooks> requests_;
    std::unique_ptr<CategoryLifecycleHooks> lifecycle_;
    // metadata, accessors, filter, sort, summary, navigation, requests, lifecycle
    enum class ArmState { unattempted, attempted, armed };
    std::array<ArmState, 8> arms_{};
    std::uint32_t thread_{};
    bool prepare_attempted_{}, prepared_{}, activation_attempted_{}, retired_{};

    void preparing() const;
    static SortOwnedCategory sort_owner() noexcept;
    static SortHook::Callback sort_callback() noexcept;
    static FilterHooks::Callbacks filter_callbacks() noexcept;
};
} // namespace point_zero::native
