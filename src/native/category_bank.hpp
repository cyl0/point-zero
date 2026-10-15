#pragma once

#include "category_badges.hpp"
#include "category_metadata.hpp"
#include "category_summary.hpp"
#include "level_membership.hpp"

namespace point_zero::native {
struct CategoryBankCalls {
    std::intptr_t (__fastcall* calculate)(std::uintptr_t manager, int style, int id){};
    void (__fastcall* set_difficulty)(std::uintptr_t category, int difficulty, int player){};
    void (__fastcall* sort)(std::uintptr_t category, int order, std::uint8_t visible){};
};
enum class CategoryBankState { empty, ready, rejected, destroyed, uncertain };

class CategoryBank final {
public:
    using Lookup = std::uintptr_t (__fastcall*)(std::uintptr_t, int, int);
    static constexpr int first_id = 500, per_style = 144;
    static const CategoryDefinition* active_definition(std::uintptr_t manager, int id) noexcept;
    int base_id() const noexcept;
    bool owned(int id, int style = -1) const noexcept;
    int level(int id) const noexcept;
    int part(int id) const noexcept;
    int id(int level, int part) const noexcept;
    // Allocates only the bank table; slots/membership wait until build and are
    // sized from the captured population. Calls no game code. definitions has the
    // exact144 ascending IDs with locked difficulty and retained art strings.
    CategoryBank(MemoryView, CategoryNativeCalls, CategoryBankCalls, CategoryBadgeCalls,
                 std::span<const CategoryMetadataEntry> definitions);
    ~CategoryBank() noexcept; // Never invokes a native destructor automatically.
    CategoryBank(const CategoryBank&) = delete;
    CategoryBank& operator=(const CategoryBank&) = delete;

    // One attempt, at the admitted ordinary population/list boundary. The
    // caller has prepared population on this native thread and excluded BATTLE.
    // Capture both native banks; construct, summarize, restore native preferences,
    // TITLE-sort visible rows and prepare optional badges from the captured source.
    // This does not publish groups, bind accessors or commit FeatureState.
    bool build(std::uintptr_t manager, std::uintptr_t list, LevelPopulation&, Lookup) noexcept;
    // Retry only a rejection before the first native storage call. No exposed
    // slot or uncertain construction may be reset this way.
    bool reset_rejected() noexcept;
    // Requires proof of NO consumers, including accessor mappings, summary
    // callbacks, current/delayed groups and graph owners. Never infer that proof
    // from OFF or shutdown. Uncertain native effects retain the whole backing.
    bool destroy() noexcept;
    CategoryBankState state() const noexcept;
    // Borrowed while this wrapper remains ready/alive; invalidated by destroy
    // or failure. Accessor installation copies values and has its own lifetime.
    std::span<const CategoryAccessBinding> bindings() const noexcept;
    // Prepared-owner check for the sorter: no allocation/page query/native call.
    // Native membership/working difficulties/cursors may change after publication;
    // the sorting operation separately validates the rows it consumes.
    bool owns(std::uintptr_t category, int id, int style, std::uintptr_t list) const noexcept;

    // Connect to the existing scoped CategorySummary hook before build. These
    // methods acknowledge the exact native operation; no summary algorithm is
    // duplicated here. Caller retires the hook callbacks before wrapper teardown.
    bool prepare_summary(const CategorySummaryRequest&, CategorySummaryBinding&) noexcept;
    void finish_summary(const CategorySummaryRequest&, const CategorySummaryBinding&, bool completed) noexcept;
private:
    struct Backing;
    std::unique_ptr<Backing> backing_;
    bool attempted_{}, busy_{};
    std::uint32_t thread_{};
    bool build_native(std::uintptr_t, std::uintptr_t, LevelPopulation&, Lookup);
    bool build_checked(std::uintptr_t, std::uintptr_t, LevelPopulation&, Lookup) noexcept;
    bool destroy_checked() noexcept;
};
} // namespace point_zero::native
