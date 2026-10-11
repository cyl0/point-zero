#include "github.hpp"

#include <rapidjson/document.h>
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace point_zero::loader {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
const rapidjson::Value& field(const rapidjson::Value& object, const char* name) {
    require(object.IsObject() && object.HasMember(name), "incomplete GitHub release metadata");
    return object[name];
}
std::string_view text(const rapidjson::Value& value) {
    require(value.IsString(), "invalid GitHub release text");
    return {value.GetString(), value.GetStringLength()};
}
std::uint32_t tag_major(std::string_view tag, Channel requested) {
    if (tag == (requested == Channel::stable ? "iidx33-stable" : "iidx33-preview")) return 33;
    // Keep the selected tag in this repository's download path. Version tags
    // need no URL escaping and cannot redirect a package to another host/path.
    if (tag.size() > 64 || !std::all_of(tag.begin(), tag.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') || c == '.' || c == '-';
        })) return 0;
    if (tag.starts_with('v')) tag.remove_prefix(1);
    const auto dot = tag.find('.');
    if (dot == tag.npos || dot == 0 || dot > 3 || dot + 1 == tag.size() || tag[0] == '0') return 0;
    std::uint32_t major{};
    for (const auto c : tag.substr(0, dot)) {
        if (c < '0' || c > '9') return 0;
        major = major * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return major;
}
std::string_view publication_time(const rapidjson::Value& doc) {
    const auto value = text(field(doc, "published_at"));
    constexpr std::string_view shape = "0000-00-00T00:00:00Z";
    require(value.size() == shape.size(), "invalid GitHub publication time");
    for (std::size_t i = 0; i != shape.size(); ++i)
        require(shape[i] == '0' ? value[i] >= '0' && value[i] <= '9' : value[i] == shape[i],
                "invalid GitHub publication time");
    return value;
}
std::uint64_t release_id(const rapidjson::Value& doc) {
    const auto& id = field(doc, "id");
    require(id.IsUint64() && id.GetUint64() != 0, "invalid GitHub release identity");
    return id.GetUint64();
}
PackageInfo package(const rapidjson::Value& doc) {
    const auto& assets = field(doc, "assets");
    require(assets.IsArray() && assets.Size() <= 256, "invalid GitHub release assets");
    const rapidjson::Value* zip = nullptr;
    for (const auto& asset : assets.GetArray()) {
        if (text(field(asset, "name")) != "point-zero.zip") continue;
        require(zip == nullptr, "duplicate POINT ZERO ZIP");
        zip = &asset;
    }
    require(zip && text(field(*zip, "state")) == "uploaded", "POINT ZERO ZIP unavailable");
    const auto& size = field(*zip, "size");
    require(size.IsUint64() && size.GetUint64() != 0 && size.GetUint64() <= 512ULL * 1024 * 1024,
            "POINT ZERO ZIP size limit");
    auto hash = text(field(*zip, "digest"));
    require(hash.starts_with("sha256:"), "GitHub ZIP checksum unavailable");
    hash.remove_prefix(7);
    require(valid_sha256(hash), "invalid GitHub ZIP checksum");
    return PackageInfo{size.GetUint64(), std::string(hash)};
}
} // namespace
bool newer_github_release(const GitHubRelease& left, const GitHubRelease& right) noexcept {
    return std::tie(left.published_at, left.release_id) > std::tie(right.published_at, right.release_id);
}
ReleasePage parse_github_releases(std::string_view bytes, Channel requested, std::uint32_t major) {
    require(major > 0 && major <= 999, "invalid update major");
    require(!bytes.empty() && bytes.size() <= 2 * 1024 * 1024 && bytes.find('\0') == bytes.npos,
            "invalid GitHub release metadata");
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(bytes.data(), bytes.size());
    require(!doc.HasParseError() && doc.IsArray() && doc.Size() <= 100, "invalid GitHub release list");
    const rapidjson::Value* newest = nullptr;
    std::string_view latest_time;
    std::uint64_t latest_id{};
    for (const auto& release : doc.GetArray()) {
        const auto& draft = field(release, "draft");
        const auto& preview = field(release, "prerelease");
        require(draft.IsBool() && preview.IsBool(), "invalid GitHub release label");
        if (draft.GetBool() || preview.GetBool() != (requested == Channel::preview)) continue;
        if (tag_major(text(field(release, "tag_name")), requested) != major) continue;
        const auto time = publication_time(release);
        const auto id = release_id(release);
        if (!newest || std::tie(time, id) > std::tie(latest_time, latest_id)) {
            newest = &release;
            latest_time = time;
            latest_id = id;
        }
    }
    if (!newest) return {doc.Size(), std::nullopt};
    GitHubRelease result{std::string(text(field(*newest, "tag_name"))), std::string(latest_time), latest_id, {}, {}};
    try { result.package = package(*newest); }
    catch (const std::runtime_error& e) { result.package_error = e.what(); }
    return {doc.Size(), std::move(result)};
}
const PackageInfo& selected_package(const GitHubRelease& release) {
    require(release.package_error.empty(), release.package_error.c_str());
    return release.package;
}
} // namespace point_zero::loader
