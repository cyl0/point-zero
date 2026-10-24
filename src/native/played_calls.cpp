#include "played_calls.hpp"

#include <windows.h>
#include <array>

namespace point_zero::native {
namespace {
using StreamContext = std::array<std::uint64_t, 2>;
bool stream_context(MemoryView memory, ImageRange afp, StreamContext& out) noexcept {
    constexpr std::array<std::size_t, 2> fields{0xF23E4, 0xF23C8};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (!memory.read || fields[i] > UINTPTR_MAX - afp.base ||
            !contains(afp.base, afp.size, afp.base + fields[i], sizeof(out[i])) ||
            !memory.read(memory.context, afp.base + fields[i],
                         std::as_writable_bytes(std::span(&out[i], 1)))) return false;
    }
    return true;
}
} // namespace

bool played_name(MemoryView memory, ImageRange afp, const PlayedNativeCalls& calls,
                 std::uint32_t stream, std::uintptr_t work, const char* name) noexcept {
    __try {
        StreamContext saved{};
        if (work < 0x10000 || !stream_context(memory, afp, saved)) return false;
        bool done{};
        __try {
            calls.select_stream(stream);
            {
                if (name) done = calls.set_name(work, &name) == 0;
                else { calls.clear_name(work); done = true; }
            }
        } __finally { calls.restore_stream(saved.data()); }
        return done;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool played_position(MemoryView memory, ImageRange afp, const PlayedNativeCalls& calls,
                     std::uint32_t stream, std::uintptr_t work, const float (&xy)[2]) noexcept {
    __try {
        StreamContext saved{};
        if (work < 0x10000 || !stream_context(memory, afp, saved)) return false;
        bool done{};
        __try {
            calls.select_stream(stream);
            {
                calls.position(work, xy);
                const int enabled = 1;
                done = calls.timeline_position(work, &enabled) == 0;
            }
        } __finally { calls.restore_stream(saved.data()); }
        return done;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace point_zero::native
