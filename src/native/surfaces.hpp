#pragma once

#include "cup.hpp"
#include "confirmation.hpp"
#include "assistant.hpp"
#include "dan.hpp"
#include "list.hpp"
#include "played.hpp"
#include "played_hooks.hpp"
#include "surface_hooks.hpp"

namespace point_zero::native {
class NativeSurfaces final {
public:
    static constexpr std::array families{CupRenderer::family, ListRenderer::family, PlayedRenderer::family,
        ConfirmationRenderer::family, PlayedRenderer::result_family, PlayedRenderer::stepup_family,
        AssistantRenderer::family};
    using AssetSlots = std::array<std::uintptr_t, families.size()>;
    explicit NativeSurfaces(const AdapterContext&) noexcept;
    ~NativeSurfaces() noexcept;
    NativeSurfaces(const NativeSurfaces&) = delete;
    NativeSurfaces& operator=(const NativeSurfaces&) = delete;
    NativeSurfaces(NativeSurfaces&&) = delete;
    NativeSurfaces& operator=(NativeSurfaces&&) = delete;

    void prepare();
    static std::span<const std::string_view> asset_families() noexcept;
    void activate(std::span<const LoadedAsset>);
    void activate(const AssetSlots&);
    bool available() const noexcept;
    // At the serialized prepublication boundary, including after another
    // component arms. This only checks owned hook integrity, not readiness.
    bool intact() const noexcept;
    // Prepublication only; retires callbacks first. False means native effects
    // cannot be certified for package release. Never unhooks or releases assets.
    bool rollback() noexcept;
    // Externally serialized after initialization has drained; may race native
    // calls. Never call from a callback/native scope. Idempotent, no native edits.
    void retire_callbacks() noexcept;

private:
    AdapterContext context_;
    std::unique_ptr<CupRenderer> cup_;
    std::unique_ptr<ListRenderer> list_;
    std::unique_ptr<PlayedRenderer> played_;
    std::unique_ptr<ConfirmationRenderer> confirmation_;
    std::unique_ptr<AssistantRenderer> assistant_;
    std::unique_ptr<DanRenderer> dan_;
    // Reverse destruction and explicit drain keep bitmap forwarding alive
    // throughout every entered played capture/named scope.
    std::unique_ptr<SurfaceHooks> surface_hooks_;
    std::unique_ptr<ListHook> list_hook_;
    std::unique_ptr<ListTitleHook> title_hook_;
    std::unique_ptr<PlayedHooks> played_hooks_;
    std::unique_ptr<AssistantHeaderHook> assistant_hook_;
    SurfaceHooks::Bitmap stock_bitmap_{};
    enum class ArmState { unattempted, attempted, armed };
    std::array<ArmState, 5> arms_{};
    std::uint32_t thread_{};
    bool prepare_attempted_{}, prepared_{}, activation_attempted_{}, retired_{};
    bool cup_ready_{}, played_ready_{}, confirmation_ready_{}, assistant_ready_{}, dan_ready_{};

    void preparing() const;
    void begin_activation();
    void activate_slots(const AssetSlots&);
    static SurfaceHooks::Callbacks bitmap_callbacks() noexcept;
    static PlayedHooks::Callbacks played_callbacks() noexcept;
};
} // namespace point_zero::native
