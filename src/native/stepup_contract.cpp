#include "build_bindings.hpp"
#include "stepup_contract.hpp"

namespace point_zero::native {
StepUpSites admit_stepup_sites(MemoryView memory, const ImageInfo& game) {
    const auto& bindings = game_bindings(game.range).stepup_contract;
    return {{game.range.base + bindings.builder.rva,
             read_code(memory, game.range, bindings.builder.rva, bindings.builder.size)},
            game.range.base + bindings.capture_rva, game.range.base + bindings.named_return_rva};
}
}
