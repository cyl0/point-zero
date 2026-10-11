#include "release.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <rapidjson/document.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <utility>

namespace point_zero::loader {
namespace {
using Hash = std::array<std::uint8_t, 32>;
using Value = rapidjson::Value;

void require(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}

struct Algorithm {
    BCRYPT_ALG_HANDLE value = nullptr;
    explicit Algorithm(LPCWSTR name) {
        require(BCryptOpenAlgorithmProvider(&value, name, nullptr, 0) >= 0, "CNG provider failed");
    }
    ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
    Algorithm(const Algorithm&) = delete;
    Algorithm& operator=(const Algorithm&) = delete;
};
struct HashHandle {
    BCRYPT_HASH_HANDLE value = nullptr;
    HashHandle() = default;
    ~HashHandle() { if (value) BCryptDestroyHash(value); }
    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;
};

void check_cancelled(const std::atomic_bool* cancelled) {
    if (cancelled && cancelled->load(std::memory_order_relaxed))
        throw std::runtime_error("startup cancelled");
}

Hash sha256(std::string_view bytes, const std::atomic_bool* cancelled = nullptr) {
    check_cancelled(cancelled);
    Algorithm algorithm(BCRYPT_SHA256_ALGORITHM);
    DWORD object_size = 0;
    DWORD written = 0;
    require(BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                             &written, 0) >= 0 && written == sizeof(object_size) && object_size != 0,
            "SHA256 object size failed");
    std::vector<std::uint8_t> object(object_size);
    HashHandle hash;
    require(BCryptCreateHash(algorithm.value, &hash.value, object.data(), object_size,
                            nullptr, 0, 0) >= 0, "SHA256 creation failed");
    // Streaming also keeps digest() usable for transaction inputs larger than ULONG_MAX.
    while (!bytes.empty()) {
        check_cancelled(cancelled);
        const auto length = static_cast<ULONG>(std::min<std::size_t>(bytes.size(), 1024 * 1024));
        require(BCryptHashData(hash.value,
                              reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                              length, 0) >= 0, "SHA256 input failed");
        bytes.remove_prefix(length);
    }
    check_cancelled(cancelled);
    Hash result{};
    require(BCryptFinishHash(hash.value, result.data(), static_cast<ULONG>(result.size()), 0) >= 0,
            "SHA256 finish failed");
    return result;
}

std::string hex(const Hash& hash) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(hash.size() * 2, '\0');
    for (std::size_t index = 0; index < hash.size(); ++index) {
        result[index * 2] = digits[hash[index] >> 4];
        result[index * 2 + 1] = digits[hash[index] & 15];
    }
    return result;
}

char lower_ascii(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}
bool equal_path(std::string_view left, std::string_view right) noexcept {
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(),
                   [](char a, char b) { return lower_ascii(a) == lower_ascii(b); });
}
struct PathLess {
    bool operator()(const std::string& left, const std::string& right) const noexcept {
        return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end(),
            [](char a, char b) { return lower_ascii(a) < lower_ascii(b); });
    }
};
bool under(std::string_view path, std::string_view prefix) noexcept {
    return path.size() > prefix.size() && equal_path(path.substr(0, prefix.size()), prefix);
}

// Bound nesting during an iterative SAX pass, before building a DOM whose
// destruction recursively releases containers. This also
// checks duplicate decoded field names inside otherwise uninterpreted metadata.
// The root container is depth 1; scalars do not add container depth.
struct JsonGuard : rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonGuard> {
    struct Frame {
        bool object;
        std::set<std::string> names;
    };
    std::vector<Frame> frames;
    bool start(bool object) {
        require(frames.size() < 16, "JSON nesting bound");
        frames.push_back({object, {}});
        return true;
    }
    bool StartObject() { return start(true); }
    bool StartArray() { return start(false); }
    bool Key(const char* text, rapidjson::SizeType length, bool) {
        require(!frames.empty() && frames.back().object, "invalid JSON object");
        require(frames.back().names.emplace(text, length).second, "duplicate JSON field");
        return true;
    }
    bool EndObject(rapidjson::SizeType) { frames.pop_back(); return true; }
    bool EndArray(rapidjson::SizeType) { frames.pop_back(); return true; }
};

rapidjson::Document parse(std::string_view bytes) {
    // RapidJSON treats a raw NUL as end-of-input, even in a bounded stream.
    require(!bytes.empty() && bytes.find('\0') == std::string_view::npos, "invalid JSON");
    constexpr unsigned flags = rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag;
    rapidjson::MemoryStream stream(bytes.data(), bytes.size());
    rapidjson::Reader reader;
    JsonGuard guard;
    require(static_cast<bool>(reader.Parse<flags>(stream, guard)), "invalid JSON");
    rapidjson::Document result;
    result.Parse<flags>(bytes.data(), bytes.size());
    require(!result.HasParseError() && result.IsObject(), "invalid JSON object");
    return result;
}

const Value& field(const Value& object, const char* name) {
    require(object.IsObject(), "expected JSON object");
    const auto member = object.FindMember(name);
    require(member != object.MemberEnd(), "missing JSON field");
    return member->value;
}
std::string_view text(const Value& value) {
    require(value.IsString(), "expected JSON string");
    return {value.GetString(), value.GetStringLength()};
}
bool integer(const Value& value, unsigned expected) {
    return value.IsUint() && value.GetUint() == expected;
}
std::uint32_t game_major(const Value& value) {
    require(value.IsUint() && value.GetUint() > 0 && value.GetUint() <= 999, "invalid game major");
    return value.GetUint();
}
} // namespace

bool valid_relative_path(std::string_view path) noexcept {
    if (path.empty() || path.size() > release_limits::path_bytes) return false;
    std::size_t offset = 0;
    while (offset < path.size()) {
        const auto slash = path.find('/', offset);
        const auto end = slash == std::string_view::npos ? path.size() : slash;
        const auto component = path.substr(offset, end - offset);
        if (component.empty() || component == "." || component == ".." || component.back() == '.')
            return false;
        for (const char c : component) {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
        }
        const auto base = component.substr(0, component.find('.'));
        if (equal_path(base, "con") || equal_path(base, "prn") ||
            equal_path(base, "aux") || equal_path(base, "nul")) return false;
        if (base.size() == 4 && base[3] >= '1' && base[3] <= '9' &&
            (equal_path(base.substr(0, 3), "com") || equal_path(base.substr(0, 3), "lpt"))) return false;
        if (slash == std::string_view::npos) return true;
        offset = slash + 1;
    }
    return false; // A trailing slash is an empty final component.
}

bool valid_sha256(std::string_view value) noexcept {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::string digest(std::string_view bytes, const std::atomic_bool* cancelled) {
    return hex(sha256(bytes, cancelled));
}

const Artifact& Release::core() const {
    const Artifact* result = nullptr;
    for (const auto& artifact : files) {
        if (artifact.role != Role::core) continue;
        require(result == nullptr, "multiple core artifacts");
        result = &artifact;
    }
    require(result != nullptr, "missing core artifact");
    return *result;
}

const Artifact* Release::updater() const noexcept {
    for (const auto& artifact : files) if (artifact.role == Role::updater) return &artifact;
    return nullptr;
}

Release parse_release(std::string_view plain_manifest) {
    require(plain_manifest.size() <= release_limits::manifest_bytes, "manifest size limit");
    const auto manifest = parse(plain_manifest);
    const auto& core_abi = field(manifest, "core_abi");
    require(core_abi.IsUint() && core_abi.GetUint() != 0, "release core ABI must be a positive uint32");
    require(integer(field(manifest, "schema"), 4) && integer(field(manifest, "updater_abi"), 1),
            "release compatibility");
    const auto major = game_major(field(manifest, "major"));
    const auto channel_text = text(field(manifest, "channel"));
    require(channel_text == "stable" || channel_text == "preview", "release channel");
    const auto& sequence = field(manifest, "sequence");
    require(sequence.IsUint64() && sequence.GetUint64() != 0, "release sequence");
    const auto& files = field(manifest, "files");
    require(files.IsArray() && !files.Empty() && files.Size() <= release_limits::artifact_count, "artifact count");
    Release result{digest(plain_manifest), channel_text == "stable" ? Channel::stable : Channel::preview,
                   sequence.GetUint64(), {}};
    result.core_abi = core_abi.GetUint();
    result.major = major;
    result.files.reserve(files.Size());
    std::set<std::string, PathLess> paths;
    std::set<std::string, PathLess> targets;
    std::uint64_t total_bytes = 0;
    std::size_t core_count = 0;
    bool has_updater = false;
    for (const auto& value : files.GetArray()) {
        const auto path = text(field(value, "path"));
        require(valid_relative_path(path), "invalid artifact path");
        const auto role_text = text(field(value, "role"));
        Role role;
        if (role_text == "core") {
            role = Role::core;
            ++core_count;
            require(path.size() >= 4 && equal_path(path.substr(path.size() - 4), ".dll"), "core path");
        } else if (role_text == "updater") {
            require(!has_updater && path == "updater.dll", "exactly one updater.dll allowed");
            has_updater = true;
            role = Role::updater;
        } else if (role_text == "art") {
            role = Role::art;
            require(under(path, "assets/"), "art path");
        } else if (role_text == "ratings") {
            role = Role::ratings;
            require(under(path, "packs/"), "ratings package path");
        } else {
            throw std::runtime_error("unknown artifact role");
        }
        const auto sha = text(field(value, "sha256"));
        require(valid_sha256(sha), "invalid artifact SHA256");
        const auto& length = field(value, "bytes");
        require(length.IsUint64() && length.GetUint64() <= release_limits::artifact_bytes, "artifact size limit");
        const auto bytes = length.GetUint64();
        // Empty ratings remain manifest-listed byte sources; the selected core owns
        // their format validation and may reject them during pure preparation.
        require(bytes != 0 || role == Role::ratings, "empty core, updater or art artifact");
        require(bytes <= release_limits::total_artifact_bytes - total_bytes, "total artifact size limit");
        total_bytes += bytes;
        std::string target;
        if (role == Role::ratings) {
            target = text(field(value, "target"));
            require(valid_relative_path(target) && under(target, "ratings/"), "invalid ratings target");
            require(targets.insert(target).second, "duplicate ratings target");
        }
        require(paths.emplace(path).second, "duplicate artifact path");
        result.files.push_back({std::string(path), role, std::string(sha), bytes, std::move(target)});
    }
    require(core_count == 1, "exactly one core required");
    require(has_updater, "exactly one updater.dll required");
    // Other top-level metadata belongs to the selected core;
    // the fixed loader deliberately knows no rating, provider or game hashes.
    return result;
}

bool select_update(const Release& installed, const Release& candidate, Channel requested, std::uint32_t major) noexcept {
    if (major == 0) major = installed.major;
    if ((requested != Channel::stable && requested != Channel::preview) ||
        major == 0 || major > 999 || candidate.major != major ||
        candidate.channel != requested || candidate.sequence == 0) return false;
    return installed.major != candidate.major || installed.channel != candidate.channel || candidate.sequence > installed.sequence;
}
} // namespace point_zero::loader
