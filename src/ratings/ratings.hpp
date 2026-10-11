#pragma once

#include "affine.hpp"
#include <array>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace point_zero::ratings {

// Internal C++ interface. Only the separately versioned C interface crosses DLLs.
inline constexpr std::string_view pipeline_revision = "point-zero-ratings-6";
enum class Adaptation { none, affine };

struct Key {
    std::int32_t song{};
    std::int32_t style{};
    std::int32_t difficulty{};
    auto operator<=>(const Key&) const = default;
};

struct Config {
    std::array<std::vector<std::filesystem::path>, 2> files;
    std::array<Adaptation, 2> adaptation{Adaptation::affine, Adaptation::affine};
    std::array<std::string, 2> reference{"last", "last"}; // Used only by affine adaptation.
};

struct SourceInfo {
    std::filesystem::path path;
    std::string sha256, adapter, id, revision;
};

struct Transform {
    int style{}, source{}, reference{-1};
    affine::Fit fit;
    std::size_t censored_pairs{};
};

struct Assignment {
    Key key;
    double supplied{}, adapted{};
    int tenths{}, source{};
    std::string entry;
};

struct Snapshot {
    Config config;
    std::vector<SourceInfo> sources;
    std::vector<Transform> transforms;
    // Globally ordered by Key, independent of configured style/file ordering.
    std::vector<Assignment> assignments;
    std::string cache_key;
    bool cache_hit{};
    std::size_t files_parsed{};

    const Assignment* find(Key key) const noexcept;
};

// The owner supplies immutable bytes by logical absolute path. No hidden disk
// fallback, globals, game calls, workers, logging or cache writes occur here.
using ReadInput = std::function<std::string(const std::filesystem::path&)>;
Config parse_config(const std::filesystem::path& absolute_config, std::string_view bytes);
Snapshot prepare(const std::filesystem::path& absolute_config, const ReadInput& read,
                 std::string_view optional_cache = {});
std::string serialize_cache(const Snapshot& snapshot);
int normalize(double finite_value);
std::string sha256(std::string_view bytes);

} // namespace point_zero::ratings
