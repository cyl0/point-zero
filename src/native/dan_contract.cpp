#include "dan_contract.hpp"
#include "build_bindings.hpp"

namespace point_zero::native {
DanSites admit_dan_sites(MemoryView memory, const ImageInfo& game) {
    const auto& b = game_bindings(game.range).dan_contract;
    return {{game.range.base + b.builder.rva,
             read_code(memory, game.range, b.builder.rva, b.builder.size)},
            game.range.base + b.capture_rva, game.range.base + b.named_return_rva,
            b.scene_stack_offset};
}
}
