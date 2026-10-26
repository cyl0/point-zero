#include "game_build.hpp"
#include "build_bindings.hpp"

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
// Raw GUID bytes in each retained bm2dx CodeView RSDS record.
constexpr GameBuildIdentity tested_2026091600{
    0x6AA23D57, 0x0C5FF000, 0x00B02B3C,
    {0xA3, 0x59, 0x6D, 0x3E, 0xE7, 0xBC, 0xBB, 0x47,
     0x87, 0x9E, 0x73, 0x54, 0x59, 0x7E, 0xD5, 0xCC}, 1};
constexpr GameBuildIdentity tested_2026091600_012{
    0x6AA240FD, 0x0C4EC000, 0x00A35BDC,
    {0xB5, 0x98, 0x9A, 0x9D, 0x7B, 0x08, 0x1D, 0x47,
     0xA9, 0x86, 0xA0, 0xBC, 0x53, 0x4F, 0x8B, 0x21}, 1};
constexpr GameBuildIdentity tested_2026093000{
    0x6AB4E1DA, 0x0C5FE000, 0x00B0211C,
    {0x82, 0x5B, 0x37, 0xE7, 0x40, 0x0B, 0x7A, 0x4E,
     0xAB, 0xAC, 0xCB, 0x20, 0xBD, 0xCA, 0x27, 0x5F}, 1};
constexpr GameBuildIdentity tested_2026101300{
    0x6AC5F8CE, 0x0C5FE000, 0x00B0294C,
    {0x27, 0x07, 0x78, 0x2B, 0x5F, 0x45, 0x9B, 0x4E,
     0xBA, 0xD3, 0x75, 0xE7, 0x65, 0x5F, 0x82, 0xCC}, 1};
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

namespace {
constexpr GameBuild builds[]{
    {tested_2026091600, "2026091600", &september16_bindings},
    {tested_2026091600_012, "2026091600-012", &september16_012_bindings},
    {tested_2026093000, "2026093000", &september30_bindings},
    {tested_2026101300, "2026101300", &october_bindings},
};
}
const GameBuild& select_game_build(MemoryView memory, const ImageInfo& image) {
    const auto identity = inspect_game_build(memory, image);
    for (const auto& build : builds) if (build.identity == identity) return build;
    throw std::runtime_error("unsupported bm2dx build; no qualified binding profile");
}
} // namespace point_zero::native
