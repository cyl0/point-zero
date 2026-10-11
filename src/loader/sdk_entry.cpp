#include "updater_api.hpp"

#include <Windows.h>
#include <atomic>
#include <bit>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
constexpr int failed = 2, cancelled = 3, pending = std::numeric_limits<int>::min();
HMODULE module{};
SPICE_SDK_V0 sdk{};
std::atomic_bool attempted{false}, stopped{false};
std::atomic_int entry_result{pending};
std::atomic<const UpdaterApi*> current{};
#ifdef POINT_ZERO_TEST_LOADER
std::string test_repository;
#endif

void log(const char* message) noexcept {
    try { if (sdk.log) (void)sdk.log(SPICE_SDK_LOG_LEVEL_WARNING, "point-zero", message); } catch (...) {}
}
int __cdecl stopping() { return stopped.load(); }
void __cdecl destroy() noexcept {
    stopped.store(true);
    if (const auto* api = current.load()) api->stop();
}
fs::path runtime_path() {
    std::wstring name(32768, L'\0');
    const auto count = GetModuleFileNameW(module, name.data(), static_cast<DWORD>(name.size()));
    if (!count || count >= name.size()) throw std::runtime_error("bootstrap path unavailable");
    name.resize(count);
    return fs::canonical(fs::path(name).parent_path());
}
bool generation_id(const std::string& id) {
    if (id.size() != 64) return false;
    for (const char c : id) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
std::string pointer(const fs::path& runtime, const wchar_t* name) {
    const auto path = runtime / L"point-zero" / name;
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("installed POINT ZERO generation unavailable");
    char bytes[128]{};
    DWORD count{};
    const bool read = ReadFile(file, bytes, sizeof(bytes), &count, nullptr) != FALSE;
    CloseHandle(file);
    if (!read) throw std::runtime_error("installed generation could not be read");
    std::string id(bytes, count);
    if (id.ends_with('\n')) id.pop_back();
    if (id.ends_with('\r')) id.pop_back();
    if (!generation_id(id)) throw std::runtime_error("invalid installed generation");
    return id;
}
const UpdaterApi* load(const fs::path& runtime, const std::string& id) {
    if (!generation_id(id)) throw std::runtime_error("invalid updater generation");
    const auto path = runtime / L"point-zero/versions" / id / L"updater.dll";
#ifdef POINT_ZERO_TEST_LOADER
    const bool configured = GetModuleHandleW(path.c_str()) != nullptr;
#endif
    const auto library = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) throw std::runtime_error("selected POINT ZERO updater could not load");
    try {
        const auto get = std::bit_cast<GetUpdaterApi>(GetProcAddress(library, "point_zero_updater_api"));
        const auto* api = get ? get(updater_abi) : nullptr;
        if (!api || api->abi != updater_abi || !api->run || !api->stop)
            throw std::runtime_error("POINT ZERO updater interface unavailable");
#ifdef POINT_ZERO_TEST_LOADER
        if (!configured && !test_repository.empty()) {
            using Configure = int32_t (__cdecl *)(const char*);
            const auto configure = std::bit_cast<Configure>(GetProcAddress(library, "point_zero_test_configure"));
            if (!configure || configure(test_repository.c_str()) != 1)
                throw std::runtime_error("test updater configuration rejected");
        }
#endif
        // Keep the normal module reference until process exit: SDK callbacks
        // and native shutdown may outlive Spice's bounded destroy callback.
        return api;
    } catch (...) { FreeLibrary(library); throw; }
}
} // namespace

#ifdef POINT_ZERO_TEST_LOADER
extern "C" __declspec(dllexport) int32_t __cdecl point_zero_test_configure(const char* repository) noexcept {
    try {
        if (attempted.load() || !test_repository.empty() || !repository || !*repository) return 0;
        test_repository = repository;
        return 1;
    } catch (...) { return 0; }
}
#endif

SPICE_SDK_ENTRY_POINT spice_sdk_entry_point(spice_sdk_init_func* initialize) noexcept {
    if (attempted.exchange(true)) {
        auto result = entry_result.load(std::memory_order_acquire);
        while (result == pending) {
            entry_result.wait(pending);
            result = entry_result.load(std::memory_order_acquire);
        }
        return result;
    }
    int result = failed;
    try {
        HMODULE pinned{};
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(module), &pinned))
            throw std::runtime_error("bootstrap lifetime could not be pinned");
        const auto runtime = runtime_path();
        sdk.size = sizeof(sdk);
        if (!initialize || initialize(0, destroy, &sdk) != SPICE_SDK_STATUS_SUCCESS)
            throw std::runtime_error("Spice SDK registration rejected");
        const UpdaterApi* selected{};
        try { selected = load(runtime, pointer(runtime, L"active.txt")); }
        catch (const std::exception& e) {
            log(e.what());
            selected = load(runtime, pointer(runtime, L"fallback.txt"));
            log("updater_recovery_fallback");
        }
        std::string offer;
        uint32_t skip_update = 0;
        // Installed -> replacement -> installed fallback. Bound a defective
        // updater's handoff loop without adding persistent failure state.
        for (unsigned attempt = 0; attempt != 4; ++attempt) {
            current.store(selected);
            const UpdaterRequest request{runtime.c_str(), &sdk, offer.c_str(), skip_update, stopping};
            UpdaterReply reply{};
            result = selected->run(&request, &reply);
            if (result != updater_handoff) {
                if (result != 0 && result != cancelled) log("POINT ZERO updater failed; see the preceding log for details.");
                break;
            }
            if (stopping()) { result = cancelled; break; }
            if (reply.generation[64] != '\0') throw std::runtime_error("invalid updater handoff");
            skip_update = 1; // One network check/download per launch.
            offer = reply.offered ? reply.generation : "";
            try { selected = load(runtime, reply.generation); }
            catch (const std::exception& e) {
                log(e.what());
                log("update_retained_installed");
                offer.clear(); // Existing updater retries its installed release.
            }
        }
        if (result == updater_handoff) throw std::runtime_error("updater handoff did not settle");
    } catch (const std::exception& e) { log(e.what()); result = failed; }
    catch (...) { log("unexpected POINT ZERO bootstrap failure"); result = failed; }
    if (stopping()) result = cancelled;
    entry_result.store(result, std::memory_order_release);
    entry_result.notify_all();
    return result;
}

BOOL WINAPI DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { module = self; DisableThreadLibraryCalls(self); }
    return TRUE;
}
