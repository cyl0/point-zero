#include "adapters.hpp"
#include "browsing.hpp"
#include "folder_art_contract.hpp"
#include "folder_contract.hpp"
#include "folder_definitions.hpp"
#include "guide.hpp"
#include "surfaces.hpp"

#include <windows.h>
#include <array>
#include <stdexcept>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }

// The selected TDJ and classic capability set is the same main-screen set.
// No Assistant/subscreen owner, cabinet-specific bank, or input provider is a
// prerequisite here. Mode-specific native owners/tuples are admitted by the
// surface and browsing components; ordinary LEVEL behavior stays scoped to its
// native modes. This is a fixed composition, not a capability/plugin registry.
constexpr std::array families{NativeSurfaces::families[0], NativeSurfaces::families[1],
                              NativeSurfaces::families[2], FolderArtwork::family};
class FinalAdapters final : public NativeAdapters {
public:
    explicit FinalAdapters(const AdapterContext& context) noexcept
        : context_(context), surfaces_(context), browsing_(context) {}
    ~FinalAdapters() override { retire_callbacks(); }

    void prepare() override {
        require(!prepare_attempted_, "native adapter preparation already attempted");
        prepare_attempted_ = true;
        thread_ = GetCurrentThreadId();
        preparing();
        folder_calls_ = admit_folder();
        preparing();
        surfaces_.prepare();
        preparing();
        const auto definitions = folder_definitions();
        browsing_.prepare(definitions);
        preparing();
        folder_ = std::make_unique<FolderArtwork>(context_, folder_calls_);
        require(folder_->before_load(), "private folder image names already present or unavailable");
        preparing();
        // Optional native hint: an unknown guide contract must not disable the
        // difficulty feature or replace the game's original INFORMATION strip.
        try { guide_ = std::make_unique<NativeGuide>(context_, browsing_, folder_calls_); }
        catch (const std::exception&) { guide_warning(); }
        preparing();
        prepared_ = true;
    }

    std::span<const std::string_view> asset_families() const noexcept override { return families; }

    void activate(std::span<const LoadedAsset> assets) override {
        require(!activation_attempted_, "native adapter activation already attempted");
        activation_attempted_ = true;
        preparing();
        require(prepared_, "native adapter preparation incomplete");
        require(assets.size() == families.size(), "incomplete native capability asset set");
        std::array<std::uintptr_t, families.size()> slots{};
        for (const auto& asset : assets) {
            std::size_t index{};
            while (index < families.size() && families[index] != asset.family) ++index;
            require(index < families.size() && !slots[index] && asset.slot,
                    "unknown, duplicate or empty native asset");
            for (const auto slot : slots)
                require(slot != asset.slot, "native asset families share a resource slot");
            slots[index] = asset.slot;
        }
        require(folder_->preload(slots.back()), "private folder images do not match native lookups");
        preparing();
        surfaces_.activate(NativeSurfaces::AssetSlots{slots[0], slots[1], slots[2]});
        preparing();
        browsing_.activate();
        preparing();
        if (guide_) {
            if (guide_->preload(slots.back())) {
                preparing();
                // Once native patching starts, an ambiguous enable belongs to
                // the existing startup failure/retention path, not omission.
                guide_->arm();
            } else { guide_->retire_callbacks(); guide_warning(); }
        }
        preparing();
        // Runtime publishes the shared FeatureState only after GameDriver has
        // also committed package retention. No partial capability publishes.
    }

    bool rollback() noexcept override {
        retire_callbacks();
        // Do not short-circuit: both owners must finish their retirement/checks
        // even if one cannot prove safe prepublication asset release.
        const bool browsing = browsing_.rollback();
        const bool surfaces = surfaces_.rollback();
        const bool guide = !guide_ || guide_->rollback();
        return context_.state.view().phase == FeaturePhase::preparing && browsing && surfaces && guide;
    }

    void retire_callbacks() noexcept override {
        if (retired_) return;
        retired_ = true;
        // A browsing transaction can call native presentation. Drain its outer
        // work before retiring render callbacks; keep all backing owned here.
        if (guide_) guide_->retire_callbacks();
        browsing_.retire_callbacks();
        surfaces_.retire_callbacks();
    }

private:
    AdapterContext context_;
    NativeSurfaces surfaces_;
    NativeBrowsing browsing_;
    std::unique_ptr<FolderArtwork> folder_;
    std::unique_ptr<NativeGuide> guide_;
    FolderArtCalls folder_calls_{};
    std::uint32_t thread_{};
    bool prepare_attempted_{}, prepared_{}, activation_attempted_{}, retired_{};

    void guide_warning() const {
        constexpr std::string_view message =
            "native keypad8 INFORMATION hint unavailable; stock guide retained";
        if (context_.host.log) context_.host.log(context_.host.context, PZ_LOG_WARNING,
            {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
    }

    void preparing() const {
        require(!retired_ && thread_ == GetCurrentThreadId() &&
                context_.state.view().phase == FeaturePhase::preparing,
                "native adapter preparation stopped or left its native thread");
    }
    FolderArtCalls admit_folder() const {
        const auto& modules = context_.modules;
        admit_folder_consumers(context_.memory, modules.game, modules.playlister);
        admit_folder_resolver(context_.memory, modules.afp, modules.utils);
        return admit_folder_art_calls(context_.memory, modules.utils);
    }
};
} // namespace

std::unique_ptr<NativeAdapters> make_native_adapters(const AdapterContext& context) {
    return std::make_unique<FinalAdapters>(context);
}
} // namespace point_zero::native
