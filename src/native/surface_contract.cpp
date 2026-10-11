#include "surface_contract.hpp"
#include "image_metadata.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };
constexpr Pin cup{0x6001D0, 1467};
// Retained LDJ-010-2026081900 afp-core.dll, named export XCd229cc000087,
// ordinal 136. The complete body is [1E8B0,1EA68), not the first unwind
// fragment [1E8B0,1E8F2) or the first ret at 1E95D. RUNTIME_FUNCTIONs at
// E5940..E597C describe six adjacent ranges with these exclusive ends:
// 1E8F2,1E946,1E95E,1E9CF,1EA32,1EA68. The last five chain (possibly via
// 1E95E) to the root at 1E8B0. The final jump at 1EA63 returns to 1E93E;
// INT3 padding starts at 1EA68, the next independent function at 1EA70.
// Disk offset 1DCB0, length 440; neither retained body has base relocations.
constexpr Pin bitmap{0x1E8B0, 440};
constexpr std::uint16_t bitmap_ordinal = 136;
constexpr std::size_t bitmap_import = 0xC91BD8;

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, std::size_t rva,
                       std::size_t size, RangeUse use) {
    require(contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "surface contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "surface contract allocation or protection changed");
    return at;
}

void exported_bitmap(MemoryView memory, const ImageInfo& image) {
    const auto at = address(memory, image.range, bitmap.rva, 1, RangeUse::image_code);
    const auto target = inspect_export(memory, image, bitmap_ordinal);
    require(target.forwarder.empty() && target.address == at, "surface bitmap export target changed");
    (void)address(memory, image.range, bitmap.rva, 1, RangeUse::image_code);
}

template<std::size_t N>
SurfaceHookSite code(MemoryView memory, ImageRange image, Pin pin, const std::array<std::byte, N>& expected) {
    SurfaceHookSite result{address(memory, image, pin.rva, pin.size, RangeUse::image_code),
                           std::vector<std::byte>(N)};
    require(read_bytes(memory, result.address, result.body) &&
            std::equal(result.body.begin(), result.body.end(), expected.begin()),
            "surface displaced instructions changed");
    (void)address(memory, image, pin.rva, pin.size, RangeUse::image_code);
    return result;
}
void bindings(MemoryView memory, const ImageInfo& game, const ImageInfo& afp) {
    detail::MetadataView game_metadata{memory, game.range}, afp_metadata{memory, afp.range};
    exported_bitmap(afp_metadata.view(), afp);
    const auto imported = inspect_import(game_metadata.view(), game, bitmap_import);
    require(imported.provider == "afp-core.dll" && imported.name.empty() &&
            imported.ordinal == bitmap_ordinal && imported.target ==
                address(memory, afp.range, bitmap.rva, 1, RangeUse::image_code),
            "surface bitmap import bypasses its admitted AFP entry");
}
} // namespace

SurfaceHookSites admit_surface_sites(MemoryView memory, const ImageInfo& game, const ImageInfo& afp) {
    bindings(memory, game, afp);
    // Independent prefixes from the retained game/AFP PE data. The bitmap
    // prefix includes its complete SUB RSP,90h; no partial instruction snapshot.
    constexpr std::array cup_entry{std::byte{0x48}, std::byte{0x89}, std::byte{0x5c}, std::byte{0x24}, std::byte{0x10}};
    constexpr std::array bitmap_entry{std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
        std::byte{0x48}, std::byte{0x81}, std::byte{0xec}, std::byte{0x90},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    SurfaceHookSites result{code(memory, game.range, cup, cup_entry), code(memory, afp.range, bitmap, bitmap_entry)};
    return result;
}
} // namespace point_zero::native
