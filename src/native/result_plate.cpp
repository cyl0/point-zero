#include "build_bindings.hpp"
#include "result_plate.hpp"

#include <windows.h>
#include <cmath>

namespace point_zero::native {
namespace {
constexpr const char* labels[]{"left", "middle", "right"};
constexpr std::uint32_t name_mask = 0x0C000000, named_bit = 0x04000000;


void placement(int i, float extension, float (&xy)[2], float (&scale)[2]) noexcept {
    xy[0] = i == 0 ? -extension : (i == 1 ? 145.f - extension : 145.f);
    xy[1] = 0; scale[0] = i == 1 ? extension : 1.f; scale[1] = 1;
}
} // namespace

ResultPlate::ResultPlate(const AdapterContext& context, PlayedNativeCalls calls)
    : game_(context.modules.game.range), afp_(context.modules.afp.range), memory_(context.memory),
      state_(context.state), host_(context.host), calls_(calls) {}

bool ResultPlate::prepare(std::uint32_t component) noexcept {
    if (attempted_) return false;
    attempted_ = true;
    if (state_.view().phase != FeaturePhase::preparing) return false;
    // The parent has already loaded and admitted this private component.
    if ((component & 0x70000000U) != 0x30000000U) return false;
    component_ = component; prepared_ = true;
    return true;
}
void ResultPlate::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !host_.log) return;
    constexpr std::string_view message = "result plate drawing skipped a native operation; later callbacks remain enabled";
    try { host_.log(host_.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
bool ResultPlate::enter() noexcept {
    if (!prepared_ || busy_) { fault(); return false; }
    busy_ = true;
    return true;
}
bool ResultPlate::same_chart(const PlayedChart& a, const PlayedChart& b) noexcept {
    return a.surface == b.surface && a.owner == b.owner && a.music == b.music &&
        a.layer == b.layer && a.stream == b.stream && a.player == b.player &&
        a.players == b.players && a.sole == b.sole && a.chart == b.chart;
}
bool ResultPlate::chart_live(const PlayedChart& chart) const {
    std::uint32_t stream{};
    return chart.surface == played_art::Surface::result && chart.owner == chart.layer &&
        chart.player >= 0 && chart.player <= 1 && chart.chart &&
        calls_.chart.valid(5, chart.stream) >= 0 &&
        read_field(memory_, chart.layer, 8, stream) && stream == chart.stream;
}
bool ResultPlate::node(const Node& n, std::uint32_t stream, std::uintptr_t parent,
                       bool root, bool exposed) const {
    int handle{-1}; std::uint32_t sid{}; std::uintptr_t actual_parent{};
    return n.handle >= 0 && n.work && (!exposed || calls_.chart.valid(6, n.handle) >= 0) &&
        calls_.resolve(n.handle) == n.work && read_field(memory_, n.work, 312, handle) &&
        handle == n.handle && read_field(memory_, n.work, 0, sid) && sid == stream &&
        (root || (read_field(memory_, n.work, 64, actual_parent) && actual_parent == parent));
}
bool ResultPlate::owner_valid(const Owner& owner) const {
    std::uintptr_t parent_source{}; int depth{};
    // Native validity/property APIs require an exposed handle (flag 0x800).
    // An anonymous timeline shape is first admitted by its raw generation,
    // stream, parent and definition; our subsequent named path exposes it.
    return chart_live(owner.chart) && node(owner.root, owner.chart.stream, 0, true) &&
        calls_.chart.path(owner.chart.stream, "") == owner.root.handle &&
        node(owner.plate, owner.chart.stream, owner.root.work, false, owner.exposed) &&
        read_field(memory_, owner.plate.work, 40, depth) && depth == owner.layout.depth &&
        // Attachment and stock bitmap restoration replace the plate's content
        // source. The unchanged native root retains the owner generation.
        read_field(memory_, owner.root.work, 320, parent_source) && parent_source == owner.source;
}
bool ResultPlate::original(const Owner& owner) const {
    std::uintptr_t definition{}, source{}; unsigned tag{}; std::uint16_t count{}; int maximum{};
    return read_field(memory_, owner.plate.work, 320, source) && source == owner.source &&
        calls_.definition(owner.source, static_cast<std::uint32_t>(owner.layout.shape)) == owner.definition &&
        read_field(memory_, owner.plate.work, 48, definition) && definition == owner.definition &&
        read_field(memory_, definition, 0, tag) && ((tag >> 1) & 0x3ff) == 0x84 &&
        read_field(memory_, owner.source, 30, count) && count &&
        read_field(memory_, owner.source, 44, maximum) && maximum >= owner.layout.shape && maximum <= 65535;
}
bool ResultPlate::name_is(const Owner& owner, bool named) const {
    std::uint32_t flags{}; std::uint64_t value{};
    if (!read_field(memory_, owner.plate.work, 32, flags) ||
        !read_field(memory_, owner.plate.work, 96, value)) return false;
    if (!named) return !(flags & name_mask) &&
        value == (owner.cleared || owner.clear_attempted ? 0 : owner.layout.anonymous);
    if (!(flags & named_bit) || !value) return false;
    // Read the actual known owner's name, never use a colliding path to choose
    // an object for cleanup. AFP owns the string and may release it on clear.
    for (std::size_t i = 0; i < 32; ++i) {
        char ch{};
        if (!read_field(memory_, static_cast<std::uintptr_t>(value), i, ch) || ch != owner.layout.name[i]) return false;
        if (!ch) return true;
    }
    return false;
}
bool ResultPlate::plate_at_depth(std::uintptr_t root, int wanted_depth,
                                 std::uintptr_t& plate, unsigned& matches) const {
    plate = 0; matches = 0;
    std::uintptr_t work{}; unsigned visited{};
    if (!read_field(memory_, root, 72, work)) return false;
    while (work && visited < 256) {
        ++visited;
        std::uintptr_t parent{}, next{}; int depth{};
        if (!read_field(memory_, work, 64, parent) || parent != root ||
            !read_field(memory_, work, 40, depth) || !read_field(memory_, work, 88, next)) return false;
        if (depth == wanted_depth) { plate = work; ++matches; }
        work = next;
    }
    return work == 0;
}
bool ResultPlate::siblings(const Owner& owner, const PlayedTarget* target, bool early) const {
    // Preflight the entire native sibling list, including children after the
    // candidate. A matching depth in a malformed tail does not admit a write.
    std::uintptr_t plate{}; unsigned matches{};
    if (!plate_at_depth(owner.root.work, owner.layout.depth, plate, matches) ||
        matches != 1 || plate != owner.plate.work) return false;
    const int number = calls_.chart.path(owner.chart.stream, owner.chart.leaf);
    if (target && !early && (target->count != 1 || number != target->handles[0])) return false;
    if (number < 0) return early && owner.number.handle < 0;
    const Node n{number, calls_.resolve(number)};
    return number != owner.plate.handle && node(n, owner.chart.stream, owner.root.work) &&
        calls_.chart.relative(number, 6) < 0 &&
        (owner.number.handle < 0 || (number == owner.number.handle && n.work == owner.number.work));
}
ResultPlate::Location ResultPlate::locate(const PlayedTarget& target, played_art::ResultPlate layout,
                                         bool early, Owner& out) const {
    out = {}; out.chart = target.chart; out.layout = layout;
    if (!chart_live(out.chart)) return Location::rejected;
    out.root.handle = calls_.chart.path(out.chart.stream, "");
    if (out.root.handle < 0) return early ? Location::absent : Location::rejected;
    out.root.work = calls_.resolve(out.root.handle);
    if (!node(out.root, out.chart.stream, 0, true)) return Location::rejected;
    unsigned matches{};
    if (!plate_at_depth(out.root.work, layout.depth, out.plate.work, matches) || matches > 1)
        return Location::rejected;
    if (!matches) return early ? Location::absent : Location::rejected;
    unsigned tag{}; std::uint16_t count{}; int maximum{};
    if (!read_field(memory_, out.plate.work, 312, out.plate.handle) ||
        !read_field(memory_, out.plate.work, 320, out.source) || !out.source ||
        !read_field(memory_, out.plate.work, 48, out.definition) || !out.definition ||
        !read_field(memory_, out.definition, 0, tag) || ((tag >> 1) & 0x3ff) != 0x84 ||
        !read_field(memory_, out.source, 30, count) || !count ||
        !read_field(memory_, out.source, 44, maximum) || maximum < layout.shape || maximum > 65535 ||
        calls_.definition(out.source, static_cast<std::uint32_t>(layout.shape)) != out.definition ||
        !owner_valid(out) || !name_is(out, false) ||
        calls_.chart.path(out.chart.stream, layout.name) >= 0 || !siblings(out, &target, early))
        return Location::rejected;
    const int number = calls_.chart.path(out.chart.stream, out.chart.leaf);
    if (number >= 0) out.number = {number, calls_.resolve(number)};
    return Location::admitted;
}

bool ResultPlate::children(Owner& owner, bool discover) {
    for (int i = 0; i < 3; ++i) {
        const int handle = calls_.path_relative(owner.plate.handle, labels[i]);
        if (handle < 0) return false;
        const Node n{handle, calls_.resolve(handle)};
        if (discover) owner.children[i] = n; // Retain even a partially observed attachment.
        if (owner.children[i].handle != handle || owner.children[i].work != n.work ||
            !node(n, owner.chart.stream, owner.plate.work)) return false;
        for (int j = 0; j < i; ++j) if (owner.children[j].handle == handle || owner.children[j].work == n.work) return false;
    }
    owner.children_known = true;
    return true;
}
bool ResultPlate::preflight(std::array<Life, 8>& life, const PlayedTarget* target, bool early) const {
    for (std::size_t i = 0; i < owners_.size(); ++i) {
        const auto& owner = owners_[i];
        if (!owner.plate.work) continue;
        auto& status = life[i];
        const bool selected = target && owner.chart.stream == target->chart.stream &&
            owner.chart.player == target->chart.player;
        const auto inspect = [&]() {
            status.gone = calls_.chart.valid(5, owner.chart.stream) < 0 ||
                (owner.exposed ? calls_.chart.valid(6, owner.plate.handle) < 0 :
                    calls_.resolve(owner.plate.handle) != owner.plate.work);
            // An incomplete attach may have produced unobserved children. Keep that
            // record; invalidating the host alone cannot certify their retirement.
            status.children_gone = !owner.replaced || owner.children_known;
            for (const auto& child : owner.children) if (child.handle >= 0) {
                if (calls_.chart.valid(6, child.handle) >= 0) {
                    status.children_gone = false;
                    // Stock may detach a child before its deferred destruction.
                    // Still verify its generation; pending/gone hosts need not own
                    // its parent while native destruction is deferred.
                    if (!node(child, owner.chart.stream, owner.plate.work, owner.pending || status.gone)) return false;
                } else if (owner.replaced && !owner.pending && !status.gone && owner.children_known) {
                    return false; // An active, completed attachment must retain every child.
                }
            }
            if (status.gone) return true;
            if (!owner_valid(owner) || !siblings(owner, selected ? target : nullptr, selected ? early : true)) return false;
            // Partial naming/clearing is recoverable only on this exact native work.
            if (owner.name_touched ? (!name_is(owner, true) && !name_is(owner, false)) : !name_is(owner, false)) return false;
            return true;
        };
        if (!inspect()) {
            status = {}; // Retain this record until its own identity is usable.
            if (!target || selected) return false;
        }
    }
    return true;
}
bool ResultPlate::clear(Owner& owner) {
    if (!owner.name_touched) return true;
    if (!owner_valid(owner)) return false;
    if (name_is(owner, false)) {
        owner.cleared |= owner.clear_attempted;
        owner.name_touched = false;
        return true;
    }
    if (!name_is(owner, true)) return false;
    owner.clear_attempted = true;
    if (!played_name(memory_, afp_, calls_, owner.chart.stream, owner.plate.work, nullptr) ||
        !owner_valid(owner) || !name_is(owner, false)) return false;
    owner.cleared = true; owner.name_touched = false;
    return true;
}
bool ResultPlate::collect_impl(const PlayedTarget* target, bool early, std::array<Life, 8>* observed) {
    std::array<Life, 8> life{};
    if (!preflight(life, target, early)) { fault(); return false; }
    if (observed) *observed = life;
    for (std::size_t i = 0; i < owners_.size(); ++i) {
        auto& owner = owners_[i];
        if (!owner.plate.work || !life[i].children_gone) continue;
        if (life[i].gone) { owner = {}; continue; }
        if (owner.pending || (!owner.replaced && owner.name_touched)) {
            if (!clear(owner)) { fault(); return false; }
            owner.pending = false; owner.replaced = false; owner.children_known = false;
            owner.children = {}; owner.extension = 0;
        }
    }
    return true;
}
bool ResultPlate::restore(Owner& owner, PlayedStockBitmap stock) {
    if (!owner_valid(owner)) { fault(); return false; }
    if (owner.pending || !owner.replaced) return true;
    if (!stock || !owner.children_known || !children(owner, false)) { fault(); return false; }
    // Publish the attempted restoration before native code: an SEH or failed
    // postcheck must never cause a duplicate stock call on an uncertain host.
    owner.pending = true; owner.extension = 0;
    if (stock(owner.plate.handle, "waku_style_diff", 0)) { fault(); return false; }
    return true;
}

bool ResultPlate::apply_impl(const PlayedTarget& target, const played_art::Plan& plan,
                            bool custom, PlayedStockBitmap stock, bool early) {
    const auto& chart = target.chart;
    if (chart.surface != played_art::Surface::result) return true;
    const bool required = custom && plan.extends_result_plate;
    const bool want = required && state_.view().enabled();
    if (!collect_impl(&target, early)) return false;
    Owner* owned{};
    for (auto& owner : owners_) if (owner.plate.work && owner.chart.stream == chart.stream &&
                                   owner.chart.player == chart.player) {
        if (!same_chart(owner.chart, chart)) { fault(); return false; }
        owned = &owner; break;
    }
    if (!want) return (!owned || restore(*owned, stock)) && !required;
    const auto layout = played_art::select_result_plate(chart.players, chart.sole, chart.player, chart.leaf);
    if (!layout) { if (owned) restore(*owned, stock); return false; }
    const float extension = plan.width - 200.f;
    if (plan.count != 3 || !std::isfinite(extension) || extension <= 0 || extension > 64) { fault(); return false; }
    if (owned && owned->pending) return false;
    if (!owned) {
        Owner candidate;
        const auto location = locate(target, layout, early, candidate);
        if (location == Location::absent) return false; // Native timeline has not placed it yet.
        if (location != Location::admitted) { fault(); return false; }
        for (auto& owner : owners_) if (!owner.plate.work) { owned = &owner; break; }
        if (!owned) { fault(); return false; }
        *owned = candidate;
    }
    auto& owner = *owned;
    if (owner.replaced && owner.children_known && owner.extension == extension) {
        // Collection admitted this invocation's generation and complete sibling
        // chain (including the named target). No setters or attachment follow
        // on unchanged presentation, so there is no second structural audit.
        if (!early) owner.number = {target.handles[0], calls_.resolve(target.handles[0])};
        if (!name_is(owner, true) ||
            calls_.chart.path(chart.stream, owner.layout.name) != owner.plate.handle ||
            !state_.view().enabled()) { fault(); return false; }
        return true;
    }
    if (!owner_valid(owner) || !siblings(owner, &target, early)) { fault(); return false; }
    if (!early) owner.number = {target.handles[0], calls_.resolve(target.handles[0])};
    if (!owner.name_touched) {
        if ((!owner.cleared && !original(owner)) || !name_is(owner, false) ||
            calls_.chart.path(chart.stream, owner.layout.name) >= 0 || !owner_valid(owner)) { fault(); return false; }
        owner.clear_attempted = false;
        owner.name_touched = true; // Set before any potentially partial rename.
        if (!played_name(memory_, afp_, calls_, chart.stream, owner.plate.work, owner.layout.name)) { fault(); return false; }
    }
    if (calls_.chart.path(chart.stream, owner.layout.name) != owner.plate.handle) { fault(); return false; }
    owner.exposed = true;
    if (!owner_valid(owner) || !name_is(owner, true) ||
        !siblings(owner, &target, early)) { fault(); return false; }
    const bool fresh = !owner.replaced;
    const bool changed = fresh || owner.extension != extension;
    if (fresh) {
        if (!owner.cleared && !original(owner)) { fault(); return false; }
        owner.replaced = true; owner.children_known = false; owner.extension = 0;
        const int result = calls_.attach(owner.plate.handle, component_);
        const bool known = children(owner, true);
        if (result || !known) { fault(); return false; }
    } else if (!children(owner, false)) { fault(); return false; }
    if (!siblings(owner, &target, early)) { fault(); return false; }
    if (changed) for (int i = 0; i < 3; ++i) {
        float xy[2]{}, scale[2]{};
        placement(i, extension, xy, scale);
        if (!node(owner.children[i], chart.stream, owner.plate.work) ||
            calls_.property(owner.children[i].handle, 0x1000, reinterpret_cast<std::intptr_t>(xy)) ||
            !node(owner.children[i], chart.stream, owner.plate.work) ||
            calls_.property(owner.children[i].handle, 0x1003, reinterpret_cast<std::intptr_t>(scale))) {
            fault(); return false;
        }
    }
    owner.extension = extension;
    return true;
}
float ResultPlate::extension_impl(const PlayedChart& chart) {
    if (chart.surface != played_art::Surface::result) return 0;
    std::array<Life, 8> life{};
    PlayedTarget selected; selected.chart = chart;
    if (!collect_impl(&selected, true, &life)) return 0;
    if (!state_.view().enabled()) return 0;
    for (std::size_t i = 0; i < owners_.size(); ++i) {
        auto& owner = owners_[i];
        if (!owner.plate.work || owner.chart.stream != chart.stream || owner.chart.player != chart.player) continue;
        if (life[i].gone || !same_chart(owner.chart, chart)) return 0;
        if (!owner.replaced || owner.pending || !owner.children_known || owner.extension <= 0) return 0;
        if (!name_is(owner, true) || calls_.chart.path(chart.stream, owner.layout.name) != owner.plate.handle) { fault(); return 0; }
        return state_.view().enabled() ? owner.extension : 0;
    }
    return 0;
}
bool ResultPlate::apply(const PlayedTarget& target, const played_art::Plan& plan,
                       bool custom, PlayedStockBitmap stock) noexcept {
    if (!enter()) return false;
    bool result{};
    __try { result = apply_impl(target, plan, custom, stock, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
    busy_ = false;
    return result;
}
void ResultPlate::early(const PlayedChart& chart, const played_art::Plan& plan,
                        bool custom, PlayedStockBitmap stock) noexcept {
    if (!enter()) return;
    __try { PlayedTarget target; target.chart = chart; apply_impl(target, plan, custom, stock, true); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
    busy_ = false;
}
float ResultPlate::extension(const PlayedChart& chart) noexcept {
    if (!enter()) return 0;
    float result{};
    __try { result = extension_impl(chart); }
    __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
    busy_ = false;
    return result;
}
} // namespace point_zero::native
