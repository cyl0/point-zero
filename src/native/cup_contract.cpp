#include "cup_contract.hpp"

#include <stdexcept>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };
struct Entry { std::uint16_t ordinal; Pin code; };
// Export bodies from retained final afp-core/afp-utils. Unwind extents include
// control071's in-function jump table. Path-relative includes four chained
// unwind fragments through 1D490; attach includes five through 1E17F.
// Leaf extents include BOTH return paths;
// the early null-argument return is not the end of the function.
constexpr Entry core_entries[]{
    {22, {0x37B0,116}},
    {103,{0x1D260,235}},
    {104,{0x1D350,320}},
    {106,{0x1DA50,416}},
    {111,{0x1DF60,543}},
    {114,{0x1EEA0,652}},
    {116,{0x22D60,238}}
};
constexpr Entry utils_entries[]{
    {56,{0xB980,5}},
    {99,{0x10930,60}}
};
struct Property { std::uint32_t table, index; Pin code; };
constexpr Property properties[]{
    {0xDD390,0, {0x22860,32}},
    {0xDD390,3, {0x228C0,106}},
    {0xDD390,30,{0x22BA0,25}}
};
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void code(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "cup AFP code outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image), "cup AFP code region changed");
}
void exported(MemoryView memory, const ImageInfo& image, Entry entry) {
    const auto value = inspect_export(memory, image, entry.ordinal);
    require(value.forwarder.empty() && value.address == image.range.base + entry.code.rva,
            "cup AFP export target changed");
    code(memory, image.range, entry.code);
}
void table(MemoryView memory, ImageRange image, Property property) {
    const auto at = image.base + property.table + 8ULL * property.index;
    std::uintptr_t target{};
    require(readable_range(memory, at, sizeof(target), RangeUse::image_read, image) &&
            read_value(memory, at, target) && target == image.base + property.code.rva,
            "cup AFP property dispatch changed");
}
} // namespace

CupAfp admit_cup_calls(MemoryView memory, const ImageInfo& core, const ImageInfo& utils) {
    for (const auto entry : core_entries) exported(memory, core, entry);
    for (const auto entry : utils_entries) exported(memory, utils, entry);
    for (const auto property : properties) { table(memory, core.range, property); code(memory, core.range, property.code); }
    // Used position/scale helper bodies from the retained layout contract, plus
    // the package export's tail target. No whole-library hash or recursive
    // admission of unrelated graphics/filesystem operations.
    code(memory, core.range, {0x21800,87});
    code(memory, core.range, {0x36E20,87});
    code(memory, utils.range, {0xB950,41});
    const auto c = core.range.base, u = utils.range.base;
    CupAfp result;
    result.valid = reinterpret_cast<decltype(result.valid)>(c + 0x37B0);
    result.path = reinterpret_cast<decltype(result.path)>(c + 0x1D260);
    result.path_relative = reinterpret_cast<decltype(result.path_relative)>(c + 0x1D350);
    result.relative = reinterpret_cast<decltype(result.relative)>(c + 0x1DA50);
    result.attach = reinterpret_cast<decltype(result.attach)>(c + 0x1DF60);
    result.control = reinterpret_cast<decltype(result.control)>(c + 0x1EEA0);
    result.property = reinterpret_cast<decltype(result.property)>(c + 0x22D60);
    result.package = reinterpret_cast<decltype(result.package)>(u + 0xB980);
    result.animation_info = reinterpret_cast<decltype(result.animation_info)>(u + 0x10930);
    return result;
}
} // namespace point_zero::native
