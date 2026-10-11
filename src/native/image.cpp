#include "image.hpp"

#include <windows.h>
#include <algorithm>
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
std::string text(MemoryView memory, const ImageInfo& image, std::size_t rva, std::size_t limit = 512) {
    std::string out;
    for (std::size_t i = 0; i < limit; ++i) {
        const auto ch = at<char>(memory, image, rva + i);
        if (!ch) return out;
        out += ch;
    }
    invalid("unterminated native image name");
}
ImageDirectory directory(MemoryView memory, const ImageInfo& image, std::size_t which, std::size_t minimum) {
    const auto entry = image.directories.at(which);
    if (!entry.rva || entry.size < minimum) invalid("required native image directory is absent");
    (void)address(memory, image, entry.rva, entry.size);
    return entry;
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
    for (std::size_t i = 0; i < nt.OptionalHeader.NumberOfRvaAndSizes; ++i)
        result.directories[i] = {nt.OptionalHeader.DataDirectory[i].VirtualAddress, nt.OptionalHeader.DataDirectory[i].Size};
    constexpr std::array<std::size_t, 2> selected_directories{IMAGE_DIRECTORY_ENTRY_IMPORT, IMAGE_DIRECTORY_ENTRY_EXPORT};
    for (const auto i : selected_directories) {
        if (i >= nt.OptionalHeader.NumberOfRvaAndSizes) continue;
        const auto& entry = nt.OptionalHeader.DataDirectory[i];
        if (entry.Size &&
            (!entry.VirtualAddress || entry.VirtualAddress > result.range.size ||
             entry.Size > result.range.size - entry.VirtualAddress)) invalid("invalid native data directory extent");
    }
    return result;
}

ImportReference inspect_import(MemoryView memory, const ImageInfo& image, std::uint32_t slot_rva) {
    const auto dir = directory(memory, image, IMAGE_DIRECTORY_ENTRY_IMPORT, sizeof(IMAGE_IMPORT_DESCRIPTOR));
    if (slot_rva % sizeof(std::uint64_t)) invalid("unaligned native import slot");
    (void)address(memory, image, slot_rva, sizeof(std::uint64_t));
    const auto count = (std::min)(static_cast<std::size_t>(dir.size) / sizeof(IMAGE_IMPORT_DESCRIPTOR), std::size_t{128});
    std::size_t walked{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto desc = at<IMAGE_IMPORT_DESCRIPTOR>(memory, image, dir.rva + i * sizeof(IMAGE_IMPORT_DESCRIPTOR));
        if (!desc.Name && !desc.FirstThunk && !desc.OriginalFirstThunk) break;
        if (!desc.Name || !desc.FirstThunk || desc.FirstThunk > slot_rva ||
            (slot_rva - desc.FirstThunk) % sizeof(std::uint64_t)) continue;
        if (!desc.OriginalFirstThunk) invalid("native import has no original lookup table");
        const auto wanted = static_cast<std::size_t>(slot_rva - desc.FirstThunk) / sizeof(std::uint64_t);
        for (std::size_t index = 0; index <= wanted; ++index) {
            if (++walked > 16384) invalid("native import lookup exceeds supported bound");
            const auto lookup = at<std::uint64_t>(memory, image,
                static_cast<std::size_t>(desc.OriginalFirstThunk) + index * sizeof(std::uint64_t));
            if (!lookup) break;
            if (index != wanted) continue;
            ImportReference result{text(memory, image, desc.Name), {}, {}, image.range.base + slot_rva,
                                   at<std::uintptr_t>(memory, image, slot_rva)};
            if (result.provider.empty() || !result.target) invalid("native import has no provider or target");
            if (lookup & IMAGE_ORDINAL_FLAG64) {
                if (lookup & ~(IMAGE_ORDINAL_FLAG64 | 0xFFFFULL)) invalid("invalid native import ordinal");
                result.ordinal = static_cast<std::uint16_t>(lookup);
            } else {
                if (lookup > UINT32_MAX - sizeof(WORD)) invalid("invalid native import name RVA");
                (void)at<WORD>(memory, image, static_cast<std::size_t>(lookup));
                result.name = text(memory, image, static_cast<std::size_t>(lookup) + sizeof(WORD));
                if (result.name.empty()) invalid("native import name is empty");
            }
            return result;
        }
    }
    invalid("selected slot is not a declared native import");
}

ExportReference inspect_export(MemoryView memory, const ImageInfo& image, std::uint16_t ordinal) {
    const auto dir = directory(memory, image, IMAGE_DIRECTORY_ENTRY_EXPORT, sizeof(IMAGE_EXPORT_DIRECTORY));
    const auto table = at<IMAGE_EXPORT_DIRECTORY>(memory, image, dir.rva);
    if (ordinal < table.Base || ordinal - table.Base >= table.NumberOfFunctions || !table.AddressOfFunctions)
        invalid("native export ordinal is absent");
    (void)address(memory, image, table.AddressOfFunctions, static_cast<std::size_t>(table.NumberOfFunctions) * sizeof(DWORD));
    const auto entry_rva = static_cast<std::size_t>(table.AddressOfFunctions) + (ordinal - table.Base) * sizeof(DWORD);
    const auto rva = at<DWORD>(memory, image, entry_rva);
    if (!rva) invalid("native export ordinal is empty");
    ExportReference result{address(memory, image, rva, 1), {}};
    if (rva >= dir.rva && rva - dir.rva < dir.size)
        result.forwarder = text(memory, image, rva, (std::min)(std::size_t{512}, static_cast<std::size_t>(dir.size - (rva - dir.rva))));
    if (rva >= dir.rva && rva - dir.rva < dir.size && result.forwarder.empty()) invalid("empty native export forwarder");
    return result;
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
