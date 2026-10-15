#pragma once
#include "played_contract.hpp"

namespace point_zero::native {
struct StepUpSites {
    PlayedFunction builder;
    std::uintptr_t capture{}, named_return{};
};
StepUpSites admit_stepup_sites(MemoryView, const ImageInfo& game);
}
