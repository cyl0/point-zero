#include "prepared.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <rapidjson/document.h>

namespace {
using namespace point_zero;
namespace fs = std::filesystem;
struct InputFailure : std::runtime_error {
    int32_t status;
    InputFailure(int32_t code, const char* message) : std::runtime_error(message), status(code) {}
};

void diagnostic(PZ_Diagnostic out, const char* text) noexcept {
    if (!out.data || !out.capacity) return;
    const auto n = std::min(std::strlen(text), static_cast<size_t>(out.capacity - 1));
    std::memcpy(out.data, text, n);
    out.data[n] = '\0';
}
bool valid(PZ_Bytes b) noexcept { return b.size == 0 || b.data != nullptr; }
std::string_view view(PZ_Bytes b) {
    if (!valid(b) || b.size > std::numeric_limits<size_t>::max())
        throw std::invalid_argument("invalid byte span");
    return b.size ? std::string_view(reinterpret_cast<const char*>(b.data), static_cast<size_t>(b.size))
                  : std::string_view{};
}
PZ_Bytes bytes(std::string_view s) noexcept {
    return {reinterpret_cast<const uint8_t*>(s.data()), static_cast<uint64_t>(s.size())};
}
fs::path path(PZ_Bytes b) {
    const auto s = view(b);
    if (s.empty() || s.find('\0') != s.npos) throw std::invalid_argument("invalid absolute path");
    fs::path p(std::u8string(s.begin(), s.end()));
    if (!p.is_absolute()) throw std::invalid_argument("expected absolute path");
    return p.lexically_normal();
}
PZ_NativeHost host(const PZ_NativeHost& value) {
    const auto& avs = value.avs;
    if (!value.log || !avs.model[0] || !std::memchr(avs.model, '\0', sizeof(avs.model)) ||
        !avs.datecode[0] || !std::memchr(avs.datecode, '\0', sizeof(avs.datecode)))
        throw std::invalid_argument("invalid native host metadata or log callback");
    return value;
}
std::string utf8(const fs::path& p) {
    const auto u8 = p.generic_u8string();
    return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

int32_t PZ_CALL prepare(const PZ_PrepareRequest* request, PZ_Prepared** out,
                        PZ_Diagnostic message) noexcept {
    diagnostic(message, "");
    if (!request || request->size != sizeof(*request) || !out || *out || !request->read_input) {
        diagnostic(message, "invalid preparation request or nonempty output handle");
        return PZ_INVALID_ARGUMENT;
    }
    try {
        const auto generation = view(request->generation_id);
        if (generation.empty() || generation.find('\0') != generation.npos)
            throw std::invalid_argument("missing generation identity");
        const auto config = path(request->config_path);
        const auto cache = view(request->optional_cache);
        auto prepared = std::make_unique<PZ_Prepared>();
        prepared->generation_id = generation;
        const auto read = [&](const fs::path& name) {
            const auto name_utf8 = utf8(name);
            PZ_Bytes input{};
            char read_message[512]{};
            const auto status = request->read_input(request->input_context, bytes(name_utf8), &input,
                                                   {read_message, sizeof(read_message)});
            read_message[sizeof(read_message) - 1] = '\0';
            if (status != PZ_OK)
                throw InputFailure(status == PZ_CANCELLED ? PZ_CANCELLED : PZ_PREPARATION_FAILED,
                                   read_message[0] ? read_message : "host input unavailable");
            return std::string(view(input));
        };
        prepared->ratings = ratings::prepare(config, read, cache);
        // The selected manifest is already pinned by the loader. Read its
        // display version once through the existing snapshot, without an ABI
        // change or any disk/parsing work in the boot rendering callback.
        if (generation.size() == 64 && std::all_of(generation.begin(), generation.end(), [](char ch) {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); })) {
            try {
                const auto manifest = read(config.parent_path() / L"point-zero" / L"versions" /
                                           std::string(generation) / L"release.json");
                if (manifest.size() <= 1024 * 1024) {
                    rapidjson::Document metadata;
                    metadata.Parse(manifest.data(), manifest.size());
                    if (!metadata.HasParseError() && metadata.IsObject() && metadata.HasMember("version") &&
                        metadata["version"].IsString()) {
                        const auto& value = metadata["version"];
                        const std::string_view version(value.GetString(), value.GetStringLength());
                        if (!version.empty() && version.size() <= 64 &&
                            std::all_of(version.begin(), version.end(), [](char ch) {
                                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
                                       (ch >= 'A' && ch <= 'Z') || ch == '.' || ch == '-' || ch == '+';
                            })) prepared->release_version = version;
                    }
                }
            } catch (const InputFailure& e) {
                if (e.status == PZ_CANCELLED) throw;
            }
        }
        // Derived cache availability cannot invalidate an otherwise valid result.
        if (!prepared->ratings.cache_hit) {
            try { prepared->cache = ratings::serialize_cache(prepared->ratings); }
            catch (...) { prepared->cache.clear(); }
        }
        *out = prepared.release();
        return PZ_OK;
    } catch (const InputFailure& e) {
        diagnostic(message, e.what());
        return e.status;
    } catch (const std::invalid_argument& e) {
        diagnostic(message, e.what());
        return PZ_INVALID_ARGUMENT;
    } catch (const std::exception& e) {
        diagnostic(message, e.what());
        return PZ_PREPARATION_FAILED;
    } catch (...) {
        diagnostic(message, "unexpected preparation failure");
        return PZ_PREPARATION_FAILED;
    }
}
PZ_Bytes PZ_CALL cache_bytes(const PZ_Prepared* prepared) noexcept {
    return prepared ? bytes(prepared->cache) : PZ_Bytes{};
}
void PZ_CALL release(PZ_Prepared** prepared) noexcept {
    if (!prepared) return;
    delete *prepared;
    *prepared = nullptr;
}
int32_t PZ_CALL initialize(PZ_Prepared** prepared, const PZ_InitializeRequest* request,
                           PZ_Diagnostic message) noexcept {
    diagnostic(message, "");
    if (!prepared || !*prepared || !request || request->size != sizeof(*request) || !request->completion) {
        diagnostic(message, "invalid native initialization request");
        return PZ_INVALID_ARGUMENT;
    }
    try {
        core::NativeRequest native{path(request->runtime_path), path(request->generation_path),
                                   request->completion_context, request->completion, host(request->host)};
        std::unique_ptr<PZ_Prepared> owned(*prepared);
        *prepared = nullptr;
        return core::initialize_native(std::move(owned), std::move(native), message);
    } catch (const std::exception& e) {
        // All throwing argument conversion precedes ownership transfer. The
        // native implementation is noexcept and owns every later failure.
        diagnostic(message, e.what());
        return PZ_INVALID_ARGUMENT;
    } catch (...) {
        diagnostic(message, "unexpected initialization argument failure");
        return PZ_INVALID_ARGUMENT;
    }
}
void PZ_CALL shutdown() noexcept { core::shutdown_native(); }

constexpr PZ_CoreApi api{PZ_CORE_ABI, sizeof(PZ_CoreApi), prepare, cache_bytes, release, initialize, shutdown};
static_assert(std::is_standard_layout_v<PZ_CoreApi> && std::is_trivially_copyable_v<PZ_CoreApi>);
static_assert(std::is_standard_layout_v<PZ_PrepareRequest> && std::is_trivially_copyable_v<PZ_PrepareRequest>);
static_assert(std::is_standard_layout_v<PZ_InitializeRequest> && std::is_trivially_copyable_v<PZ_InitializeRequest>);
static_assert(std::is_standard_layout_v<PZ_NativeHost> && std::is_trivially_copyable_v<PZ_NativeHost>);
} // namespace

extern "C" __declspec(dllexport) const PZ_CoreApi* PZ_CALL point_zero_get_api(uint32_t abi) noexcept {
    return abi == PZ_CORE_ABI ? &api : nullptr;
}
