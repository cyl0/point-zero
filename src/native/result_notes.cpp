#include "result_notes.hpp"
#include "result_plate.hpp"

#include <windows.h>
#include <limits>
#include <cmath>
#include <cstring>
#include <string_view>

namespace point_zero::native {
namespace {
constexpr std::uintptr_t layer_vtable = 0xDC86E0;
constexpr std::size_t sibling_limit = 128;

bool close(float a, float b) noexcept {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) < .0001f;
}
void report(PZ_NativeHost host) {
    constexpr std::string_view message =
        "result NOTES skipped a native operation; later callbacks remain enabled";
    try {
        if (host.log) host.log(host.context, PZ_LOG_WARNING,
            {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
    } catch (...) {}
}
} // namespace

ResultNotes::ResultNotes(const AdapterContext& context, PlayedNativeCalls calls, ResultPlate& plate)
    : game_(context.modules.game.range), afp_(context.modules.afp.range), memory_(context.memory),
      catalog_(context.catalog), state_(context.state), host_(context.host), calls_(calls), plate_(plate) {}

bool ResultNotes::prepare() noexcept {
    if (attempted_) return false;
    attempted_ = true;
    if (state_.view().phase != FeaturePhase::preparing) return false;
    prepared_ = memory_.read &&
        game_.base >= 0x10000 && layer_vtable <= UINTPTR_MAX - game_.base &&
        contains(game_.base, game_.size, game_.base + layer_vtable, sizeof(std::uintptr_t)) &&
        afp_.base >= 0x10000 && 0xE1144 <= UINTPTR_MAX - afp_.base &&
        contains(afp_.base, afp_.size, afp_.base + 0xE1144, sizeof(std::uint64_t)) &&
        played_art::notes_roles().size() == 6;
    return prepared_;
}
void ResultNotes::fault() noexcept {
    if (logged_.exchange(true, std::memory_order_relaxed)) return;
    __try { report(host_); } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

void ResultNotes::remember(const PlayedCapture& capture, const PlayedChart& chart) noexcept {
    if (!prepared_) return;
    __try { remember_impl(capture, chart); } __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
void ResultNotes::remember_impl(const PlayedCapture& capture, const PlayedChart& chart) {
    if (capture.player < 0 || capture.player > 1 ||
        played_capture_kind(capture, game_) != PlayedCaptureKind::result) return;
    auto& saved = captures_[static_cast<std::size_t>(capture.player)];
    saved = {};
    if (chart.chart && chart.surface == played_art::Surface::result && chart.players == 2 &&
        chart.player == capture.player && chart.owner == capture.owner)
        saved = {capture, chart};
}
std::array<float, 2> ResultNotes::wanted() {
    std::array<float, 2> out{};
    if (!state_.view().enabled()) return out;
    for (std::size_t i = 0; i < captures_.size(); ++i) {
        const auto& saved = captures_[i];
        PlayedChart current;
        if (!saved.chart.chart ||
            !read_played_chart(memory_, game_, catalog_, calls_.chart, saved.input, current) ||
            current != saved.chart || current.players != 2) continue;
        // This is an acknowledged, currently verified plate extension. Never
        // infer it from a rating, glyph plan, pending replacement or preference.
        const float extension = plate_.extension(current);
        if (std::isfinite(extension) && extension > 0 && extension <= 64)
            out[i] = i == 0 ? -extension : extension;
    }
    // The plate's verification may itself close the shared feature gate.
    return state_.view().enabled() ? out : std::array<float, 2>{};
}

bool ResultNotes::alive(const Owner& owner) {
    std::uintptr_t vtable{}, source{};
    std::uint32_t stream{}, root_stream{};
    int root_handle{-1};
    return calls_.chart.valid(5, owner.stream) >= 0 &&
        calls_.chart.valid(6, owner.root) >= 0 && calls_.resolve(owner.root) == owner.root_work &&
        read_field(memory_, owner.wrapper, 0, vtable) && vtable == game_.base + layer_vtable &&
        read_field(memory_, owner.wrapper, 8, stream) && stream == owner.stream &&
        calls_.chart.path(stream, "") == owner.root &&
        read_field(memory_, owner.root_work, 0, root_stream) && root_stream == stream &&
        read_field(memory_, owner.root_work, 312, root_handle) && root_handle == owner.root &&
        read_field(memory_, owner.root_work, 320, source) && source == owner.source && source >= 0x10000;
}
bool ResultNotes::matches(const Owner& owner, const Node& node,
                          const played_art::NotesRole& role) {
    std::uintptr_t parent{}, source{}, definition{};
    std::uint64_t anonymous{};
    std::uint32_t stream{}, flags{};
    int depth{}, handle{-1};
    // Anonymous timeline shapes need not have an exposed handle (flag 0x800).
    // The native setter takes their work directly; retain raw generation and
    // complete role checks without naming or exposing those stock objects.
    if (!node.work || node.handle < 0 ||
        calls_.resolve(node.handle) != node.work ||
        !read_field(memory_, node.work, 312, handle) || handle != node.handle ||
        !read_field(memory_, node.work, 0, stream) || stream != owner.stream ||
        !read_field(memory_, node.work, 64, parent) || parent != owner.root_work ||
        !read_field(memory_, node.work, 320, source) || source != owner.source ||
        !read_field(memory_, node.work, 48, definition) || definition < 0x10000 ||
        definition != calls_.definition(source, role.shape) ||
        !read_field(memory_, node.work, 40, depth) || depth != role.depth ||
        !read_field(memory_, node.work, 32, flags) || (flags & 0x40000U) ||
        !read_field(memory_, node.work, 96, anonymous)) return false;
    if (role.name) {
        // Native path lookup publishes exposure (0x800) for a named timeline
        // role. Validate the returned handle after that existing lookup;
        // a live named role may legitimately be unexposed before it.
        if (calls_.chart.path(owner.stream, role.name) != node.handle ||
            calls_.chart.valid(6, node.handle) < 0) return false;
    } else if ((flags & 0x0C000000U) || anonymous != role.anonymous) return false;
    return true;
}

bool ResultNotes::preflight(Owner& owner) {
    if (!alive(owner)) return false;
    const auto roles = played_art::notes_roles();
    std::array<Node, 6> found{};
    std::array<bool, 6> seen{};
    std::uintptr_t work{};
    if (!read_field(memory_, owner.root_work, 72, work)) return false;
    std::size_t visited{};
    while (work) {
        if (visited++ == sibling_limit) return false;
        std::uintptr_t parent{}, next{};
        int depth{};
        if (!read_field(memory_, work, 40, depth) || !read_field(memory_, work, 64, parent) ||
            parent != owner.root_work || !read_field(memory_, work, 88, next)) return false;
        for (std::size_t i = 0; i < roles.size(); ++i) if (depth == roles[i].depth) {
            if (seen[i]) return false;
            seen[i] = true;
            found[i].work = work;
            if (!read_field(memory_, work, 312, found[i].handle)) return false;
        }
        work = next;
    }
    for (std::size_t i = 0; i < roles.size(); ++i) {
        const auto& old = owner.nodes[i];
        auto& next = found[i];
        if (old.work && (old.work != next.work || old.handle != next.handle)) {
            // Absence from the chain is not proof that a shifted node retired.
            if (calls_.resolve(old.handle)) return false;
        } else if (old.work) next = old;
        if (!next.work) continue; // Native entrance frames place roles gradually.
        if (!matches(owner, next, roles[i])) return false;
        // Native timelines can reassert placement between callbacks. Observe
        // only x/y to decide whether to apply the requested absolute position.
        float x{}, y{};
        if (!read_field(memory_, next.work, 176, x) || !read_field(memory_, next.work, 180, y)) return false;
        next.offset = close(y, roles[i].y) ? x - roles[i].x : std::numeric_limits<float>::quiet_NaN();
    }
    if (!alive(owner)) return false;
    owner.nodes = found; // Publish admission only after EVERY existing sibling.
    return true;
}
bool ResultNotes::move(Owner& owner, std::size_t index, float offset) {
    auto& node = owner.nodes[index];
    const auto& role = played_art::notes_roles()[index];
    if (!node.work) return true;
    if (!alive(owner) || !matches(owner, node, role)) return false;
    if (close(node.offset, offset)) return true;
    const float xy[]{role.x + offset, role.y};
    if (!played_position(memory_, afp_, calls_, owner.stream, node.work, xy)) return false;
    node.offset = offset;
    return true;
}

void ResultNotes::collect_impl() {
    for (auto& owner : owners_) if (owner.wrapper) {
        const bool stream_gone = calls_.chart.valid(5, owner.stream) < 0;
        const bool root_gone = calls_.chart.valid(6, owner.root) < 0;
        if (stream_gone || root_gone) {
            bool retired = true;
            for (const auto& node : owner.nodes)
                if (node.work && calls_.resolve(node.handle)) retired = false;
            if (retired) { owner = {}; continue; }
            fault(); // Neither stream nor root loss proves child retirement.
        } else if (!alive(owner)) fault();
    }
}
void ResultNotes::update(std::uintptr_t wrapper) noexcept {
    if (!prepared_) return;
    __try { update_impl(wrapper); } __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
void ResultNotes::update_impl(std::uintptr_t wrapper) {
    collect_impl();
    Owner* owner{};
    for (auto& candidate : owners_) if (candidate.wrapper == wrapper && wrapper) { owner = &candidate; break; }
    const auto offsets = wanted();
    if (!owner) {
        if (!state_.view().enabled() || (offsets[0] == 0 && offsets[1] == 0)) return;
        std::uintptr_t vtable{};
        std::uint32_t stream{};
        if (!read_field(memory_, wrapper, 0, vtable) || vtable != game_.base + layer_vtable ||
            !read_field(memory_, wrapper, 8, stream) || calls_.chart.valid(5, stream) < 0) return;
        for (const auto& candidate : owners_) if (candidate.wrapper && candidate.stream == stream) {
            fault(); return; // A live generation cannot silently acquire another wrapper.
        }
        for (auto& candidate : owners_) if (!candidate.wrapper) { owner = &candidate; break; }
        if (!owner) { fault(); return; }
        Owner candidate;
        candidate.wrapper = wrapper; candidate.stream = stream;
        candidate.root = calls_.chart.path(stream, "");
        if (candidate.root < 0 || calls_.chart.valid(6, candidate.root) < 0 ||
            !(candidate.root_work = calls_.resolve(candidate.root)) ||
            !read_field(memory_, candidate.root_work, 320, candidate.source) || !preflight(candidate)) {
            fault(); return;
        }
        *owner = candidate;
    } else if (!preflight(*owner)) { fault(); return; }

    // One complete role/owner admission is enough when this invocation makes
    // no setters. Each actual move below retains its fresh pre/post checks.
    const bool enabled = state_.view().enabled();
    bool changed{};
    for (std::size_t i = 0; i < owner->nodes.size(); ++i) {
        const auto& node = owner->nodes[i];
        const float offset = enabled ? offsets[played_art::notes_roles()[i].player] : 0;
        if (node.work && !close(node.offset, offset)) changed = true;
    }
    if (!changed && (!enabled || state_.view().enabled())) return;

    // Known owners reach this path even after OFF, BATTLE, scene/mode changes,
    // unavailable/uncertain/stopped state. Only their verified poses can restore.
    for (std::size_t i = 0; i < owner->nodes.size(); ++i) {
        const float offset = state_.view().enabled() ? offsets[played_art::notes_roles()[i].player] : 0;
        if (!move(*owner, i, offset)) { fault(); return; }
    }
    // A native call may close the shared gate after an earlier sibling moved.
    // Revalidate the complete set before retiring any such verified offsets.
    if (!state_.view().enabled()) {
        bool shifted{};
        for (const auto& node : owner->nodes) if (node.work && node.offset != 0) shifted = true;
        if (shifted) {
            if (!preflight(*owner)) { fault(); return; }
            for (std::size_t i = 0; i < owner->nodes.size(); ++i)
                if (!move(*owner, i, 0)) { fault(); return; }
        }
    }
}
} // namespace point_zero::native
