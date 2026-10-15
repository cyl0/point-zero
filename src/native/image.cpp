#include "image.hpp"

#include <windows.h>
#include <stdexcept>
#include <utility>

namespace point_zero::native {
namespace {
[[noreturn]] void invalid(const char* what) { throw std::runtime_error(what); }
template<class T> T value(MemoryView memory, std::uintptr_t address) {
    T out{};
    if (!read_value(memory, address, out)) invalid("native image read failed");
    return out;
}
std::uintptr_t address(MemoryView memory, const ImageInfo& image, std::size_t rva, std::size_t count) {
    if (rva > image.range.size || count > image.range.size - rva ||
        !contains(image.range.base, image.range.size, image.range.base, image.range.size))
        invalid("native image reference is out of bounds");
    const auto result = image.range.base + rva;
    if (!readable_range(memory, result, count, RangeUse::image_read, image.range))
        invalid("native image reference has the wrong allocation or protection");
    return result;
}
template<class T> T at(MemoryView memory, const ImageInfo& image, std::size_t rva) {
    return value<T>(memory, address(memory, image, rva, sizeof(T)));
}
} // namespace

ImageInfo inspect_image(MemoryView memory, std::uintptr_t base) {
    // Bound the initial header reads before SizeOfImage is available. Each copy
    // still requires readable pages belonging to the selected image allocation.
    const ImageInfo headers{{base, 0x20000}, {}};
    const auto dos = at<IMAGE_DOS_HEADER>(memory, headers, 0);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 0x10000)
        invalid("invalid native DOS header");
    const auto nt_rva = static_cast<std::uint32_t>(dos.e_lfanew);
    const auto nt = at<IMAGE_NT_HEADERS64>(memory, headers, nt_rva);
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64) ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt.OptionalHeader.NumberOfRvaAndSizes > 16)
        invalid("unsupported or malformed native PE64 header");
    const auto header_end = static_cast<std::size_t>(nt_rva) + sizeof(nt);
    if (nt.OptionalHeader.SizeOfHeaders < header_end || nt.OptionalHeader.SizeOfHeaders > 0x20000 ||
        nt.OptionalHeader.SizeOfImage < nt.OptionalHeader.SizeOfHeaders)
        invalid("invalid native image/header extent");
    ImageInfo result{{base, nt.OptionalHeader.SizeOfImage}, {}};
    result.timestamp = nt.FileHeader.TimeDateStamp;
    result.entry_rva = nt.OptionalHeader.AddressOfEntryPoint;
    for (std::size_t i = 0; i < nt.OptionalHeader.NumberOfRvaAndSizes; ++i)
        result.directories[i] = {nt.OptionalHeader.DataDirectory[i].VirtualAddress, nt.OptionalHeader.DataDirectory[i].Size};
    return result;
}

std::uintptr_t resolve_export(MemoryView memory, const ImageInfo& image, std::uint16_t ordinal) {
    const auto target = reinterpret_cast<std::uintptr_t>(GetProcAddress(
        reinterpret_cast<HMODULE>(image.range.base), MAKEINTRESOURCEA(ordinal)));
    if (!callable_range(memory, target)) invalid("native export is not callable");
    return target;
}

std::vector<std::byte> read_code(MemoryView memory, ImageRange image, std::uint32_t rva, std::size_t size) {
    if (rva > image.size || size > image.size - rva ||
        !contains(image.base, image.size, image.base, image.size) ||
        !callable_range(memory, image.base + rva, size)) invalid("native code range is unavailable");
    std::vector<std::byte> body(size);
    if (!read_bytes(memory, image.base + rva, body)) invalid("native code read failed");
    return body;
}

ModuleLease::ModuleLease(std::wstring_view name) {
    if (name.empty() || name.find(L'\0') != name.npos || name.find_first_of(L"/\\") != name.npos)
        invalid("expected a loaded module basename");
    const std::wstring owned(name);
    HMODULE module{};
    if (!GetModuleHandleExW(0, owned.c_str(), &module)) invalid("required native module is not loaded");
    try { image_ = inspect_image(current_process_memory(), reinterpret_cast<std::uintptr_t>(module)); }
    catch (...) { FreeLibrary(module); throw; }
    handle_ = module;
}
ModuleLease::~ModuleLease() { if (handle_) FreeLibrary(static_cast<HMODULE>(handle_)); }
ModuleLease::ModuleLease(ModuleLease&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), image_(std::move(other.image_)) { other.image_ = {}; }
ModuleLease& ModuleLease::operator=(ModuleLease&& other) noexcept {
    if (this != &other) {
        if (handle_) FreeLibrary(static_cast<HMODULE>(handle_));
        handle_ = std::exchange(other.handle_, nullptr);
        image_ = std::move(other.image_); other.image_ = {};
    }
    return *this;
}
} // namespace point_zero::native
