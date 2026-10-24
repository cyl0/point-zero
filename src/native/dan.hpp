#pragma once

#include "adapters.hpp"
#include "dan_contract.hpp"

namespace point_zero::native {
// Owns presentation only. NativeSurfaces owns the shared named/bitmap hooks.
class DanRenderer final {
public:
    using StockBitmap = int (__fastcall*)(int, const char*, int);
    explicit DanRenderer(const AdapterContext&);
    ~DanRenderer() noexcept;
    DanRenderer(const DanRenderer&) = delete;
    DanRenderer& operator=(const DanRenderer&) = delete;
    bool preload(std::uintptr_t primary, std::uintptr_t list) noexcept;
    void begin(const DanCapture&, std::uintptr_t wrapper, const char* name) noexcept;
    void end() noexcept;
    bool bitmap(int mc, const char* name, int mode, int& result, StockBitmap) noexcept;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
