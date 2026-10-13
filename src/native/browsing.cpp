#include "build_bindings.hpp"
#include "browsing.hpp"
#include "category_accessors_contract.hpp"
#include "category_badges_contract.hpp"
#include "category_lifecycle_contract.hpp"
#include "category_metadata_contract.hpp"
#include "category_navigation_contract.hpp"
#include "category_request_contract.hpp"
#include "category_request_sites_contract.hpp"
#include "category_storage_contract.hpp"
#include "category_summary_contract.hpp"
#include "category_vector_contract.hpp"
#include "filter_contract.hpp"
#include "sort_contract.hpp"

#include <windows.h>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
struct Admission {
    CategoryMetadataSite metadata;
    CategoryAccessorSites accessors;
    CategorySummarySite summary;
    CategoryNavigationContract navigation;
    CategoryLifeSites lifecycle;
    CategoryRequestSites requests;
    SortContract sort;
    FilterContract filter;
    CategoryGenerationCalls calls;
};
Admission admit(const AdapterContext& context) {
    const auto memory = context.memory;
    const auto& modules = context.modules;
    const auto& game = modules.game;
    Admission out;
    out.metadata = admit_category_metadata_contract(memory, game, modules.playlister);
    out.accessors = admit_category_accessors_contract(memory, game, modules.playlister);
    out.summary = admit_category_summary_contract(memory, game);
    out.navigation = admit_category_navigation_contract(memory, game);
    out.lifecycle = admit_category_lifecycle_contract(memory, game, modules.playlister);
    out.requests = admit_category_request_sites(memory, game, modules.fervidex);
    out.sort = admit_sort_contract(memory, game);
    out.filter = admit_filter_contract(memory, game);
    auto& calls = out.calls;
    calls.storage = admit_category_storage_contract(memory, game);
    calls.badges = admit_category_badges_contract(memory, game);
    calls.vectors = admit_category_vector_contract(memory, game);
    calls.selection = admit_category_restore_contract(memory, game);
    calls.requests = admit_category_request_calls(memory, game);
    require(out.filter.mode == out.sort.calls.mode, "browsing mode contracts disagree");
    calls.mode = out.filter.mode;
    calls.rebuild = out.filter.rebuild;
    calls.close = out.navigation.calls.close;
    calls.bank.set_difficulty = out.navigation.calls.set_difficulty;
    // These entry addresses are explicitly covered by the contracts above.
    // Call the game entry (including its admitted PL route), not a trampoline
    // that would bypass our private lookup/summary/restore routing.
    calls.lookup = reinterpret_cast<decltype(calls.lookup)>(game.range.base + game_bindings(game.range).browsing.lookup_rva);
    calls.bank.calculate = reinterpret_cast<decltype(calls.bank.calculate)>(out.summary.address);
    calls.bank.sort = reinterpret_cast<decltype(calls.bank.sort)>(game.range.base + game_bindings(game.range).browsing.bank_sort_rva);
    calls.chart_difficulty = reinterpret_cast<decltype(calls.chart_difficulty)>(game.range.base + game_bindings(game.range).browsing.chart_difficulty_rva);
    // Complete lifecycle supporting bodies admit these native style/input calls.
    calls.style = reinterpret_cast<decltype(calls.style)>(game.range.base + game_bindings(game.range).browsing.style_rva);
    calls.options = reinterpret_cast<decltype(calls.options)>(game.range.base + game_bindings(game.range).browsing.options_rva);
    calls.battle = reinterpret_cast<decltype(calls.battle)>(game.range.base + game_bindings(game.range).browsing.battle_rva);
    calls.keypad = reinterpret_cast<decltype(calls.keypad)>(game.range.base + game_bindings(game.range).browsing.keypad_rva);
    calls.modal = reinterpret_cast<decltype(calls.modal)>(game.range.base + game_bindings(game.range).browsing.modal_rva);
    calls.list_state = reinterpret_cast<decltype(calls.list_state)>(game.range.base + game_bindings(game.range).browsing.list_state_rva);
    calls.reset_list = reinterpret_cast<decltype(calls.reset_list)>(game.range.base + game_bindings(game.range).browsing.reset_list_rva);
    return out;
}
} // namespace

NativeBrowsing::NativeBrowsing(const AdapterContext& context) noexcept : context_(context) {}
NativeBrowsing::~NativeBrowsing() noexcept { retire_callbacks(); }
void NativeBrowsing::preparing() const {
    require(!retired_ && thread_ == GetCurrentThreadId() &&
            context_.state.view().phase == FeaturePhase::preparing,
            "browsing preparation stopped or left its native thread");
}
void NativeBrowsing::prepare(std::span<const CategoryMetadataEntry> definitions) {
    require(!prepare_attempted_, "browsing preparation already attempted");
    prepare_attempted_ = true;
    thread_ = GetCurrentThreadId();
    preparing();
    std::array<int, CategoryBank::per_style> ids{};
    require(definitions.size() == ids.size(), "incomplete private LEVEL definitions");
    for (std::size_t n = 0; n < ids.size(); ++n) {
        ids[n] = CategoryBank::first_id + static_cast<int>(n);
        require(definitions[n].id == ids[n] && definitions[n].definition.difficulty_locked == 1,
                "invalid private LEVEL definition identity or difficulty policy");
    }
    // Every finite contract succeeds before constructing even disabled hooks.
    const auto checked = admit(context_);
    preparing();
    const auto memory = context_.memory;
    metadata_ = std::make_unique<CategoryMetadata>(memory, checked.metadata, definitions);
    std::array<CategoryMetadataEntry, CategoryBank::per_style> retained{};
    for (std::size_t n = 0; n < retained.size(); ++n) {
        const auto definition = metadata_->definition(ids[n]);
        require(definition != nullptr, "copied private LEVEL definition unavailable");
        retained[n] = {ids[n], *definition};
    }
    accessors_ = std::make_unique<CategoryAccessors>(memory, checked.accessors, ids);
    navigation_ = std::make_unique<CategoryNavigation>(memory, context_.modules.game.range,
        context_.state, checked.navigation.sites, checked.navigation.calls);
    filter_ = std::make_unique<OriginalFilter>(context_, checked.filter.mode);
    generations_ = std::make_unique<CategoryGenerations>(context_, checked.calls, retained,
        *accessors_, *navigation_, *filter_);
    auto owner = sort_owner(); owner.context = this;
    sorter_ = std::make_unique<DifficultySort>(context_, checked.sort.calls, owner);
    filter_->prepare();
    generations_->prepare();
    sorter_->prepare();
    filter_hooks_ = std::make_unique<FilterHooks>(memory, checked.filter.sites, this, filter_callbacks());
    sort_hook_ = std::make_unique<SortHook>(memory, checked.sort.hook, this, sort_callback());
    summary_ = std::make_unique<CategorySummary>(memory, checked.summary, ids,
        generations_.get(), CategoryGenerations::summary_callbacks());
    requests_ = std::make_unique<CategoryRequestHooks>(memory, checked.requests, generations_.get(),
        CategoryGenerations::request_callback(), CategoryGenerations::request_failure_callback());
    lifecycle_ = std::make_unique<CategoryLifecycleHooks>(memory, checked.lifecycle,
        generations_.get(), CategoryGenerations::lifecycle_callbacks());
    preparing();
    prepared_ = true;
}
void NativeBrowsing::activate() {
    require(!activation_attempted_, "browsing activation already attempted");
    activation_attempted_ = true;
    preparing();
    require(prepared_, "browsing contracts and owners have not been prepared");
    const auto arm = [this](std::size_t index, auto& hook) {
        arms_[index] = ArmState::attempted;
        hook->arm();
        arms_[index] = ArmState::armed;
        preparing();
    };
    arm(0, metadata_);
    arm(1, accessors_);
    arm(2, filter_hooks_);
    arm(3, sort_hook_);
    arm(4, summary_);
    arm(5, navigation_);
    arm(6, requests_);
    arm(7, lifecycle_);
}
bool NativeBrowsing::intact() const noexcept {
    return (arms_[0] == ArmState::unattempted || (arms_[0] == ArmState::armed && metadata_->intact())) &&
           (arms_[1] == ArmState::unattempted || (arms_[1] == ArmState::armed && accessors_->intact())) &&
           (arms_[2] == ArmState::unattempted || (arms_[2] == ArmState::armed && filter_hooks_->intact())) &&
           (arms_[3] == ArmState::unattempted || (arms_[3] == ArmState::armed && sort_hook_->intact())) &&
           (arms_[4] == ArmState::unattempted || (arms_[4] == ArmState::armed && summary_->intact())) &&
           (arms_[5] == ArmState::unattempted || (arms_[5] == ArmState::armed && navigation_->intact())) &&
           (arms_[6] == ArmState::unattempted || (arms_[6] == ArmState::armed && requests_->intact())) &&
           (arms_[7] == ArmState::unattempted || (arms_[7] == ArmState::armed && lifecycle_->intact()));
}
bool NativeBrowsing::toggle_available() const noexcept {
    return thread_ == GetCurrentThreadId() && !retired_ && generations_ &&
        generations_->toggle_available();
}
bool NativeBrowsing::rollback() noexcept {
    retire_callbacks();
    return context_.state.view().phase == FeaturePhase::preparing && intact();
}
void NativeBrowsing::retire_callbacks() noexcept {
    if (retired_) return;
    retired_ = true;
    // Entered outer transactions may still require every inner callback.
    // Metadata and accessor tables own their data and remain process-resident.
    if (lifecycle_) lifecycle_->retire_callbacks();
    if (requests_) requests_->retire_callbacks();
    if (navigation_) navigation_->retire_callbacks();
    if (summary_) summary_->retire_callbacks();
    if (sort_hook_) sort_hook_->retire_callback();
    if (filter_hooks_) filter_hooks_->retire_callbacks();
}
SortOwnedCategory NativeBrowsing::sort_owner() noexcept {
    return {nullptr, [](void* opaque, std::uintptr_t category, int id, int style, std::uintptr_t list) noexcept {
        return static_cast<NativeBrowsing*>(opaque)->generations_->owns(category, id, style, list);
    }};
}
SortHook::Callback NativeBrowsing::sort_callback() noexcept {
    return [](void* opaque, const SortRequest& request, NativeQSort original) noexcept {
        auto& self = *static_cast<NativeBrowsing*>(opaque);
        return self.sorter_->sort(request, original, self.generations_->effective());
    };
}
FilterHooks::Callbacks NativeBrowsing::filter_callbacks() noexcept {
    return {
        [](void* opaque, const FilterApplyRequest& request, NativeFilterApply original) {
            auto& self = *static_cast<NativeBrowsing*>(opaque);
            return self.filter_->apply(request, original, self.generations_->effective());
        },
        [](void* opaque, std::uintptr_t filter, std::uintptr_t candidate, bool& reject) noexcept {
            return static_cast<NativeBrowsing*>(opaque)->filter_->level(filter, candidate, reject);
        }
    };
}
} // namespace point_zero::native
