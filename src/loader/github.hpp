#pragma once

#include "release.hpp"
#include <optional>

namespace point_zero::loader {
struct PackageInfo {
    std::uint64_t bytes{};
    std::string sha256;
};
struct GitHubRelease {
    std::string tag, published_at;
    std::uint64_t release_id;
    PackageInfo package;
    std::string package_error;
};
struct ReleasePage {
    std::size_t count;
    std::optional<GitHubRelease> newest;
};
// Versioned releases use publication time, the detected game major and
// GitHub's prerelease flag. Titles, notes, asset labels and datecodes are not
// selection inputs. Exact legacy fixed tags remain usable within their major.
ReleasePage parse_github_releases(std::string_view json, Channel requested, std::uint32_t major = 33);
bool newer_github_release(const GitHubRelease&, const GitHubRelease&) noexcept;
// Page winners retain compact asset data or its validation error. Only the
// final winner's error matters; withdrawn older assets cannot veto it.
// Never trust an API-supplied download URL.
const PackageInfo& selected_package(const GitHubRelease&);
} // namespace point_zero::loader
