#include "asset_contract.hpp"
#include "image_metadata.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size; };
// Complete extents from the retained final-build PE files, read as data.
// 3519A0 chains through 351A35,351A70,351A9B,351BE9 to 351C03.
// 11240 chains through 1127B,11324 to 11392. BA60 includes both native
// jump tables, ending BCD0. CF70 is a leaf ending CF9F before INT3 padding.
// The remaining roots each have one unwind extent and no descendants.
// Owned bytes contain no base relocations. Unwind metadata is provenance,
// not runtime admission of exception handlers or the entire native backend.
constexpr Pin game_code[]{
    {0x3519A0, 611},
    {0x350170, 661},
    {0x350410, 963},
    {0x3507E0, 193},
    {0x354900, 220},
};
constexpr Pin utils_code[]{
    {0xAF50, 45},
    {0x127F0, 735},
    {0x11240, 338},
    {0xBA60, 624},
    {0xCFD0, 67},
    {0xAC70, 731},
    {0xB820, 81},
    {0xCCE0, 105},
    {0xCF70, 47},
};
// Private utils bodies keep their reviewed RVAs. Public AVS ordinals resolve
// through the retained provider's export table instead of an internal offset.
struct Binding { std::uint32_t slot; std::uint16_t ordinal; std::uint32_t target{}; };
constexpr Binding utils_imports[]{
    {0xC91D18, 35, 0xAF50}, {0xC91CF8, 55, 0x127F0},
    {0xC91D00, 59, 0x11240}, {0xC91D20, 62, 0xBA60}, {0xC91D28, 66, 0xCFD0},
};
constexpr Binding avs_imports[]{
    {0xC91FE0, 16}, {0xC91FE8, 17},
    {0xC91F90, 381}, {0xC91D90, 258},
    {0xC91DB0, 76}, {0xC91F00, 77},
};
struct Literal { std::uint32_t rva; std::string_view text; };
constexpr Literal literals[]{
    {0xE6C33C, "%s"}, {0xE6C374, "%s%s"},
    {0xE6C380, "/afp%d%s"}, {0xE6C3C8, "imagefs"},
};
constexpr std::size_t manager_rva = 0xB8B7E90;
constexpr std::size_t order_offset = AssetOwner::slot_count * AssetOwner::slot_stride;
static_assert(order_offset == 0xA3000);
struct Tail {
    std::array<std::int32_t, AssetOwner::slot_count> order;
    std::uint32_t mutex;
};
static_assert(sizeof(Tail) == 0x804);

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, std::size_t rva,
                       std::size_t size, RangeUse use = RangeUse::image_read) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "asset contract range outside selected image");
    const auto at = image.base + rva;
    require(readable_range(memory, at, size, use, image),
            "asset contract allocation or protection changed");
    return at;
}
void code(MemoryView memory, ImageRange image, Pin pin) {
    (void)address(memory, image, pin.rva, pin.size, RangeUse::image_code);
}
template<std::size_t N>
void bindings(MemoryView memory, const ImageInfo& game,
                                     const ImageInfo& provider, std::string_view name,
                                     const Binding (&entries)[N]) {
    detail::MetadataView game_metadata{memory, game.range}, provider_metadata{memory, provider.range};
    for (const auto binding : entries) {
        const auto exported = inspect_export(provider_metadata.view(), provider, binding.ordinal);
        require(exported.forwarder.empty(), "asset operation export is forwarded");
        const auto rva = binding.target ? binding.target : exported.address - provider.range.base;
        const auto target = address(memory, provider.range, rva, 1, RangeUse::image_code);
        const auto imported = inspect_import(game_metadata.view(), game, binding.slot);
        require(imported.provider == name && imported.name.empty() &&
                imported.ordinal == binding.ordinal && imported.target == target &&
                exported.address == target,
                "asset operation import/export binding changed");
        // Image helpers copy and recheck the selected metadata. Recheck code
        // ownership after those reads too; an IAT value is not a code lease.
        (void)address(memory, provider.range, rva, 1, RangeUse::image_code);
    }
}
void strings(MemoryView memory, ImageRange game) {
    for (const auto literal : literals) {
        const auto size = literal.text.size() + 1;
        std::array<char, 16> data{};
        const auto at = address(memory, game, literal.rva, size);
        require(read_bytes(memory, at, std::as_writable_bytes(std::span(data)).first(size)),
                "asset path format unreadable");
        (void)address(memory, game, literal.rva, size);
        require(data[literal.text.size()] == '\0' &&
                std::string_view(data.data(), literal.text.size()) == literal.text,
                "asset path format changed");
    }
}
std::uintptr_t manager(MemoryView memory, ImageRange game) {
    constexpr auto size = order_offset + sizeof(Tail);
    const auto begin = address(memory, game, manager_rva, size);
    for (auto at = begin; at < begin + size;) {
        MemoryRegion region{};
        require(memory.query && memory.query(memory.context, at, region) &&
                contains(region.begin, region.size, at, 1) && region.committed &&
                region.readable && region.writable && !region.executable && !region.guarded &&
                region.kind == MemoryKind::image && region.allocation == game.base,
                "asset manager must be writable selected-image data");
        at = (std::min)(begin + size, region.begin + region.size);
    }
    return begin;
}
void tail(MemoryView memory, ImageRange game) {
    Tail result{};
    const auto at = manager(memory, game);
    require(read_value(memory, at + order_offset, result), "asset manager tail unreadable");
    (void)manager(memory, game);
    require(result.mutex != 0, "asset manager mutex is not initialized");
    // Native load treats every negative entry as absent, but directly indexes
    // its slot array for each nonnegative entry. Do not accept out-of-bounds IDs.
    require(std::all_of(result.order.begin(), result.order.end(), [](auto value) {
                return value < static_cast<std::int32_t>(AssetOwner::slot_count);
            }), "asset manager order index is outside the slot array");
}
} // namespace

AssetNative admit_asset_calls(MemoryView memory, const ImageInfo& game,
                              const ImageInfo& utils, const ImageInfo& avs) {
    for (const auto pin : game_code) code(memory, game.range, pin);
    for (const auto pin : utils_code) code(memory, utils.range, pin);
    bindings(memory, game, utils, "afp-utils.dll", utils_imports);
    bindings(memory, game, avs, "avs2-core.dll", avs_imports);
    strings(memory, game.range);
    tail(memory, game.range);
    // AVS bodies are deliberately not pristine-body gates here: Spice/IFS may
    // wrap filesystem entries. Each selected ordinal must bind the game's IAT
    // to a stable executable export in the retained AVS image. This does not
    // authenticate its implementation or hook chain.
    return {game.range.base + manager_rva, game.range.base + game_code[0].rva,
            game.range.base + game_code[3].rva};
}
} // namespace point_zero::native
