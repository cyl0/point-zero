#pragma once

#include "played_chart.hpp"

namespace point_zero::native {
using PlayedStockBitmap = int (__fastcall*)(int, const char*, int);
// Exact, complete, already-admitted native functions. Resource loading/export lookup and
// code admission belong to the selected adapter; none occurs in rendering.
struct PlayedNativeCalls {
    PlayedChartCalls chart;
    std::uint32_t (__fastcall* package)(const char*){};
    int (__fastcall* image_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* arguments)(void*, const void*){};
    int (__fastcall* animation_info)(void*, std::uint32_t, const char*){};
    int (__fastcall* bind)(int, const void*, int){};
    int (__fastcall* attach)(std::uint32_t, std::uint32_t){};
    int (__fastcall* control)(int, std::uint32_t, ...){};
    int (__fastcall* property)(int, std::uint32_t, std::intptr_t){};
    int (__fastcall* path_relative)(std::uint32_t, const char*){};
    std::uintptr_t (__fastcall* resolve)(std::uint32_t){};
    std::uintptr_t (__fastcall* definition)(std::uintptr_t, std::uint32_t){};
    void (__fastcall* select_stream)(std::uint32_t){};
    void (__fastcall* restore_stream)(const void*){};
    void (__fastcall* set_name)(std::uintptr_t, const char*){};
    void (__fastcall* clear_name)(std::uintptr_t){};
    void (__fastcall* position)(std::uintptr_t, const float*){};
    int (__fastcall* timeline_position)(std::uintptr_t, const int*){};
};

// Restore the prior AFP stream context even if a selected native action faults.
// false means the action/context cannot be certified; caller stops custom work.
// Naming uses nullptr to clear an owned temporary name. Position returns native
// timeline-position status and preserves native animated translation behavior.
bool played_name(MemoryView, ImageRange afp, const PlayedNativeCalls&, std::uint32_t stream,
                 std::uintptr_t work, const char* name) noexcept;
bool played_position(MemoryView, ImageRange afp, const PlayedNativeCalls&, std::uint32_t stream,
                     std::uintptr_t work, const float (&xy)[2]) noexcept;
} // namespace point_zero::native
