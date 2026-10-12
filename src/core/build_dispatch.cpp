#include "point_zero/core_api.h"
#include "native/game_build.hpp"

#include <windows.h>

extern "C" const PZ_CoreApi* PZ_CALL point_zero_get_api_august(uint32_t) noexcept;
extern "C" const PZ_CoreApi* PZ_CALL point_zero_get_api_september(uint32_t) noexcept;

extern "C" __declspec(dllexport) const PZ_CoreApi* PZ_CALL point_zero_get_api(uint32_t abi) noexcept {
    if (abi != PZ_CORE_ABI) return nullptr;
    try {
        if (GetModuleHandleW(L"bm2dx.dll")) {
            point_zero::native::ModuleLease game(L"bm2dx.dll");
            if (point_zero::native::check_game_build(point_zero::native::current_process_memory(), game.image()) == "2026090900")
                return point_zero_get_api_september(abi);
        }
    } catch (...) {}
    return point_zero_get_api_august(abi);
}
