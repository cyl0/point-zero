#include "staging.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace point_zero::loader {
// Deterministic fault injection at the otherwise unobservable publication
// boundary. Absent from production builds and from the public interface.
#ifdef POINT_ZERO_TEST_STAGING
void staging_before_publish_for_test();
#endif
namespace {
namespace fs = std::filesystem;
constexpr std::size_t manifest_limit = release_limits::manifest_bytes;
constexpr std::size_t remnant_limit = 256;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void check_stop(const std::atomic_bool* stop) {
    if (stop && stop->load(std::memory_order_relaxed)) throw std::runtime_error("startup cancelled");
}
std::string folded(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
// false = plain file, true = plain directory. Include the manifest so even a
// core named release.json/core.dll cannot collide with staging metadata.
using Layout = std::map<std::string, bool>;
Layout layout(const Release& release) {
    Layout result{{"release.json", false}};
    for (const auto& artifact : release.files) {
        const auto name = folded(artifact.path);
        require(result.emplace(name, false).second, "generation file/directory collision");
        for (auto slash = name.find('/'); slash != name.npos; slash = name.find('/', slash + 1)) {
            const auto [it, added] = result.emplace(name.substr(0, slash), true);
            require(added || it->second, "generation file/directory collision");
        }
    }
    return result;
}
bool atomic_remnant(std::string name, const Layout& expected) {
    if (!name.ends_with(".tmp")) return false;
    name.resize(name.size() - 4);
    // Files::put uses <known-file>.<decimal-pid>.<decimal-serial>.tmp.
    for (unsigned field = 0; field < 2; ++field) {
        const auto dot = name.rfind('.');
        if (dot == name.npos || dot + 1 == name.size() ||
            !std::all_of(name.begin() + dot + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }))
            return false;
        name.resize(dot);
    }
    const auto it = expected.find(name);
    return it != expected.end() && !it->second;
}
bool exists(Files& files, const std::string& name) {
    return fs::exists(files.path(name)); // path() rejects reparse traversal, including the leaf.
}
void inspect_tree(Files& files, const std::string& prefix, const Layout& expected,
                  bool staging, const std::atomic_bool* stop) {
    if (!exists(files, prefix)) return;
    require(fs::is_directory(files.path(prefix)), "generation must be a plain directory");
    std::vector<std::string> directories{""}, remnants;
    std::set<std::string> seen;
    std::size_t visited = 0;
    for (std::size_t index = 0; index < directories.size(); ++index) {
        const auto relative = directories[index];
        // Pin the directory being enumerated, including its ancestors. A path
        // check alone would leave a gap before directory_iterator opens it.
        Files directory(files.path(prefix + relative), stop);
        for (const auto& entry : fs::directory_iterator(directory.root())) {
            check_stop(stop);
            require(++visited <= expected.size() + remnant_limit, "generation entry limit");
            const auto name = relative.empty() ? entry.path().filename().string() :
                relative.substr(1) + "/" + entry.path().filename().string();
            const auto path = files.path(prefix + "/" + name);
            const auto lower = folded(name);
            require(seen.insert(lower).second, "generation case-alias collision");
            const auto it = expected.find(lower);
            if (it == expected.end()) {
                require(staging && remnants.size() < remnant_limit && atomic_remnant(lower, expected) &&
                        fs::is_regular_file(path), "unexpected generation entry");
                remnants.push_back(prefix + "/" + name);
            } else if (it->second) {
                require(fs::is_directory(path), "generation directory occupied by a file");
                directories.push_back("/" + name);
            } else {
                require(fs::is_regular_file(path), "generation file occupied by a directory");
            }
        }
    }
    // Never walk/delete an unknown subtree. These are only Files::put names
    // adjacent to a known artifact or manifest in this owned staging ID.
    for (const auto& name : remnants) { check_stop(stop); files.erase(name); }
}
void verify(Files& files, const std::string& prefix, const Release& release,
            const Layout& expected, const std::atomic_bool* stop) {
    inspect_tree(files, prefix, expected, false, stop);
    std::vector<std::unique_ptr<FilePin>> pins;
    auto manifest = files.pin(prefix + "/release.json");
    require(digest(manifest->read(manifest_limit), stop) == release.id,
            "generation manifest identity mismatch");
    pins.push_back(std::move(manifest));
    for (const auto& artifact : release.files) {
        check_stop(stop);
        auto pin = files.pin(prefix + "/" + artifact.path);
        require(pin->size() == artifact.bytes, "artifact length mismatch");
        require(digest(pin->read(static_cast<std::size_t>(artifact.bytes)), stop) == artifact.sha256,
                "artifact hash mismatch");
        pins.push_back(std::move(pin));
    }
    check_stop(stop);
    // All files stay pinned through validation, but these pins (and their parent
    // directory handles) must close before Windows can move the staging directory.
}
} // namespace

std::optional<std::string> stage_generation(
    Files& files, const Release& release, std::string_view manifest,
    const Release& installed, Channel requested,
    const std::function<std::string(const Artifact&, std::size_t)>& fetch,
    const std::atomic_bool* cancelled, std::uint32_t game_major) {
    check_stop(cancelled);
    if (!select_update(installed, release, requested, game_major)) return {};
    const auto expected = layout(release);
    const auto staging = "staging/" + release.id, version = "versions/" + release.id;
    for (const auto& [name, directory] : expected) {
        (void)directory;
        require(valid_relative_path(staging + "/" + name) && valid_relative_path(version + "/" + name),
                "generation owned path length limit");
    }
    // The supplied Files may not carry this stop token. Use a confined view with
    // it for chunked reads/writes; the caller's Transaction retains the lock.
    Files io(files.root(), cancelled);
    if (exists(io, version)) {
        verify(io, version, release, expected, cancelled);
        return release.id;
    }
    if (const auto prior = io.read(staging + "/release.json", manifest_limit))
        require(digest(*prior, cancelled) == release.id, "staging manifest identity mismatch");
    inspect_tree(io, staging, expected, true, cancelled);
    io.put(staging + "/release.json", manifest);
    for (const auto& artifact : release.files) {
        check_stop(cancelled);
        const auto name = staging + "/" + artifact.path;
        const auto limit = static_cast<std::size_t>(artifact.bytes); // parse_release bounds this to 128 MiB.
        if (exists(io, name)) {
            auto pin = io.pin(name);
            if (pin->size() == artifact.bytes && digest(pin->read(limit), cancelled) == artifact.sha256)
                continue;
        }
        auto bytes = fetch(artifact, limit);
        check_stop(cancelled);
        require(bytes.size() == artifact.bytes, "downloaded artifact length mismatch");
        require(digest(bytes, cancelled) == artifact.sha256, "downloaded artifact hash mismatch");
        io.put(name, bytes);
    }
    verify(io, staging, release, expected, cancelled);
#ifdef POINT_ZERO_TEST_STAGING
    staging_before_publish_for_test();
#endif
    check_stop(cancelled);
    // Recheck after callbacks and revalidation: never let Files::rename's
    // REPLACE_EXISTING flag replace even a colliding plain file at this ID.
    if (exists(io, version)) {
        verify(io, version, release, expected, cancelled);
        return release.id;
    }
    check_stop(cancelled);
    io.rename(staging, version);
    return release.id;
}
} // namespace point_zero::loader
