#include "build_bindings.hpp"
#include "played_contract.hpp"

namespace point_zero::native {
namespace {
PlayedFunction code(MemoryView memory, ImageRange image, build_bindings::PlayedContract::Pin pin) {
    return {image.base + pin.rva, read_code(memory, image, pin.rva, pin.size)};
}
}

PlayedSites admit_played_sites(MemoryView memory, const ImageInfo& game) {
    PlayedSites result{code(memory, game.range, game_bindings(game.range).played_contract.pins[0]),
                       code(memory, game.range, game_bindings(game.range).played_contract.pins[1]), code(memory, game.range, game_bindings(game.range).played_contract.pins[2]),
                       code(memory, game.range, game_bindings(game.range).played_contract.pins[3])};
    result.bindings = game.range.bindings;
    result.notes_update = code(memory, game.range, game_bindings(game.range).played_contract.pins[4]);
    const auto base = game.range.base;
    result.captures = {base + game_bindings(game.range).played_contract.hud_capture_rva, base + game_bindings(game.range).played_contract.result_capture_rva};
    result.named_returns = {base + game_bindings(game.range).played_contract.hud_return_rva, base + game_bindings(game.range).played_contract.named_return_rva};
    result.notes_capture = base + game_bindings(game.range).played_contract.notes_capture_rva;
    result.result_returns = {base + game_bindings(game.range).played_contract.result_return_first_rva, base + game_bindings(game.range).played_contract.result_return_second_rva};
    return result;
}

} // namespace point_zero::native
