#pragma once

#include "adapters.hpp"

#include <string_view>

namespace point_zero::native {
// Finite read/query calls admitted from afp-utils. Output image info has an
// image ID at +0 and dimensions at +20/+24; it has no package identity field.
struct FolderArtCalls {
    std::uint32_t (__fastcall* package)(const char*){};
    std::int32_t (__fastcall* global_id)(const char*){};
    int (__fastcall* global_info)(void*, const char*){};
    int (__fastcall* package_info)(void*, std::uint32_t, const char*){};
    bool (__fastcall* test_flags)(std::uint32_t){};
};

// Startup-only verification. AssetOwner loads and retains the private package;
// native metadata consumers do all drawing. No renderer, resource replacement,
// registry walk, ongoing callback or per-frame work is introduced here.
class FolderArtwork final {
public:
    static constexpr std::string_view family = "/pz_folder.ifs";
    FolderArtwork(const AdapterContext&, FolderArtCalls);
    FolderArtwork(const FolderArtwork&) = delete;
    FolderArtwork& operator=(const FolderArtwork&) = delete;

    // One attempt each, on the same native thread while the shared state is
    // preparing. Require no existing private names, then require every name's
    // native global lookup to resolve to the explicitly loaded package's image.
    bool before_load() noexcept;
    bool preload(std::uintptr_t slot) noexcept;
private:
    MemoryView memory_;
    FeatureState& state_;
    FolderArtCalls calls_;
    std::uint32_t thread_{};
    bool before_attempted_{}, absent_{}, preload_attempted_{};

    bool preparing() const noexcept;
    bool before_impl();
    bool preload_impl(std::uintptr_t);
    bool live(std::uintptr_t slot, std::uint32_t package) const;
};
} // namespace point_zero::native
