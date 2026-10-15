#include "build_bindings.hpp"
#include "category_generations.hpp"
#include "category_chart_match.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
bool pointer(std::uintptr_t at, std::size_t bytes = 0) noexcept {
    return at >= 0x10000 && !(at & 7) && bytes <= UINTPTR_MAX - at;
}
bool same_view(const FeatureView& a, const FeatureView& b) noexcept {
    return a.phase == b.phase && a.preference == b.preference && a.battle == b.battle;
}
bool filtered(int id) noexcept { return id == 110 || (id >= 210 && id <= 214); }
bool vector_fits(const category_layout::Vector& v, int count, std::size_t width) noexcept {
    if (count < 0 || count > 20003) return false;
    if (v == category_layout::Vector{}) return !count;
    return pointer(v.begin) && v.begin <= v.end && v.end <= v.capacity &&
        (v.end - v.begin) % width == 0 && (v.capacity - v.begin) % width == 0 &&
        static_cast<std::size_t>(count) <= (v.end - v.begin) / width;
}
template<class T> bool read(MemoryView m, std::uintptr_t at, T& out) noexcept {
    __try {
        return read_field(m, at, 0, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool browsing_mode(int mode) noexcept { return mode == 0 || mode == 6; }
bool native_level(std::uintptr_t manager, std::uintptr_t category, int id, int style) noexcept {
    return id >= 6 && id <= 17 && style >= 0 && style < 2 &&
        category == manager + 16 + sizeof(category_layout::Category) * (230ULL * style + id);
}
bool read_scene_owner(MemoryView memory, ImageRange game, std::uintptr_t& manager,
                      std::uintptr_t& list, std::uintptr_t expected_manager = 0,
                      std::uintptr_t expected_list = 0) noexcept {
    return read(memory, game.base + game_bindings(game).category_generations.category_manager_rva, manager) &&
        (!expected_manager || manager == expected_manager) && pointer(manager, 0x2A428) &&
        read(memory, manager + 0x175D0, list) &&
        (!expected_list || list == expected_list) && pointer(list, 0x88);
}
bool read_scene_styles(MemoryView memory, std::uintptr_t manager, std::uintptr_t list,
                       int& raw, int& normalized) noexcept {
    return read(memory, manager + 0x2A424, raw) && read(memory, list + 0x78, normalized);
}
bool closed_leaf(MemoryView memory, std::uintptr_t navigation, std::uintptr_t category) noexcept {
    std::uint8_t open{}, category_open{};
    return read(memory, navigation + 5, open) && !open &&
        read(memory, category + offsetof(category_layout::Category, open), category_open) && !category_open;
}
} // namespace

struct CategoryGenerations::Generation {
    CategoryBank bank;
    std::unique_ptr<CategoryRoots> roots;
    std::array<CategoryAccessBinding, 2 * CategoryBank::per_style> bindings{};
    std::uintptr_t manager{}, list{}, group{}, navigation{};
    bool mapped{}, navigation_bound{}, group_reference{}, uncertain{}, cleanup_attempted{};
    int repair_style{-1}; // Completed native style whose custom publication needs retry.

    Generation(const AdapterContext& c, CategoryGenerationCalls calls,
               std::span<const CategoryMetadataEntry> definitions)
        : bank(c.memory, calls.storage, calls.bank, calls.badges, definitions),
          roots(std::make_unique<CategoryRoots>(c.memory, c.modules.game.range, calls.vectors)) {}
    bool references(std::uintptr_t category) const noexcept {
        return category && std::any_of(bindings.begin(), bindings.end(), [category](const auto& b) {
            return b.category == category;
        });
    }
};

struct CategoryGenerations::Selection {
    Generation* generation{};
    FeatureView view{};
    std::uintptr_t manager{}, list{}, navigation{}, category{};
    int style{}, mode{}, group{}, dp{}, players{}, id{}, song{-1}, p1{-1}, p2{-1};
    std::array<int, 2> active{};
    std::array<int, 2> working{};
    category_layout::Vector charts{};
    std::uintptr_t chart_address{}, music{};
    int inserted{};
    std::uint8_t open{};
    bool chart{}, working_done{}, working_busy{};
};

CategoryGenerations::CategoryGenerations(const AdapterContext& context, CategoryGenerationCalls calls,
        std::span<const CategoryMetadataEntry> definitions, CategoryAccessors& accessors,
        CategoryNavigation& navigation, OriginalFilter& filter)
    : context_(context), calls_(calls), accessors_(accessors), navigation_(navigation), filter_(filter),
      population_(context.memory, context.modules.game.range, context.catalog) {
    const auto& s = calls.storage;
    const auto& b = calls.badges;
    const auto& r = calls.requests;
    if (!context.memory.read || !context.memory.query || !s.construct || !s.initialize || !s.assign_string ||
        !s.insert || !s.destroy || !s.auxiliary_vtable || !calls.bank.calculate || !calls.bank.set_difficulty ||
        !calls.bank.sort || !b.music_owner || !b.event_owner || !b.chart_at || !b.music_of || !b.weekly ||
        !b.recommend || !b.shop || !b.kac || !calls.vectors.grow || !calls.vectors.destroy || !calls.lookup ||
        !calls.mode || !calls.style || !calls.options || !calls.battle || !calls.close ||
        !calls.selection.current || !calls.selection.restore || !calls.chart_difficulty || !calls.keypad || !calls.modal || !calls.rebuild ||
        !calls.list_state || !calls.reset_list ||
        !r.settings || !r.persistent || !r.store || !r.save || !r.clear || !r.group || !r.sound ||
        definitions.size() != definitions_.size())
        throw std::invalid_argument("incomplete native category generation dependencies");
    for (std::size_t n = 0; n < definitions.size(); ++n) {
        if (definitions[n].id != definitions.front().id + static_cast<int>(n) ||
            definitions[n].definition.difficulty_locked != 1)
            throw std::invalid_argument("invalid native category generation definitions");
        definitions_[n] = definitions[n];
    }
}
CategoryGenerations::~CategoryGenerations() noexcept = default;
void CategoryGenerations::prepare() {
    if (attempted_) throw std::logic_error("category generations already prepared");
    attempted_ = true;
    thread_ = GetCurrentThreadId();
    population_.prepare();
    prepared_ = true;
}
bool CategoryGenerations::thread() const noexcept { return prepared_ && thread_ == GetCurrentThreadId(); }
void CategoryGenerations::fault(std::string_view message) noexcept {
    if (!warned_.test_and_set(std::memory_order_relaxed) && context_.host.log) {
        try { context_.host.log(context_.host.context, PZ_LOG_WARNING,
            {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
    }
}
bool CategoryGenerations::detach() noexcept {
    if (!current_) return true;
    auto& g = *current_;
    if (g.navigation_bound) {
        if (!navigation_.unbind()) { fault(); return false; }
        g.navigation_bound = false;
    }
    if (g.mapped) {
        auto empty = g.bindings;
        for (auto& binding : empty) binding.category = binding.summary = 0;
        if (!accessors_.bind(g.manager, empty)) { fault(); return false; }
        g.mapped = false;
    }
    current_ = nullptr;
    return true;
}
void CategoryGenerations::reject_initialization() noexcept {
    // Keep mappings available until the last exposed vectors are restored.
    // A refused bind has no navigation effect, but publication preceded it.
    if (current_) {
        auto& g = *current_;
        const auto roots = g.roots->state();
        if ((g.group_reference || g.navigation_bound) &&
            (g.uncertain || roots == CategoryRootsState::uncertain)) {
            g.uncertain = true;
            fault();
            return; // Possible exposure/rollback requires the resident routing owners.
        }
        if (roots == CategoryRootsState::published && withdraw_roots(g) != Exchange::withdrawn) {
            fault();
            return; // Only a completed withdrawal permits detachment; never replay uncertainty.
        }
    }
    fault();
    (void)detach();
}

bool CategoryGenerations::initialize(std::uintptr_t navigation) {
    const auto view = context_.state.view();
    if (view.phase != FeaturePhase::ready) return true;
    const int mode = calls_.mode();
    if (!browsing_mode(mode)) return true;
    std::uintptr_t manager{}, list{}, group{};
    int style{}, list_style{};
    const auto game = context_.modules.game.range;
    if (!read_scene_owner(context_.memory, game, manager, list) ||
        !read_scene_styles(context_.memory, manager, list, style, list_style) ||
        style < 0 || style > 2 || list_style != (style == 2 ? 0 : style)) return false;
    // The completed initializer has already installed the native style/banks.
    if (!find_level_group(context_.memory, game, list, group)) return false;
    for (const auto& old : generations_)
        if (old->group == group && old->group_reference) return false;

    auto owned = std::make_unique<Generation>(context_, calls_, definitions_);
    auto& g = *owned;
    g.manager = manager; g.list = list; g.group = group; g.navigation = navigation;
    generations_.push_back(std::move(owned));
    current_ = &g;
    if (view.preference && style != 2 && (!build_bank(g) || !publish_roots(g))) return false;
    return context_.state.commit_view(view.preference, style == 2);
}
bool CategoryGenerations::build_bank(Generation& g) {
    if (g.bank.state() == CategoryBankState::rejected && !g.uncertain)
        (void)g.bank.reset_rejected();
    if (g.bank.state() == CategoryBankState::ready) {
        if (!g.mapped) {
            if (!accessors_.bind(g.manager, g.bank.bindings())) return false;
            g.mapped = true;
        }
        return true;
    }
    if (g.bank.state() != CategoryBankState::empty || building_) return false;
    building_ = &g;
    const bool built = g.bank.build(g.manager, g.list, population_, calls_.lookup);
    building_ = nullptr;
    if (!built) { g.uncertain = g.bank.state() == CategoryBankState::uncertain; return false; }
    const auto bindings = g.bank.bindings();
    if (bindings.size() != g.bindings.size()) return false;
    std::copy(bindings.begin(), bindings.end(), g.bindings.begin());
    if (!accessors_.bind(g.manager, bindings)) return false;
    g.mapped = true;
    return true;
}
CategoryGenerations::ExchangeResult CategoryGenerations::publish_roots(Generation& g) {
    if (!g.mapped || g.navigation_bound || g.group_reference || g.uncertain)
        return {g.uncertain ? Exchange::uncertain : Exchange::unchanged, false};
    // OFF already returned these vectors to native and unbound navigation.
    // Reuse the category bank; only the small paired root owner is renewed.
    if (g.roots->state() == CategoryRootsState::rejected && !g.roots->discard()) {
        g.uncertain = g.uncertain || g.roots->state() == CategoryRootsState::uncertain;
        return {g.uncertain ? Exchange::uncertain : Exchange::unchanged, false};
    }
    if (g.roots->state() == CategoryRootsState::withdrawn)
        g.roots = std::make_unique<CategoryRoots>(context_.memory, context_.modules.game.range, calls_.vectors);
    if (g.roots->state() != CategoryRootsState::empty ||
        !g.roots->prepare(g.manager, g.list, g.group, g.bindings))
        return {g.roots->state() == CategoryRootsState::uncertain ? Exchange::uncertain : Exchange::unchanged, false};
    // publish can fail after a native vector write; completion alone authorizes
    // publication, never an inference that no group reference escaped.
    const bool published = g.roots->publish();
    const auto state = g.roots->state();
    g.group_reference = state == CategoryRootsState::published || state == CategoryRootsState::uncertain;
    if (!published) {
        g.uncertain = g.uncertain || state == CategoryRootsState::uncertain;
        return {state == CategoryRootsState::uncertain ? Exchange::uncertain : Exchange::unchanged, false};
    }
    if (!navigation_.bind(*g.roots, g.navigation)) {
        // Bind refusal preserves its old binding. Roll back only roots whose
        // publication completed; partial rollback retains the mappings.
        const auto withdrawn = withdraw_roots(g);
        return {withdrawn == Exchange::unchanged ? Exchange::published : withdrawn, false};
    }
    g.navigation_bound = true;
    return {Exchange::published, true};
}
CategoryGenerations::Exchange CategoryGenerations::withdraw_roots(Generation& g) noexcept {
    if (!g.roots->withdraw()) {
        if (g.roots->state() != CategoryRootsState::uncertain) return Exchange::unchanged;
        g.uncertain = true;
        return Exchange::uncertain;
    }
    g.group_reference = false;
    if (g.navigation_bound) {
        if (!navigation_.unbind()) { g.uncertain = true; return Exchange::uncertain; }
        g.navigation_bound = false;
    }
    return Exchange::withdrawn;
}

bool CategoryGenerations::tick_context(std::uintptr_t scene, Tick& out) {
    if (!current_ || current_->uncertain || changing_ || building_ || summary_ || restoring_ || returning_depth_ || refreshing_depth_ || requesting_ ||
        initializing_depth_ || !pointer(scene, 0x2011) || scene + 0x4C0 != current_->navigation) return false;
    out.generation = current_; out.scene = scene; out.navigation = current_->navigation;
    out.manager = current_->manager; out.list = current_->list; out.view = context_.state.view();
    if (out.view.phase != FeaturePhase::ready || out.view.battle) return false;
    out.mode = calls_.mode();
    if (!browsing_mode(out.mode)) return false;
    const auto game = context_.modules.game.range.base;
    std::uintptr_t manager{}, list{}, target{}, callback{}, queued{};
    int adjustment{}, style{};
    std::uint8_t enabled{}, confirm{}, busy{}, editor{}, open{}, category_open{};
    if (!read_scene_owner(context_.memory, context_.modules.game.range, manager, list, out.manager, out.list) ||
        !read_scene_styles(context_.memory, manager, list, out.style, style) ||
        out.style < 0 || out.style > 1 || style != out.style ||
        !read(context_.memory, game + game_bindings(context_.modules.game.range).category_generations.dp_flag_rva, out.dp) || out.dp != out.style ||
        !read(context_.memory, game + game_bindings(context_.modules.game.range).category_generations.active_flags_rva, out.active) ||
        !read(context_.memory, scene + 0x90, out.controller) || !pointer(out.controller, 0x52) ||
        !read(context_.memory, out.controller, target) || target != scene ||
        !read(context_.memory, out.controller + 8, callback) || callback != game + game_bindings(context_.modules.game.range).category_generations.idle_callback_rva ||
        !read(context_.memory, out.controller + 0x10, adjustment) || adjustment ||
        !read(context_.memory, out.controller + 0x38, queued) || queued ||
        !read(context_.memory, out.controller + 0x51, enabled) || !enabled ||
        !read(context_.memory, scene + 0x2010, confirm) || confirm ||
        !read(context_.memory, out.navigation + 6, busy) || busy ||
        !read(context_.memory, out.navigation + 5, open) || open > 1 || calls_.modal() != 0) return false;
    for (const auto active : out.active) if (active < 0 || active > 1) return false;
    if (!out.active[0] && !out.active[1]) return false;
    const auto category = calls_.selection.current(out.navigation);
    if (!pointer(category, sizeof(category_layout::Category)) ||
        !read(context_.memory, category + 4, category_open) || category_open != open ||
        !read(context_.memory, category + 0xAC, editor) || editor) return false;
    const auto& g = *out.generation;
    if (out.view.preference) {
        if (!g.mapped || !g.navigation_bound || !g.group_reference || !g.roots->current()) return false;
    } else if (g.navigation_bound || g.group_reference ||
               (g.roots->state() != CategoryRootsState::empty && g.roots->state() != CategoryRootsState::withdrawn &&
                g.roots->state() != CategoryRootsState::rejected)) return false;
    return current_ == out.generation && same_view(context_.state.view(), out.view) && calls_.mode() == out.mode;
}
bool CategoryGenerations::toggle_available() noexcept {
    if (!thread() || collecting_ || !current_ || current_->uncertain ||
        current_->navigation < 0x4C0) return false;
    __try {
        Tick tick;
        return tick_context(current_->navigation - 0x4C0, tick);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void CategoryGenerations::signal_ready(std::uintptr_t scene) noexcept {
    (void)scene;
    try {
        if (ready_attempted_ || !context_.host.ready) return;
        // The lifecycle contract proves this completed outer graphics boundary;
        // LEVEL ownership and keypad eligibility do not establish graphics readiness.
        if (!thread() || depth_ || context_.state.view().phase == FeaturePhase::stopped) return;
        ready_attempted_ = true; // Contain host reentry/throw; delivery is best-effort.
        context_.host.ready(context_.host.context);
    } catch (...) {} // Optional observation/UI failures must not close the feature gate.
}
void CategoryGenerations::sample_toggle(const CategoryLifeRequest& request) {
    if (depth_ != 1) return;
    toggle_ = {};
    const auto view = context_.state.view();
    if (!request.collection_boundary || !current_ || view.phase != FeaturePhase::ready || view.battle ||
        !calls_.keypad(8) || calls_.keypad(3)) return;
    // Native queries return the current sample's fresh edge, without consuming
    // it. Native3 wins even when its later, additional switching guards refuse.
    Tick tick;
    if (tick_context(request.owner, tick)) toggle_ = tick;
}
void CategoryGenerations::finish_toggle(const CategoryLifeRequest& request, CategoryLifeOutcome outcome) {
    const auto sampled = toggle_;
    toggle_ = {}; // Never retry/defer a request across ticks or native unwinding.
    if (!sampled.generation || outcome != CategoryLifeOutcome::completed || !request.collection_boundary ||
        request.owner != sampled.scene) return;
    Tick now;
    if (!tick_context(request.owner, now) || now.generation != sampled.generation ||
        now.controller != sampled.controller || now.manager != sampled.manager || now.list != sampled.list ||
        now.navigation != sampled.navigation || now.style != sampled.style || now.dp != sampled.dp ||
        now.mode != sampled.mode || now.active != sampled.active || !same_view(now.view, sampled.view)) return;
    Selection selected;
    if (!capture(selected) || selected.generation != sampled.generation || !same_view(selected.view, sampled.view)) return;
    bool okay{};
    changing_ = true;
    __try { okay = change_checked(selected, !sampled.view.preference); }
    __finally {
        changing_ = false;
        if (!okay) fault();
    }
    // The native tick has finished and the new view/selection is committed.
    // Reuse SP/DP's complete entrance and scroll reset without changing style.
    // Entry/result restoration already starts this transition in native code.
    if (okay || context_.state.view().preference != sampled.view.preference)
        calls_.reset_list(calls_.list_state(), 0);
}
bool CategoryGenerations::change_checked(Selection& before, bool enabled) noexcept {
    try { return change_preference(before, enabled); }
    catch (...) { return false; }
}
void CategoryGenerations::refresh_filters(Generation& generation, int style, int skip,
        bool effective, std::uintptr_t protected_category) {
    // Refresh each existing filter independently. An unavailable/disabled
    // filter must not veto the toggle after custom roots have already changed.
    // Native owns its category storage; this pass neither writes nor aliases it.
    constexpr std::array<int, 6> ids{110, 210, 211, 212, 213, 214};
    // Both production callers have validated style 0/1.
    for (const int id : ids) {
        const auto at = generation.manager + 16 + sizeof(category_layout::Category) * (230ULL * style + id);
        if (id == skip || at == protected_category) continue;
        OriginalFilterState filter;
        if (read_saved_original_filter_state(context_.memory, context_.modules.game.range,
                at, style, filter) != SavedFilterResult::ready || !filter.enabled) continue;
        if (!filter_.rebuild(at, style, calls_.rebuild, effective, calls_.bank.set_difficulty))
            fault("Original Filter refresh skipped a category; later callbacks remain enabled");
    }
}
bool CategoryGenerations::change_preference(Selection& before, bool enabled) {
    auto& g = *before.generation;
    const int source = before.id;
    const bool integer = native_level(before.manager, before.category, source, before.style);
    const bool custom = g.bank.owned(source, before.style);
    const bool level = before.group == 1 && (integer || custom);
    int wanted_level = integer ? source - 5 : custom ? g.bank.level(source) : 0;
    if (level && before.chart) {
        const int difficulty = before.active[0] ? before.p1 : before.p2;
        const auto* row = context_.catalog.find({before.song, before.style, difficulty});
        if (row) wanted_level = enabled ? row->effective_tenths / 10 : row->native_level;
    }
    if (level && before.open && !close_leaf(before.navigation, before.category)) return false;
    if (enabled) {
        if (!build_bank(g) || !publish_roots(g)) {
            fault("native browsing ownership uncertain during toggle root publication; operation skipped; later callbacks remain enabled");
            return false;
        }
    } else {
        if (withdraw_roots(g) != Exchange::withdrawn) {
            fault("native browsing ownership uncertain during toggle root withdrawal; operation skipped; later callbacks remain enabled");
            return false;
        }
        // Keep the unchanged private bank/map for graph borrows and the next ON.
        // The separate native-filter pass below refreshes only its affected slots.
    }
    // Root publication/withdrawal completes the toggle. Optional filter and
    // selection refreshes must not leave its published preference behind the
    // actual roots if one of those later operations declines a native state.
    if (!context_.state.commit_view(enabled, false)) return false;
    before.view = context_.state.view();
    int target = source;
    auto force_open = before.open;
    if (level) {
        if (enabled && before.open && wanted_level >= 1 && wanted_level <= 12 &&
            g.bindings[before.style * CategoryBank::per_style + g.bank.id(wanted_level, 0) - g.bank.base_id()].category) {
            if (!g.roots->select_parent(wanted_level, before.style) || !g.roots->enter(wanted_level, before.style)) return false;
            target = g.bank.id(wanted_level, 1); // Native opens ALL and searches the original chart tuple.
        } else if (!enabled && before.chart) {
            target = wanted_level + 5;
        } else {
            const auto category = calls_.selection.current(before.navigation);
            if (!pointer(category, sizeof(category_layout::Category)) || !read(context_.memory, category, target)) return false;
            force_open = !enabled && before.open && target == wanted_level + 5 ? 1 : 0;
        }
        before.id = target;
        before.category = calls_.lookup(before.manager, target, before.style);
        before.open = force_open;
    }
    if (!pointer(before.category, sizeof(category_layout::Category)) || current_ != &g ||
        !same_view(context_.state.view(), before.view)) return false;
    refresh_filters(g, before.style, before.id, enabled, before.category);
    restore_current(before, enabled);
    return true;
}

bool CategoryGenerations::route_return(const CategoryLifeRequest& request) {
    if (!request.restore || !current_) return true;
    auto& g = *current_;
    if (request.owner != g.navigation - 0x328) return true;
    if (request.refresh_restore) return route_style(request);
    if (!context_.state.view().enabled()) return true;
    if (returning_depth_ || initializing_depth_ || changing_ || restoring_ || requesting_) return false;
    const int mode = calls_.mode();
    if (!browsing_mode(mode)) return true;
    int group{};
    if (!read(context_.memory, g.list + 0x84, group)) return false;
    if (group != 1) return true; // Preserve native group overrides on entry.
    auto& target = *request.restore;
    int style{};
    if (!read(context_.memory, g.list + 0x78, style) || style < 0 || style > 1) return true;
    const auto category = calls_.lookup(g.manager, target.category, style);
    const bool searched = target.category == 110 &&
        category == g.manager + 16 + sizeof(category_layout::Category) * (230ULL * style + 110);
    const bool private_id = g.bank.owned(target.category, style);
    const bool integer = native_level(g.manager, category, target.category, style);
    if (!searched && !private_id && !integer && !g.roots->page_level()) return true;

    Selection before;
    if (!g.mapped || !g.navigation_bound || !g.group_reference || !g.roots->current() || !capture(before)) return false;
    returning_depth_ = depth_;
    returning_ = {&g, before.view, before.active, before.style, before.mode, before.dp};
    // Close the existing leaf before changing the visible vector. Native's
    // one forthcoming607F30 call owns all chart/graph/presentation work.
    if (before.open && !close_leaf(g.navigation, before.category)) return false;
    int level{};
    bool leaf{};
    if (searched) {
        // Search has no surviving folder. Keep native's Level1 fallback on the
        // full roots page, without opening our decimal submenu on return.
        level = 1;
    } else if (private_id) {
        level = g.bank.level(target.category);
        leaf = g.bank.part(target.category) != 0 &&
            g.bindings[before.style * CategoryBank::per_style + target.category - g.bank.base_id()].category;
    } else if (integer) {
        level = target.category - 5;
        // A stock saved LEVEL may predate this launch's custom preference.
        // Resolve its actual native played tuple through the prepared catalogue;
        // absent/unrated rows retain the supplied integer as native fallback.
        if (target.song > 0 && target.p1 >= 0 && target.p1 <= 4) {
            if (const auto* row = context_.catalog.find({target.song, before.style, target.p1}))
                level = row->effective_tenths / 10;
        }
        target.category = g.bank.id(level, 1);
        leaf = g.bindings[before.style * CategoryBank::per_style + target.category - g.bank.base_id()].category != 0;
    }
    if (g.roots->page_level() && (!leaf || g.roots->page_level() != level))
        if (!g.roots->leave()) return false;
    if (leaf) {
        if (!g.roots->page_level() &&
            (!g.roots->select_parent(level, before.style) || !g.roots->enter(level, before.style))) return false;
        returning_.target = target.category;
    } else if (level) {
        // An empty former child is not an ALL request. Show the integer roots,
        // allowing native's existing ID fallback when its parent also vanished.
        target.category = g.bank.id(level, 0);
        target.force_open = 0;
        returning_.closed = true;
        if (g.bindings[before.style * CategoryBank::per_style + target.category - g.bank.base_id()].category)
            returning_.target = target.category;
    }
    return current_ == &g && same_view(context_.state.view(), before.view);
}
bool CategoryGenerations::returned() {
    Selection after;
    return capture(after) && current_ == returning_.generation &&
        after.style == returning_.style && after.mode == returning_.mode && after.dp == returning_.dp &&
        after.active == returning_.active && same_view(after.view, returning_.view) &&
        after.group == 1 && (returning_.target < 0 || after.id == returning_.target) &&
        (!returning_.closed || (!after.open && current_->roots->page_level() == 0));
}

bool CategoryGenerations::before_refresh(const CategoryLifeRequest& request) {
    auto view = context_.state.view();
    if (!request.refresh_boundary || !current_ ||
        request.owner > UINTPTR_MAX - 0x4C0 || request.owner + 0x4C0 != current_->navigation) return true;
    if (view.phase != FeaturePhase::ready) return true;
    const int mode = calls_.mode();
    if (!browsing_mode(mode)) return true;
    if (refreshing_depth_ || initializing_depth_ || changing_ || restoring_ || returning_depth_ || requesting_) return false;
    auto& g = *current_;
    if (g.repair_style >= 0) {
        if (!repair_style(g)) return false;
        view = context_.state.view();
    }
    if (g.uncertain) return false;
    int old_style{}, list_style{};
    if (!read_scene_styles(context_.memory, g.manager, g.list, old_style, list_style) ||
        old_style < 0 || old_style > 2 || list_style != (old_style == 2 ? 0 : old_style) ||
        view.battle != (old_style == 2)) return false;
    if (view.enabled()) {
        if (!g.mapped || !g.navigation_bound || !g.group_reference || !g.roots->current()) return false;
    } else if (g.navigation_bound || g.group_reference) return false;
    int destination = calls_.style();
    if (destination < 0 || destination > 1) return false;
    // Mirror the native requested selector, without applying it ourselves.
    if (destination) {
        const auto options = calls_.options();
        if (!pointer(options)) return false;
        if (calls_.battle(options)) destination = 2;
    }
    if (destination == old_style) return true; // Native option-only refresh.
    refresh_ = {&g, view, {}, old_style, mode};
    refreshing_depth_ = depth_;
    refreshing_style_ = destination;
    if (!read(context_.memory, g.list + 0x84, refresh_.group) ||
        !read(context_.memory, context_.modules.game.range.base + game_bindings(context_.modules.game.range).category_generations.active_flags_rva, refresh_.active)) return false;
    refresh_.level = refresh_.group == 1;
    for (const auto active : refresh_.active) if (active < 0 || active > 1) return false;
    if ((!refresh_.active[0] && !refresh_.active[1]) || !refresh_context(old_style)) return false;
    if (destination == 2 || old_style == 2) return true;
    if (!view.enabled()) return true;
    if (!g.roots->page_level() || g.roots->page_available(destination)) return true;

    int group{};
    if (!read(context_.memory, g.list + 0x84, group)) return false;
    if (group == 1) {
        CategoryRootSelection selected, closed;
        std::uint8_t open{};
        if (!g.roots->selection(selected) || selected.style != old_style ||
            !read(context_.memory, g.navigation + 5, open) || open > 1 || selected.open != open) return false;
        // Close the OLD leaf before exchanging vectors. The native refresh then
        // captures the integer parent and performs its own ID-based fallback.
        if (open) {
            calls_.close(g.navigation, 0);
            if (!g.roots->selection(closed) || closed.category != selected.category || closed.style != old_style ||
                closed.open || !read(context_.memory, g.navigation + 5, open) || open) return false;
        }
    }
    return g.roots->leave();
}
bool CategoryGenerations::after_refresh() {
    if (!current_ || !refresh_.generation || !refresh_.applied || !refresh_.restored ||
        !refresh_context(refreshing_style_)) return false;
    const auto& g = *current_;
    const bool enabled = refresh_.view.preference && refreshing_style_ != 2;
    if (enabled && (!g.mapped || !g.navigation_bound || !g.group_reference || !g.roots->current())) return false;
    if (!enabled && (g.navigation_bound || g.group_reference)) return false;
    const auto category = calls_.selection.current(g.navigation);
    int id{};
    std::uint8_t open{}, category_open{};
    if (!pointer(category, sizeof(category_layout::Category)) || !read(context_.memory, category, id) ||
        !read(context_.memory, g.navigation + 5, open) || open > 1 ||
        !read(context_.memory, category + 4, category_open) || category_open != open ||
        (refresh_.target >= 0 && id != refresh_.target)) return false;
    if (enabled && refresh_.group == 1) {
        CategoryRootSelection selected;
        if (!g.roots->selection(selected) || selected.style != refreshing_style_ || selected.open != open) return false;
    }
    return context_.state.commit_view(refresh_.view.preference, refreshing_style_ == 2);
}

bool CategoryGenerations::refresh_context(int style) {
    const auto& r = refresh_;
    if (!r.generation || current_ != r.generation ||
        !same_view(context_.state.view(), r.view) || calls_.mode() != r.mode) return false;
    const auto& g = *r.generation;
    std::uintptr_t manager{}, list{};
    int raw{}, normalized{}, group{}, dp{};
    std::array<int, 2> active{};
    const auto game = context_.modules.game.range.base;
    return read_scene_owner(context_.memory, context_.modules.game.range, manager, list, g.manager, g.list) &&
        read_scene_styles(context_.memory, manager, list, raw, normalized) &&
        raw == style && normalized == (style == 2 ? 0 : style) &&
        read(context_.memory, list + 0x84, group) && group == r.group &&
        read(context_.memory, game + game_bindings(context_.modules.game.range).category_generations.dp_flag_rva, dp) && dp == (refreshing_style_ != 0) &&
        read(context_.memory, game + game_bindings(context_.modules.game.range).category_generations.active_flags_rva, active) && active == r.active;
}
bool CategoryGenerations::before_style(const CategoryLifeRequest& request) {
    if (!request.style_boundary || !refresh_.generation) return true;
    auto& g = *refresh_.generation;
    if (request.owner != g.navigation - 0x328) return true;
    if (depth_ != refreshing_depth_ + 1 || refresh_.policy || !refresh_context(refresh_.source)) return false;
    if (!read(context_.memory, g.navigation + 5, refresh_.open) || refresh_.open > 1) return false;
    if (refresh_.source != 2 && refreshing_style_ != 2) {
        refresh_.policy = true;
        return true; // Ordinary SP/DP retains its published roots and native captured tuple.
    }
    // Native has now captured category, song and difficulty. Close a LEVEL
    // leaf only here, after that capture and before exchanging its vectors.
    if (refresh_.view.preference && refresh_.group == 1 && refresh_.open) {
        const auto category = calls_.selection.current(g.navigation);
        int id{};
        std::uint8_t open{};
        if (!pointer(category, sizeof(category_layout::Category)) ||
            !read(context_.memory, category, id) || !read(context_.memory, category + 4, open) || open != refresh_.open) return false;
        const int source = refresh_.source == 2 ? 0 : refresh_.source;
        if (native_level(g.manager, category, id, source) || g.bank.owns(category, id, source, g.list)) {
            if (!close_leaf(g.navigation, category)) return false;
        }
    }
    if (refresh_.view.enabled()) {
        if (withdraw_roots(g) != Exchange::withdrawn) return false;
    }
    refresh_.policy = true;
    return refresh_context(refresh_.source);
}
bool CategoryGenerations::after_style(const CategoryLifeRequest& request) {
    if (!request.style_boundary || !refresh_.generation ||
        request.owner != refresh_.generation->navigation - 0x328) return true;
    if (depth_ != refreshing_depth_ + 1 || !refresh_.policy || refresh_.applied) return false;
    // Native63AAF0 can select its default group when the old group is absent
    // in the destination style. Accept that completed native choice here only;
    // the following restoration and outer completion must retain it.
    int group{};
    if (!read(context_.memory, refresh_.generation->list + 0x84, group) || group < 0) return false;
    refresh_.group = group;
    if (!refresh_context(refreshing_style_)) return false;
    auto& g = *refresh_.generation;
    if (refresh_.source == 2 && refresh_.view.preference && refreshing_style_ != 2 &&
        (!build_bank(g) || !publish_roots(g))) return false;
    refresh_.applied = true;
    return refresh_context(refreshing_style_);
}
bool CategoryGenerations::invalidate_refresh_cache(std::uintptr_t scene) noexcept {
    // 8EC389..8EC3CC compares the cached requested style, stores style/BATTLE,
    // THEN calls refresh. Invalidating only that cache forces the next native
    // tick to retry; it neither replays refresh here nor changes actual style.
    if (!pointer(scene, 0x2010)) return false;
    std::uintptr_t vtable{};
    if (!read(context_.memory, scene, vtable) || vtable != context_.modules.game.range.base + game_bindings(context_.modules.game.range).category_generations.scene_vtable_rva ||
        !readable_range(context_.memory, scene + 0x200C, sizeof(int), RangeUse::private_writable)) return false;
    const int invalid = -1; // Requested native style is always 0 or 1.
    __try {
        std::memcpy(reinterpret_cast<void*>(scene + 0x200C), &invalid, sizeof(invalid));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool CategoryGenerations::repair_style(Generation& g) {
    std::uintptr_t manager{}, list{};
    int style{}, normalized{};
    const auto view = context_.state.view();
    if (g.uncertain || current_ != &g || view.phase != FeaturePhase::ready ||
        !read(context_.memory, context_.modules.game.range.base + game_bindings(context_.modules.game.range).category_generations.category_manager_rva, manager) || manager != g.manager ||
        !read(context_.memory, g.manager + 0x175D0, list) || list != g.list ||
        !read(context_.memory, g.manager + 0x2A424, style) || style != g.repair_style ||
        !read(context_.memory, g.list + 0x78, normalized) || normalized != (style == 2 ? 0 : style)) return false;
    if (view.preference && style != 2) {
        if (g.group_reference) {
            if (!g.mapped || !g.navigation_bound || !g.roots->current()) return false;
        } else if (!build_bank(g) || !publish_roots(g)) return false;
    } else if (g.group_reference || g.navigation_bound) return false;
    if (!context_.state.commit_view(view.preference, style == 2)) return false;
    g.repair_style = -1;
    return true;
}
void CategoryGenerations::retain_graphs(std::uintptr_t category) noexcept {
    // Unknown native references can reach any bank exposed at this operation,
    // but cannot reach private allocations constructed by later initializers.
    for (auto& g : generations_)
        if (g.get() == building_ || g.get() == summary_ ||
            (category ? g->references(category) :
                std::any_of(g->bindings.begin(), g->bindings.end(), [](const auto& b) { return b.category != 0; })))
            g->uncertain = true;
}
void CategoryGenerations::recover_failure(const CategoryLifeRequest& request, CategoryLifeOutcome outcome) noexcept {
    fault();
    if (request.event == CategoryLifeEvent::initialize) {
        reject_initialization();
        return;
    }
    if (request.event == CategoryLifeEvent::graph_bind) { retain_graphs(); return; }
    if ((request.event != CategoryLifeEvent::style_apply && request.event != CategoryLifeEvent::scene_refresh) ||
        !current_ || outcome == CategoryLifeOutcome::unwound) return;
    auto& g = *current_;
    const auto scene = g.navigation - 0x4C0;
    if ((request.event == CategoryLifeEvent::scene_refresh && (!request.refresh_boundary || request.owner != scene)) ||
        (request.event == CategoryLifeEvent::style_apply && (!request.style_boundary || !refresh_.generation ||
            request.owner != g.navigation - 0x328))) return;
    // A native unwind provides no completed style receipt. Never retry an
    // uncertain vector owner. Clean cancellation/finished native style keeps
    // the generation and native caches coherent through an explicit retry.
    if (g.roots->state() == CategoryRootsState::uncertain || g.bank.state() == CategoryBankState::uncertain) {
        g.uncertain = true;
        return;
    }
    if (g.uncertain) return;
    int style{}, normalized{};
    if (!read(context_.memory, g.manager + 0x2A424, style) || style < 0 || style > 2 ||
        !read(context_.memory, g.list + 0x78, normalized) || normalized != (style == 2 ? 0 : style)) {
        // A refused scalar observation does not make the already committed
        // scene cache a completion receipt. Retry ownership observation at a
        // later native boundary; before_refresh still gates native admission.
        (void)invalidate_refresh_cache(scene);
        return;
    }
    g.repair_style = style;
    if (!invalidate_refresh_cache(scene)) fault();
}
bool CategoryGenerations::route_style(const CategoryLifeRequest& request) {
    if (!request.restore || !refresh_.generation ||
        request.owner != refresh_.generation->navigation - 0x328) return true;
    if (depth_ != refreshing_depth_ + 1 || !refresh_.applied || refresh_.restored ||
        !refresh_context(refreshing_style_)) return false;
    auto& r = refresh_;
    auto& g = *r.generation;
    auto& target = *request.restore;
    const int source = r.source == 2 ? 0 : r.source;
    const bool custom = g.bank.owned(target.category, source);
    const bool integer = native_level(g.manager, calls_.lookup(g.manager, target.category, source), target.category, source);
    if ((r.source == 2 || refreshing_style_ == 2) && r.view.preference && r.level && r.group == 1 && (custom || integer)) {
        const int style = refreshing_style_ == 2 ? 0 : refreshing_style_;
        int level = custom ? g.bank.level(target.category) : target.category - 5;
        if (target.song > 0 && target.p1 >= 0 && target.p1 <= 4) {
            if (const auto* row = context_.catalog.find({target.song, style, target.p1}))
                level = refreshing_style_ == 2 ? row->native_level : row->effective_tenths / 10;
        }
        target.force_open = r.open;
        if (refreshing_style_ == 2) {
            target.category = level + 5;
            // Reopen only a surviving native LEVEL. For a missing ID, native's
            // close-and-fallback must stay closed rather than force-open index0.
            category_layout::Vector roots{}, same{};
            if (!read(context_.memory, g.group + 0x78, roots) || !vector_fits(roots, 0, 8) ||
                (roots.end - roots.begin) / 8 > 4096) return false;
            const auto wanted = g.manager + 16 + sizeof(category_layout::Category) * target.category;
            bool present{};
            for (auto at = roots.begin; at < roots.end; at += 8) {
                std::uintptr_t row{};
                if (!read(context_.memory, at, row)) return false;
                present |= row == wanted;
            }
            if (!read(context_.memory, g.group + 0x78, same) || same != roots) return false;
            if (!present) target.force_open = 0;
        } else {
            const auto parent = g.bank.id(level, 0);
            const bool present = g.bindings[style * CategoryBank::per_style + parent - g.bank.base_id()].category != 0;
            if (r.open && present) {
                if (!g.roots->select_parent(level, style) || !g.roots->enter(level, style)) return false;
                target.category = parent + 1;
            } else { target.category = parent; target.force_open = 0; }
            if (present) r.target = target.category;
        }
    }
    const int bank = refreshing_style_ == 2 ? 0 : refreshing_style_;
    int skip = -1;
    // Only exclude a filter we can prove is already the requested current
    // destination. Otherwise the six-folder pass may include it once before
    // native's final restore; no extra navigation/fallback algorithm is added.
    const auto selected = calls_.selection.current(g.navigation);
    if (filtered(target.category) && selected == g.manager + 16 +
        sizeof(category_layout::Category) * (230ULL * bank + target.category)) skip = target.category;
    refresh_filters(g, bank, skip, r.view.preference && refreshing_style_ != 2, 0);
    r.restored = true;
    return refresh_context(refreshing_style_);
}

bool CategoryGenerations::capture(Selection& out) {
    if (!current_ || !pointer(current_->navigation, 0x448)) return false;
    out.generation = current_;
    out.manager = current_->manager; out.list = current_->list; out.navigation = current_->navigation;
    out.view = context_.state.view();
    if (out.view.phase != FeaturePhase::ready) return false;
    out.mode = calls_.mode();
    if (!browsing_mode(out.mode)) return false;
    std::uintptr_t manager{}, list{};
    int list_style{};
    std::uint8_t busy{}, editor{};
    const auto game = context_.modules.game.range;
    if (!read_scene_owner(context_.memory, game, manager, list, out.manager, out.list) ||
        !read_scene_styles(context_.memory, manager, list, out.style, list_style) ||
        out.style < 0 || out.style > 1 || list_style != out.style ||
        !read(context_.memory, list + 0x84, out.group) || out.group < 0 ||
        !read(context_.memory, game.base + game_bindings(context_.modules.game.range).category_generations.dp_flag_rva, out.dp) || out.dp != out.style ||
        !read(context_.memory, game.base + game_bindings(context_.modules.game.range).category_generations.active_flags_rva, out.active) ||
        !read(context_.memory, out.navigation + 5, out.open) || out.open > 1 ||
        !read(context_.memory, out.navigation + 6, busy) || busy) return false;
    for (const auto active : out.active) if (active < 0 || active > 1) return false;
    out.players = out.active[0] + out.active[1];
    if (out.dp && out.players == 2) out.players = 1;
    if (out.players < 1 || out.players > 2) return false;
    out.category = calls_.selection.current(out.navigation);
    category_layout::Category category;
    if (!pointer(out.category, sizeof(category)) || !read(context_.memory, out.category, category) ||
        category.open != out.open || category.insert_count < 0 ||
        !vector_fits(category.charts, category.insert_count, sizeof(category_layout::Chart)) ||
        !vector_fits(category.bars, category.bar_count, sizeof(category_layout::Bar)) ||
        !read(context_.memory, out.category + 0xAC, editor) || editor) return false;
    out.id = category.id;
    out.charts = category.charts; out.inserted = category.insert_count;
    out.working = category.preferences;
    if (filtered(out.id)) for (const int diff : out.working) if (diff < 0 || diff > 4) return false;
    if (out.id < 0 || calls_.lookup(manager, out.id, out.style) != out.category) return false;
    if (current_->bank.owned(out.id, out.style) &&
        (!current_->mapped || !current_->bank.owns(out.category, out.id, out.style, list))) return false;
    if (out.open) {
        if (category.cursor < 0 || category.cursor >= category.bar_count) return false;
        category_layout::Bar bar;
        if (!read(context_.memory, category.bars.begin + sizeof(bar) * category.cursor, bar)) return false;
        if (bar.type == 1) {
            category_layout::Chart chart;
            if (bar.data < category.charts.begin ||
                bar.data - category.charts.begin >= static_cast<std::size_t>(category.insert_count) * sizeof(chart) ||
                (bar.data - category.charts.begin) % sizeof(chart) ||
                !read(context_.memory, bar.data, chart) || chart.category != out.id || chart.style != out.style ||
                chart.p1 < 0 || chart.p1 > 4 || chart.p2 < 0 || chart.p2 > 4 ||
                !pointer(chart.music, 0x680) ||
                !read(context_.memory, chart.music + 0x67C, out.song) || out.song <= 0) return false;
            out.p1 = chart.p1; out.p2 = chart.p2; out.chart = true;
            out.chart_address = bar.data; out.music = chart.music;
        }
    }
    return current_ == out.generation && calls_.mode() == out.mode &&
        same_view(context_.state.view(), out.view);
}
void CategoryGenerations::restore_current(Selection& before, bool effective) {
    bool completed{};
    restoring_ = &before; destination_ = effective;
    __try {
        // Native's one continuation owns filter, sort/search, graph and panels;
        // RAX is not a success boolean. Missing charts use native fallback.
        (void)calls_.selection.restore(before.navigation - 0x328, before.id, before.song,
            before.p1, before.p2, before.open, -1);
        completed = true;
    } __finally {
        restoring_ = nullptr;
        if (!completed)
            fault("native browsing ownership uncertain during selection restoration; operation skipped; later callbacks remain enabled");
    }
}
bool CategoryGenerations::effective() const noexcept {
    const auto view = context_.state.view();
    return view.phase == FeaturePhase::ready &&
        (thread() && restoring_ ? destination_ : thread() && refresh_.policy
            ? refresh_.view.preference && refreshing_style_ != 2 : thread() && current_ && current_->repair_style >= 0
                ? view.preference && current_->repair_style != 2 && current_->group_reference : view.enabled());
}
void CategoryGenerations::restore_working(const CategorySummaryRequest& request,
        std::uintptr_t caller, bool completed) {
    if (!restoring_ || caller != context_.modules.game.range.base + game_bindings(context_.modules.game.range).category_generations.filter_restore_caller_rva) return;
    auto& before = *restoring_;
    if (!filtered(before.id) || request.manager != before.manager || request.style != before.style ||
        request.id != before.id) return;
    if (!completed || before.working_done || before.working_busy) { fault(); return; }
    category_layout::Category category;
    if (!read(context_.memory, before.category, category) || category.id != before.id ||
        category.charts != before.charts || category.insert_count != before.inserted ||
        !vector_fits(category.bars, category.bar_count, sizeof(category_layout::Bar)) ||
        category.bar_count > before.inserted + 3) { fault(); return; }
    unsigned visible{};
    for (int n = 0; before.chart && n < category.bar_count; ++n) {
        category_layout::Bar bar;
        if (!read(context_.memory, category.bars.begin + sizeof(bar) * n, bar)) { fault(); return; }
        visible += bar.type == 1 && bar.data == before.chart_address;
    }
    if (visible > 1) { fault(); return; }
    before.working_busy = true;
    __try {
        // Native summary iterates all five difficulties. Restore the category's
        // prior working preferences while its destination filter is installed,
        // then restore a selected VEFX override on its still-visible native row.
        // Native setters retain availability/filter/paired-player behavior.
        for (int side = 0; side < 2; ++side) {
            calls_.bank.set_difficulty(before.category, before.working[side], side);
        }
        if (visible) {
            category_layout::Chart row;
            if (!read(context_.memory, before.chart_address, row) || row.music != before.music ||
                row.category != before.id || row.style != before.style) { fault(); return; }
            for (int side = 0; side < 2; ++side) if (before.active[side]) {
                    calls_.chart_difficulty(before.chart_address, side ? before.p2 : before.p1, side, 0, 1);
                if (before.players == 1) break;
            }
        }
        before.working_done = true;
    } __finally { before.working_busy = false; }
}
void CategoryGenerations::restore_working_checked(const CategorySummaryRequest& request,
        std::uintptr_t caller, bool completed) noexcept {
    try { restore_working(request, caller, completed); }
    catch (...) { fault(); }
}
bool CategoryGenerations::begin(const CategoryLifeRequest& request) {
    if (request.event == CategoryLifeEvent::preview_assign) {
        if (!request.preview_boundary || !restoring_) return true;
        const auto& before = *restoring_;
        if (!before.chart || request.owner != before.navigation + 0x80 ||
            request.argument != static_cast<std::uint32_t>(before.song)) return true;
        int song{}, prepared{}, native{};
        if (!read(context_.memory, request.owner + 4, song) ||
            !read(context_.memory, request.owner + 12, prepared) ||
            !read(context_.memory, before.music + 0x404, native)) { fault(); return true; }
        // Only omit native's redundant same-song assignment. It resets the
        // preview request/timer; all navigation and presentation still run.
        // A different target or metadata keeps native initialization intact.
        return song != before.song || prepared != native || current_ != before.generation ||
            !same_view(context_.state.view(), before.view);
    }
    if (collecting_ || summary_ || building_) { fault(); return false; }
    if (request.event == CategoryLifeEvent::style_apply) {
        const bool okay = before_style(request);
        if (!okay) fault();
        return okay;
    }
    if (request.event == CategoryLifeEvent::selection_restore) {
        const bool okay = route_return(request);
        if (!okay) fault();
        return okay;
    }
    if (request.event == CategoryLifeEvent::scene_tick) sample_toggle(request);
    if (request.event == CategoryLifeEvent::scene_refresh) {
        const bool okay = before_refresh(request);
        if (!okay) fault();
        return okay;
    }
    if (request.event == CategoryLifeEvent::initialize) {
        if (initializing_depth_ || !pointer(request.owner, 0x448)) { fault(); return true; }
        initializing_depth_ = depth_;
        initialization_ready_ = false;
        const auto graph = request.owner + 0x438;
        auto found = std::find_if(graphs_.begin(), graphs_.end(), [graph](const auto& g) { return g.owner == graph; });
        std::uintptr_t category{};
        if (!read(context_.memory, graph + 8, category)) { retain_graphs(); fault(); return true; }
        if (found == graphs_.end()) {
            try { graphs_.push_back({graph, category}); }
            catch (...) { retain_graphs(); fault(); return true; }
        }
        else found->category = category;
        initialization_ready_ = detach();
        return true;
    }
    if (request.event == CategoryLifeEvent::adopt && current_ && request.owner == current_->manager) {
        std::uintptr_t incoming{};
        if (!pointer(request.argument, 16) || !read(context_.memory, request.argument, incoming)) { fault(); return true; }
        if (incoming != current_->list) (void)detach();
    }
    if (request.event == CategoryLifeEvent::list_destroy && current_ && request.owner == current_->list)
        (void)detach();
    if (request.event != CategoryLifeEvent::populate && request.event != CategoryLifeEvent::group_destroy) return true;
    if (current_ && current_->group == request.owner && !current_->group_reference) {
        // A withdrawn bank cannot be reused after its native source population
        // changes. Full repopulation recovery is an enclosing scene operation.
        if (request.event == CategoryLifeEvent::group_destroy || current_->bank.state() == CategoryBankState::ready) {
            if (request.event == CategoryLifeEvent::populate) fault();
            (void)detach();
        }
    }
    for (auto& owned : generations_) {
        auto& g = *owned;
        if (g.group != request.owner || !g.group_reference) continue;
        if (request.event == CategoryLifeEvent::group_destroy) {
            if (&g == current_ && !detach()) return false;
            if (!g.roots->handoff()) { g.uncertain = true; fault(); }
        } else {
            // Repopulating an exposed group is a separate native transaction.
            // Restore its canonical vectors before native touches them. Never
            // build here: native style/filter/summary setup has not finished.
            if (withdraw_roots(g) != Exchange::withdrawn) { fault(); return false; }
            if (&g == current_ && !detach()) return false;
            // A matched initializer already owns the complete native rebuild.
            // Its completion publishes fresh membership after native summaries,
            // style and navigation setup. Standalone population has no such
            // completion boundary, so it still requires conservative failure.
            if (!initializing_depth_) fault();
        }
    }
    return true;
}
void CategoryGenerations::finish(const CategoryLifeRequest& request, CategoryLifeOutcome outcome) {
    if (request.event == CategoryLifeEvent::scene_tick && depth_ == 1) finish_toggle(request, outcome);
    if (request.event == CategoryLifeEvent::graph_bind) {
        if (outcome == CategoryLifeOutcome::completed) graph_bound(request.owner);
        else { retain_graphs(); fault(); }
        return;
    }
    if (request.event == CategoryLifeEvent::selection_destroy) {
        selection_destroyed(request.owner, outcome == CategoryLifeOutcome::completed);
        return;
    }
    if (request.event == CategoryLifeEvent::group_destroy) {
        for (auto& g : generations_) if (g->group == request.owner && g->group_reference) {
            if (outcome == CategoryLifeOutcome::completed && g->roots->state() == CategoryRootsState::handed_off)
                g->group_reference = false;
            else { g->uncertain = true; fault(); }
        }
    }
    if (outcome == CategoryLifeOutcome::unwound) {
        if (refresh_.generation && (request.event == CategoryLifeEvent::scene_refresh || request.event == CategoryLifeEvent::style_apply))
            refresh_.generation->uncertain = true;
        fault(); return;
    }
    if (request.event == CategoryLifeEvent::style_apply && outcome == CategoryLifeOutcome::completed &&
        !after_style(request)) recover_failure(request, outcome);
    if (request.event == CategoryLifeEvent::style_apply && outcome == CategoryLifeOutcome::cancelled)
        recover_failure(request, outcome);
    if (request.event == CategoryLifeEvent::selection_restore && returning_depth_ == depth_ &&
        outcome == CategoryLifeOutcome::completed && !returned()) fault();
    if (request.event == CategoryLifeEvent::scene_refresh &&
        ((refreshing_depth_ == depth_ && outcome == CategoryLifeOutcome::completed && !after_refresh()) ||
            (outcome == CategoryLifeOutcome::cancelled && (!refreshing_depth_ || refreshing_depth_ == depth_))))
        recover_failure(request, outcome);
    if (request.event == CategoryLifeEvent::initialize && outcome == CategoryLifeOutcome::completed &&
        initializing_depth_ == depth_ && initialization_ready_)
        if (!initialize(request.owner)) reject_initialization();
}
bool CategoryGenerations::begin_checked(const CategoryLifeRequest& request) noexcept {
    try { return begin(request); }
    catch (...) {
        if (request.event == CategoryLifeEvent::initialize || request.event == CategoryLifeEvent::graph_bind) retain_graphs();
        fault(); return false;
    }
}
void CategoryGenerations::finish_checked(const CategoryLifeRequest& request, CategoryLifeOutcome outcome) noexcept {
    try { finish(request, outcome); }
    catch (...) { recover_failure(request, outcome); }
}
CategoryLifecycleHooks::Callbacks CategoryGenerations::lifecycle_callbacks() noexcept {
    return {
        [](void* context, const CategoryLifeRequest& request) noexcept {
            auto& self = *static_cast<CategoryGenerations*>(context);
            if (!self.thread()) return request.event != CategoryLifeEvent::populate;
            ++self.depth_;
            __try { return self.begin_checked(request); }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                if (request.event == CategoryLifeEvent::initialize || request.event == CategoryLifeEvent::graph_bind) self.retain_graphs();
                self.fault(); return false;
            }
        },
        [](void* context, const CategoryLifeRequest& request, CategoryLifeOutcome outcome) noexcept {
            auto& self = *static_cast<CategoryGenerations*>(context);
            if (!self.thread() || !self.depth_) return;
            __try {
                __try { self.finish_checked(request, outcome); }
                __except (EXCEPTION_EXECUTE_HANDLER) { self.recover_failure(request, outcome); }
            } __finally {
                if (request.event == CategoryLifeEvent::initialize && self.initializing_depth_ == self.depth_)
                    self.initializing_depth_ = 0;
                if (request.event == CategoryLifeEvent::scene_refresh && self.refreshing_depth_ == self.depth_) {
                    self.refreshing_depth_ = 0;
                    self.refresh_ = {};
                }
                if (request.event == CategoryLifeEvent::selection_restore && self.returning_depth_ == self.depth_) {
                    self.returning_depth_ = 0;
                    self.returning_ = {};
                }
                --self.depth_;
                if (!self.depth_ && request.collection_boundary && outcome == CategoryLifeOutcome::completed &&
                    (request.event == CategoryLifeEvent::scene_entry || request.event == CategoryLifeEvent::scene_tick)) {
                    (void)self.collect();
                    // Completion proves readiness even when LEVEL collection fails.
                    __try { self.signal_ready(request.owner); }
                    __except (EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        }
    };
}
CategorySummary::Callbacks CategoryGenerations::summary_callbacks() noexcept {
    return {
        [](void* context, const CategorySummaryRequest& request, CategorySummaryBinding& binding) noexcept {
            auto& self = *static_cast<CategoryGenerations*>(context);
            if (!self.thread() || self.collecting_ || self.summary_) return false;
            auto* g = self.building_ ? self.building_ : self.current_;
            if (!g || !g->bank.prepare_summary(request, binding)) return false;
            self.summary_ = g;
            return true;
        },
        [](void* context, const CategorySummaryRequest& request, const CategorySummaryBinding& binding, bool completed) noexcept {
            auto& self = *static_cast<CategoryGenerations*>(context);
            if (!self.thread() || !self.summary_) return;
            auto& g = *self.summary_;
            g.bank.finish_summary(request, binding, completed);
            self.summary_ = nullptr;
            if (g.bank.state() == CategoryBankState::uncertain) { g.uncertain = true; self.fault(); }
        },
        [](void* context, const CategorySummaryRequest& request, std::uintptr_t caller, bool completed) noexcept {
            auto& self = *static_cast<CategoryGenerations*>(context);
            if (!self.thread()) return;
            self.filter_.summary_return(request, caller, completed);
            if (!self.restoring_) return;
            __try {
                self.restore_working_checked(request, caller, completed);
            } __except (EXCEPTION_EXECUTE_HANDLER) { self.fault(); }
        }
    };
}
bool CategoryGenerations::owns(std::uintptr_t category, int id, int style, std::uintptr_t list) const noexcept {
    return thread() && current_ && current_->bank.owns(category, id, style, list);
}
bool CategoryGenerations::request_current(const Selection& before) {
    if (current_ != before.generation || !same_view(context_.state.view(), before.view) ||
        calls_.mode() != before.mode || !current_->mapped || !current_->navigation_bound ||
        !current_->group_reference || !current_->roots->current()) return false;
    const auto memory = context_.memory;
    const auto base = context_.modules.game.range.base;
    std::uintptr_t manager{}, list{};
    int style{}, list_style{}, group{}, dp{};
    std::array<int, 2> active{};
    std::uint8_t busy{};
    return read_scene_owner(memory, context_.modules.game.range, manager, list, before.manager, before.list) &&
        read_scene_styles(memory, manager, list, style, list_style) && style == before.style && list_style == style &&
        read(memory, list + 0x84, group) && group == before.group &&
        read(memory, base + game_bindings(context_.modules.game.range).category_generations.dp_flag_rva, dp) && dp == before.dp &&
        read(memory, base + game_bindings(context_.modules.game.range).category_generations.active_flags_rva, active) && active == before.active &&
        read(memory, before.navigation + 6, busy) && !busy && same_view(context_.state.view(), before.view);
}
bool CategoryGenerations::close_leaf(std::uintptr_t navigation, std::uintptr_t category) {
    calls_.close(navigation, 0);
    return closed_leaf(context_.memory, navigation, category);
}
bool CategoryGenerations::close_request(Selection& before) {
    const auto& r = calls_.requests;
    const auto settings = r.settings();
    if (!pointer(settings, 0x60) || !request_current(before)) return false;
    if (r.persistent(before.manager, before.id)) {
        const auto store = r.store();
        if (!pointer(store) || !request_current(before)) return false;
        r.save(store, before.id, settings + 72);
        if (!request_current(before)) return false;
    }
    if (before.open) {
        calls_.close(before.navigation, 0);
        if (!request_current(before) || !closed_leaf(context_.memory, before.navigation, before.category)) return false;
    }
    r.clear(settings, 1);
    return request_current(before);
}
CategoryRequestResult CategoryGenerations::route_request(const CategoryRequest& request) {
    using Action = CategoryRequestAction;
    const CategoryRequestResult reject{Action::reject, request.category}, native{Action::native, request.category};
    if (!current_ || request.selection != current_->navigation - 0x328) return native;
    auto& g = *current_;
    int style{};
    if (!read(context_.memory, g.list + 0x78, style) || style < 0 || style > 1) return native;
    const bool owned = g.mapped && g.bank.owned(request.category, style);
    const int page = g.roots->page_level();
    if (!owned) {
        if (page && !requesting_ && !changing_ && !building_ && !collecting_) {
            Selection before;
            if (capture(before) && before.group == 1 && before.open &&
                g.bank.owns(before.category, before.id, before.style, before.list))
                (void)close_request(before);
            (void)g.roots->leave();
        }
        return native;
    }
    if (!context_.state.view().enabled()) return reject;
    if (requesting_ || collecting_ || changing_ || building_ || summary_ || initializing_depth_ ||
        refreshing_depth_ || restoring_ || returning_depth_) return reject;
    const int mode = calls_.mode();
    if (!browsing_mode(mode)) return reject;
    std::uint8_t busy{};
    if (!read(context_.memory, g.navigation + 6, busy)) { fault(); return reject; }
    if (busy) return reject; // Same later native guard; do not mutate the page before it.
    Selection before;
    if (!capture(before) || !request_current(before)) { fault(); return reject; }
    if (request.kind == CategoryRequestKind::song &&
        (request.style != before.style || request.song <= 0 || request.p1 < -1 || request.p1 > 4 ||
         request.p2 < -1 || request.p2 > 4)) return reject;
    const int level = g.bank.level(request.category);
    const bool heading = g.bank.part(request.category) == 0;
    const bool heading_only = heading && request.kind == CategoryRequestKind::category;
    const int target = heading && request.kind == CategoryRequestKind::song ? request.category + 1 : request.category;
    if (owned) {
        if (request.kind == CategoryRequestKind::rival) return reject; // Native rival always targets197.
        const auto& binding = g.bindings[before.style * CategoryBank::per_style + target - g.bank.base_id()];
        if (!binding.category) return reject;
        if (!g.bank.owns(binding.category, target, before.style, before.list)) { fault(); return reject; }
        if (request.kind == CategoryRequestKind::song) {
            CategoryChartMatch match;
            const auto result = match_category_chart(context_.memory, binding.category, target, before.style,
                request.song, request.p1, request.p2, match);
            if (result == CategoryChartResult::invalid) fault();
            if (result != CategoryChartResult::found) return reject;
        }
        // The list's actual available-group vector authorizes selecting LEVEL;
        // a category lookup alone does not establish native group availability.
        category_layout::Vector groups;
        if (!read(context_.memory, before.list + 0x48 + 24ULL * before.style, groups)) { fault(); return reject; }
        if (groups == category_layout::Vector{}) return reject;
        if (groups.begin < 0x10000 || (groups.begin & 3) || groups.begin > groups.end || groups.end > groups.capacity ||
            (groups.end - groups.begin) % 4 || (groups.capacity - groups.begin) % 4 ||
            groups.capacity - groups.begin > 256) { fault(); return reject; }
        bool available{};
        for (auto at = groups.begin; at < groups.end; at += sizeof(int)) {
            int id{};
            if (!read(context_.memory, at, id)) { fault(); return reject; }
            available = available || id == 1;
        }
        if (!available) return reject;
        if (!request_current(before)) { fault(); return reject; }
        if (before.group == 1 && page == level && !heading_only) return {Action::native, target};
    }
    requesting_ = true;
    bool completed{};
    __try {
        if (!close_request(before)) return reject;
        if (page && (page != level || before.group != 1))
            if (!g.roots->leave() || !request_current(before)) return reject;
        if (before.group != 1) {
            const int group = 1;
            std::memcpy(reinterpret_cast<void*>(before.list + 0x84), &group, sizeof(group));
            before.group = group;
            calls_.requests.group(before.list); // Native recomputes its index from actual available IDs.
            if (!request_current(before)) return reject;
        }
        if (!g.roots->page_level() &&
            (!g.roots->select_parent(level, before.style) || !g.roots->enter(level, before.style))) return reject;
        if (!request_current(before) || g.roots->page_level() != level || !g.roots->page_available(before.style)) return reject;
        const auto target_category = g.bindings[before.style * CategoryBank::per_style + target - g.bank.base_id()].category;
        if (!g.bank.owns(target_category, target, before.style, before.list)) return reject;
        if (request.kind == CategoryRequestKind::song) {
            CategoryChartMatch match;
            if (match_category_chart(context_.memory, target_category, target, before.style,
                    request.song, request.p1, request.p2, match) != CategoryChartResult::found) return reject;
        }
        if (heading_only) {
            CategoryRootSelection selected;
            if (!g.roots->selection(selected)) return reject;
            if (selected.open) calls_.close(g.navigation, 0);
            std::uint8_t open{};
            CategoryRootSelection closed;
            if (!request_current(before) || !g.roots->selection(closed) || closed.category != selected.category ||
                closed.open || !read(context_.memory, g.navigation + 5, open) || open) return reject;
            calls_.requests.sound(0x2C80);
            completed = request_current(before);
            return completed ? CategoryRequestResult{Action::heading, target} : reject;
        }
        completed = true;
        return {Action::native, target};
    } __finally {
        requesting_ = false;
        if (!completed) fault(); // Never replay a partly completed page transaction.
    }
}
CategoryRequestResult CategoryGenerations::request_checked(const CategoryRequest& request) noexcept {
    try { return route_request(request); }
    catch (...) {
        fault();
        const bool owned = current_ && current_->mapped && current_->bank.owned(request.category);
        return {owned ? CategoryRequestAction::reject : CategoryRequestAction::native, request.category};
    }
}
CategoryRequestHooks::Route CategoryGenerations::request_callback() noexcept {
    return [](void* context, const CategoryRequest& request) noexcept {
        auto& self = *static_cast<CategoryGenerations*>(context);
        if (!self.thread()) return CategoryRequestResult{CategoryRequestAction::native, request.category};
        __try { return self.request_checked(request); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            self.fault();
            const bool owned = self.current_ && self.current_->mapped && self.current_->bank.owned(request.category);
            return CategoryRequestResult{owned ? CategoryRequestAction::reject : CategoryRequestAction::native, request.category};
        }
    };
}
CategoryRequestHooks::Failure CategoryGenerations::request_failure_callback() noexcept {
    return [](void* context) noexcept {
        auto& self = *static_cast<CategoryGenerations*>(context);
        if (self.thread()) self.fault();
    };
}
void CategoryGenerations::graph_bound(std::uintptr_t graph) noexcept {
    if (!thread()) return;
    auto found = std::find_if(graphs_.begin(), graphs_.end(), [graph](const auto& g) { return g.owner == graph; });
    std::uintptr_t category{};
    if (!pointer(graph, 16) || !read(context_.memory, graph + 8, category)) {
        retain_graphs(); fault();
    } else if (found != graphs_.end()) {
        found->category = category;
    } else if (std::any_of(generations_.begin(), generations_.end(), [category](const auto& g) { return g->references(category); })) {
        // Unrelated native graphs are ordinary passthrough. A private pointer
        // escaping into one is an untracked lifetime, not permission to free.
        retain_graphs(category); fault();
    }
}
void CategoryGenerations::selection_destroyed(std::uintptr_t selection, bool completed) noexcept {
    if (!thread() || !pointer(selection, 0x780)) return;
    const auto graph = selection + 0x760;
    const auto found = std::find_if(graphs_.begin(), graphs_.end(), [graph](const auto& g) { return g.owner == graph; });
    if (found == graphs_.end()) return;
    if (!completed) { fault(); return; }
    if (current_ && current_->navigation == selection + 0x328) (void)detach();
    graphs_.erase(found); // Identity only: never read the destroyed native owner.
}
bool CategoryGenerations::collect_checked() noexcept {
    try {
        for (auto it = generations_.begin(); it != generations_.end();) {
            auto& g = **it;
            if (&g == current_ || g.mapped || g.navigation_bound || g.group_reference || g.uncertain || g.cleanup_attempted ||
                std::any_of(graphs_.begin(), graphs_.end(), [&g](const auto& graph) { return g.references(graph.category); })) {
                ++it; continue;
            }
            g.cleanup_attempted = true;
            const auto roots = g.roots->state();
            if ((roots != CategoryRootsState::withdrawn && roots != CategoryRootsState::handed_off && !g.roots->discard()) ||
                !g.bank.destroy()) { g.uncertain = true; fault(); return false; }
            it = generations_.erase(it);
        }
        return true;
    } catch (...) { fault(); return false; }
}
bool CategoryGenerations::collect() noexcept {
    if (!thread() || depth_ || building_ || summary_ || restoring_ || changing_ || collecting_ || requesting_) return false;
    collecting_ = true;
    bool result{};
    __try {
        __try { result = collect_checked(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
    } __finally { collecting_ = false; }
    return result;
}
} // namespace point_zero::native
