#include "build_bindings.hpp"
#include "category_metadata_contract.hpp"

namespace point_zero::native {
CategoryMetadataSite admit_category_metadata_contract(MemoryView memory, const ImageInfo& game) {
    const auto rva = game_bindings(game.range).category_metadata_contract.game_rva;
    return {game.range.base + rva, read_code(memory, game.range, rva, 16)};
}
}
