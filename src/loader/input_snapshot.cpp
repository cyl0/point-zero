#include "input_snapshot.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

namespace point_zero::loader {
namespace {
namespace fs = std::filesystem;
struct Cancelled : std::runtime_error { Cancelled() : std::runtime_error("startup cancelled") {} };
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& h) noexcept : value(std::exchange(h.value, INVALID_HANDLE_VALUE)) {}
    Handle& operator=(Handle&& h) noexcept {
        if (this != &h) {
            if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = std::exchange(h.value, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
};
struct PathLess {
    bool operator()(const fs::path& a, const fs::path& b) const noexcept {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};
fs::path logical_path(const fs::path& p) {
    if (!p.is_absolute() || p.native().find(L'\0') != p.native().npos)
        throw std::runtime_error("input path must be absolute");
    return p.lexically_normal();
}
std::string utf8(const fs::path& p) {
    const auto text = p.generic_u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
void check_stop(const std::atomic_bool* cancelled) {
    if (cancelled && cancelled->load(std::memory_order_relaxed)) throw Cancelled();
}
struct Input { Handle handle; std::string bytes; };
Input capture(const fs::path& p, const std::atomic_bool* cancelled) {
    check_stop(cancelled);
    Input input{Handle(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)), {}};
    if (input.handle.value == INVALID_HANDLE_VALUE)
        throw std::runtime_error("cannot capture input: " + utf8(p));
    LARGE_INTEGER length{};
    constexpr LONGLONG source_limit = 128 * 1024 * 1024;
    if (!GetFileSizeEx(input.handle.value, &length) || length.QuadPart < 0 || length.QuadPart > source_limit)
        throw std::runtime_error("input exceeds 128 MiB limit: " + utf8(p));
    input.bytes.resize(static_cast<size_t>(length.QuadPart));
    size_t offset = 0;
    while (offset < input.bytes.size()) {
        check_stop(cancelled);
        const auto chunk = static_cast<DWORD>(std::min<size_t>(input.bytes.size() - offset, 1024 * 1024));
        DWORD received{};
        if (!ReadFile(input.handle.value, input.bytes.data() + offset, chunk, &received, nullptr) || !received)
            throw std::runtime_error("short or failed input read: " + utf8(p));
        offset += received;
    }
    check_stop(cancelled);
    return input;
}
void diagnostic(PZ_Diagnostic out, const char* text) noexcept {
    if (!out.data || !out.capacity) return;
    const auto n = std::min(std::strlen(text), static_cast<size_t>(out.capacity - 1));
    std::memcpy(out.data, text, n);
    out.data[n] = '\0';
}
} // namespace

struct InputSnapshot::Impl {
    std::map<fs::path, fs::path, PathLess> replacements;
    std::map<fs::path, Input, PathLess> inputs;
    const std::atomic_bool* cancelled;
};
InputSnapshot::InputSnapshot(Replacements replacements, const std::atomic_bool* cancelled)
    : impl_(std::make_unique<Impl>()) {
    impl_->cancelled = cancelled;
    for (const auto& [logical, actual] : replacements)
        if (!impl_->replacements.emplace(logical_path(logical), logical_path(actual)).second)
            throw std::runtime_error("duplicate managed input substitution");
}
InputSnapshot::~InputSnapshot() = default;

PZ_Bytes InputSnapshot::read(const fs::path& logical) {
    check_stop(impl_->cancelled);
    const auto path = logical_path(logical);
    auto found = impl_->inputs.find(path);
    if (found == impl_->inputs.end()) {
        const auto replacement = impl_->replacements.find(path);
        const auto& actual = replacement == impl_->replacements.end() ? path : replacement->second;
        found = impl_->inputs.emplace(path, capture(actual, impl_->cancelled)).first;
    }
    const auto& bytes = found->second.bytes;
    return {reinterpret_cast<const uint8_t*>(bytes.data()), static_cast<uint64_t>(bytes.size())};
}
void InputSnapshot::bind_installed() {
    check_stop(impl_->cancelled);
    if (impl_->replacements.empty()) return;
    std::vector<std::pair<Input*, Handle>> installed;
    installed.reserve(impl_->inputs.size());
    for (auto& [logical, captured] : impl_->inputs) {
        // Config and personal inputs already retain their original deny-write
        // handles. Only staged substitutions need an installed-byte handoff.
        if (!impl_->replacements.contains(logical)) continue;
        auto current = capture(logical, impl_->cancelled);
        if (current.bytes != captured.bytes)
            throw std::runtime_error("input changed during activation: " + utf8(logical));
        installed.emplace_back(&captured, std::move(current.handle));
    }
    for (auto& [captured, handle] : installed) captured->handle = std::move(handle);
    impl_->replacements.clear();
}

int32_t PZ_CALL InputSnapshot::read_for_core(void* context, PZ_Bytes logical, PZ_Bytes* out,
                                           PZ_Diagnostic message) noexcept {
    diagnostic(message, "");
    if (out) *out = {};
    if (!context || !out || !logical.data || !logical.size || logical.size > 32768 * 4) {
        diagnostic(message, "invalid input callback arguments");
        return PZ_INVALID_ARGUMENT;
    }
    try {
        const auto* chars = reinterpret_cast<const char*>(logical.data);
        const fs::path p(std::u8string(chars, chars + logical.size));
        *out = static_cast<InputSnapshot*>(context)->read(p);
        return PZ_OK;
    } catch (const Cancelled& e) {
        diagnostic(message, e.what());
        return PZ_CANCELLED;
    } catch (const std::exception& e) {
        diagnostic(message, e.what());
        return PZ_PREPARATION_FAILED;
    } catch (...) {
        diagnostic(message, "unexpected input capture failure");
        return PZ_PREPARATION_FAILED;
    }
}
} // namespace point_zero::loader
