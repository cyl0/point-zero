#include "folder_art_contract.hpp"
#include "image_metadata.hpp"

#include <windows.h>
#include <cstdio>
#include <stdexcept>

namespace point_zero::native {
namespace {
struct Export { std::uint16_t ordinal; std::uint32_t rva; };

// Ordinals and output-info ABI from retained afp-utils.dll. Resolve through
// the shared inspected export table; internal helper bodies are not copied.
constexpr Export exports[]{
    {56, 0xB980},
    {68, 0xD9E0},
    {70, 0xDC00},
    {71, 0xDC10},
    {5, 0x1930},
};

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::size_t rva,
                       std::size_t size, RangeUse use = RangeUse::image_read) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "folder art call range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "folder art call allocation or protection changed");
    return at;
}
void exported(MemoryView memory, const ImageInfo& image, Export entry) {
    const auto at = address(memory, image.range, entry.rva, 1, RangeUse::image_code);
    const auto target = inspect_export(memory, image, entry.ordinal);
    require(target.forwarder.empty() && target.address == at, "folder art export target changed");
    (void)address(memory, image.range, entry.rva, 1, RangeUse::image_code);
}
void bindings(MemoryView memory, const ImageInfo& image) {
    detail::MetadataView metadata{memory, image.range};
    for (const auto entry : exports) exported(metadata.view(), image, entry);
}
} // namespace

FolderArtCalls admit_folder_art_calls(MemoryView memory, const ImageInfo& utils) {
    static_assert(sizeof(std::uintptr_t) == 8);
    bindings(memory, utils);
    const auto base = utils.range.base;
    FolderArtCalls result;
    result.package = reinterpret_cast<decltype(result.package)>(base + 0xB980);
    result.global_id = reinterpret_cast<decltype(result.global_id)>(base + 0xD9E0);
    result.global_info = reinterpret_cast<decltype(result.global_info)>(base + 0xDC00);
    result.package_info = reinterpret_cast<decltype(result.package_info)>(base + 0xDC10);
    result.test_flags = reinterpret_cast<decltype(result.test_flags)>(base + 0x1930);
    return result;
}
} // namespace point_zero::native
