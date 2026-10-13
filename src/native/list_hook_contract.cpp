#include "build_bindings.hpp"
#include "list_hook_contract.hpp"

#include <cstring>
#include <array>
#include <stdexcept>

namespace point_zero::native {
namespace {
using Pin = build_bindings::ListHookContract::Pin;

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

std::uintptr_t address(MemoryView memory, ImageRange image, Pin pin) {
    require(contains(image.base, image.size, image.base, image.size) &&
            pin.rva <= image.size && pin.size <= image.size - pin.rva,
            "list hook contract range outside selected image");
    const auto at = image.base + pin.rva;
    require(readable_range(memory, at, pin.size, RangeUse::image_code, image),
            "list hook contract allocation or protection changed");
    return at;
}

} // namespace

ListHookSite admit_list_hook_site(MemoryView memory, const ImageInfo& game) {
    ListHookSite result{address(memory, game.range, game_bindings(game.range).list_hook_contract.pins[0])};
    (void)address(memory, game.range, game_bindings(game.range).list_hook_contract.pins[1]);
    (void)address(memory, game.range, game_bindings(game.range).list_hook_contract.pins[2]);
    const auto base = game.range.base;
    result.callers = {base + game_bindings(game.range).list_hook_contract.background_return_rva, base + game_bindings(game.range).list_hook_contract.p1_return_rva, base + game_bindings(game.range).list_hook_contract.p2_return_rva, base + game_bindings(game.range).list_hook_contract.solo_return_rva};
    for (const auto caller : result.callers) {
        const auto at = address(memory, game.range, {static_cast<std::uint32_t>(caller - base - 5), 5});
        std::array<std::byte, 5> bytes{};
        std::int32_t displacement{};
        require(read_bytes(memory, at, bytes), "list draw CALL unavailable");
        std::memcpy(&displacement, bytes.data() + 1, sizeof(displacement));
        require(bytes[0] == std::byte{0xe8} && static_cast<std::int64_t>(caller) + displacement == static_cast<std::int64_t>(result.address),
                "list draw CALL target changed");
    }
    return result;
}

} // namespace point_zero::native
