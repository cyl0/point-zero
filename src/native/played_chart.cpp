#include "build_bindings.hpp"
#include "played_chart.hpp"

#include <windows.h>
#include <algorithm>
#include <cstdio>

namespace point_zero::native {
namespace {
// Final-only bindings from retained played_context.inl/native_resource_bindings.








constexpr std::size_t resource_stride = 0x518, resource_count = 512;

template<class T> bool global(MemoryView memory, ImageRange game,
                             std::uintptr_t rva, T& out) noexcept {
    return rva <= UINTPTR_MAX - game.base &&
        contains(game.base, game.size, game.base + rva, sizeof(T)) &&
        read_field(memory, game.base + rva, 0, out);
}
struct Mode {
    std::array<int, 2> active{};
    int dp{}, count{}, sole{-1}, style{};
};
bool read_mode(MemoryView memory, ImageRange game, PlayedChartCalls calls, Mode& out) noexcept {
    out.style = calls.style();
    if (!global(memory, game, game_bindings(game).played_chart.active_flags, out.active) ||
        !global(memory, game, game_bindings(game).played_chart.dp_flag, out.dp) || out.dp < 0 || out.dp > 1) return false;
    for (const int flag : out.active) if (flag < 0 || flag > 1) return false;
    out.sole = out.active[0] ? 0 : (out.active[1] ? 1 : -1);
    out.count = out.active[0] + out.active[1];
    if (out.dp && out.count == 2) out.count = 1;
    return out.count > 0 && out.style >= 0 && out.style <= 1;
}
struct Observation {
    std::uintptr_t music{}, layer{}, vtable{};
    std::uint32_t stream{};
    int player{}, style{}, difficulty{}, level{}, song{};
    std::uintptr_t resource{};
    std::uint32_t package{};
};
bool published(MemoryView memory, ImageRange game, const Observation& out) noexcept {
    std::uintptr_t music{};
    int difficulty{};
    return out.player >= 0 && out.player <= 1 &&
        global(memory, game, game_bindings(game).played_chart.published_music, music) && music == out.music &&
        global(memory, game, game_bindings(game).played_chart.published_difficulties + 4ULL * out.player, difficulty) &&
        difficulty == out.difficulty;
}
bool stepup(MemoryView memory, ImageRange game, const PlayedCapture& capture,
             const Mode& mode, Observation& out) noexcept {
    if (game_bindings(game).played_chart.stepup_callback > UINTPTR_MAX - game.base ||
        !contains(game.base, game.size, game.base + game_bindings(game).played_chart.stepup_callback, 1)) return false;
    StepUpSource source;
    std::uintptr_t callback{};
    out.player = mode.sole;
    if (!read_stepup_source_identity(memory, game, capture.owner, source) ||
        !read_field(memory, capture.owner - game_bindings(game).played_chart.stepup_owner_offset, 0x98, callback) ||
        callback != game.base + game_bindings(game).played_chart.stepup_callback) return false;
    out.resource = source.resource; out.package = source.package; out.layer = source.layer;
    return read_field(memory, capture.owner, 56, out.music) &&
        read_field(memory, capture.owner, 64, out.style) &&
        read_field(memory, capture.owner, 68, out.difficulty) &&
        read_field(memory, capture.owner, 72, out.level) && published(memory, game, out);
}
bool observe(MemoryView memory, ImageRange game, const PlayedCapture& capture,
             PlayedCaptureKind kind, const Mode& mode, Observation& out) noexcept {
    if (capture.owner < 0x10000) return false;
    if (kind == PlayedCaptureKind::confirmation_first || kind == PlayedCaptureKind::confirmation_second) {
        const unsigned copy = kind == PlayedCaptureKind::confirmation_second ? 1U : 0U;
        if (copy && mode.count != 2) return false;
        out.player = mode.count == 1 ? mode.sole : static_cast<int>(copy);
        if (!read_field(memory, capture.owner, 40, out.music) ||
            !read_field(memory, capture.owner, 48, out.style) ||
            !read_field(memory, capture.owner, 52 + 4 * copy, out.difficulty) ||
            !read_field(memory, capture.owner, 60 + 4 * copy, out.level) ||
            !read_field(memory, capture.owner, 8, out.layer)) return false;
    } else if (kind == PlayedCaptureKind::stepup) {
        if (!stepup(memory, game, capture, mode, out)) return false;
    } else {
        out.music = capture.music; out.player = capture.player;
        out.style = capture.style; out.difficulty = capture.difficulty;
        out.level = capture.native_level;
        if (out.player < 0 || out.player > 1) return false;
        // The reviewed 9493B0/949430 getters read these committed fields.
        // Course fallback is already published here; never reconstruct it.
        if (!published(memory, game, out)) return false;
        if (kind == PlayedCaptureKind::hud) {
            const auto offset = mode.dp ? 16 + 8ULL * out.player : 8ULL;
            if (!read_field(memory, capture.owner, offset, out.layer)) return false;
        } else out.layer = capture.owner;
    }
    // A physical DP flag paired with an SP chart is BATTLE/DBM. Decline this
    // observation even before the caller's shared FeatureState check.
    // HUD's native eligibility helper can explicitly enable a physical side.
    // Results test physical activity. Neither path restricts DP to the logical
    // sole player: both physical slots may emit while logical count is one.
    if (out.player < 0 || out.player > 1 ||
        (kind != PlayedCaptureKind::hud && !mode.active[out.player]) ||
        out.style < 0 || out.style > 1 || out.style != mode.dp || out.style != mode.style ||
        out.difficulty < 0 || out.difficulty > 4 || out.level < 1 || out.level > 12 ||
        !read_field(memory, out.music, 0x67C, out.song) || out.song <= 0 ||
        !read_field(memory, out.layer, 0, out.vtable) ||
        game_bindings(game).played_chart.layer_vtable > UINTPTR_MAX - game.base ||
        !contains(game.base, game.size, game.base + game_bindings(game).played_chart.layer_vtable, sizeof(std::uintptr_t)) ||
        out.vtable != game.base + game_bindings(game).played_chart.layer_vtable ||
        !read_field(memory, out.layer, 8, out.stream)) return false;
    return true;
}
bool resolve(MemoryView memory, ImageRange game, const Catalog& catalog,
             PlayedChartCalls calls, const PlayedCapture& capture, PlayedChart& out) noexcept {
    if (!memory.read || game.base < 0x10000) return false;
    const auto kind = played_capture_kind(capture, game);
    if (kind == PlayedCaptureKind::unknown) return false;
    Mode mode;
    Observation first;
    if (!read_mode(memory, game, calls, mode) || !observe(memory, game, capture, kind, mode, first)) return false;
    const auto* row = catalog.find({first.song, first.style, first.difficulty});
    if (!row || calls.valid(5, first.stream) < 0) return false;
    PlayedChart candidate;
    candidate.owner = capture.owner; candidate.music = first.music;
    candidate.layer = first.layer; candidate.stream = first.stream;
    candidate.player = first.player; candidate.players = mode.count; candidate.chart = row;
    candidate.sole = mode.sole;
    candidate.resource = first.resource; candidate.package = first.package;
    constexpr char letters[] = "bnhal";
    if (kind == PlayedCaptureKind::confirmation_first || kind == PlayedCaptureKind::confirmation_second) {
        candidate.surface = played_art::Surface::confirmation;
        candidate.leaf = kind == PlayedCaptureKind::confirmation_second ? "decide_diff_num_2p" : "decide_diff_num_1p";
        std::snprintf(candidate.expected.data(), candidate.expected.size(),
                      "decide_diff_%c%02d", letters[first.difficulty], first.level);
    } else if (kind == PlayedCaptureKind::stepup) {
        candidate.surface = played_art::Surface::stepup;
        candidate.leaf = "diff";
        std::snprintf(candidate.expected.data(), candidate.expected.size(),
                      "su_diff_%c%02d", letters[first.difficulty], first.level);
    } else if (kind == PlayedCaptureKind::hud) {
        candidate.surface = played_art::Surface::hud;
        candidate.leaf = first.player ? "lvlnum_2p" : "lvlnum_1p";
        std::snprintf(candidate.expected.data(), candidate.expected.size(), "lvlnum_%02d", first.level);
    } else {
        candidate.surface = played_art::Surface::result;
        // observe() validated the captured physical slot and its published
        // difficulty. Both DP callbacks now own the same logical result
        // leaf, including the plate selected for that leaf.
        if (mode.count == 1) candidate.player = mode.sole;
        candidate.leaf = mode.count == 1 ? "diff_number" :
            (first.player ? "diff_number_2p" : "diff_number_1p");
        std::snprintf(candidate.expected.data(), candidate.expected.size(),
                      "diff_%c%02d", letters[first.difficulty], first.level);
    }
    out = candidate;
    return true;
}
} // namespace

PlayedCaptureKind played_capture_kind(const PlayedCapture& capture, ImageRange game) noexcept {
    using enum PlayedCaptureKind;
    if (capture.kind != unknown) {
        switch (capture.kind) {
        case confirmation_first: case confirmation_second: case hud: case result: case stepup:
            return capture.kind;
        default: return unknown;
        }
    }
    if (game.base < 0x10000 || !contains(game.base, game.size, capture.site, 1)) return unknown;
    const auto site = capture.site - game.base;
    if (site == game_bindings(game).played_chart.decide_first) return confirmation_first;
    if (site == game_bindings(game).played_chart.decide_second) return confirmation_second;
    if (site == game_bindings(game).played_chart.hud_site) return hud;
    if (site == game_bindings(game).played_chart.result_site) return result;
    if (site == game_bindings(game).played_chart.stepup_site) return stepup;
    return unknown;
}

bool read_stepup_source_identity(MemoryView memory, ImageRange game, std::uintptr_t owner,
                                 StepUpSource& out) noexcept {
    out = {};
    __try {
        if (owner < 0x10000 + game_bindings(game).played_chart.stepup_owner_offset ||
            game_bindings(game).played_chart.resource_slots > UINTPTR_MAX - game.base ||
            !contains(game.base, game.size, game.base + game_bindings(game).played_chart.resource_slots, resource_stride * resource_count) ||
            game_bindings(game).played_chart.stepup_vtable > UINTPTR_MAX - game.base ||
            !contains(game.base, game.size, game.base + game_bindings(game).played_chart.stepup_vtable, sizeof(std::uintptr_t))) return false;
        StepUpSource candidate;
        std::uintptr_t vtable{};
        int state{};
        constexpr char path[] = "/1/stepup.ifs";
        std::array<char, sizeof(path)> name{};
        const auto first = game.base + game_bindings(game).played_chart.resource_slots;
        if (!read_field(memory, owner - game_bindings(game).played_chart.stepup_owner_offset, 0, vtable) ||
            vtable != game.base + game_bindings(game).played_chart.stepup_vtable ||
            !read_field(memory, owner, 0, candidate.resource) ||
            !contains(first, resource_stride * resource_count, candidate.resource, resource_stride) ||
            (candidate.resource - first) % resource_stride != 0 ||
            !read_field(memory, candidate.resource, 8, name) ||
            !std::equal(name.begin(), name.end(), std::begin(path)) ||
            !read_field(memory, candidate.resource, 0x510, state) || state != 2 ||
            !read_field(memory, candidate.resource, 0x514, candidate.package) ||
            (candidate.package & 0x78000000U) != 0x30000000U ||
            !read_field(memory, owner, 16, candidate.layer)) return false;
        out = candidate;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { out = {}; return false; }
}

bool read_played_chart(MemoryView memory, ImageRange game, const Catalog& catalog,
                       PlayedChartCalls calls, const PlayedCapture& capture, PlayedChart& out) noexcept {
    out = {};
    __try {
        const auto captured = capture; // Native re-entry cannot replace this observation.
        PlayedChart candidate;
        if (!resolve(memory, game, catalog, calls, captured, candidate)) return false;
        out = candidate;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}

bool read_played_target(MemoryView memory, ImageRange game, const Catalog& catalog,
                        PlayedChartCalls calls, const PlayedCapture& capture,
                        std::uintptr_t wrapper, const char* name, PlayedTarget& out) noexcept {
    out = {};
    __try {
        if (!name) return false;
        const auto captured = capture; // Preserve the invocation through native queries.
        PlayedTarget candidate;
        if (!resolve(memory, game, catalog, calls, captured, candidate.chart)) return false;
        std::array<char, 32> requested{};
        bool terminated{};
        for (std::size_t i = 0; i < requested.size(); ++i) {
            requested[i] = name[i];
            if (!requested[i]) { terminated = true; break; }
        }
        if (!terminated || requested != candidate.chart.expected) return false;
        int first{};
        if (!read_field(memory, wrapper, 512, first) ||
            first != calls.path(candidate.chart.stream, candidate.chart.leaf)) return false;
        int mc = first;
        while (mc >= 0) {
            if (candidate.count == candidate.handles.size() || calls.valid(6, mc) < 0) return false;
            for (std::size_t i = 0; i < candidate.count; ++i)
                if (candidate.handles[i] == mc) return false;
            candidate.handles[candidate.count++] = mc;
            mc = calls.relative(mc, 6);
        }
        if (!candidate.count ||
            (candidate.chart.surface == played_art::Surface::stepup && candidate.count != 1)) return false;
        out = candidate;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = {};
        return false;
    }
}
} // namespace point_zero::native
