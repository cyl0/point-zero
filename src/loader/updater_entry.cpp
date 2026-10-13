#include "settings.hpp"
#include "github.hpp"
#include "game_major.hpp"
#include "staging.hpp"
#include "startup.hpp"
#include "zip.hpp"
#include "updater_api.hpp"
#include <spicesdk.h>

#include <Windows.h>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace {
using namespace point_zero::loader;
HMODULE module{}; // DllMain only records this versioned updater.
struct State;
std::atomic<State*> owner{}; // Process-owned; bootstrap serializes run/handoff.

void construction_log(const SPICE_SDK_V0* sdk, const char* message) noexcept {
    __try {
        if (sdk && sdk->log) (void)sdk->log(SPICE_SDK_LOG_LEVEL_WARNING, "point-zero", message);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

std::filesystem::path module_path() {
    std::wstring path(32768, L'\0');
    const auto count = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!count || count >= path.size()) throw std::runtime_error("loader module path unavailable");
    path.resize(count);
    return std::filesystem::path(path);
}
SPICE_SDK_V0 sdk_callbacks(const SPICE_SDK_V0& source) noexcept {
    SPICE_SDK_V0 result{};
    result.size = source.size;
    result.log = source.log;
    result.get_avs_info = source.get_avs_info;
    result.add_toast = source.add_toast;
    return result;
}
struct State {
    SPICE_SDK_V0 sdk{};
    std::mutex notice_mutex;
    std::optional<StartupNotice> pending_notice;
    bool graphics_ready{}, update_notified{}, failure_notified{}, notices_stopped{};
    std::filesystem::path runtime;
    std::uint32_t game_major{};
    Startup startup;
    State(const UpdaterRequest& request, UpdaterReply& reply, ReleaseSettings settings)
        : sdk(sdk_callbacks(*request.sdk)), runtime(request.runtime), startup(StartupOptions{request.runtime,
            [this, runtime = std::filesystem::path(request.runtime), settings,
             skip = request.skip_update, offer = std::string(request.offer)]
            (const Release& installed, Channel channel, const Stop& stop) {
                if (!game_major) game_major = installed.major;
                Files files(runtime / L"point-zero", &stop.flag());
                if (skip) {
                    if (offer.empty()) return std::optional<std::string>{};
                    if (!valid_sha256(offer)) throw std::runtime_error("invalid updater offer");
                    const auto manifest = files.pin("versions/" + offer + "/release.json")->read(release_limits::manifest_bytes);
                    const auto hinted = parse_release(manifest);
                    if (hinted.id != offer) throw std::runtime_error("updater offer manifest mismatch");
                    if (hinted.major == game_major) return std::optional<std::string>{offer};
                    // An older major-only selector may deliver this updater as
                    // a bridge after the player has already upgraded the game.
                    // Discover the actual family before any incompatible core
                    // is loaded. Subsequent matching handoffs reuse their hint.
                    log("update_major_redirect");
                }
                const std::string stream = channel == Channel::stable ? "stable" : "preview";
                auto api = "https://api.github.com/repos/" + settings.repository + "/releases";
                auto downloads = "https://github.com/" + settings.repository + "/releases/download/";
#ifdef POINT_ZERO_TEST_LOADER
                if (settings.repository.starts_with("http://127.0.0.1:") && settings.repository.ends_with('/')) {
                    api = settings.repository + "api/releases";
                    downloads = settings.repository + "download/";
                }
#endif
                Http http(stop);
                std::optional<GitHubRelease> selected;
                for (unsigned page = 1; ; ++page) {
                    const auto releases = parse_github_releases(http.get(
                        api + "?per_page=100&page=" + std::to_string(page), 2 * 1024 * 1024), channel, game_major);
                    if (releases.newest && (!selected || newer_github_release(*releases.newest, *selected)))
                        selected = releases.newest;
                    if (releases.count < 100) break;
                    if (page == std::numeric_limits<unsigned>::max()) throw std::runtime_error("GitHub release page overflow");
                }
                if (!selected) return std::optional<std::string>{};
                const auto& info = selected_package(*selected);
                const auto receipt = "package-" + stream + ".txt";
                // A receipt avoids downloading an unchanged ZIP on every boot.
                // Bind it to the verified installed generation so rollback and
                // channel changes cannot hide an available update.
                try {
                    if (installed.major == game_major && installed.channel == channel &&
                        files.read(receipt, 130) == info.sha256 + '\n' + installed.id + '\n') {
                        return std::optional<std::string>{};
                    }
                    // The checked receipt also identifies a failed package.
                    // A new ZIP remains discoverable; removing failed.txt is an
                    // explicit retry of this same native generation.
                    auto failed = files.read("failed.txt", 128);
                    if (failed && failed->ends_with('\n')) failed->pop_back();
                    if (failed && valid_sha256(*failed) &&
                        files.read(receipt, 130) == info.sha256 + '\n' + *failed + '\n')
                        return std::optional<std::string>{};
                } catch (const std::exception&) {} // Optional cache: unreadable is a miss.
                auto bytes = http.get(downloads + selected->tag + "/point-zero.zip", static_cast<size_t>(info.bytes));
                if (bytes.size() != info.bytes || digest(bytes, &stop.flag()) != info.sha256)
                    throw std::runtime_error("GitHub ZIP checksum/length mismatch");
                Zip zip(std::move(bytes), &stop.flag());
                auto id = zip.read("point-zero/active.txt", 128);
                if (id.ends_with('\n')) id.pop_back();
                if (id.ends_with('\r')) id.pop_back();
                if (!valid_sha256(id)) throw std::runtime_error("invalid packaged generation");
                const auto prefix = "point-zero/versions/" + id + "/";
                const auto manifest = zip.read(prefix + "release.json", release_limits::manifest_bytes);
                const auto release = parse_release(manifest);
                if (release.id != id) throw std::runtime_error("GitHub package generation mismatch");
                if (release.major != game_major) throw std::runtime_error("GitHub package major mismatch");
                const auto staged = stage_generation(files, release, manifest, installed, channel,
                    [&](const Artifact& artifact, size_t limit) {
                        // Only declared payloads enter the existing transaction;
                        // the install ZIP's config, bootstrap and personal files do not.
                        return zip.read(prefix + artifact.path, limit);
                    }, &stop.flag(), game_major);
                // The manifest still controls monotonic update selection. Cache
                // only a checked package, against the generation that will use it.
                try { files.put(receipt, info.sha256 + '\n' + staged.value_or(installed.id) + '\n'); }
                catch (const std::exception&) {} // Cache failure must not veto an update.
                if (stop.requested()) throw std::runtime_error("startup cancelled");
                return staged;
            }, [this](std::string_view message) { log(message); }, [this] { return native_host(); },
            [this](StartupNotice notice) { queue_notice(notice); },
            [hash = digest(FilePin(module_path()).read(128 * 1024 * 1024)), &reply]
            (const VerifiedGeneration& chosen, bool offered) {
                const auto* updater = chosen.release.updater();
                if (updater->sha256 == hash) return false;
                std::memcpy(reply.generation, chosen.release.id.c_str(), sizeof(reply.generation));
                reply.offered = offered;
                return true;
            }, [this] { return detect_major(); }, request.skip_update == 0}) {}
    std::uint32_t detect_major() {
#ifdef POINT_ZERO_TEST_LOADER
        game_major = music_database_major(FilePin(runtime / L"game-major.bin").read_prefix(8));
#else
        game_major = detect_game_major(runtime);
#endif
        log("detected_iidx_major=" + std::to_string(game_major));
        return game_major;
    }
    PZ_NativeHost native_host() {
        // Poison the output so success without filling/terminating it cannot
        // accidentally pass validation because of caller-provided zeroes.
        SPICE_SDK_AVS_INFO info;
        std::memset(&info, 0xff, sizeof(info));
        if (sdk.get_avs_info(&info) != SPICE_SDK_STATUS_SUCCESS)
            throw std::runtime_error("Spice SDK AVS metadata unavailable");
        if (!info.model[0] || !std::memchr(info.model, '\0', sizeof(info.model)) ||
            !info.ext[0] || !std::memchr(info.ext, '\0', sizeof(info.ext)))
            throw std::runtime_error("Spice SDK AVS metadata malformed");
        PZ_NativeHost result{};
        static_assert(sizeof(result.avs.model) == sizeof(info.model));
        static_assert(sizeof(result.avs.datecode) == sizeof(info.ext));
        std::memcpy(result.avs.model, info.model, sizeof(info.model));
        std::memcpy(result.avs.datecode, info.ext, sizeof(info.ext));
        result.avs.dest = info.dest; result.avs.spec = info.spec; result.avs.rev = info.rev;
        result.context = this;
        result.log = native_log;
        result.notice = sdk.add_toast ? native_notice : nullptr;
        result.ready = sdk.add_toast ? native_ready : nullptr;
        return result;
    }
    static std::string_view message(PZ_Bytes bytes) {
        if ((bytes.size && !bytes.data) || bytes.size > 64 * 1024)
            throw std::invalid_argument("invalid native message");
        const auto text = bytes.size ? std::string_view(reinterpret_cast<const char*>(bytes.data),
                                                       static_cast<size_t>(bytes.size)) : std::string_view{};
        if (text.find('\0') != text.npos) throw std::invalid_argument("NUL in native message");
        return text;
    }
    static void PZ_CALL native_log(void* context, int32_t level, PZ_Bytes bytes) noexcept {
        try {
            // No cast from the core's severity into the SDK: FATAL terminates
            // Spice. Errors and unknown levels are nonfatal warnings here.
            auto& self = *static_cast<State*>(context);
            self.log(message(bytes),
                level == PZ_LOG_INFO ? SPICE_SDK_LOG_LEVEL_INFO : SPICE_SDK_LOG_LEVEL_WARNING);
            if (level == PZ_LOG_ERROR) self.queue_notice(StartupNotice::failed);
        } catch (...) {}
    }
    static void PZ_CALL native_notice(void* context, PZ_Bytes bytes) noexcept {
        try {
            auto& self = *static_cast<State*>(context);
            const std::string text(message(bytes));
            if (self.sdk.add_toast) (void)self.sdk.add_toast(SPICE_SDK_TOAST_LEVEL_INFO, text.c_str());
        } catch (...) {}
    }
    // Only two fixed outcomes, not a notification queue. A failure supersedes
    // pending success; attempted delivery is never retried because SDK success
    // does not prove visibility (the user may disable the overlay).
    std::optional<StartupNotice> take_notice() noexcept {
        if (!graphics_ready || notices_stopped || !pending_notice) return {};
        const auto notice = *pending_notice;
        pending_notice.reset();
        if (failure_notified || (notice == StartupNotice::updated && update_notified)) return {};
        if (notice == StartupNotice::failed) failure_notified = true;
        else update_notified = true;
        return notice;
    }
    void deliver(std::optional<StartupNotice> notice) noexcept {
        if (!notice || !sdk.add_toast) return;
        const auto* text = *notice == StartupNotice::updated ? "POINT ZERO updated." :
            "POINT ZERO encountered an error. Restart the game; see the Spice log for details.";
        try { (void)sdk.add_toast(SPICE_SDK_TOAST_LEVEL_INFO, text); } catch (...) {}
    }
    void queue_notice(StartupNotice notice) noexcept {
        std::optional<StartupNotice> ready;
        {
            std::lock_guard lock(notice_mutex);
            if (notices_stopped || failure_notified || !sdk.add_toast) return;
            if (!pending_notice || notice == StartupNotice::failed) pending_notice = notice;
            ready = take_notice();
        }
        deliver(ready); // No lock held across the external SDK callback.
    }
    static void PZ_CALL native_ready(void* context) noexcept {
        auto& self = *static_cast<State*>(context);
        std::optional<StartupNotice> ready;
        {
            std::lock_guard lock(self.notice_mutex);
            self.graphics_ready = true;
            ready = self.take_notice();
        }
        self.deliver(ready);
    }
    void stop_notices() noexcept {
        std::lock_guard lock(notice_mutex);
        notices_stopped = true;
        pending_notice.reset();
    }
    void log(std::string_view text, SPICE_SDK_LOG_LEVEL level = SPICE_SDK_LOG_LEVEL_INFO) noexcept {
        try {
            if (sdk.log) {
                const std::string message(text);
                (void)sdk.log(level, "point-zero", message.c_str());
            }
        } catch (...) {}
    }
};
void __cdecl stop() noexcept {
    if (auto* state = owner.load()) {
        state->stop_notices();
        state->startup.request_stop();
        // One total wait, below Spice's inspected one-second force-exit timer.
        // The owner and updater survive if an OS/core call has not returned.
        (void)state->startup.wait_finished(650);
    }
}
int __cdecl run(const UpdaterRequest* request, UpdaterReply* reply) noexcept {
    int result = PZ_PREPARATION_FAILED;
    const SPICE_SDK_V0* diagnostics{};
    State* state{};
    try {
        if (!request || !reply || !request->runtime || !request->sdk || !request->offer || !request->stopping)
            throw std::runtime_error("invalid updater request");
        if (request->sdk->log) diagnostics = request->sdk;
        if (!request->sdk->log || !request->sdk->get_avs_info)
            throw std::runtime_error("required Spice SDK capabilities unavailable");
        *reply = {};
        state = new State(*request, *reply, release_settings());
        // Paired with bootstrap's sequentially consistent stop/dispatch latch:
        // either stop sees this owner or the following stopping() sees stop.
        owner.store(state);
        if (request->stopping()) state->startup.request_stop();
        result = state->startup.run();
        if (result == updater_handoff) {
            // No core was loaded or initialized. Drain the inert owner before
            // another updater reopens the transaction or receives SDK shutdown.
            (void)state->startup.wait_finished(INFINITE);
            state->stop_notices();
        }
    } catch (const std::exception& e) {
        if (state) state->log(e.what());
        else construction_log(diagnostics, e.what());
    } catch (...) {
        if (state) state->log("unexpected updater failure");
        else construction_log(diagnostics, "unexpected updater failure");
    }
    return result;
}
const UpdaterApi api{updater_abi, run, stop};
} // namespace

extern "C" __declspec(dllexport) const UpdaterApi* __cdecl point_zero_updater_api(uint32_t abi) noexcept {
    return abi == updater_abi ? &api : nullptr;
}

BOOL WINAPI DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { module = self; DisableThreadLibraryCalls(self); }
    return TRUE;
}
