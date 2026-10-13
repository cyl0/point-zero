#include "build_bindings.hpp"
#include "category_request_contract.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace point_zero::native {
namespace {
using Pin = build_bindings::CategoryRequestContract::Pin;

// Retained PE, inspected as data: ../03_bm2dx/binaries/LDJ-010-2026081900/bm2dx.dll
// SHA256 c61b6dcb8894062e56d60da8ca90053b27f129e1a8e8da5e54457aa42602397d.
// Independent full exception/CHAININFO and relocation scans establish these
// bounds. Three leaves have no runtime record. Four roots have one record each:
// save F5EDE0 flags2, clear F5FC40 flags2, group F2E1A8 flags2, sound F0EC40 flags0.
// No descendants, DIR64 overlaps, inline data or external jump tables. Group's
// error tails AFTER RET63ABE6 are included through 63ABF9. Leaf padding is not.

static_assert([] { std::size_t n{}; for (const auto p : build_bindings::CategoryRequestContract{}.pins) n += p.end - p.begin; return n; }() == 647);




void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
std::uintptr_t address(ImageRange image, std::uint32_t rva, std::size_t size) {
    require(image.base >= 0x10000 && contains(image.base, image.size, image.base, image.size) &&
            rva <= image.size && size <= image.size - rva,
            "category request contract extent outside selected image");
    return image.base + rva;
}
void check_body(MemoryView memory, ImageRange image, Pin pin) {
    const auto size = static_cast<std::size_t>(pin.end - pin.begin);
    const auto at = address(image, pin.begin, size);
    require(readable_range(memory, at, size, RangeUse::image_code, image),
            "category request contract ownership/protection changed");
    std::vector<std::byte> bytes(size);
    require(read_bytes(memory, at, bytes), "category request contract read failed");
    require(readable_range(memory, at, size, RangeUse::image_code, image),
            "category request contract ownership/protection changed during copy");
}
} // namespace

CategoryRequestCalls admit_category_request_calls(MemoryView memory, const ImageInfo& game) {
    static_assert(sizeof(std::uintptr_t) == 8);
    for (const auto target : game_bindings(game.range).category_request_contract.direct_targets) (void)address(game.range, target, 1);
    for (const auto slot : game_bindings(game.range).category_request_contract.singleton_slots) (void)address(game.range, slot, 8);
    for (const auto pin : game_bindings(game.range).category_request_contract.pins) check_body(memory, game.range, pin);
    const auto base = game.range.base;
    return {
        reinterpret_cast<decltype(CategoryRequestCalls::settings)>(base + game_bindings(game.range).category_request_contract.settings_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::persistent)>(base + game_bindings(game.range).category_request_contract.persistent_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::store)>(base + game_bindings(game.range).category_request_contract.store_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::save)>(base + game_bindings(game.range).category_request_contract.save_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::clear)>(base + game_bindings(game.range).category_request_contract.clear_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::group)>(base + game_bindings(game.range).category_request_contract.group_rva),
        reinterpret_cast<decltype(CategoryRequestCalls::sound)>(base + game_bindings(game.range).category_request_contract.sound_rva),
    };
}
} // namespace point_zero::native
