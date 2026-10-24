#pragma once

#include "adapters.hpp"
#include "category_lifecycle_hooks.hpp"
#include "category_navigation.hpp"
#include "category_restore_contract.hpp"
#include "category_requests.hpp"
#include "original_filter.hpp"

namespace point_zero::native {
struct CategoryGenerationCalls {
    CategoryNativeCalls storage;
    CategoryBankCalls bank;
    CategoryBadgeCalls badges;
    CategoryVectorCalls vectors;
    CategoryBank::Lookup lookup{};
    int (__fastcall* mode)(){};
    int (__fastcall* style)(){}; // 949570, requested normalized native style.
    std::uintptr_t (__fastcall* options)(){}; // 8854A0.
    std::uint8_t (__fastcall* battle)(std::uintptr_t options){}; // 897BB0, AL.
    std::uintptr_t (__fastcall* close)(std::uintptr_t navigation, std::uint8_t sound){}; // 603810.
    CategoryRestoreCalls selection;
    void (__fastcall* chart_difficulty)(std::uintptr_t chart, int difficulty, int player,
                                       int depth, std::uint8_t filtered){}; // 5F9B00.
    std::uint8_t (__fastcall* keypad)(unsigned key){}; // 800E70, native key number; AL fresh edge.
    int (__fastcall* modal)(){}; // 806F60, zero only when native modal UI is idle.
    CategoryRequestCalls requests;
    NativeFilterRebuild rebuild{};
    std::uintptr_t (__fastcall* list_state)(){}; // 7FB4B0, inline entrance/scroll state.
    void (__fastcall* reset_list)(std::uintptr_t state, int){}; // 7FB460, full native reset.
};

// Browsing's concrete bank/root owner. All code, metadata and native calls must
// already be admitted; accessor/navigation hooks are armed by the enclosing
// adapter. Definitions point to the retained CategoryMetadata strings.
// No native calls at construction or destruction. Keypad8 uses the existing
// admitted native tick, not a separate input loop or repeat timer.
// Retire all borrowing callbacks before destruction; live/uncertain banks and
// vector backing are retained by their existing owners, never freed at shutdown.
class CategoryGenerations final {
public:
    CategoryGenerations(const AdapterContext&, CategoryGenerationCalls,
                        std::span<const CategoryMetadataEntry>, CategoryAccessors&, CategoryNavigation&, OriginalFilter&);
    ~CategoryGenerations() noexcept;
    CategoryGenerations(const CategoryGenerations&) = delete;
    CategoryGenerations& operator=(const CategoryGenerations&) = delete;
    void prepare(); // One attempt on the future native browsing thread.

    static CategoryLifecycleHooks::Callbacks lifecycle_callbacks() noexcept;
    static CategorySummary::Callbacks summary_callbacks() noexcept;
    static CategoryRequestHooks::Route request_callback() noexcept;
    static CategoryRequestHooks::Failure request_failure_callback() noexcept;
    bool owns(std::uintptr_t category, int id, int style, std::uintptr_t list) const noexcept;
    // Read-only keypad8 eligibility on the admitted native thread; OFF is valid.
    // Does not sample input or authorize a later mutation.
    bool toggle_available() noexcept;

    // Policy for this owner's filter/sort callbacks. Entered native restoration
    // or BATTLE style application sees its destination; normal calls use the
    // shared view. This never publishes the shared preference prematurely.
    bool effective() const noexcept;

private:
    // AFTER the admitted graph bind687E20 returns. Read its ACTUAL saved
    // category, since mode guards can return without rebinding. No allocation
    // or query. This records the reference; it does not collect/free a bank.
    // Unrelated native graphs pass through. An unknown graph holding a private
    // category conservatively prevents collection of the affected bank.
    void graph_bound(std::uintptr_t graph) noexcept;
    // AFTER actual selection-owner lifetime completion, not scene exit or
    // graph mode-map reset. A failed/unwound destructor retains its reference.
    void selection_destroyed(std::uintptr_t selection, bool completed) noexcept;

    // Only at an admitted outer native transaction completion, after callbacks
    // with register/local category copies have returned. Internal lifecycle
    // depth/summary checks are necessary but do NOT prove that outer boundary.
    // Removes only retired, unmapped banks with no live group or graph refs.
    // Native destructor failure retains that generation without retry.
    // True means the scan completed; referenced/uncertain banks may remain.
    bool collect() noexcept;
    struct Generation;
    struct Selection;
    struct Tick {
        Generation* generation{};
        std::uintptr_t scene{}, controller{}, manager{}, list{}, navigation{};
        FeatureView view{};
        int mode{}, style{}, dp{};
        std::array<int, 2> active{};
    };
    struct Graph { std::uintptr_t owner{}, category{}; };
    AdapterContext context_;
    CategoryGenerationCalls calls_;
    std::array<CategoryMetadataEntry, CategoryBank::per_style> definitions_{};
    CategoryAccessors& accessors_;
    CategoryNavigation& navigation_;
    OriginalFilter& filter_;
    LevelPopulation population_;
    std::vector<std::unique_ptr<Generation>> generations_;
    std::vector<Graph> graphs_;
    Generation* current_{}, *building_{}, *summary_{};
    std::uint32_t thread_{};
    unsigned depth_{}, initializing_depth_{};
    bool initialization_ready_{}, initialization_detached_{};
    std::uintptr_t initialization_manager_{}, initialization_list_{}, initialization_control_{};
    unsigned refreshing_depth_{};
    int refreshing_style_{};
    struct Refresh {
        Generation* generation{};
        FeatureView view{};
        std::array<int, 2> active{};
        int source{}, mode{}, group{}, target{-1};
        std::uint8_t open{};
        bool applied{}, restored{}, policy{}, level{};
    } refresh_;
    bool attempted_{}, prepared_{}, collecting_{};
    bool ready_attempted_{};
    Selection* restoring_{}; // Borrowed stack frame for one native restore.
    bool destination_{};
    Tick toggle_{}; // Identities only, from this outer tick's native input sample.
    bool changing_{};
    bool requesting_{};
    struct Return {
        Generation* generation{};
        FeatureView view{};
        std::array<int, 2> active{};
        int style{}, mode{}, dp{}, target{-1};
        bool closed{};
    } returning_;
    unsigned returning_depth_{};

    bool thread() const noexcept;
    std::atomic_flag warned_{};
    void fault(std::string_view message =
        "native browsing ownership uncertain; operation skipped; later callbacks remain enabled") noexcept;
    bool detach() noexcept;
    void reject_initialization() noexcept;
    bool initialize(std::uintptr_t navigation);
    bool build_bank(Generation&);
    enum class Exchange { unchanged, published, withdrawn, uncertain };
    struct ExchangeResult {
        Exchange mutation;
        bool completed;
        explicit operator bool() const noexcept { return completed; }
    };
    ExchangeResult publish_roots(Generation&);
    Exchange withdraw_roots(Generation&) noexcept;
    void retain_graphs(std::uintptr_t category = 0) noexcept;
    void recover_failure(const CategoryLifeRequest&, CategoryLifeOutcome) noexcept;
    bool invalidate_refresh_cache(std::uintptr_t scene) noexcept;
    bool repair_style(Generation&);
    bool tick_context(std::uintptr_t scene, Tick&);
    void signal_ready(std::uintptr_t scene) noexcept;
    void sample_toggle(const CategoryLifeRequest&);
    void finish_toggle(const CategoryLifeRequest&, CategoryLifeOutcome);
    bool change_preference(Selection&, bool);
    bool change_checked(Selection&, bool) noexcept;
    bool route_return(const CategoryLifeRequest&);
    bool returned();
    bool before_refresh(const CategoryLifeRequest&);
    bool after_refresh();
    bool refresh_context(int style);
    bool before_style(const CategoryLifeRequest&);
    bool after_style(const CategoryLifeRequest&);
    bool route_style(const CategoryLifeRequest&);
    bool capture(Selection&);
    CategoryRequestResult route_request(const CategoryRequest&);
    CategoryRequestResult request_checked(const CategoryRequest&) noexcept;
    bool request_current(const Selection&);
    void refresh_filters(Generation&, int style, int skip, bool effective, std::uintptr_t protected_category);
    bool close_request(Selection&);
    bool close_leaf(std::uintptr_t navigation, std::uintptr_t category);
    void restore_current(Selection&, bool);
    void restore_working(const CategorySummaryRequest&, std::uintptr_t caller, bool completed);
    void restore_working_checked(const CategorySummaryRequest&, std::uintptr_t caller, bool completed) noexcept;
    bool begin(const CategoryLifeRequest&);
    void finish(const CategoryLifeRequest&, CategoryLifeOutcome);
    bool begin_checked(const CategoryLifeRequest&) noexcept;
    void finish_checked(const CategoryLifeRequest&, CategoryLifeOutcome) noexcept;
    bool collect_checked() noexcept;
};
} // namespace point_zero::native
