#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string>
#include <string_view>
#include <vector>

namespace point_zero::loader {
// Manifest-reader format limits; transport and staging limits may differ.
namespace release_limits {
inline constexpr std::size_t manifest_bytes = 1024U * 1024U;
inline constexpr std::size_t artifact_count = 256;
inline constexpr std::uint64_t artifact_bytes = 128ULL * 1024ULL * 1024ULL;
inline constexpr std::uint64_t total_artifact_bytes = 512ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t path_bytes = 240;
}

enum class Channel { stable, preview };
enum class Role { core, art, ratings, updater };
struct Artifact {
    std::string path;
    Role role;
    std::string sha256;
    std::uint64_t bytes;
    std::string target; // ratings only, relative to the point-zero directory
};
struct Release {
    std::string id; // SHA256 of the exact plain manifest bytes
    Channel channel;
    std::uint64_t sequence;
    std::vector<Artifact> files;
    std::uint32_t core_abi{5};
    std::uint32_t major{33};
    const Artifact& core() const;
    // Non-null after schema-4 parsing; hand-built Release values may lack it.
    const Artifact* updater() const noexcept;
};

// Plain JSON envelope: schema4, updater ABI1, positive game/release major.
// Update selection follows the detected game major. Core ABI is
// descriptive here: the selected updater checks it before loading the core.
// Requires exactly one core and one updater.dll; art/ratings roles are optional.
// The exact manifest is bounded by release_limits and hashed without normalization.
// No filesystem, transport, game-module checks or rating parser in this module.
Release parse_release(std::string_view plain_manifest);
bool select_update(const Release& installed, const Release& candidate, Channel requested,
                   std::uint32_t game_major = 0) noexcept;
bool valid_relative_path(std::string_view) noexcept;
bool valid_sha256(std::string_view) noexcept;
std::string digest(std::string_view bytes, const std::atomic_bool* cancelled = nullptr);
} // namespace point_zero::loader
