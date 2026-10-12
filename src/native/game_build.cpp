#include "game_build.hpp"

#include <windows.h>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}
template<class T> T at(MemoryView memory, const ImageInfo& image, std::size_t rva) {
    require(rva <= image.range.size && sizeof(T) <= image.range.size - rva &&
            contains(image.range.base, image.range.size, image.range.base, image.range.size),
            "game build metadata is out of image bounds");
    const auto address = image.range.base + rva;
    T result{};
    require(readable_range(memory, address, sizeof(T), RangeUse::image_read, image.range) &&
            read_value(memory, address, result), "game build metadata is inaccessible");
    return result;
}
template<class T> void unchanged(MemoryView memory, const ImageInfo& image, std::size_t rva, const T& before) {
    const auto after = at<T>(memory, image, rva);
    require(std::memcmp(&before, &after, sizeof(T)) == 0, "game build metadata changed during admission");
}
struct CodeView {
    std::uint32_t signature{};
    std::array<std::uint8_t, 16> guid{};
    std::uint32_t age{};
};
static_assert(sizeof(CodeView) == 24);
// Raw GUID bytes in the retained 2026081900 bm2dx CodeView RSDS record.
constexpr GameBuildIdentity tested_2026081900{
    0x6a713632, 0x0bb95000, 0x00b1cccc,
    {0xe7, 0xd4, 0x62, 0x31, 0xe0, 0x2f, 0x64, 0x4c,
     0xb3, 0xdf, 0x93, 0x60, 0x4f, 0x09, 0x51, 0x54}, 1};
constexpr GameBuildIdentity tested_2026090900{
    0x6a8bafd1, 0x0bb93000, 0x00b1ba0c,
    {0x7e, 0xf4, 0xba, 0xff, 0x61, 0x97, 0x2e, 0x47,
     0x8e, 0x2d, 0x10, 0x63, 0xc7, 0x94, 0x4f, 0xf3}, 1};
} // namespace

GameBuildIdentity inspect_game_build(MemoryView memory, const ImageInfo& image) {
    const auto dos = at<IMAGE_DOS_HEADER>(memory, image, 0);
    require(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew >= static_cast<LONG>(sizeof(dos)) &&
            dos.e_lfanew <= 0x10000, "invalid game build DOS header");
    const auto nt_rva = static_cast<std::size_t>(dos.e_lfanew);
    const auto nt = at<IMAGE_NT_HEADERS64>(memory, image, nt_rva);
    require(nt.Signature == IMAGE_NT_SIGNATURE && nt.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
            nt.FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64) &&
            nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
            nt.OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_DEBUG &&
            nt.OptionalHeader.NumberOfRvaAndSizes <= 16 &&
            nt.OptionalHeader.SizeOfImage == image.range.size &&
            nt.FileHeader.TimeDateStamp == image.timestamp &&
            nt.OptionalHeader.AddressOfEntryPoint == image.entry_rva,
            "game build headers disagree with selected image");
    const auto dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    require(dir.VirtualAddress == image.directories[IMAGE_DIRECTORY_ENTRY_DEBUG].rva &&
            dir.Size == image.directories[IMAGE_DIRECTORY_ENTRY_DEBUG].size && dir.VirtualAddress &&
            dir.Size && dir.Size % sizeof(IMAGE_DEBUG_DIRECTORY) == 0 &&
            dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY) <= 32 &&
            dir.VirtualAddress <= image.range.size && dir.Size <= image.range.size - dir.VirtualAddress,
            "invalid or missing game build debug directory");
    std::optional<CodeView> selected;
    for (std::size_t i = 0; i < dir.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i) {
        const auto rva = dir.VirtualAddress + i * sizeof(IMAGE_DEBUG_DIRECTORY);
        const auto record = at<IMAGE_DEBUG_DIRECTORY>(memory, image, rva);
        if (record.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
            require(!selected && record.SizeOfData >= sizeof(CodeView) && record.AddressOfRawData &&
                    record.AddressOfRawData <= image.range.size &&
                    record.SizeOfData <= image.range.size - record.AddressOfRawData,
                    "invalid or ambiguous game build CodeView record");
            selected = at<CodeView>(memory, image, record.AddressOfRawData);
            require(selected->signature == 0x53445352, "unsupported game build CodeView format"); // RSDS
            unchanged(memory, image, record.AddressOfRawData, *selected);
        }
        unchanged(memory, image, rva, record);
    }
    unchanged(memory, image, 0, dos);
    unchanged(memory, image, nt_rva, nt);
    require(selected.has_value(), "game build CodeView identity is absent");
    return {nt.FileHeader.TimeDateStamp, nt.OptionalHeader.SizeOfImage,
            nt.OptionalHeader.AddressOfEntryPoint, selected->guid, selected->age};
}

std::string_view supported_game_datecode(const GameBuildIdentity& identity) noexcept {
    if (identity == tested_2026081900) return "2026081900";
    if (identity == tested_2026090900) return "2026090900";
    return {};
}
std::string_view check_game_build(MemoryView memory, const ImageInfo& image) {
    const auto datecode = supported_game_datecode(inspect_game_build(memory, image));
    require(!datecode.empty(), "unsupported bm2dx build; POINT ZERO supports tested LDJ2026081900 and LDJ2026090900 only");
    return datecode;
}
} // namespace point_zero::native
