#include "files.hpp"
#include "release.hpp"

#include <Windows.h>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace point_zero::loader {
namespace {
namespace fs = std::filesystem;
struct Handle {
    HANDLE value;
    explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : value(h) {}
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(Handle&& other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = std::exchange(other.value, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
[[noreturn]] void failure(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": Windows error " + std::to_string(GetLastError()));
}
void require(bool test, const char* error) { if (!test) throw std::runtime_error(error); }
void check_stop(const std::atomic_bool* cancelled) {
    if (cancelled && cancelled->load(std::memory_order_relaxed)) throw std::runtime_error("startup cancelled");
}
DWORD attributes(const fs::path& p) {
    const auto attr = GetFileAttributesW(p.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) failure("file attributes");
    }
    require(attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_REPARSE_POINT), "reparse point in owned path");
    return attr;
}
Handle directory(const fs::path& p) {
    Handle h(CreateFileW(p.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (h.value == INVALID_HANDLE_VALUE) failure("pin owned directory");
    // Only attributes are needed; FileAttributeTagInfo is unsupported on FAT.
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h.value, &info)) failure("inspect owned directory");
    require((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT),
            "owned parent must be a plain directory");
    return h;
}
std::vector<Handle> parents(const fs::path& root, const fs::path& file, bool create) {
    std::vector<Handle> held;
    auto p = root;
    held.push_back(directory(p));
    for (const auto& component : file.parent_path().lexically_relative(root)) {
        if (component == L".") continue;
        p /= component;
        if (attributes(p) == INVALID_FILE_ATTRIBUTES && create)
            if (!CreateDirectoryW(p.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) failure("create owned directory");
        held.push_back(directory(p));
    }
    return held;
}
} // namespace

struct FilePin::Impl { std::vector<Handle> directories; Handle handle; const std::atomic_bool* cancelled{}; };
FilePin::FilePin(const fs::path& path, const std::atomic_bool* cancelled, bool allow_removal) : impl_(std::make_unique<Impl>()) {
    impl_->cancelled = cancelled;
    check_stop(cancelled);
    require(path.is_absolute(), "pinned path must be absolute");
    impl_->directories = parents(path.root_path(), path, false);
    impl_->handle = Handle(CreateFileW(path.c_str(), GENERIC_READ | (allow_removal ? DELETE : 0), FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (impl_->handle.value == INVALID_HANDLE_VALUE) failure("pin file");
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(impl_->handle.value, &info)) failure("inspect pinned file");
    require(!(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)), "expected plain pinned file");
}
FilePin::~FilePin() = default;
FilePin::FilePin(FilePin&&) noexcept = default;
FilePin& FilePin::operator=(FilePin&&) noexcept = default;
void FilePin::erase() {
    check_stop(impl_->cancelled);
    FILE_DISPOSITION_INFO disposition{TRUE};
    if (!SetFileInformationByHandle(impl_->handle.value, FileDispositionInfo, &disposition, sizeof(disposition)))
        failure("erase pinned file");
}
std::uint64_t FilePin::size() const {
    LARGE_INTEGER bytes{};
    if (!GetFileSizeEx(impl_->handle.value, &bytes)) failure("pinned file length");
    require(bytes.QuadPart >= 0, "invalid pinned file length");
    return static_cast<std::uint64_t>(bytes.QuadPart);
}
std::string FilePin::read(std::size_t limit) const {
    check_stop(impl_->cancelled);
    const auto bytes = size();
    require(bytes <= limit, "pinned file exceeds size limit");
    if (!SetFilePointerEx(impl_->handle.value, {}, nullptr, FILE_BEGIN)) failure("rewind pinned file");
    std::string result(static_cast<size_t>(bytes), '\0');
    size_t offset = 0;
    while (offset < result.size()) {
        check_stop(impl_->cancelled);
        const auto chunk = static_cast<DWORD>(std::min<size_t>(result.size() - offset, 1024 * 1024));
        DWORD got{};
        if (!ReadFile(impl_->handle.value, result.data() + offset, chunk, &got, nullptr)) failure("read pinned file");
        require(got != 0, "short pinned file read");
        offset += got;
    }
    check_stop(impl_->cancelled);
    return result;
}
std::string FilePin::read_prefix(std::size_t bytes) const {
    check_stop(impl_->cancelled);
    require(bytes <= 1024 * 1024 && size() >= bytes, "pinned prefix size limit");
    if (!SetFilePointerEx(impl_->handle.value, {}, nullptr, FILE_BEGIN)) failure("rewind pinned file");
    std::string result(bytes, '\0');
    DWORD got{};
    if (!ReadFile(impl_->handle.value, result.data(), static_cast<DWORD>(bytes), &got, nullptr)) failure("read pinned prefix");
    require(got == bytes, "short pinned prefix read");
    check_stop(impl_->cancelled);
    return result;
}

struct Files::Impl {
    fs::path root;
    std::vector<Handle> ancestors;
    Handle lock;
    std::uint64_t serial{};
    const std::atomic_bool* cancelled{};
};
Files::Files(fs::path root, const std::atomic_bool* cancelled) : impl_(std::make_unique<Impl>()) {
    impl_->cancelled = cancelled;
    check_stop(cancelled);
    require(root.is_absolute(), "owned directory must be absolute");
    impl_->root = root.lexically_normal();
    auto p = impl_->root.root_path();
    impl_->ancestors.push_back(directory(p));
    for (const auto& component : impl_->root.relative_path()) {
        p /= component;
        impl_->ancestors.push_back(directory(p));
    }
}
Files::~Files() = default;
const fs::path& Files::root() const noexcept { return impl_->root; }
fs::path Files::path(std::string_view relative) const {
    check_stop(impl_->cancelled);
    require(valid_relative_path(relative), "invalid owned relative path");
    auto p = impl_->root;
    for (const auto& part : fs::path(std::string(relative))) {
        p /= part;
        (void)attributes(p);
    }
    return p;
}
std::unique_ptr<FilePin> Files::pin(std::string_view relative) const {
    return std::make_unique<FilePin>(path(relative), impl_->cancelled);
}
std::optional<std::string> Files::read(std::string_view relative, size_t limit) const {
    const auto p = path(relative);
    if (attributes(p) == INVALID_FILE_ATTRIBUTES) return {};
    return FilePin(p, impl_->cancelled).read(limit);
}
void Files::put(std::string_view relative, std::string_view bytes, bool replace) {
    const auto p = path(relative);
    const auto held = parents(impl_->root, p, true);
    auto temp = p;
    temp += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++impl_->serial) + L".tmp";
    bool created = false;
    try {
        {
            Handle h(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (h.value == INVALID_HANDLE_VALUE) failure("create atomic temporary file");
            created = true;
            size_t offset = 0;
            while (offset < bytes.size()) {
                check_stop(impl_->cancelled);
                const auto chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1024 * 1024));
                DWORD wrote{};
                if (!WriteFile(h.value, bytes.data() + offset, chunk, &wrote, nullptr)) failure("write atomic temporary file");
                require(wrote != 0, "short atomic file write");
                offset += wrote;
            }
            if (!FlushFileBuffers(h.value)) failure("flush atomic temporary file");
        }
        check_stop(impl_->cancelled);
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temp.c_str(), p.c_str(), flags)) failure("publish atomic file");
    } catch (...) {
        if (created) DeleteFileW(temp.c_str());
        throw;
    }
}
void Files::erase(std::string_view relative) {
    const auto p = path(relative);
    if (attributes(p) == INVALID_FILE_ATTRIBUTES) return;
    const auto held = parents(impl_->root, p, false);
    if (!DeleteFileW(p.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND) failure("erase owned file");
}
void Files::erase(std::string_view relative, std::string_view expected_sha256) {
    require(erase_if_matches(relative, expected_sha256), "new rating changed before rollback");
}
bool Files::erase_if_matches(std::string_view relative, std::string_view expected_sha256) {
    const auto p = path(relative);
    const auto attr = attributes(p);
    if (attr == INVALID_FILE_ATTRIBUTES) return true;
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return false;
    FilePin file(p, impl_->cancelled, true);
    if (file.size() > 128 * 1024 * 1024 ||
        digest(file.read(128 * 1024 * 1024), impl_->cancelled) != expected_sha256) return false;
    file.erase();
    return true;
}
void Files::rename(std::string_view from, std::string_view to) {
    const auto a = path(from), b = path(to);
    const auto first = parents(impl_->root, a, false), second = parents(impl_->root, b, true);
    if (!MoveFileExW(a.c_str(), b.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) failure("rename owned file");
}
void Files::lock() {
    require(impl_->lock.value == INVALID_HANDLE_VALUE, "update lock already acquired");
    impl_->lock = Handle(CreateFileW(path("update.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (impl_->lock.value == INVALID_HANDLE_VALUE) failure("update already owned");
}
} // namespace point_zero::loader
