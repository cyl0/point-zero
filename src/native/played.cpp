#include "played.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
constexpr const char* labels[]{"whole", "point", "fraction"};
constexpr std::uintptr_t layer_vtable = 0xDC86E0;

bool same_layout(const played_art::Plan& a, const played_art::Plan& b) noexcept {
    if (a.count != b.count || a.width != b.width ||
        a.extends_result_plate != b.extends_result_plate) return false;
    for (std::size_t i = 0; i < a.parts.size(); ++i) {
        const auto& x = a.parts[i]; const auto& y = b.parts[i];
        if (x.image != y.image || x.scale != y.scale || x.x != y.x || x.y != y.y) return false;
    }
    return true;
}
bool same_owner(const PlayedChart& a, const PlayedChart& b) noexcept {
    // A surviving native node may display a different chart later. Ownership
    // follows the complete stream, layer, scene and leaf, not the rating key.
    return a.owner == b.owner && a.layer == b.layer && a.stream == b.stream &&
        a.surface == b.surface && a.player == b.player && a.players == b.players &&
        a.sole == b.sole && a.leaf && b.leaf && std::strcmp(a.leaf, b.leaf) == 0 &&
        (a.surface != played_art::Surface::stepup ||
            (a.resource == b.resource && a.package == b.package));
}
} // namespace

PlayedRenderer::PlayedRenderer(const AdapterContext& context, PlayedNativeCalls calls)
    : game_(context.modules.game.range), memory_(context.memory), catalog_(context.catalog),
      state_(context.state), host_(context.host), calls_(calls), plate_(context, calls),
      notes_(context, calls, plate_) {
    if (!memory_.read || !calls.chart.valid || !calls.chart.path || !calls.chart.relative || !calls.chart.style ||
        !calls.package || !calls.image_info || !calls.arguments || !calls.animation_info ||
        !calls.bind || !calls.attach || !calls.control || !calls.property ||
        !calls.path_relative || !calls.resolve || !calls.definition || !calls.select_stream ||
        !calls.restore_stream || !calls.set_name || !calls.clear_name || !calls.position ||
        !calls.timeline_position)
        throw std::invalid_argument("incomplete admitted played native calls");
}

bool PlayedRenderer::same_thread() const noexcept { return thread_ && thread_ == GetCurrentThreadId(); }
bool PlayedRenderer::live() const {
    int state{}; std::uint32_t id{};
    return same_thread() && read_field(memory_, slot_, 0x510, state) && state == 2 &&
        read_field(memory_, slot_, 0x514, id) && id == package_ && calls_.package(family.data()) == id;
}
bool PlayedRenderer::preload_impl(std::uintptr_t slot) {
    if (attempted_) return false;
    attempted_ = true;
    if (state_.view().phase != FeaturePhase::preparing) return false;
    thread_ = GetCurrentThreadId(); slot_ = slot;
    std::array<char, 256> name{};
    if (!read_field(memory_, slot_, 8, name) ||
        std::string_view(name.data(), family.size()) != family || name[family.size()] != '\0' ||
        !read_field(memory_, slot_, 0x514, package_) || (package_ & 0x78000000U) != 0x30000000U ||
        !live()) return false;
    const auto images = played_art::images();
    if (images.size() != images_.size()) return false;
    for (std::size_t i = 0; i < images.size(); ++i) {
        alignas(8) std::array<std::byte, 64> info{};
        if (calls_.image_info(info.data(), package_, images[i].name)) return false;
        if (calls_.arguments(images_[i].args.data(), info.data())) return false;
    }
    std::uint64_t info[8]{};
    if (calls_.animation_info(info, package_, "lab_played_slots") ||
        (std::uint32_t(info[3]) & 0x70000000U) != 0x30000000U) return false;
    component_ = std::uint32_t(info[3]);
    std::fill(std::begin(info), std::end(info), 0);
    if (calls_.animation_info(info, package_, "lab_played_plate") ||
        (std::uint32_t(info[3]) & 0x70000000U) != 0x30000000U ||
        !plate_.prepare(std::uint32_t(info[3])) || !notes_.prepare()) return false;
    ready_ = live();
    return ready_;
}
bool PlayedRenderer::preload(std::uintptr_t slot) noexcept {
    __try { return preload_impl(slot); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void PlayedRenderer::fault() noexcept {
    if (warned_.test_and_set(std::memory_order_relaxed) || !host_.log) return;
    constexpr std::string_view message = "played numeral drawing skipped a native operation; later callbacks remain enabled";
    try { host_.log(host_.context, PZ_LOG_WARNING,
        {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()}); } catch (...) {}
}
bool PlayedRenderer::owned(const Owner& owner) const {
    std::uintptr_t vtable{}; std::uint32_t stream{};
    if (owner.retired || !owner.mc || !owner.chart.leaf ||
        calls_.chart.valid(5, owner.chart.stream) < 0 || calls_.chart.valid(6, owner.mc) < 0 ||
        !read_field(memory_, owner.chart.layer, 0, vtable) || vtable != game_.base + layer_vtable ||
        !read_field(memory_, owner.chart.layer, 8, stream) || stream != owner.chart.stream) return false;
    const auto stepup = owner.chart.surface == played_art::Surface::stepup;
    const auto source_owned = [&] {
        if (!stepup) return true;
        const auto& chart = owner.chart;
        if (std::strcmp(chart.leaf, "diff") != 0) return false;
        StepUpSource source;
        // Persistent source ownership permits the confirmation callback to
        // advance, unlike the capture reader's committed-tuple admission.
        return read_stepup_source_identity(memory_, game_, chart.owner, source) &&
            source.resource == chart.resource && source.package == chart.package &&
            source.layer == chart.layer;
    };
    if (!source_owned()) return false;
    std::array<int, 8> chain{};
    std::size_t count{}; bool found{};
    int mc = calls_.chart.path(stream, owner.chart.leaf);
    while (mc >= 0) {
        if (count == chain.size() || calls_.chart.valid(6, mc) < 0) return false;
        for (std::size_t i = 0; i < count; ++i) if (chain[i] == mc) return false;
        chain[count++] = mc; found |= mc == owner.mc;
        mc = calls_.chart.relative(mc, 6);
    }
    return found;
}
PlayedRenderer::Owner* PlayedRenderer::find(int mc) noexcept {
    for (auto& owner : owners_) if (owner.mc == mc) return &owner;
    return nullptr;
}
void PlayedRenderer::collect() {
    // Plate and NOTES collect at their own result operations. Other named
    // surfaces only need to retire this renderer's numeral attachments.
    for (auto& owner : owners_) if (owner.mc) {
        if (calls_.chart.valid(5, owner.chart.stream) < 0 || calls_.chart.valid(6, owner.mc) < 0)
            owner.retired = true;
        if (owner.retired || owner.restoring) {
            bool gone = !owner.attached || owner.children_known;
            for (const int child : owner.children)
                if (child > 0 && calls_.chart.valid(6, child) >= 0) gone = false;
            if (gone) owner = {};
        }
    }
}

void PlayedRenderer::capture(const PlayedCapture& capture, PlayedStockBitmap stock) noexcept {
    if (!ready_) return;
    if (!same_thread()) { fault(); return; }
    __try {
        if (state_.view().phase == FeaturePhase::preparing) return;
        if (played_capture_kind(capture, game_) != PlayedCaptureKind::result) return;
        PlayedChart chart;
        const bool observed = read_played_chart(memory_, game_, catalog_, calls_.chart, capture, chart);
        notes_.remember(capture, chart); // Also clears a failed/replaced player capture.
        if (!stock || !observed) return;
        bool custom = state_.view().enabled() && chart.chart->assigned;
        if (custom && !live()) { fault(); custom = false; }
        const auto plan = played_art::make(chart.surface, chart.chart->key.difficulty,
                                           chart.chart->effective_tenths);
        plate_.early(chart, plan, custom, stock);
    } __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
void PlayedRenderer::begin_impl(const PlayedCapture& capture, std::uintptr_t wrapper, const char* name) {
    if (!ready_) return;
    if (!same_thread()) { fault(); return; }
    if (++depth_ != 1) return;
    current_ = {}; current_valid_ = false;
    const auto phase = state_.view().phase;
    if (phase == FeaturePhase::preparing || phase == FeaturePhase::stopped) return;
    if (played_capture_kind(capture, game_) == PlayedCaptureKind::unknown) return; // Shadow unknown named calls.
    collect();
    if (state_.view().enabled() && !live()) { fault(); return; }
    // OFF still resolves the actual native target so an owned result plate can
    // return to its stock shape through the same verified binder.
    current_valid_ = read_played_target(memory_, game_, catalog_, calls_.chart,
                                        capture, wrapper, name, current_);
}
void PlayedRenderer::begin(const PlayedCapture& capture, std::uintptr_t wrapper, const char* name) noexcept {
    __try { begin_impl(capture, wrapper, name); }
    __except (EXCEPTION_EXECUTE_HANDLER) { current_valid_ = false; fault(); }
}
void PlayedRenderer::end() noexcept {
    if (same_thread() && depth_ && --depth_ == 0) { current_ = {}; current_valid_ = false; }
}

bool PlayedRenderer::restore(Owner& owner, int mc, const char* name, int mode,
                              int& result, PlayedStockBitmap stock) {
    if (!owned(owner)) { fault(); return false; }
    ++mutations_;
    result = stock(mc, name, mode);
    if (!result) owner.restoring = true;
    else fault();
    return true;
}
bool PlayedRenderer::apply(Owner* owner, int mc, const played_art::Plan& plan, int value,
                            const char* name, int mode, int& result, PlayedStockBitmap stock) {
    if (!owner) for (auto& candidate : owners_) if (!candidate.mc) { owner = &candidate; break; }
    if (!owner) { fault(); return false; }
    const bool fresh = !owner->mc;
    const bool changed = fresh || owner->value != value || !same_layout(owner->plan, plan);
    int rc{};
    if (fresh) {
        *owner = {}; owner->mc = mc; owner->chart = current_.chart;
        ++mutations_;
        rc = stock(mc, name, mode);
        if (rc) { fault(); result = rc; return true; }
        owner->attached = true; // Keep uncertain attachment until its children are known/retired.
        rc = calls_.attach(mc, component_);
        if (!rc) rc = calls_.control(mc, 0xF04, 0);
    }
    for (std::size_t i = 0; i < plan.parts.size() && !rc; ++i) {
        const int child = calls_.path_relative(mc, labels[i]);
        if (child <= 0 || calls_.chart.valid(6, child) < 0 ||
            (!fresh && child != owner->children[i])) { rc = -1; break; }
        for (std::size_t j = 0; j < i; ++j) if (owner->children[j] == child) rc = -1;
        owner->children[i] = child;
    }
    if (!rc) owner->children_known = true;
    // Observe every child before the first glyph write, retaining a partial
    // attachment when one of the native lookups failed.
    for (std::size_t i = 0; i < plan.parts.size() && !rc; ++i) {
        const int child = owner->children[i];
        const auto& part = plan.parts[i];
        if (changed) {
            ++mutations_;
            rc = calls_.bind(child, images_[static_cast<std::size_t>(part.image)].args.data(), 0);
            const float scale[]{part.scale, part.scale}, xy[]{part.x, part.y};
            if (!rc) rc = calls_.property(child, 0x1003, reinterpret_cast<std::intptr_t>(scale));
            if (!rc) rc = calls_.property(child, 0x1000, reinterpret_cast<std::intptr_t>(xy));
            if (!rc) rc = calls_.property(child, 0x101E, 0);
        }
    }
    if (rc) {
        fault();
        return restore(*owner, mc, name, mode, result, stock);
    }
    if (!state_.view().enabled()) return restore(*owner, mc, name, mode, result, stock);
    owner->chart = current_.chart; owner->plan = plan; owner->value = value;
    result = 0;
    return true;
}

bool PlayedRenderer::bitmap_impl(int mc, const char* name, int mode, int& result, PlayedStockBitmap stock) {
    if (!stock || mc <= 0) return false;
    auto* owner = find(mc);
    const auto forward = [&] { return owner ? restore(*owner, mc, name, mode, result, stock) : false; };
    if (state_.view().phase == FeaturePhase::preparing || state_.view().phase == FeaturePhase::stopped)
        return false;
    if (depth_ != 1 || !current_valid_ || !name || mode != 0 ||
        std::strcmp(name, current_.chart.expected.data()) != 0 ||
        std::find(current_.handles.begin(), current_.handles.begin() + current_.count, mc) ==
            current_.handles.begin() + current_.count) return forward();
    if (owner && (!same_owner(owner->chart, current_.chart) || !owned(*owner))) {
        fault(); return false;
    }
    if (owner && owner->restoring) return forward();
    bool custom = state_.view().enabled() && current_.chart.chart->assigned;
    if (custom && !live()) { fault(); custom = false; }
    auto plan = played_art::make(current_.chart.surface, current_.chart.chart->key.difficulty,
                                 current_.chart.chart->effective_tenths);
    custom = custom && plan.count == 3;
    if (custom) {
        for (const auto& part : plan.parts)
            if (part.image < 0 || static_cast<std::size_t>(part.image) >= images_.size() ||
                part.scale != 1.f || !std::isfinite(part.x) || !std::isfinite(part.y)) {
                fault(); custom = false; break;
            }
    }
    if (current_.chart.surface == played_art::Surface::result) {
        if (!plate_.apply(current_, plan, custom, stock)) return forward();
        const auto layout = played_art::select_result_plate(current_.chart.players, current_.chart.sole,
                                                           current_.chart.player, current_.chart.leaf);
        const auto shift = played_art::number_shift(layout, plan.width);
        for (auto& part : plan.parts) part.x += shift;
    }
    if (!custom || !state_.view().enabled()) return forward();
    return apply(owner, mc, plan, current_.chart.chart->effective_tenths, name, mode, result, stock);
}
bool PlayedRenderer::bitmap(int mc, const char* name, int mode, int& result, PlayedStockBitmap stock) noexcept {
    // Wrong-thread entry must not race native-thread owner bookkeeping.
    if (!ready_) return false;
    if (!same_thread()) { fault(); return false; }
    const auto before = mutations_;
    __try {
        const bool handled = bitmap_impl(mc, name, mode, result, stock);
        if (handled || mutations_ == before) return handled;
        // Include writes by nested callbacks, even when an outer native read
        // then returns an error before apply itself attempts any mutation.
        // A status-error decline needs the same containment as a native fault.
        fault(); result = -1;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        fault();
        if (mutations_ != before) { result = -1; return true; }
        return false;
    }
}
void PlayedRenderer::update_notes(std::uintptr_t wrapper) noexcept {
    if (!ready_) return;
    if (!same_thread()) { fault(); return; }
    __try {
        if (state_.view().phase == FeaturePhase::preparing || state_.view().phase == FeaturePhase::stopped) return;
        if (state_.view().enabled() && !live()) fault();
        notes_.update(wrapper);
    } __except (EXCEPTION_EXECUTE_HANDLER) { fault(); }
}
} // namespace point_zero::native
