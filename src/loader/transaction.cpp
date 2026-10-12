#include "transaction.hpp"

#include <Windows.h>
#include <wincrypt.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace point_zero::loader {
namespace {
namespace fs = std::filesystem;
using J = rapidjson::Value;
using D = rapidjson::Document;
using A = D::AllocatorType;
constexpr size_t manifest_limit = release_limits::manifest_bytes;
constexpr size_t data_limit = static_cast<size_t>(release_limits::artifact_bytes);
constexpr size_t journal_limit = 768 * 1024 * 1024;
void require(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
fs::path installation(const fs::path& runtime) {
    require(runtime.is_absolute(), "runtime must be absolute");
    return fs::canonical(runtime);
}
std::string text(const J& v) {
    require(v.IsString(), "expected journal string");
    return {v.GetString(), v.GetStringLength()};
}
const J& field(const J& v, const char* key) {
    require(v.IsObject() && v.HasMember(key), "missing journal field");
    return v[key];
}
void unique(const J& v, unsigned depth = 0) {
    require(depth < 16, "journal JSON nesting limit");
    if (v.IsObject()) {
        std::set<std::string> seen;
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
            require(seen.insert(text(it->name)).second, "duplicate journal field");
            unique(it->value, depth + 1);
        }
    } else if (v.IsArray()) for (const auto& row : v.GetArray()) unique(row, depth + 1);
}
D json(std::string_view bytes) {
    D d;
    require(bytes.find('\0') == bytes.npos, "NUL in journal/registry JSON");
    d.Parse<rapidjson::kParseValidateEncodingFlag | rapidjson::kParseIterativeFlag>(bytes.data(), bytes.size());
    require(!d.HasParseError() && d.IsObject(), "invalid journal/registry JSON");
    unique(d);
    return d;
}
J string(std::string_view s, A& a) { return J(s.data(), static_cast<rapidjson::SizeType>(s.size()), a); }
std::string dump(const J& v) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    require(v.Accept(writer), "journal serialization failed");
    return {buffer.GetString(), buffer.GetSize()};
}
std::string base64(std::string_view bytes) {
    require(bytes.size() <= data_limit, "before-image size limit");
    if (bytes.empty()) return {};
    DWORD size{};
    require(CryptBinaryToStringA(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size) != FALSE, "base64 length");
    std::string out(size, '\0');
    require(CryptBinaryToStringA(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()),
                               CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &size) != FALSE, "base64 encoding");
    out.resize(size);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}
std::string unbase64(const J& v) {
    const auto raw = text(v);
    if (raw.empty()) return {};
    DWORD size{};
    require(raw.size() <= journal_limit && CryptStringToBinaryA(raw.data(), static_cast<DWORD>(raw.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT, nullptr, &size, nullptr, nullptr), "journal base64 rejected");
    require(size <= data_limit, "journal before-image size limit");
    std::string bytes(size, '\0');
    require(CryptStringToBinaryA(raw.data(), static_cast<DWORD>(raw.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
            reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr) != FALSE, "journal base64 rejected");
    bytes.resize(size);
    return bytes;
}
std::string folded(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 'a' - 'A');
    return s;
}
using Registry = std::map<std::string, std::string>;
Registry registry(std::string_view bytes) {
    auto d = json(bytes);
    require(d.MemberCount() <= 256, "managed registry count limit");
    Registry result;
    for (auto it = d.MemberBegin(); it != d.MemberEnd(); ++it) {
        const auto path = text(it->name), hash = text(it->value);
        require(valid_relative_path(path) && folded(path).starts_with("ratings/") && valid_sha256(hash), "invalid managed registry row");
        require(result.emplace(folded(path), hash).second, "duplicate managed registry path");
    }
    return result;
}
std::string serialize(const Registry& r) {
    D d(rapidjson::kObjectType);
    auto& a = d.GetAllocator();
    for (const auto& [path, hash] : r) d.AddMember(string(path, a), string(hash, a), a);
    return dump(d);
}
struct Write {
    std::string target, package_path;
    std::optional<std::string> before;
    std::string after_hash;
};
struct Journal {
    std::string old_active, candidate, old_registry;
    std::vector<Write> writes;
};
std::string serialize(const Journal& tx) {
    D payload(rapidjson::kObjectType);
    auto& a = payload.GetAllocator();
    payload.AddMember("schema", 2, a).AddMember("old_active", string(tx.old_active, a), a)
        .AddMember("candidate", string(tx.candidate, a), a).AddMember("old_registry", string(base64(tx.old_registry), a), a);
    J writes(rapidjson::kArrayType);
    for (const auto& w : tx.writes) {
        J row(rapidjson::kObjectType);
        const auto before = w.before.value_or("");
        row.AddMember("target", string(w.target, a), a).AddMember("package_path", string(w.package_path, a), a)
            .AddMember("before_exists", w.before.has_value(), a).AddMember("before", string(base64(before), a), a)
            .AddMember("before_sha256", string(digest(before), a), a).AddMember("after_sha256", string(w.after_hash, a), a);
        writes.PushBack(row, a);
    }
    payload.AddMember("writes", writes, a);
    D out(rapidjson::kObjectType);
    auto& b = out.GetAllocator();
    out.AddMember("sha256", string(digest(dump(payload)), b), b);
    J copy;
    copy.CopyFrom(payload, b);
    out.AddMember("payload", copy, b);
    auto bytes = dump(out);
    require(bytes.size() <= journal_limit, "journal size limit");
    return bytes;
}
Journal journal(std::string_view bytes) {
    const auto envelope = json(bytes);
    require(envelope.MemberCount() == 2, "journal envelope schema");
    const auto& p = field(envelope, "payload");
    require(text(field(envelope, "sha256")) == digest(dump(p)), "journal checksum mismatch");
    require(p.IsObject() && p.MemberCount() == 5 && field(p, "schema").IsUint() && p["schema"].GetUint() == 2, "journal schema");
    Journal tx{text(field(p, "old_active")), text(field(p, "candidate")), unbase64(field(p, "old_registry")), {}};
    require(valid_sha256(tx.old_active) && valid_sha256(tx.candidate) && tx.old_active != tx.candidate, "journal generation identities");
    (void)registry(tx.old_registry);
    const auto& writes = field(p, "writes");
    require(writes.IsArray() && writes.Size() <= 256, "journal writes schema");
    std::set<std::string> targets;
    for (const auto& row : writes.GetArray()) {
        require(row.IsObject() && row.MemberCount() == 6, "journal write schema");
        Write w{text(field(row, "target")), text(field(row, "package_path")), {}, text(field(row, "after_sha256"))};
        require(valid_relative_path(w.target) && folded(w.target).starts_with("ratings/") && valid_relative_path(w.package_path) &&
                valid_sha256(w.after_hash) && targets.insert(folded(w.target)).second, "journal write identity");
        require(field(row, "before_exists").IsBool(), "journal before existence");
        auto before = unbase64(field(row, "before"));
        require(digest(before) == text(field(row, "before_sha256")), "journal before-image checksum mismatch");
        if (row["before_exists"].GetBool()) w.before = std::move(before);
        else require(before.empty(), "absent before-image must be empty");
        tx.writes.push_back(std::move(w));
    }
    return tx;
}
const Artifact* rating(const Release& r, const Write& w) {
    const auto it = std::find_if(r.files.begin(), r.files.end(), [&](const Artifact& a) {
        return a.role == Role::ratings && a.path == w.package_path && folded(a.target) == folded(w.target) && a.sha256 == w.after_hash;
    });
    return it == r.files.end() ? nullptr : &*it;
}
} // namespace

struct Transaction::Impl {
    Files files;
    Event event;
    std::unique_ptr<VerifiedGeneration> installed, offered;
    std::string registry_before, journal_bytes, registry_after;
    bool activated = false, terminal = false;
    const std::atomic_bool* cancelled;

    Impl(const fs::path& runtime, Event callback, const std::atomic_bool* stop)
        : files(installation(runtime) / L"point-zero", stop), event(std::move(callback)), cancelled(stop) {
        files.lock(); note("lock_acquired");
    }
    void note(std::string_view message) { if (event) event(message); }
    std::string pointer(const char* name) const {
        auto bytes = files.read(name, 128);
        require(bytes.has_value(), "missing generation pointer");
        if (bytes->ends_with('\n')) bytes->pop_back();
        if (bytes->ends_with('\r')) bytes->pop_back();
        require(valid_sha256(*bytes), "invalid generation pointer");
        return *bytes;
    }
    Release manifest(std::string_view id) const {
        require(valid_sha256(id), "invalid generation identity");
        const auto bytes = files.read("versions/" + std::string(id) + "/release.json", manifest_limit);
        require(bytes.has_value(), "missing generation manifest");
        auto r = parse_release(*bytes);
        require(r.id == id, "manifest generation identity mismatch");
        return r;
    }
    std::unique_ptr<VerifiedGeneration> admit(std::string_view id, bool candidate = false) const {
        auto out = std::make_unique<VerifiedGeneration>();
        const auto prefix = "versions/" + std::string(id) + "/";
        require(valid_sha256(id), "invalid generation identity");
        auto release_file = files.pin(prefix + "release.json");
        out->release = parse_release(release_file->read(manifest_limit));
        require(out->release.id == id, "manifest generation identity mismatch");
        out->directory = files.path("versions/" + std::string(id));
        out->pins.push_back(std::move(release_file));
        for (const auto& artifact : out->release.files) {
            if (!candidate && artifact.role == Role::ratings) continue;
            auto file = files.pin(prefix + artifact.path);
            const auto actual_bytes = file->size();
            if (actual_bytes != artifact.bytes)
                throw std::runtime_error("release artifact length mismatch: " + artifact.path +
                    " (expected " + std::to_string(artifact.bytes) + " bytes, found " +
                    std::to_string(actual_bytes) + " bytes)");
            if (digest(file->read(data_limit), cancelled) != artifact.sha256)
                throw std::runtime_error("release artifact hash mismatch: " + artifact.path);
            out->pins.push_back(std::move(file));
        }
        return out;
    }
    void retire_registry() {
        const auto bytes = files.read("managed.json", manifest_limit);
        if (!bytes) return;
        auto managed = registry(*bytes);
        std::set<std::string> bundled;
        for (const auto& artifact : installed->release.files)
            if (artifact.role == Role::ratings) bundled.insert(folded(artifact.target));
        const auto count = managed.size();
        std::erase_if(managed, [&](const auto& row) { return !bundled.contains(row.first); });
        if (managed.size() != count) {
            // Preserve every former bundle as a personal file, including edits
            // and configured aliases. Only its obsolete registration retires.
            files.put("managed.json", serialize(managed));
            note("managed_registry_retired");
        }
    }
    void recover() {
        const auto bytes = files.read("journal.json", journal_limit);
        if (!bytes) {
            // An acknowledged transaction or a discarded pre-journal offer may
            // leave only this staging hint. It is never activation authority.
            if (files.read("pending.txt", 128)) { (void)pointer("pending.txt"); files.erase("pending.txt"); }
            files.erase("fallback.txt");
            files.erase("native-attempt.txt");
            files.erase("recovery-manifest.json");
            return;
        }
        auto tx = journal(*bytes);
        const auto prior = admit(tx.old_active); // Channel-independent recovery.
        // Keep schema 2 readable by an older fallback updater. The checked
        // manifest has a separate exact-byte copy, bound by its generation hash,
        // so this updater can recover even if the candidate directory is removed.
        const auto retained = files.read("recovery-manifest.json", manifest_limit);
        const auto candidate = retained ? parse_release(*retained) : manifest(tx.candidate);
        require(candidate.id == tx.candidate, "recovery manifest generation mismatch");
        auto tracked = registry(tx.old_registry);
        auto after = tracked;
        for (const auto& w : tx.writes) {
            require((tracked.contains(folded(w.target)) || !w.before) && rating(candidate, w),
                    "journal target not registered or absent-before in release");
            after[folded(w.target)] = w.after_hash;
            (void)files.path(w.target);
        }
        require(after.size() <= 256, "managed registry count limit");
        const auto active = pointer("active.txt");
        require(active == tx.old_active || active == tx.candidate, "journal active generation mismatch");
        const auto current_registry = files.read("managed.json", manifest_limit);
        require(current_registry && (*current_registry == tx.old_registry || *current_registry == serialize(after)), "journal registry state mismatch");
        note("recovery_validated"); // Everything above completes before first mutation.
        if (files.read("native-attempt.txt", 128)) {
            require(pointer("native-attempt.txt") == tx.candidate, "native attempt generation mismatch");
            files.put("failed.txt", tx.candidate + "\n");
            note("candidate_quarantined");
        }
        for (const auto& w : tx.writes) {
            if (w.before) files.put(w.target, *w.before);
            else if (!files.erase_if_matches(w.target, w.after_hash)) {
                note("preserved_personal:" + w.target);
                continue;
            }
            note("restore:" + w.target);
        }
        files.put("active.txt", tx.old_active + "\n"); note("recovery_pointer");
        files.put("managed.json", tx.old_registry); note("recovery_registry");
        files.erase("pending.txt"); note("recovery_pending");
        files.erase("journal.json"); note("recovery_complete");
        files.erase("fallback.txt");
        files.erase("native-attempt.txt");
        files.erase("recovery-manifest.json");
    }
};

Transaction::Transaction(const fs::path& runtime, Event event, const std::atomic_bool* cancelled)
    : impl_(std::make_unique<Impl>(runtime, std::move(event), cancelled)) {}
Transaction::~Transaction() = default;
void Transaction::begin() {
    impl_->offered.reset(); impl_->installed.reset();
    impl_->journal_bytes.clear(); impl_->registry_before.clear(); impl_->registry_after.clear();
    impl_->activated = impl_->terminal = false;
    impl_->recover();
    impl_->installed = impl_->admit(impl_->pointer("active.txt"));
    impl_->note("installed_verified");
    try { impl_->retire_registry(); }
    catch (const std::exception& e) {
        if (impl_->cancelled && impl_->cancelled->load(std::memory_order_relaxed)) throw;
        // Optional housekeeping must not prevent installed native startup.
        impl_->note(e.what());
    }
}
bool Transaction::offer(std::string_view id, Channel requested, std::uint32_t game_major) {
    require(impl_->installed && !impl_->activated && !impl_->offered && !impl_->terminal, "offer outside pre-native selection");
    if (impl_->files.read("failed.txt", 128) && impl_->pointer("failed.txt") == id) {
        impl_->note("candidate_suppressed");
        return false;
    }
    auto next = impl_->admit(id, true);
    if (!select_update(impl_->installed->release, next->release, requested, game_major)) return false;
    const auto tracked = impl_->files.read("managed.json", manifest_limit);
    require(tracked.has_value(), "managed registry missing");
    const auto managed = registry(*tracked);
    for (const auto& artifact : next->release.files)
        if (artifact.role == Role::ratings && !managed.contains(folded(artifact.target)))
            require(!impl_->files.read(artifact.target, data_limit), "release rating collides with unmanaged existing file");
    impl_->registry_before = *tracked;
    impl_->offered = std::move(next);
    impl_->note("candidate_verified");
    return true;
}
const VerifiedGeneration& Transaction::selected() const {
    require(impl_->installed != nullptr, "transaction not begun");
    return impl_->offered ? *impl_->offered : *impl_->installed;
}
InputSnapshot::Replacements Transaction::replacements() const {
    InputSnapshot::Replacements paths;
    if (!impl_->offered) return paths;
    const auto tracked = registry(impl_->registry_before);
    for (const auto& file : impl_->offered->release.files) {
        if (file.role != Role::ratings) continue;
        if (!tracked.contains(folded(file.target)))
            require(!impl_->files.read(file.target, data_limit), "new rating appeared during preparation");
        paths.emplace_back(impl_->files.path(file.target), impl_->offered->directory / file.path);
    }
    return paths;
}
void Transaction::discard_offer() {
    require(!impl_->activated && !impl_->terminal, "cannot discard initialized/activated generation");
    impl_->offered.reset(); impl_->registry_before.clear();
}
void Transaction::activate(InputSnapshot& inputs) {
    auto& s = *impl_;
    require(s.installed && !s.activated && !s.terminal, "activation outside preparation boundary");
    require(s.pointer("active.txt") == s.installed->release.id, "active generation changed during preparation");
    if (!s.offered) {
        inputs.bind_installed(); s.activated = true; s.note("installed_inputs_verified"); return;
    }
    require(s.files.read("managed.json", manifest_limit) == s.registry_before, "managed registry changed during preparation");
    auto next_registry = registry(s.registry_before);
    Journal tx{s.installed->release.id, s.offered->release.id, s.registry_before, {}};
    std::vector<std::unique_ptr<FilePin>> before_pins;
    std::vector<const FilePin*> incoming_pins;
    std::uint64_t before_bytes = 0;
    for (size_t i = 0; i < s.offered->release.files.size(); ++i) {
        const auto& artifact = s.offered->release.files[i];
        // Bundled optional packs stay current even while disabled in the config.
        // New packs are eligible only while absent, never by matching personal
        // bytes to the publisher hash. Registration happens after all writes.
        if (artifact.role != Role::ratings) continue;
        Write w{artifact.target, artifact.path, {}, artifact.sha256};
        const bool exists = fs::exists(s.files.path(artifact.target));
        require(next_registry.contains(folded(artifact.target)) || !exists,
                "new rating collides with unmanaged existing file");
        if (exists) {
            auto pin = s.files.pin(artifact.target);
            require(pin->size() <= release_limits::total_artifact_bytes - before_bytes,
                    "transaction before-image total size limit");
            before_bytes += pin->size();
            w.before = pin->read(data_limit);
            before_pins.push_back(std::move(pin));
        }
        tx.writes.push_back(std::move(w));
        incoming_pins.push_back(s.offered->pins[i + 1].get());
        next_registry[folded(artifact.target)] = artifact.sha256;
    }
    require(next_registry.size() <= 256, "managed registry count limit");
    s.journal_bytes = serialize(tx);
    s.registry_after = serialize(next_registry);
    s.files.put("pending.txt", tx.candidate + "\n"); s.note("pending_published");
    s.files.put("recovery-manifest.json", s.offered->pins.front()->read(manifest_limit));
    s.files.put("journal.json", s.journal_bytes); s.note("journal_published");
    // Locate the old updater if an interrupted candidate's DLL cannot load.
    // This is only a recovery locator; the updater validates the full journal.
    s.files.put("fallback.txt", tx.old_active + "\n");
    // Drop before-image handles only immediately before publication. Recheck each
    // actual value; the registered publisher hash never vetoes an edited file.
    before_pins.clear();
    for (size_t i = 0; i < tx.writes.size(); ++i) {
        const auto& w = tx.writes[i];
        require(s.files.read(w.target, data_limit) == w.before, "managed file changed before publication");
        // Admission verified these bytes and holds deny-write/delete pins until
        // the transaction ends. Read that same file without reopening/rehashing.
        const auto bytes = incoming_pins[i]->read(data_limit);
        // An absent-before target must still be absent at the atomic rename.
        s.files.put(w.target, bytes, w.before.has_value());
        s.note("write:" + w.target);
    }
    inputs.bind_installed(); s.note("installed_inputs_verified");
    require(s.files.read("managed.json", manifest_limit) == s.registry_before, "registry changed before publication");
    s.files.put("managed.json", s.registry_after); s.note("registry_published");
    s.files.put("active.txt", tx.candidate + "\n");
    s.activated = true; s.note("pointer_published");
}
void Transaction::enter_native() {
    auto& s = *impl_;
    require(s.activated && !s.terminal, "native entry outside activated transaction");
    if (s.offered) {
        // Entry has won Startup's cancellation boundary. Keep this small durable
        // checkpoint independent of a stop arriving before the native call.
        Files metadata(s.files.root());
        metadata.put("native-attempt.txt", s.offered->release.id + "\n");
        s.note("native_attempted");
    }
}
void Transaction::finish(bool admitted, bool cancelled) {
    auto& s = *impl_;
    require(s.activated && !s.terminal, "terminal admission outside activated transaction");
    s.terminal = true;
    if (!admitted) {
        Files metadata(s.files.root());
        if (cancelled) metadata.erase("native-attempt.txt");
        else if (s.offered) metadata.put("failed.txt", s.offered->release.id + "\n");
        s.note(cancelled ? "native_cancelled" : "native_rejected");
        return;
    }
    try {
        s.note("before_ack");
        require(s.pointer("active.txt") == selected().release.id, "active pointer changed before ACK");
        if (s.offered) {
            require(s.files.read("journal.json", journal_limit) == s.journal_bytes, "journal changed before ACK");
            require(s.files.read("managed.json", manifest_limit) == s.registry_after, "registry changed before ACK");
            for (const auto& artifact : s.offered->release.files) {
                if (artifact.role != Role::ratings) continue;
                const auto bytes = s.files.read(artifact.target, data_limit);
                require(bytes && digest(*bytes, s.cancelled) == artifact.sha256, "managed bytes changed before ACK");
            }
            s.files.rename("journal.json", "previous.json"); s.note("journal_acknowledged");
            s.files.erase("pending.txt"); s.note("pending_cleared");
            s.files.erase("fallback.txt");
            s.files.erase("native-attempt.txt");
            s.files.erase("recovery-manifest.json");
        }
    } catch (...) {
        // Keep hashing/I/O cancellable. Once native admission succeeded, a
        // cancellation-aborted ACK must not be mistaken for native rejection.
        if (s.cancelled && s.cancelled->load(std::memory_order_relaxed)) {
            try { Files metadata(s.files.root()); metadata.erase("native-attempt.txt"); }
            catch (...) { s.note("cancel_checkpoint_retained"); }
        }
        throw;
    }
    // ACK is durable; keep selection/pins and registry_before for replacements().
    std::string{}.swap(s.journal_bytes);
    std::string{}.swap(s.registry_after);
    s.note("native_admitted");
}
} // namespace point_zero::loader
