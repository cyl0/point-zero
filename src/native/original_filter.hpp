#pragma once

#include "adapters.hpp"
#include "filter_hooks.hpp"
#include "original_filter_state.hpp"
#include "category_layout.hpp"
#include "category_summary.hpp"

namespace point_zero::native {
using FilterWorkingSetter = void (__fastcall*)(std::uintptr_t category, int difficulty, int player);

class OriginalFilter final {
public:
    using Mode = int (__fastcall*)();
    // Adapter catalogue remains immutable for this owner's lifetime.
    OriginalFilter(const AdapterContext&, Mode);
    OriginalFilter(const OriginalFilter&) = delete;
    OriginalFilter& operator=(const OriginalFilter&) = delete;
    void prepare(); // One attempt on the future native application thread.
    // Executes original exactly once. The caller chooses effective from the
    // published view or the destination of its serialized membership rebuild.
    // Destination ON may be prepared while the old view is OFF or BATTLE;
    // ordinary BATTLE applications must supply effective=false.
    // Shared preference follows the completed root exchange, externally.
    bool apply(const FilterApplyRequest&, NativeFilterApply original, bool effective);
    bool level(std::uintptr_t filter, std::uintptr_t candidate, bool& reject) noexcept;
    // At the admitted serialized browsing boundary, with category/bar consumers
    // excluded by the caller. Rebuild one native Original Filter category with
    // filtered=1; native owns membership, RANDOM, summary and saved-map handling.
    // style is its actual manager bank, also forwarded to native. An inactive
    // bank need not match the current UI style; candidates prove their own bank.
    // Matching apply callbacks use this destination policy even while the old
    // view is published. Other categories retain their caller-supplied policy.
    // No direct editor/map writes or FeatureState publication. Native resets
    // the bar cursor; the enclosing toggle must restore its selected tuple and
    // refresh its graph/presentation.
    // Rejected preflight has no effects;
    // a failed native completion skips this operation without closing later calls.
    // With a setter, require the exact native summary-return callback and repair
    // working rows/header/RANDOM while the target's filter is still installed.
    // The caller connects summary_return through the existing summary hook.
    bool rebuild(std::uintptr_t category, int style, NativeFilterRebuild, bool effective,
                 FilterWorkingSetter = nullptr) noexcept;
    void summary_return(const CategorySummaryRequest&, std::uintptr_t caller, bool completed) noexcept;
private:
    struct Frame {
        FilterApplyRequest request;
        OriginalFilterState owner;
        bool eligible{}, used{}, failed{}, querying{};
    };
    AdapterContext context_;
    Mode mode_;
    Frame* scope_{};
    struct Rebuild {
        std::uintptr_t category{}, manager{}, list{}, active{}, saved{};
        int id{}, style{}, mode{}, inserted{};
        std::array<int, 2> difficulty{};
        category_layout::Vector charts{};
        FeatureView view{};
        bool effective{};
        FilterWorkingSetter setter{};
        OriginalFilterState filter;
        bool working_done{}, working_busy{};
    };
    Rebuild* rebuilding_{}; // Borrowed stack scope, never a second session preference.
    std::uint32_t thread_{};
    bool attempted_{}, prepared_{};
    void begin(Frame&, bool effective) noexcept;
    bool query(Frame&, std::uintptr_t filter, std::uintptr_t candidate, bool& reject) noexcept;
    bool rebuild_owner(Rebuild&) const;
    bool rebuild_call(Rebuild&, NativeFilterRebuild);
    bool rebuild_checked(std::uintptr_t, int, NativeFilterRebuild, bool, FilterWorkingSetter) noexcept;
    void restore_working(const CategorySummaryRequest&, std::uintptr_t, bool);
    void working_checked(const CategorySummaryRequest&, std::uintptr_t, bool) noexcept;
    std::atomic_flag warned_{};
    void fault() noexcept;
};
} // namespace point_zero::native
