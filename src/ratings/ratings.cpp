#include "ratings.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace point_zero::ratings {
namespace {
namespace fs = std::filesystem;
using J = rapidjson::Value;
using Doc = rapidjson::Document;
using Alloc = Doc::AllocatorType;
constexpr std::string_view codes[]{"SPB", "SPN", "SPH", "SPA", "SPL",
                                   "", "DPN", "DPH", "DPA", "DPL"};

struct Value { double number; std::string entry; };
struct Source {
    SourceInfo info;
    std::string bytes;
    std::optional<double> lower, upper;
    std::map<Key, Value> ratings;
};

std::string utf8(const fs::path& p) {
    const auto s = p.generic_u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
// Match the loader's logical path identity without filesystem reads.
struct PathLess {
    bool operator()(const fs::path& a, const fs::path& b) const noexcept {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};
std::string trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == s.npos) return {};
    return std::string(s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1));
}
std::string_view without_bom(std::string_view s) {
    return s.starts_with("\xef\xbb\xbf") ? s.substr(3) : s;
}
fs::path resolve(const fs::path& config, const std::string& value) {
    if (value.empty() || value.find("://") != value.npos)
        throw std::runtime_error("expected a nonempty local path");
    const fs::path p(std::u8string(value.begin(), value.end()));
    // No filesystem canonicalization or hidden reads in pure preparation.
    return (p.is_absolute() ? p : config.parent_path() / p).lexically_normal();
}
J string(std::string_view s, Alloc& a) {
    if (s.size() > std::numeric_limits<rapidjson::SizeType>::max())
        throw std::runtime_error("string too long");
    return J(s.data(), static_cast<rapidjson::SizeType>(s.size()), a);
}
std::string_view text_view(const J& v) {
    if (!v.IsString()) throw std::runtime_error("expected string");
    return {v.GetString(), v.GetStringLength()};
}
std::string text(const J& v) { return std::string(text_view(v)); }
void object(const J& v) {
    if (!v.IsObject()) throw std::runtime_error("expected object");
    // Metadata/cache objects have only a few fields. Chart keys and song IDs
    // use their own bounded or bulk duplicate checks below.
    for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
        if (std::find_if(v.MemberBegin(), it, [&](const auto& earlier) {
                return text_view(earlier.name) == text_view(it->name);
            }) != it)
            throw std::runtime_error("duplicate field: " + text(it->name));
}
const J& field(const J& v, const char* name) {
    if (!v.IsObject() || !v.HasMember(name))
        throw std::runtime_error(std::string("missing field: ") + name);
    return v[name];
}
double number(const J& v) {
    if (!v.IsNumber() || !std::isfinite(v.GetDouble()))
        throw std::runtime_error("expected finite number");
    return v.GetDouble();
}
int integer(const J& v) {
    if (!v.IsInt()) throw std::runtime_error("expected integer");
    return v.GetInt();
}
std::size_t count(const J& v) {
    if (!v.IsUint64() || v.GetUint64() > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("expected count");
    return static_cast<std::size_t>(v.GetUint64());
}
Doc json(std::string_view s) {
    if (s.find('\0') != s.npos) throw std::runtime_error("NUL byte in JSON input");
    Doc d;
    d.Parse<rapidjson::kParseFullPrecisionFlag | rapidjson::kParseValidateEncodingFlag>(
        s.data(), s.size());
    if (d.HasParseError())
        throw std::runtime_error("invalid JSON at byte " + std::to_string(d.GetErrorOffset()));
    return d;
}
std::string dump(const J& value) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!value.Accept(writer)) throw std::runtime_error("JSON serialization failed");
    return {buffer.GetString(), buffer.GetSize()};
}
int song(std::string_view s) {
    int value{};
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (s.empty() || s.front() < '1' || s.front() > '9' ||
        result.ec != std::errc{} || result.ptr != s.data() + s.size())
        throw std::runtime_error("expected canonical positive int32 song ID");
    return value;
}
bool valid_key(Key key) {
    return key.song > 0 && key.style >= 0 && key.style <= 1 &&
           key.difficulty >= 0 && key.difficulty <= 4 &&
           !(key.style == 1 && key.difficulty == 0);
}
void put(Source& s, Key key, double value, std::string entry) {
    // Both adapters admit finite numbers before calling this shared merge.
    if (!valid_key(key)) throw std::runtime_error("invalid chart");
    const auto [it, inserted] = s.ratings.emplace(key, Value{value, std::move(entry)});
    if (!inserted && it->second.number != value)
        throw std::runtime_error("conflicting assignments for chart " + std::to_string(key.song));
}
void parse_json(Source& s) {
    const auto d = json(without_bom(s.bytes));
    object(d);
    if (text(field(d, "format")) != "iidx-decimal-ratings" ||
        integer(field(d, "version")) != 2)
        throw std::runtime_error("unsupported ratings format/version");
    if (d.HasMember("id") && d["id"].IsString()) s.info.id = text(d["id"]);
    if (d.HasMember("revision") && d["revision"].IsString()) s.info.revision = text(d["revision"]);
    if (d.HasMember("scale")) {
        const auto& scale = d["scale"];
        object(scale);
        if (scale.HasMember("type") && text(scale["type"]) != "numeric")
            throw std::runtime_error("unsupported scale type");
        if (scale.HasMember("censoring")) {
            const auto& c = scale["censoring"];
            object(c);
            if (c.HasMember("lower")) s.lower = number(c["lower"]);
            if (c.HasMember("upper")) s.upper = number(c["upper"]);
            if (s.lower && s.upper && *s.lower >= *s.upper)
                throw std::runtime_error("invalid censoring endpoints");
        }
    }
    const auto& songs = field(d, "songs");
    if (!songs.IsObject()) throw std::runtime_error("expected object");
    std::vector<int> song_ids;
    song_ids.reserve(songs.MemberCount());
    for (auto it = songs.MemberBegin(); it != songs.MemberEnd(); ++it) {
        const int id = song(text_view(it->name));
        song_ids.push_back(id);
        if (!it->value.IsObject()) throw std::runtime_error("expected object");
        unsigned seen{};
        for (auto row = it->value.MemberBegin(); row != it->value.MemberEnd(); ++row) {
            const auto name = text_view(row->name);
            const auto found = std::find(std::begin(codes), std::end(codes), name);
            if (name.empty() || found == std::end(codes))
                throw std::runtime_error("unknown chart key: " + std::string(name));
            const auto mapped = static_cast<int>(found - std::begin(codes));
            const auto bit = 1u << mapped;
            if (seen & bit) throw std::runtime_error("duplicate field: " + std::string(name));
            seen |= bit;
            put(s, {id, mapped / 5, mapped % 5}, number(row->value),
                "songs/" + std::to_string(id) + "/" + std::string(name));
        }
    }
    std::sort(song_ids.begin(), song_ids.end());
    if (std::adjacent_find(song_ids.begin(), song_ids.end()) != song_ids.end())
        throw std::runtime_error("duplicate song ID");
}
template<std::size_t N>
std::array<std::optional<YAML::Node>, N> yaml_fields(
    const YAML::Node& n, const std::array<std::string_view, N>& names) {
    if (!n.IsMap()) throw std::runtime_error("expected playlist map");
    std::array<std::optional<YAML::Node>, N> fields;
    for (auto it : n) {
        if (!it.first.IsScalar()) throw std::runtime_error("expected playlist field name");
        const auto found = std::find(names.begin(), names.end(), it.first.Scalar());
        if (found == names.end()) continue; // Unrelated Playlister metadata.
        auto& value = fields[static_cast<std::size_t>(found - names.begin())];
        if (value) throw std::runtime_error("duplicate playlist field");
        value.emplace(it.second);
    }
    return fields;
}
const YAML::Node& yaml_required(const std::optional<YAML::Node>& field, const char* name) {
    if (!field) throw std::runtime_error(std::string("missing playlist field: ") + name);
    return *field;
}
std::string yaml_scalar(const std::optional<YAML::Node>& field, const char* name) {
    const auto& node = yaml_required(field, name);
    if (!node.IsScalar()) throw std::runtime_error(std::string("expected playlist scalar: ") + name);
    return node.Scalar();
}
std::optional<double> folder_number(std::string_view s) {
    if (s.empty() || s.find_first_of(" \t\r\n") != s.npos ||
        (s.front() != '-' && (s.front() < '0' || s.front() > '9')))
        return {};
    Doc d;
    d.Parse<rapidjson::kParseFullPrecisionFlag>(s.data(), s.size());
    if (d.HasParseError()) {
        if (d.GetParseError() == rapidjson::kParseErrorNumberTooBig)
            throw std::runtime_error("numeric folder exceeds finite number range");
        return {};
    }
    return d.IsNumber() ? std::optional(number(d)) : std::nullopt;
}
void parse_playlist(Source& s) {
    const auto docs = YAML::LoadAll(s.bytes);
    if (docs.size() != 1 || !docs[0].IsSequence())
        throw std::runtime_error("expected one Playlister folder sequence");
    std::size_t folder{};
    for (const auto& n : docs[0]) {
        ++folder;
        const auto f = yaml_fields(n, std::array<std::string_view, 3>{"name", "play style", "charts"});
        const auto name = yaml_scalar(f[0], "name");
        const auto value = folder_number(name);
        if (!value) continue; // Unrelated authored folders do not supply ratings.
        const auto style = yaml_scalar(f[1], "play style");
        if (style != "SP" && style != "DP") throw std::runtime_error("invalid playlist style");
        const auto& charts = yaml_required(f[2], "charts");
        if (!charts.IsSequence()) throw std::runtime_error("expected playlist chart sequence");
        constexpr std::string_view difficulties[]{
            "beginner", "normal", "hyper", "another", "leggendaria"};
        for (const auto& item : charts) {
            const auto row = yaml_fields(item, std::array<std::string_view, 3>{"entry id", "difficulty", "rating"});
            if (row[2])
                throw std::runtime_error("per-chart rating conflicts with numeric folder");
            const auto name_diff = yaml_scalar(row[1], "difficulty");
            const auto diff = std::find(std::begin(difficulties), std::end(difficulties), name_diff);
            if (diff == std::end(difficulties)) throw std::runtime_error("unknown playlist difficulty");
            put(s, {song(yaml_scalar(row[0], "entry id")), style == "DP",
                    static_cast<int>(diff - std::begin(difficulties))},
                *value, "folder/" + std::to_string(folder) + "/" + name);
        }
    }
}
bool censored(const Source& s, double value) {
    return (s.lower && value <= *s.lower) || (s.upper && value >= *s.upper);
}
bool has_style(const Source& s, int style) {
    return std::any_of(s.ratings.begin(), s.ratings.end(),
                       [style](const auto& row) { return row.first.style == style; });
}
bool suitable(const Source& s, int style) {
    std::size_t anchors{};
    double low = std::numeric_limits<double>::infinity(), high = -low;
    for (const auto& [key, value] : s.ratings) {
        if (key.style != style || censored(s, value.number)) continue;
        ++anchors;
        low = std::min(low, value.number);
        high = std::max(high, value.number);
    }
    // Distinct chart keys, not distinct values. No threshold in source units.
    return anchors >= affine::minimum_anchors && low < high;
}
using Lists = std::array<std::vector<int>, 2>;
std::string fingerprint(const Config& c, const std::vector<Source>& sources, const Lists& lists) {
    Doc d(rapidjson::kObjectType);
    auto& a = d.GetAllocator();
    d.AddMember("pipeline", string(pipeline_revision, a), a);
    J styles(rapidjson::kArrayType);
    for (int style = 0; style < 2; ++style) {
        J row(rapidjson::kObjectType), files(rapidjson::kArrayType);
        row.AddMember("adaptation", string(c.adaptation[style] == Adaptation::none ? "none" : "affine", a), a);
        if (c.adaptation[style] == Adaptation::affine)
            row.AddMember("reference", string(c.reference[style], a), a);
        for (int i : lists[style]) {
            const auto& s = sources[i].info;
            J file(rapidjson::kArrayType);
            file.PushBack(string(utf8(s.path), a), a).PushBack(string(s.sha256, a), a)
                .PushBack(string(s.adapter, a), a);
            files.PushBack(file, a);
        }
        row.AddMember("files", files, a);
        styles.PushBack(row, a);
    }
    d.AddMember("styles", styles, a);
    return sha256(dump(d));
}
J payload(const Snapshot& snapshot, Alloc& a) {
    J d(rapidjson::kObjectType);
    J sources(rapidjson::kArrayType), transforms(rapidjson::kArrayType), rows(rapidjson::kArrayType);
    sources.Reserve(static_cast<rapidjson::SizeType>(snapshot.sources.size()), a);
    transforms.Reserve(static_cast<rapidjson::SizeType>(snapshot.transforms.size()), a);
    rows.Reserve(static_cast<rapidjson::SizeType>(snapshot.assignments.size()), a);
    for (const auto& source : snapshot.sources) {
        J s(rapidjson::kObjectType);
        s.AddMember("path", string(utf8(source.path), a), a)
            .AddMember("sha256", string(source.sha256, a), a)
            .AddMember("adapter", string(source.adapter, a), a)
            .AddMember("id", string(source.id, a), a)
            .AddMember("revision", string(source.revision, a), a);
        sources.PushBack(s, a);
    }
    for (const auto& t : snapshot.transforms) {
        J f(rapidjson::kObjectType);
        f.AddMember("style", t.style, a).AddMember("source", t.source, a)
            .AddMember("reference", t.reference, a).AddMember("a", t.fit.a, a)
            .AddMember("b", t.fit.b, a).AddMember("reason", string(t.fit.reason, a), a)
            .AddMember("overlap", static_cast<uint64_t>(t.fit.overlap), a)
            .AddMember("iterations", t.fit.iterations, a)
            .AddMember("censored_pairs", static_cast<uint64_t>(t.censored_pairs), a);
        transforms.PushBack(f, a);
    }
    for (const auto& row : snapshot.assignments) {
        J r(rapidjson::kArrayType);
        r.PushBack(row.key.song, a).PushBack(row.key.style, a).PushBack(row.key.difficulty, a)
            .PushBack(row.supplied, a).PushBack(row.adapted, a).PushBack(row.source, a)
            .PushBack(string(row.entry, a), a);
        rows.PushBack(r, a);
    }
    d.AddMember("sources", sources, a).AddMember("transforms", transforms, a).AddMember("rows", rows, a);
    return d;
}
bool restore_cache(Snapshot& out, const Lists& lists, std::string_view bytes) {
    if (bytes.empty()) return false;
    try {
        const auto d = json(bytes);
        object(d);
        if (text(field(d, "pipeline")) != pipeline_revision ||
            text(field(d, "key")) != out.cache_key)
            return false;
        const auto& p = field(d, "payload");
        object(p);
        if (text(field(d, "payload_sha256")) != sha256(dump(p))) return false;
        const auto& sources = field(p, "sources");
        const auto& transforms = field(p, "transforms");
        const auto& rows = field(p, "rows");
        if (!sources.IsArray() || sources.Size() != out.sources.size() ||
            !transforms.IsArray() || transforms.Size() != lists[0].size() + lists[1].size() ||
            !rows.IsArray())
            return false;
        auto metadata = out.sources;
        for (rapidjson::SizeType i = 0; i < sources.Size(); ++i) {
            const auto& s = sources[i];
            object(s);
            if (text(field(s, "path")) != utf8(metadata[i].path) ||
                text(field(s, "sha256")) != metadata[i].sha256 ||
                text(field(s, "adapter")) != metadata[i].adapter)
                return false;
            metadata[i].id = text(field(s, "id"));
            metadata[i].revision = text(field(s, "revision"));
        }
        std::vector<Transform> restored_transforms;
        restored_transforms.reserve(transforms.Size());
        rapidjson::SizeType ti{};
        for (int style = 0; style < 2; ++style) for (int source : lists[style]) {
            const auto& row = transforms[ti++];
            object(row);
            Transform t;
            t.style = integer(field(row, "style"));
            t.source = integer(field(row, "source"));
            t.reference = integer(field(row, "reference"));
            if (t.style != style || t.source != source ||
                (t.reference != -1 &&
                 std::find(lists[style].begin(), lists[style].end(), t.reference) == lists[style].end()))
                return false;
            t.fit.a = number(field(row, "a")); t.fit.b = number(field(row, "b"));
            t.fit.reason = text(field(row, "reason"));
            t.fit.overlap = count(field(row, "overlap"));
            t.fit.iterations = integer(field(row, "iterations"));
            t.censored_pairs = count(field(row, "censored_pairs"));
            if (!(t.fit.a > 0) || t.fit.iterations < 0 || t.fit.iterations > 20 || t.fit.reason.empty())
                return false;
            restored_transforms.push_back(std::move(t));
        }
        std::vector<Assignment> restored_rows;
        restored_rows.reserve(rows.Size());
        for (const auto& row : rows.GetArray()) {
            if (!row.IsArray() || row.Size() != 7) return false;
            Assignment r{{integer(row[0]), integer(row[1]), integer(row[2])},
                         number(row[3]), number(row[4]), 0, integer(row[5]), text(row[6])};
            if (!valid_key(r.key) ||
                (!restored_rows.empty() && !(restored_rows.back().key < r.key)) ||
                std::find(lists[r.key.style].begin(), lists[r.key.style].end(), r.source) == lists[r.key.style].end())
                return false;
            r.tenths = normalize(r.adapted);
            restored_rows.push_back(std::move(r));
        }
        out.sources = std::move(metadata);
        out.transforms = std::move(restored_transforms);
        out.assignments = std::move(restored_rows);
        out.cache_hit = true;
        return true;
    } catch (const std::exception&) {
        return false; // Optional derived data never conceals current source errors.
    }
}
} // namespace

std::string sha256(std::string_view bytes) {
    if (bytes.size() > std::numeric_limits<ULONG>::max()) throw std::runtime_error("input too large");
    std::array<unsigned char, 32> hash{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                   reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),
                   static_cast<ULONG>(bytes.size()), hash.data(), static_cast<ULONG>(hash.size())) < 0)
        throw std::runtime_error("SHA256 failed");
    std::string result;
    result.reserve(64);
    for (const auto byte : hash) {
        result += "0123456789abcdef"[byte >> 4];
        result += "0123456789abcdef"[byte & 15];
    }
    return result;
}

int normalize(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("nonfinite adapted rating");
    // Clamping first is equivalent at these endpoints and avoids large products.
    return static_cast<int>(std::floor(std::clamp(value, 1.0, 12.9) * 10.0 + 0.5));
}

Config parse_config(const fs::path& config_path, std::string_view bytes) {
    if (!config_path.is_absolute()) throw std::runtime_error("config path must be absolute");
    if (bytes.find('\0') != bytes.npos) throw std::runtime_error("NUL byte in config input");
    Config config;
    std::istringstream input{std::string(without_bom(bytes))};
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto equal = line.find('=');
        if (equal == line.npos)
            throw std::runtime_error("config line " + std::to_string(line_number) + ": expected key=value");
        const auto key = trim(std::string_view(line).substr(0, equal));
        const auto value = trim(std::string_view(line).substr(equal + 1));
        const int style = key.ends_with("dp") ? 1 : 0;
        if (key == "rating-file-sp" || key == "rating-file-dp") {
            config.files[style].push_back(resolve(config_path, value));
        } else if (key == "scale-adaptation-sp" || key == "scale-adaptation-dp") {
            if (value != "none" && value != "affine")
                throw std::runtime_error("unsupported adaptation: " + value);
            config.adaptation[style] = value == "none" ? Adaptation::none : Adaptation::affine;
        } else if (key == "scale-reference-sp" || key == "scale-reference-dp") {
            config.reference[style] = value == "last" ? value : utf8(resolve(config_path, value));
        } else if (key == "update-channel") {
            if (value != "stable" && value != "preview")
                throw std::runtime_error("unsupported update channel: " + value);
        } else {
            throw std::runtime_error("unknown config key: " + key);
        }
    }
    return config;
}

Snapshot prepare(const fs::path& config_path, const ReadInput& read, std::string_view optional_cache) {
    Snapshot result;
    const auto config_bytes = read(config_path);
    result.config = parse_config(config_path, config_bytes);
    std::vector<Source> sources;
    sources.reserve(result.config.files[0].size() + result.config.files[1].size());
    Lists lists;
    for (int style = 0; style < 2; ++style) lists[style].reserve(result.config.files[style].size());
    std::map<fs::path, int, PathLess> indices;
    for (int style = 0; style < 2; ++style) for (const auto& path : result.config.files[style]) {
        auto existing = indices.find(path);
        int index;
        if (existing == indices.end()) {
            index = static_cast<int>(sources.size());
            Source s;
            s.info.path = path;
            s.bytes = read(path);
            if (s.bytes.find('\0') != s.bytes.npos) throw std::runtime_error("NUL byte in rating input: " + utf8(path));
            s.info.sha256 = sha256(s.bytes);
            auto extension = utf8(path.extension());
            for (auto& c : extension) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            if (extension == ".json") s.info.adapter = "json-v2";
            else if (extension == ".yml" || extension == ".yaml") s.info.adapter = "playlist-yaml";
            else throw std::runtime_error("unsupported rating file extension: " + utf8(path));
            sources.push_back(std::move(s));
            indices.emplace(path, index);
        } else index = existing->second;
        lists[style].push_back(index);
    }
    result.sources.reserve(sources.size());
    for (const auto& s : sources) result.sources.push_back(s.info);
    result.cache_key = fingerprint(result.config, sources, lists);
    if (restore_cache(result, lists, optional_cache)) return result;
    for (auto& s : sources) {
        try {
            if (s.info.adapter == "json-v2") parse_json(s);
            else parse_playlist(s);
        } catch (const std::exception& e) {
            throw std::runtime_error(utf8(s.info.path) + ": " + e.what());
        }
        std::string{}.swap(s.bytes);
        ++result.files_parsed;
    }
    std::map<Key, Assignment> merged;
    result.transforms.reserve(lists[0].size() + lists[1].size());
    for (int style = 0; style < 2; ++style) {
        int reference = -1;
        if (result.config.adaptation[style] == Adaptation::affine) {
            if (result.config.reference[style] == "last") {
                for (auto i = lists[style].rbegin(); i != lists[style].rend(); ++i)
                    if (suitable(sources[*i], style)) { reference = *i; break; }
            } else {
                const auto path = fs::path(std::u8string(result.config.reference[style].begin(),
                                                        result.config.reference[style].end())).lexically_normal();
                const auto found = indices.find(path);
                if (found != indices.end() &&
                    std::find(lists[style].begin(), lists[style].end(), found->second) != lists[style].end() &&
                    has_style(sources[found->second], style))
                    reference = found->second;
                if (reference < 0) throw std::runtime_error("reference must be a listed source providing its style");
            }
        }
        for (int i : lists[style]) {
            Transform t;
            t.style = style; t.source = i; t.reference = reference;
            t.fit.reason = result.config.adaptation[style] == Adaptation::none ? "disabled"
                         : i == reference ? "reference" : "no_reference";
            if (result.config.adaptation[style] == Adaptation::affine && reference >= 0 && i != reference) {
                std::vector<affine::Pair> pairs;
                for (const auto& [key, value] : sources[i].ratings) {
                    if (key.style != style) continue;
                    const auto match = sources[reference].ratings.find(key);
                    if (match == sources[reference].ratings.end()) continue;
                    if (censored(sources[i], value.number) || censored(sources[reference], match->second.number)) {
                        ++t.censored_pairs;
                    } else pairs.push_back({value.number, match->second.number});
                }
                t.fit = affine::fit(std::move(pairs));
            }
            for (const auto& [key, value] : sources[i].ratings) {
                if (key.style != style) continue;
                const double adapted = std::fma(t.fit.a, value.number, t.fit.b);
                merged[key] = {key, value.number, adapted, normalize(adapted), i, value.entry};
            }
            result.transforms.push_back(std::move(t));
        }
    }
    result.sources.clear();
    for (auto& s : sources) result.sources.push_back(std::move(s.info));
    result.assignments.reserve(merged.size());
    for (auto& [key, value] : merged) result.assignments.push_back(std::move(value));
    return result;
}

const Assignment* Snapshot::find(Key key) const noexcept {
    const auto it = std::lower_bound(assignments.begin(), assignments.end(), key,
                                    [](const Assignment& row, Key k) { return row.key < k; });
    return it != assignments.end() && it->key == key ? &*it : nullptr;
}

std::string serialize_cache(const Snapshot& snapshot) {
    Doc d(rapidjson::kObjectType);
    auto& a = d.GetAllocator();
    auto body = payload(snapshot, a);
    const auto body_hash = sha256(dump(body));
    d.AddMember("pipeline", string(pipeline_revision, a), a)
        .AddMember("key", string(snapshot.cache_key, a), a)
        .AddMember("payload_sha256", string(body_hash, a), a);
    d.AddMember("payload", body, a);
    return dump(d);
}
} // namespace point_zero::ratings
