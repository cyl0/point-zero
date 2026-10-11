#pragma once

#include "played_mid_patch.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace point_zero::native {
// Optional native boot row. The enclosing game driver and patch backing remain
// resident until process exit; stop only makes the draw callback inert.
class BootText final {
public:
    BootText(MemoryView, ImageRange, std::string version);
    void arm();
    void stop() noexcept { enabled_.store(false, std::memory_order_release); }

private:
    struct Color { float r, g, b, a; };
    using Render = void (*)(int, int, int, Color*, const char*, ...);
    Render render_{};
    Color color_{1.f, 1.f, 1.f, 1.f};
    const int row_y_;
    std::string version_;
    std::unique_ptr<detail::PlayedMidPatch> patch_;
    std::atomic_bool enabled_{false};
    static std::atomic<BootText*> owner_;
    static void draw(safetyhook::Context&) noexcept;
    void render() noexcept;
};
} // namespace point_zero::native
