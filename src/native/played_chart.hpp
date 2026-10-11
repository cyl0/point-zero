#pragma once

#include "catalog.hpp"
#include "memory.hpp"
#include "played_layout.hpp"

#include <array>

namespace point_zero::native {
struct PlayedChartCalls {
    int (__fastcall* valid)(int, std::uint32_t){};
    int (__fastcall* path)(std::uint32_t, const char*){};
    int (__fastcall* relative)(int, int){};
    int (__fastcall* style)(){};
};

// Captured at one of the admitted LDJ2026081900 instruction boundaries below.
// Ordinary/Step Up confirmation read the committed tuple from owner; other fields
// are unused. HUD/result use the native callback's already-resolved played tuple,
// including course fallback, and recheck the published music/difficulty fields.
// This is not a request to resolve song selection.
enum class PlayedCaptureKind { unknown, confirmation_first, confirmation_second, hud, result, stepup };
struct PlayedCapture {
    std::uintptr_t site{}, owner{}, music{};
    int player{}, style{}, difficulty{}, native_level{};
    // Hooks carry the admitted semantic kind directly. site remains physical
    // evidence for hook admission/caller matching, never a result-owner key.
    PlayedCaptureKind kind{PlayedCaptureKind::unknown};
};
// The native hook sets kind. Unknown preserves the existing internal/manual
// site-based callers; invalid explicit kinds always decline.
PlayedCaptureKind played_capture_kind(const PlayedCapture&, ImageRange game) noexcept;
struct PlayedChart {
    played_art::Surface surface{};
    std::uintptr_t owner{}, music{}, layer{};
    std::uint32_t stream{};
    // Result player is the logical leaf owner after physical tuple validation.
    // Other surfaces retain the physical player; capture.player is unchanged.
    int player{}, players{}, sole{-1};
    const CatalogRow* chart{};
    const char* leaf{};
    std::array<char, 32> expected{};
    // Step Up's native source slot/full package generation. Zero for ordinary
    // surfaces, whose private-art ownership is checked by their renderer.
    std::uintptr_t resource{};
    std::uint32_t package{};
    bool operator==(const PlayedChart&) const = default;
};
struct StepUpSource {
    std::uintptr_t resource{}, layer{};
    std::uint32_t package{};
};
// Guarded scene/source-slot/package/layer identity only. Keeps live bounds here;
// callback phase, tuple publication and persistent-generation equality remain
// the caller's separate policies. Failure clears out.
bool read_stepup_source_identity(MemoryView, ImageRange game, std::uintptr_t owner,
                                 StepUpSource& out) noexcept;

struct PlayedTarget {
    PlayedChart chart;
    std::array<int, 8> handles{};
    std::size_t count{};
};

// Selected boundaries: confirmation 601715/601749, HUD 9045FD, result 91C56D,
// Step Up 601F4E. Step Up additionally requires its exact confirmation scene,
// committed published tuple and live /1/stepup.ifs slot/package generation.
// The caller supplies a complete admitted call table and code/register contracts before
// capture. False is ordinary native fallback and clears out; it does not change
// feature state. No allocation, writes, module scan or per-field page queries.
// The observation is valid only within its native callback. Resource/child
// ownership beyond the observations above, named-call matching and publication
// policy are separate checks. This reader does not imply art/hook admission.
// Published tuple checks use the physical capture before result-owner normalization.
bool read_played_chart(MemoryView, ImageRange game, const Catalog&, PlayedChartCalls,
                       const PlayedCapture&, PlayedChart& out) noexcept;

// The named-bitmap wrapper must point to the exact native leaf for this chart.
// This binds a bounded sibling chain to the full current stream handle. Name and
// wrapper are borrowed only during this call; no global pending chart is stored.
bool read_played_target(MemoryView, ImageRange game, const Catalog&, PlayedChartCalls,
                        const PlayedCapture&, std::uintptr_t wrapper, const char* name,
                        PlayedTarget& out) noexcept;
} // namespace point_zero::native
