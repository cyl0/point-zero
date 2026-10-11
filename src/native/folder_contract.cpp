#include "folder_contract.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <stdexcept>

namespace point_zero::native {
namespace {
struct Pin { std::uint32_t rva, size;  };
// Retained final bm2dx.dll, read as PE data. Complete extents recovered from
// all 57,745 .pdata entries and their unwind chains; no body relocations.
// Row: one record [5F4BC0,5F5A88). Metadata +20 selects its image and +28
// selects the plate. Native text fallback remains outside the private path.
// Hover: five chained records ending F0C8,F148,F361,F369,F383 (base5F0000).
// Metadata +10/+18 binds the existing category_change_text/category_change.
// Origin: five chained records ending0A58,0A5F,0B1D,0B60,0BB4 (base340000).
// Current-image dimensions: one record [34B940,34BB61). The origin wrapper
// consumes those dimensions when binding the differently sized whole labels.
// Shared bitmap/named emitter bodies are already owned/admitted by surfaces;
// including those patched bodies here would require unnecessary normalization.
constexpr Pin pins[]{
    {0x5F4BC0, 3784},
    {0x5FF070, 787},
    {0x340A50, 356},
    {0x34B940, 545},
};
constexpr Pin resolver_pins[]{
    {0x1E7E0, 110},
    {0x20E0, 229},
    {0x2A500, 164},
    {0x25C20, 31},
    {0x26200, 45},
};
// AFP Utils replaces core's default drain during native initialization. Admit
// that actual registered route, not the unused core266A0 default. Three unwind
// fragments cover the whole150-byte body; its37-entry table is separate data.
constexpr Pin command_drain{0x1DCF0, 150};
constexpr std::array<std::uint32_t, 37> command_targets{
    0x1CE50,0x1CE60,0x1CEC0,0x1E700,0x1D0C0,0x1D0F0,0x1D140,0x1E710,
    0x1E720,0x1D740,0x1D840,0x1D870,0x1CE50,0x1CE50,0x1D920,0x1CE50,
    0x1CE50,0x1CE50,0x1CE50,0x1DB90,0x1DBC0,0x192E0,0x1DBF0,0x1DC20,
    0x1DC50,0x1CE50,0x1DC90,0x1DCC0,0x1CE50,0x1CE50,0x1CE50,0x1CE50,
    0x1CE50,0x1CE50,0x1CE50,0x1D8E0,0x1D180,
};
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "folder consumer range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "folder consumer allocation or protection changed");
    return at;
}
void code(MemoryView memory, ImageRange image, Pin pin) {
    (void)address(memory, image, pin);
}
void consumers(MemoryView memory, ImageRange game) {
    // These consumers receive private image names through native metadata.
    // POINT ZERO neither patches nor decodes their instructions. Foreign row
    // hooks stay owned by their providers; no Playlister-specific veto applies.
    for (const auto pin : pins) code(memory, game, pin);
}
std::uintptr_t cell(MemoryView memory, ImageRange image, std::size_t rva) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && sizeof(std::uintptr_t) <= image.size - rva,
            "folder resolver cell outside selected image");
    const auto at = image.base + rva;
    std::uintptr_t result{};
    require(readable_range(memory, at, sizeof(result), RangeUse::image_read, image) &&
            read_value(memory, at, result) &&
            readable_range(memory, at, sizeof(result), RangeUse::image_read, image),
            "folder resolver cell ownership/read changed");
    return result;
}
bool owner_region(const MemoryRegion& region, std::uintptr_t owner) noexcept {
    return contains(region.begin, region.size, owner, 16) && region.allocation &&
        region.allocation <= region.begin && region.kind == MemoryKind::private_memory &&
        region.committed && region.readable && region.writable && !region.executable && !region.guarded;
}
void bindings(MemoryView memory, ImageRange core, ImageRange utils) {
    require(cell(memory, core, 0xE0E78) == core.base + 0x26200 &&
            cell(memory, core, 0xE1210) == utils.base + command_drain.rva,
            "folder bitmap resolver or command-drain callback changed");
    const auto owner = cell(memory, core, 0xE1218);
    require(owner >= 0x10000 && !(owner & 7) && 16 <= UINTPTR_MAX - owner,
            "folder bitmap resolver owner invalid");
    MemoryRegion before{}, after{};
    std::uintptr_t table{};
    require(memory.query && memory.query(memory.context, owner, before) &&
            owner_region(before, owner) &&
            readable_range(memory, owner, 16, RangeUse::private_writable) &&
            read_value(memory, owner + 8, table) &&
            readable_range(memory, owner, 16, RangeUse::private_writable) &&
            memory.query(memory.context, owner, after) && owner_region(after, owner) &&
            before.allocation == after.allocation,
            "folder bitmap resolver owner allocation/read changed");
    require(table == core.base + 0xE1220 &&
            cell(memory, core, 0xE1290) == utils.base + 0x185A0 &&
            readable_range(memory, utils.base + 0x185A0, 118, RangeUse::image_code, utils),
            "folder native image resolver function-table route changed");
}
void drain(MemoryView memory, ImageRange utils) {
    code(memory, utils, command_drain);
    std::array<std::uintptr_t, command_targets.size()> targets{};
    const auto at = utils.base + 0x28480;
    require(readable_range(memory, at, sizeof(targets), RangeUse::image_read, utils) &&
            read_bytes(memory, at, std::as_writable_bytes(std::span{targets})) &&
            readable_range(memory, at, sizeof(targets), RangeUse::image_read, utils),
            "folder native command table ownership/read changed");
    for (std::size_t i = 0; i < targets.size(); ++i)
        require(targets[i] == utils.base + command_targets[i] &&
                readable_range(memory, targets[i], 1, RangeUse::image_code, utils),
                "folder native command table target changed");
    // Command implementations remain native. This finite resolver contract
    // authenticates dispatch bindings, not every renderer operation they call.
}
} // namespace

void admit_folder_consumers(MemoryView memory, const ImageInfo& game, const std::optional<ImageInfo>& playlister) {
    (void)playlister; // Kept for source compatibility with the fixed adapter.
    consumers(memory, game.range);
}
void admit_folder_resolver(MemoryView memory, const ImageInfo& core, const ImageInfo& utils) {
    require(utils.range.base >= 0x10000 &&
            contains(utils.range.base, utils.range.size, utils.range.base, utils.range.size) &&
            utils.range.size >= 0x28480 + sizeof(std::uintptr_t) * command_targets.size(),
            "folder resolver target outside selected utils image");
    bindings(memory, core.range, utils.range);
    for (const auto pin : resolver_pins) code(memory, core.range, pin);
    drain(memory, utils.range);
}
} // namespace point_zero::native
