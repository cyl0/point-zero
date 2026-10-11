#pragma once

#include "adapters.hpp"
#include "folder_art.hpp"

namespace point_zero::native {
class NativeBrowsing;

// One extra bitmap in the ordinary native INFORMATION list. The game owns
// its vector, pooled sprites, clipping, width calculation and scroll timing.
class NativeGuide final {
public:
    NativeGuide(const AdapterContext&, const NativeBrowsing&, FolderArtCalls);
    ~NativeGuide() noexcept;
    NativeGuide(const NativeGuide&) = delete;
    NativeGuide& operator=(const NativeGuide&) = delete;
    bool preload(std::uintptr_t package_slot) noexcept;
    void arm();
    // Prepublication: drain and verify any exposed patch before asset release.
    bool rollback() noexcept;
    void retire_callbacks() noexcept;
private:
    struct State;
    State* state_{};
};
} // namespace point_zero::native
