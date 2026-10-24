#include "adapters.hpp"
#include "browsing.hpp"
#include "folder_art_contract.hpp"
#include "folder_definitions.hpp"
#include "guide.hpp"
#include "surfaces.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace point_zero::native {
namespace {
void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
constexpr auto families = [] {
    std::array<std::string_view, NativeSurfaces::families.size() + 1> result{};
    std::copy(NativeSurfaces::families.begin(), NativeSurfaces::families.end(), result.begin());
    result.back() = FolderArtwork::family;
    return result;
}();
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
        surfaces_.prepare();
        try {
            folder_calls_ = admit_folder_art_calls(context_.memory, context_.modules.utils);
            folder_ = std::make_unique<FolderArtwork>(context_, folder_calls_);
            require(folder_->before_load(), "private folder image names already present or unavailable");
            browsing_.prepare(folder_definitions());
            browsing_prepared_ = true;
        } catch (const std::exception& error) {
            browsing_.retire_callbacks();
            warning("folder browsing", error.what());
        }
        preparing();
        if (browsing_prepared_) {
            try { guide_ = std::make_unique<NativeGuide>(context_, browsing_, folder_calls_); }
            catch (const std::exception& error) { warning("keypad8 hint", error.what()); }
        }
        preparing();
        prepared_ = true;
    }

    std::span<const std::string_view> asset_families() const noexcept override { return families; }

    void activate(std::span<const LoadedAsset> assets) override {
        require(!activation_attempted_, "native adapter activation already attempted");
        activation_attempted_ = true;
        preparing();
        require(prepared_, "native adapter preparation incomplete");
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
        NativeSurfaces::AssetSlots surface_slots{};
        std::copy_n(slots.begin(), surface_slots.size(), surface_slots.begin());
        surfaces_.activate(surface_slots);
        preparing();
        if (browsing_prepared_) {
            try {
                require(slots.back() && folder_->preload(slots.back()), "folder artwork unavailable");
                browsing_.activate();
                browsing_active_ = true;
            } catch (const std::exception& error) {
                browsing_.retire_callbacks();
                warning("folder browsing", error.what());
            }
        }
        preparing();
        if (guide_ && browsing_active_) {
            try {
                require(guide_->preload(slots.back()), "hint artwork unavailable");
                guide_->arm();
            } catch (const std::exception& error) {
                guide_->retire_callbacks();
                warning("keypad8 hint", error.what());
            }
        }
        preparing();
        require(surfaces_.available() || browsing_active_, "no native features available");
    }

    bool rollback() noexcept override {
        retire_callbacks();
        const bool browsing = browsing_.rollback();
        const bool surfaces = surfaces_.rollback();
        const bool guide = !guide_ || guide_->rollback();
        return context_.state.view().phase == FeaturePhase::preparing && browsing && surfaces && guide;
    }

    void retire_callbacks() noexcept override {
        if (retired_) return;
        retired_ = true;
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
    bool browsing_prepared_{}, browsing_active_{};

    void warning(const char* feature, const char* reason) const {
        if (!context_.host.log) return;
        const auto message = std::string(feature) + " unavailable: " + reason;
        context_.host.log(context_.host.context, PZ_LOG_WARNING,
            {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
    }
    void preparing() const {
        require(!retired_ && thread_ == GetCurrentThreadId() &&
                context_.state.view().phase == FeaturePhase::preparing,
                "native adapter preparation stopped or left its native thread");
    }
};
} // namespace

std::unique_ptr<NativeAdapters> make_native_adapters(const AdapterContext& context) {
    return std::make_unique<FinalAdapters>(context);
}
} // namespace point_zero::native
