#include "folder_art.hpp"
#include "folder_metrics.hpp"

#include <windows.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace point_zero::native {
namespace {
template<class T> bool field(MemoryView memory, std::uintptr_t at, std::size_t offset, T& out) noexcept {
    return at >= 0x10000 && offset <= UINTPTR_MAX - at && sizeof(T) <= UINTPTR_MAX - (at + offset) &&
        read_value(memory, at + offset, out);
}
struct Info {
    alignas(8) std::array<std::byte, 56> bytes{};
    template<class T> T value(std::size_t offset) const noexcept {
        T result{};
        std::memcpy(&result, bytes.data() + offset, sizeof(result));
        return result;
    }
};
}
FolderArtwork::FolderArtwork(const AdapterContext& context, FolderArtCalls calls)
    : memory_(context.memory), state_(context.state), calls_(calls) {
    if (!memory_.read || !memory_.query || !calls.package || !calls.global_id ||
        !calls.global_info || !calls.package_info || !calls.test_flags)
        throw std::invalid_argument("incomplete admitted folder art calls");
}
bool FolderArtwork::preparing() const noexcept {
    return thread_ == GetCurrentThreadId() && state_.view().phase == FeaturePhase::preparing;
}
bool FolderArtwork::before_impl() {
    if (before_attempted_) return false;
    before_attempted_ = true;
    thread_ = GetCurrentThreadId();
    if (!preparing() || calls_.test_flags(0x80)) return false;
    for (const auto& image : folder_art::images)
        if (!preparing() || calls_.global_id(image.name) != -2) return false;
    absent_ = !calls_.test_flags(0x80) && preparing();
    return absent_;
}
bool FolderArtwork::before_load() noexcept {
    __try { return before_impl(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool FolderArtwork::live(std::uintptr_t slot, std::uint32_t package) const {
    std::array<char, 256> name{};
    int state{};
    std::uint32_t id{};
    return preparing() && field(memory_, slot, 8, name) &&
        std::string_view(name.data(), family.size()) == family && name[family.size()] == '\0' &&
        field(memory_, slot, 0x510, state) && state == 2 &&
        field(memory_, slot, 0x514, id) && id == package && calls_.package(family.data()) == id;
}
bool FolderArtwork::preload_impl(std::uintptr_t slot) {
    if (preload_attempted_) return false;
    preload_attempted_ = true;
    std::uint32_t package{};
    if (!absent_ || !preparing() || calls_.test_flags(0x80) ||
        !field(memory_, slot, 0x514, package) || (package & 0x78000000U) != 0x30000000U ||
        !live(slot, package)) return false;
    for (const auto& image : folder_art::images) {
        Info local, global;
        if (!preparing() || calls_.package_info(local.bytes.data(), package, image.name) ||
            calls_.global_info(global.bytes.data(), image.name)) return false;
        // Compare named semantic fields only: +2C is native unwritten padding,
        // and +30 is a borrowed string pointer. Do not dereference or retain it.
        if (local.value<std::int32_t>(0) <= 0 ||
            local.value<std::uint32_t>(0) != global.value<std::uint32_t>(0)) return false;
    }
    return live(slot, package) && !calls_.test_flags(0x80) && preparing();
}
bool FolderArtwork::preload(std::uintptr_t slot) noexcept {
    __try { return preload_impl(slot); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace point_zero::native
