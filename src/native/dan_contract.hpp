#pragma once

#include "played_contract.hpp"

namespace point_zero::native {
// Each course row has its own chart; it is unrelated to the published played song.
struct DanCapture {
    std::uintptr_t scene{}, wrapper{}, music{};
    int row{}, style{}, difficulty{}, native_level{};
};
struct DanSites {
    PlayedFunction builder;
    std::uintptr_t capture{}, named_return{};
    std::uint32_t scene_stack_offset{};
};
DanSites admit_dan_sites(MemoryView, const ImageInfo& game);
}
