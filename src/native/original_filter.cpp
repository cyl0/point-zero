#include "build_bindings.hpp"
#include "original_filter.hpp"

#include <windows.h>
#include <stdexcept>

namespace point_zero::native {
namespace {
enum class FilterDecision { native, accept, reject, uncertain };
struct Query {
    std::uintptr_t music{};
    int mapped{}, song{}, lower{}, upper{};
};
std::optional<CatalogRow> chart(const Catalog& catalog, int song, int mapped) noexcept {
    const auto* row = catalog.find({song, mapped / 5, mapped % 5});
    return row ? std::optional(*row) : std::nullopt;
}
bool observe(MemoryView memory, std::uintptr_t filter, std::uintptr_t candidate, Query& out) noexcept {
    return read_field(memory, candidate, 0, out.music) && read_field(memory, candidate, 8, out.mapped) &&
        out.mapped >= 0 && out.mapped < 10 &&
        read_field(memory, out.music, 0x67C, out.song) && out.song > 0 &&
        read_field(memory, filter, 16, out.lower) && read_field(memory, filter, 20, out.upper);
}
FilterDecision run(const AdapterContext& context, const OriginalFilterState& owner,
                   std::uintptr_t filter, std::uintptr_t candidate) noexcept {
    if (!owner.enabled || filter != owner.object) return FilterDecision::native;
    Query observed;
    if (!observe(context.memory, filter, candidate, observed)) return FilterDecision::uncertain;
    if (observed.mapped / 5 != owner.style) return FilterDecision::native;
    // Resolve the current native candidate in the immutable catalogue.
    const auto row = chart(context.catalog, observed.song, observed.mapped);
    // Explicit presence matters. A dormant supplied rating cannot create a
    // native chart, and native-only charts retain the exact stock predicate.
    if (!row || !row->assigned) return FilterDecision::native;
    const int level = row->effective_tenths / 10;
    return level < observed.lower || level > observed.upper ? FilterDecision::reject : FilterDecision::accept;
}
} // namespace
OriginalFilter::OriginalFilter(const AdapterContext& context, Mode mode) : context_(context), mode_(mode) {
    if (!context.memory.read || !mode) throw std::invalid_argument("incomplete Original Filter dependencies");
}
void OriginalFilter::prepare() {
    if (attempted_) throw std::runtime_error("Original Filter preparation already attempted");
    attempted_ = true;
    if (context_.state.view().phase != FeaturePhase::preparing)
        throw std::runtime_error("Original Filter preparation outside native initialization");
    thread_ = GetCurrentThreadId(); prepared_ = true;
}
void OriginalFilter::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !context_.host.log) return;
    constexpr std::string_view message = "Original Filter evaluation skipped a native operation; later callbacks remain enabled";
    try { context_.host.log(context_.host.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
void OriginalFilter::begin(Frame& frame, bool effective) noexcept {
    __try {
        const auto view = context_.state.view();
        // The caller supplies the destination policy during a serialized
        // rebuild. The old BATTLE/preference flags stay published until that
        // transaction completes; they must not veto its explicit target.
        if (!effective || view.phase != FeaturePhase::ready) return;
        const auto game = context_.modules.game.range;
        const auto& request = frame.request;
        if (game.base < 0x10000 || !contains(game.base, game.size, game.base, game.size) ||
            game.size < game_bindings(context_.modules.game.range).original_filter.filter_candidate_state_rva + 12 || request.caller != game.base + game_bindings(context_.modules.game.range).original_filter.filter_apply_caller_rva ||
            request.set != 1 || request.style < 0 || request.style > 1 ||
            !(request.category == 119 || (request.category >= 219 && request.category <= 223)) ||
            request.candidate != game.base + game_bindings(context_.modules.game.range).original_filter.filter_candidate_state_rva) return;
        const auto mode = mode_();
        if (mode != 0 && mode != 6) return;
        std::uintptr_t manager{};
        if (!read_field(context_.memory, game.base, game_bindings(context_.modules.game.range).original_filter.category_manager_rva, manager) || manager < 0x10000) return;
        const auto offset = 16 + 208 * (226ULL * request.style + request.category);
        if (offset > UINTPTR_MAX - manager ||
            !read_original_filter_state(context_.memory, game, manager + offset, request.style, frame.owner) ||
            request.owner != frame.owner.active || !frame.owner.enabled) return;
        frame.eligible = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { frame.failed = true; fault(); }
}
bool OriginalFilter::apply(const FilterApplyRequest& request, NativeFilterApply original, bool effective) {
    if (!original) { fault(); return true; }
    if (!prepared_ || thread_ != GetCurrentThreadId()) return original(request.owner, request.set, request.candidate);
    if (rebuilding_ && request.category == rebuilding_->id && request.style == rebuilding_->style)
        effective = rebuilding_->effective;
    Frame frame; frame.request = request;
    auto* previous = scope_;
    scope_ = &frame;
    bool completed{}, result{};
    __try {
        begin(frame, effective);
        result = original(request.owner, request.set, request.candidate);
        completed = true;
    } __finally {
        if (!completed && frame.used) fault();
        scope_ = previous;
    }
    return result;
}
bool OriginalFilter::query(Frame& frame, std::uintptr_t filter, std::uintptr_t candidate, bool& rejected) noexcept {
    if (candidate != frame.request.candidate || filter != frame.owner.object) return false;
    const auto decision = run(context_, frame.owner, filter, candidate);
    if (decision == FilterDecision::uncertain) { frame.failed = true; fault(); return false; }
    if (decision == FilterDecision::native) return false;
    rejected = decision == FilterDecision::reject;
    frame.used = true;
    return true;
}
bool OriginalFilter::level(std::uintptr_t filter, std::uintptr_t candidate, bool& rejected) noexcept {
    if (!prepared_ || thread_ != GetCurrentThreadId() || !scope_ || !scope_->eligible ||
        scope_->failed || scope_->querying) return false;
    auto& frame = *scope_;
    frame.querying = true;
    __try {
        __try { return query(frame, filter, candidate, rejected); }
        __except (EXCEPTION_EXECUTE_HANDLER) { frame.failed = true; fault(); return false; }
    } __finally { frame.querying = false; }
}

bool OriginalFilter::rebuild_owner(Rebuild& out) const {
    const auto memory = context_.memory;
    const auto game = context_.modules.game.range;
    const auto pointer = [](std::uintptr_t at) { return at >= 0x10000 && !(at & 7); };
    if (!pointer(out.category) || out.style < 0 || out.style > 1 ||
        game.base < 0x10000 || !contains(game.base, game.size, game.base, game.size) || game.size < game_bindings(context_.modules.game.range).original_filter.filter_state_end_rva)
        return false;
    out.view = context_.state.view();
    if (out.view.phase != FeaturePhase::ready) return false;
    out.mode = mode_();
    if (out.mode != 0 && out.mode != 6) return false;
    std::uintptr_t vtable{};
    int menu_id{}, menu_set{};
    std::uint8_t has_set{};
    if (!read_field(memory, out.category, 0, out.id) ||
        !(out.id == 119 || (out.id >= 219 && out.id <= 223)) ||
        !read_field(memory, game.base, game_bindings(context_.modules.game.range).original_filter.category_manager_rva, out.manager) || !pointer(out.manager)) return false;
    const auto offset = 16 + 208 * (226ULL * out.style + out.id);
    if (offset > UINTPTR_MAX - out.manager || out.category != out.manager + offset ||
        !read_field(memory, out.manager, 0x16F50, out.list) || !pointer(out.list) ||
        !read_field(memory, out.list, 0, vtable) || vtable != game.base + game_bindings(context_.modules.game.range).original_filter.list_vtable_rva ||
        !read_field(memory, out.category, 160, menu_id) || menu_id != out.id ||
        !read_field(memory, out.category, 164, menu_set) || menu_set != 1 ||
        // Rebuilding mutates native membership: retain the stricter canonical
        // set1 byte (1). Read-only state capture accepts native nonzero flags.
        !read_field(memory, out.category, 168, has_set) || has_set != 1 ||
        !read_field(memory, out.category, 0x10, out.inserted) || out.inserted < 0 ||
        !read_field(memory, out.category, 0x18, out.difficulty) ||
        !read_field(memory, out.category, 0x78, out.charts) ||
        !read_field(memory, game.base, game_bindings(context_.modules.game.range).original_filter.active_filter_rva, out.active) || !pointer(out.active) ||
        !read_field(memory, game.base, game_bindings(context_.modules.game.range).original_filter.saved_filter_rva, out.saved) || !pointer(out.saved)) return false;
    for (const int difficulty : out.difficulty) if (difficulty < 0 || difficulty > 4) return false;
    const auto& v = out.charts;
    if (v != category_layout::Vector{} &&
        (!pointer(v.begin) || v.begin > v.end || v.end > v.capacity ||
         (v.end - v.begin) % sizeof(category_layout::Chart) ||
         (v.capacity - v.begin) % sizeof(category_layout::Chart))) return false;
    return true;
}
bool OriginalFilter::rebuild_call(Rebuild& request, NativeFilterRebuild original) {
    bool completed{}, stable{};
    rebuilding_ = &request;
    __try {
        original(request.category, 1, request.style);
        completed = true;
        stable = !request.setter || request.working_done;
        return stable;
    } __finally {
        rebuilding_ = nullptr;
        if (!completed || !stable) fault();
    }
}
bool OriginalFilter::rebuild_checked(std::uintptr_t category, int style, NativeFilterRebuild original,
        bool effective, FilterWorkingSetter setter) noexcept {
    try {
        Rebuild request; request.category = category; request.style = style; request.effective = effective;
        request.setter = setter;
        if (!rebuild_owner(request)) return false;
        if (setter && (read_saved_original_filter_state(context_.memory, context_.modules.game.range,
                category, style, request.filter) != SavedFilterResult::ready || !request.filter.enabled)) return false;
        return rebuild_call(request, original);
    } catch (...) { return false; }
}
bool OriginalFilter::rebuild(std::uintptr_t category, int style, NativeFilterRebuild original,
        bool effective, FilterWorkingSetter setter) noexcept {
    if (!prepared_ || thread_ != GetCurrentThreadId() || !original || scope_ || rebuilding_) return false;
    __try { return rebuild_checked(category, style, original, effective, setter); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void OriginalFilter::restore_working(const CategorySummaryRequest& summary, std::uintptr_t caller, bool completed) {
    auto* const request = rebuilding_;
    if (!request || !request->setter || caller != context_.modules.game.range.base + game_bindings(context_.modules.game.range).original_filter.filter_restore_caller_rva ||
        summary.manager != request->manager || summary.style != request->style || summary.id != request->id) return;
    if (!completed || request->working_done || request->working_busy) return;
    request->working_busy = true;
    __try {
        for (int side = 0; side < 2; ++side) {
            request->setter(request->category, request->difficulty[side], side);
        }
        request->working_done = true;
    } __finally { request->working_busy = false; }
}
void OriginalFilter::working_checked(const CategorySummaryRequest& request, std::uintptr_t caller, bool completed) noexcept {
    try { restore_working(request, caller, completed); }
    catch (...) { fault(); }
}
void OriginalFilter::summary_return(const CategorySummaryRequest& request, std::uintptr_t caller, bool completed) noexcept {
    if (!prepared_ || thread_ != GetCurrentThreadId() || !rebuilding_) return;
    __try { working_checked(request, caller, completed); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
} // namespace point_zero::native
