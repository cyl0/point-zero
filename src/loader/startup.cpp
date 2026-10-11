#include "startup.hpp"
#include "point_zero/core_api.h"

#include <Windows.h>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

namespace point_zero::loader {
namespace {
namespace fs = std::filesystem;
constexpr int pending = std::numeric_limits<int>::min();
constexpr unsigned stopping = 1, native_begun = 2;
void require(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
PZ_Bytes bytes(std::string_view s) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()};
}
std::string utf8(const fs::path& p) {
    const auto s = p.generic_u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
std::string_view view(PZ_Bytes b) {
    require(b.size <= 128ULL * 1024 * 1024 && (!b.size || b.data), "invalid core byte span");
    return b.size ? std::string_view(reinterpret_cast<const char*>(b.data), static_cast<size_t>(b.size)) : std::string_view{};
}
std::string_view trim(std::string_view s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == s.npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}
// Only update-channel belongs to the updater. The selected core remains
// the sole authority for all rating/config semantics and rejects invalid lines.
Channel channel(std::string_view config) {
    require(config.find('\0') == config.npos, "NUL in configuration");
    if (config.starts_with("\xef\xbb\xbf")) config.remove_prefix(3);
    Channel result = Channel::stable;
    while (!config.empty()) {
        const auto end = config.find('\n');
        const auto line = trim(config.substr(0, end));
        if (!line.empty() && line.front() != '#') {
            const auto equal = line.find('=');
            if (equal != line.npos && trim(line.substr(0, equal)) == "update-channel") {
                const auto value = trim(line.substr(equal + 1));
                require(value == "stable" || value == "preview", "update-channel must be stable or preview");
                result = value == "stable" ? Channel::stable : Channel::preview;
            }
        }
        if (end == config.npos) break;
        config.remove_prefix(end + 1);
    }
    return result;
}
struct Event {
    HANDLE value = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Event() { require(value != nullptr, "startup event creation failed"); }
    ~Event() { CloseHandle(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
};
struct Core {
    HMODULE module{};
    const PZ_CoreApi* api{};
    PZ_Prepared* prepared{};
    bool entered = false;
    explicit Core(const fs::path& path) {
        // The core is statically linked. Do not search unmanifested neighboring
        // DLLs or the working directory for its dependencies.
        module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        require(module != nullptr, "selected core could not load");
        try {
            const auto get = std::bit_cast<PZ_GetCoreApi>(GetProcAddress(module, "point_zero_get_api"));
            require(get != nullptr, "selected core export unavailable");
            api = get(PZ_CORE_ABI);
            require(api && api->abi == PZ_CORE_ABI && api->size == sizeof(PZ_CoreApi) && api->prepare &&
                    api->release_prepared && api->cache_bytes && api->initialize_prepared && api->shutdown,
                    "selected core ABI 5 unavailable");
        } catch (...) { FreeLibrary(module); module = nullptr; throw; }
    }
    ~Core() {
        if (prepared) api->release_prepared(&prepared);
        // No hot-unload after entering native initialization, including failure.
        // Windows retires this module reference at process exit.
        if (module && !entered) FreeLibrary(module);
    }
};
} // namespace

struct Startup::Impl : std::enable_shared_from_this<Impl> {
    StartupOptions options;
    Stop stop;
    Event entry_done, completed, finished;
    std::atomic_bool attempted{false};
    std::atomic_uint phase{0};
    std::atomic_int entry_result{pending}, admission{pending};
    std::unique_ptr<Transaction> transaction;
    std::unique_ptr<InputSnapshot> inputs;
    std::unique_ptr<Core> core;
    std::string cache;
    bool updated{}; // Selected candidate survived inert preparation.

    explicit Impl(StartupOptions o) : options(std::move(o)) {
        require(options.runtime.is_absolute(), "runtime must be absolute");
        options.runtime = fs::canonical(options.runtime);
        require(static_cast<bool>(options.update), "update source unavailable");
        require(static_cast<bool>(options.native_host), "native host source unavailable");
    }
    void note(std::string_view message) noexcept {
        try { if (options.log) options.log(message); } catch (...) {}
    }
    void report(StartupNotice notice) noexcept {
        try { if (!stop.requested() && options.notice) options.notice(notice); } catch (...) {}
    }
    void check_stop() const { require(!(phase.load(std::memory_order_acquire) & stopping), "startup cancelled"); }
    void cancel() noexcept {
        // Linearizes cancellation against the native initialization boundary.
        phase.fetch_or(stopping, std::memory_order_acq_rel);
        stop.request();
    }
    static void PZ_CALL completion(void* data, int32_t status) noexcept {
        auto& self = *static_cast<Impl*>(data);
        auto before = pending;
        const auto result = status == PZ_OK ? PZ_OK : status == PZ_CANCELLED ? PZ_CANCELLED : PZ_NATIVE_FAILED;
        if (self.admission.compare_exchange_strong(before, result, std::memory_order_acq_rel))
            SetEvent(self.completed.value);
    }
    static DWORD WINAPI worker(void* data) noexcept {
        std::unique_ptr<std::shared_ptr<Impl>> holder(static_cast<std::shared_ptr<Impl>*>(data));
        auto self = std::move(*holder);
        holder.reset();
        self->retire();
        return 0;
    }
    void start_worker() {
        auto reference = std::make_unique<std::shared_ptr<Impl>>(shared_from_this());
        const auto thread = CreateThread(nullptr, 0, worker, reference.get(), 0, nullptr);
        require(thread != nullptr, "startup worker unavailable");
        reference.release();
        CloseHandle(thread); // 'finished' owns the observable join boundary.
    }
    void retire() noexcept {
        // Entry owns all core/transaction calls until this handoff, even after
        // shutdown's bounded wait has expired. No concurrent cleanup is allowed.
        WaitForSingleObject(entry_done.value, INFINITE);
        if (core && core->entered) {
            HANDLE waits[]{static_cast<HANDLE>(stop.event()), completed.value};
            const auto wake = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            const auto terminal_status = admission.load(std::memory_order_acquire);
            const auto returned_status = entry_result.load(std::memory_order_acquire);
            // A known rejection survives a later stop. Cancellation is an
            // actual cancelled result, or stop with no known native rejection.
            const bool rejected = terminal_status == PZ_NATIVE_FAILED || returned_status == PZ_NATIVE_FAILED;
            bool cancelled = !rejected && (stop.requested() || terminal_status == PZ_CANCELLED || returned_status == PZ_CANCELLED);
            const bool admitted = wake == WAIT_OBJECT_0 + 1 && !stop.requested() &&
                                  terminal_status == PZ_OK && returned_status == PZ_OK;
            bool acknowledged = false;
            try {
                transaction->finish(admitted, cancelled);
                acknowledged = admitted;
                if (admitted && !cache.empty()) {
                    try {
                        Files files(options.runtime / L"point-zero", &stop.flag());
                        files.put("cache.bin", cache);
                        note("cache_published");
                    } catch (const std::exception& e) { note(e.what()); }
                }
            } catch (const std::exception& e) {
                note(e.what());
                if (!rejected && stop.requested()) cancelled = true;
            }
            // Source protection lasts through the terminal result and ACK.
            inputs.reset();
            std::string{}.swap(cache);
            note(cancelled ? "admission_cancelled" : admitted ? (acknowledged ? "admission_complete" : "admission_ack_failed") : "admission_failed");
            if (!cancelled && (!admitted || !acknowledged)) report(StartupNotice::failed);
            else if (admitted && acknowledged && updated) report(StartupNotice::updated);
            if (!stop.requested()) WaitForSingleObject(static_cast<HANDLE>(stop.event()), INFINITE);
            core->api->shutdown(); // Serialized after initialize, exactly once.
            note("core_shutdown");
        }
        inputs.reset(); core.reset(); transaction.reset();
        std::string{}.swap(cache);
        note("startup_finished");
        SetEvent(finished.value);
    }
    bool prepare_selected(const std::string& config_path) {
        check_stop();
        const auto& chosen = transaction->selected();
        require(chosen.release.core_abi == PZ_CORE_ABI, "selected core ABI unavailable in this updater");
        note("before_core_load"); check_stop();
        core = std::make_unique<Core>(chosen.directory / chosen.release.core().path);
        note("core_loaded"); check_stop();
        inputs = std::make_unique<InputSnapshot>(transaction->replacements(), &stop.flag());
        std::string old_cache;
        try {
            Files files(options.runtime / L"point-zero", &stop.flag());
            old_cache = files.read("cache.bin", 128 * 1024 * 1024).value_or("");
        } catch (const std::exception& e) { check_stop(); note(e.what()); }
        const PZ_PrepareRequest request{sizeof(request), bytes(chosen.release.id), bytes(config_path),
                                        inputs.get(), InputSnapshot::read_for_core, bytes(old_cache)};
        char message[1024]{};
        note("before_prepare"); check_stop();
        const auto result = core->api->prepare(&request, &core->prepared, {message, sizeof(message)});
        message[sizeof(message) - 1] = '\0';
        if (message[0]) note(message);
        check_stop();
        if (result != PZ_OK) return false;
        require(core->prepared != nullptr, "core returned no prepared state");
        try { cache = view(core->api->cache_bytes(core->prepared)); }
        catch (const std::exception& e) { cache.clear(); note(e.what()); }
        note("prepared");
        return true;
    }
    bool handoff(bool offered) {
        if (!options.handoff || !options.handoff(transaction->selected(), offered)) return false;
        // Nothing native has started. The next updater reopens the transaction
        // and revalidates the offer; active.txt still names the installed release.
        transaction.reset();
        note("updater_handoff");
        return true;
    }
    int execute() {
        check_stop();
        transaction = std::make_unique<Transaction>(options.runtime,
            [this](std::string_view s) { note(s); }, &stop.flag());
        transaction->begin();
        // Recover the installed before-image even when SDK metadata is
        // unavailable. No new config/update/activation work precedes this fetch.
        check_stop();
        const auto host = options.native_host();
        const auto game_major = options.game_major ? options.game_major() : transaction->selected().release.major;
        require(game_major > 0 && game_major <= 999, "invalid detected game major");
        check_stop();
        const auto config_path = utf8(options.runtime / L"point-zero.conf");
        // Keep the small early config snapshot pinned until core preparation has
        // captured the same file; an edit cannot change channels under its feet.
        auto config = std::make_unique<InputSnapshot>(InputSnapshot::Replacements{}, &stop.flag());
        const auto requested = channel(view(config->read(options.runtime / L"point-zero.conf")));
        bool offered = false;
        try {
            check_stop();
            const auto id = options.update(transaction->selected().release, requested, stop);
            check_stop();
            if (id) offered = transaction->offer(*id, requested, game_major);
        } catch (const std::exception& e) { check_stop(); note(e.what()); note("update_retained_installed"); }
        require(transaction->selected().release.major == game_major,
                "POINT ZERO has no available release for the detected IIDX major");
        if (handoff(offered)) return updater_handoff;
        try {
            require(prepare_selected(config_path), "selected preparation rejected");
        } catch (const std::exception& e) {
            check_stop();
            if (!offered) throw;
            note(e.what());
            // All candidate work was inert. Dispose before selecting installed.
            inputs.reset(); core.reset(); cache.clear();
            transaction->discard_offer();
            offered = false;
            note("candidate_disposed");
            require(transaction->selected().release.major == game_major,
                    "POINT ZERO cannot fall back to a different IIDX major");
            if (handoff(false)) return updater_handoff;
            require(prepare_selected(config_path), "installed preparation rejected");
        }
        config.reset();
        updated = offered;
        check_stop();
        transaction->activate(*inputs);
        // Finish request construction before recording a native attempt. No
        // allocating path conversion belongs after the durable checkpoint.
        const auto runtime = utf8(options.runtime), generation = utf8(transaction->selected().directory);
        const PZ_InitializeRequest request{sizeof(request), bytes(runtime), bytes(generation), this, completion, host};
        char message[1024]{};
        note("before_native_initialize");
        unsigned expected = 0;
        if (!phase.compare_exchange_strong(expected, native_begun, std::memory_order_acq_rel))
            throw std::runtime_error("startup cancelled");
        // This is the serialized point after which disposal/retry is forbidden.
        try { transaction->enter_native(); }
        catch (...) {
            // A checkpoint failure prevented the native call. Preserve a stop
            // that arrived meanwhile, but do not claim native entry occurred.
            phase.fetch_and(~native_begun, std::memory_order_acq_rel);
            throw;
        }
        core->entered = true;
        const auto result = core->api->initialize_prepared(&core->prepared, &request, {message, sizeof(message)});
        message[sizeof(message) - 1] = '\0';
        if (message[0]) note(message);
        if (result != PZ_OK || core->prepared) {
            completion(this, result == PZ_CANCELLED ? PZ_CANCELLED : PZ_NATIVE_FAILED);
            note("native_initialization_rejected");
            return result == PZ_CANCELLED ? PZ_CANCELLED : PZ_NATIVE_FAILED;
        }
        note("native_initialization_returned");
        return PZ_OK; // Actual game admission is a later callback, never awaited.
    }
    int run() noexcept {
        bool no = false;
        if (!attempted.compare_exchange_strong(no, true)) {
            WaitForSingleObject(entry_done.value, INFINITE);
            return entry_result.load(std::memory_order_acquire);
        }
        bool worker_started = false;
        int result = PZ_PREPARATION_FAILED;
        try {
            start_worker(); worker_started = true;
            result = execute();
        } catch (const std::exception& e) { note(e.what()); }
        catch (...) { note("unexpected startup failure"); }
        if (stop.requested()) result = PZ_CANCELLED;
        // Before native readiness, startup failure is reported through the log.
        // No surviving native owner can make a queued toast visible here.
        if (result != PZ_OK && core && core->entered) completion(this, result == PZ_CANCELLED ? PZ_CANCELLED : PZ_NATIVE_FAILED);
        entry_result.store(result, std::memory_order_release);
        SetEvent(entry_done.value);
        if (!worker_started) SetEvent(finished.value);
        return result;
    }
};

Startup::Startup(StartupOptions options) : impl_(std::make_shared<Impl>(std::move(options))) {}
Startup::~Startup() { request_stop(); (void)wait_finished(650); }
int Startup::run() noexcept { const auto self = impl_; return self->run(); }
void Startup::request_stop() noexcept { impl_->cancel(); }
bool Startup::wait_finished(std::uint32_t ms) const noexcept {
    if (!impl_->attempted.load(std::memory_order_acquire)) return true;
    return WaitForSingleObject(impl_->finished.value, ms) == WAIT_OBJECT_0;
}
} // namespace point_zero::loader
