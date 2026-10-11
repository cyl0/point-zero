#include "core/runtime.hpp"
#include "adapters.hpp"
#include "assets.hpp"
#include "asset_contract.hpp"
#include "avs_read.hpp"
#include "catalog_contract.hpp"
#include "catalog_hook.hpp"
#include "boot_text.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace point_zero::core {
namespace {
using namespace native;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
ImageRange range(const std::optional<ImageInfo>& image) noexcept {
    return image ? image->range : ImageRange{};
}
class GameDriver final : public NativeDriver {
public:
    GameDriver(const NativeSetup& setup, FeatureState& state) : setup_(setup), state_(state) {
        require(std::strcmp(setup.host.avs.model, "LDJ") == 0 &&
                std::strcmp(setup.host.avs.datecode, "2026081900") == 0,
                "unsupported game/datecode; POINT ZERO requires LDJ2026081900");
        // The bootstrap supplies its own directory, independently of the game's
        // working directory. New installs live in modules; existing root installs
        // keep their paths when receiving an ordinary generation update.
        const auto working = std::filesystem::current_path();
        const bool at_root = std::filesystem::equivalent(working, setup.runtime);
        require(at_root || std::filesystem::equivalent(working / L"modules", setup.runtime),
                "POINT ZERO must be installed in the game root or modules directory");
        prefix_ = std::string(at_root ? "/point-zero/versions/" : "/modules/point-zero/versions/") +
                  setup.generation_id + "/assets";

        // Pin already-loaded modules once. No polling, rating source read, or DLL
        // load is introduced here. Module hashes are not blanket vetoes.
        modules_.game = hold(L"bm2dx.dll");
        modules_.avs = hold(L"avs2-core.dll");
        modules_.afp = hold(L"afp-core.dll");
        modules_.utils = hold(L"afp-utils.dll");
        modules_.omnifix = optional(L"omnifix.dll");
        modules_.playlister = optional(L"playlister.dll");
        modules_.fervidex = optional(L"fervidex.dll");
    }

    void install(Observer observer, void* context) override {
        require(!hook_ && observer, "catalogue startup installation already attempted or invalid");
        initial_ = check_catalog_contract(memory_, modules_.game.range, range(modules_.omnifix));
        check_read();
        std::uint8_t graphics{};
        require(read_value(memory_, modules_.game.range.base + 0xB2A9000, graphics) && graphics == 0,
                "game graphics startup already entered; late installation refused");
        if (!setup_.release_version.empty()) {
            try {
                boot_text_ = std::make_unique<BootText>(memory_, modules_.game.range, setup_.release_version);
                boot_text_->arm();
            } catch (const std::exception& e) {
                if (boot_text_) boot_text_->stop();
                const std::string message = std::string("optional boot version row unavailable: ") + e.what();
                setup_.host.log(setup_.host.context, PZ_LOG_WARNING,
                    {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
            }
        }
        // Independent retained entry bytes, not a copy of live code. Foreign
        // edits admitted elsewhere in the function remain outside this prefix.
        constexpr std::array prefix{std::byte{0x40}, std::byte{0x53}, std::byte{0x48},
                                    std::byte{0x83}, std::byte{0xec}, std::byte{0x30}};
        hook_ = std::make_unique<CatalogHook>(memory_, initial_.load, prefix, context, observer);
        arming_started_ = true;
        hook_->arm();
    }

    CapturedCatalog capture() override {
        // CatalogHook publishes its atomic armed phase before observation.
        // No post-arm flag on the installation thread participates here.
        require(hook_ && hook_->intact(), "catalogue startup hook is not intact");
        const auto base = modules_.game.range.base;
        std::uint8_t graphics{};
        std::uint32_t mutex{};
        require(read_value(memory_, base + 0xB2A9000, graphics) && graphics == 1 &&
                read_value(memory_, base + 0xB95B690, mutex) && mutex,
                "native graphics/resource boundary is not ready");
        return read_current_catalog(initial_.storage, initial_.getters);
    }

    void activate(const Catalog& catalog) override {
        preparing();
        require(!adapters_ && !assets_, "native adapters already prepared");
        const auto asset_calls = admit_asset_calls(memory_, modules_.game, modules_.utils, modules_.avs);
        adapters_ = make_native_adapters({modules_, memory_, catalog, state_, setup_.host});
        require(adapters_ != nullptr, "required native adapters unavailable");
        adapters_->prepare();
        const auto families = adapters_->asset_families();
        require(!families.empty(), "required native asset family set is empty");
        assets_ = std::make_unique<AssetOwner>(prefix_, families,
            asset_calls, memory_);
        std::vector<LoadedAsset> loaded;
        loaded.reserve(families.size());
        for (const auto family : families) {
            preparing();
            const auto asset = assets_->load(family);
            require(static_cast<bool>(asset), "required private native asset failed to load");
            loaded.push_back({family, asset.slot});
        }
        preparing();
        adapters_->activate(loaded);
        preparing();
        require(static_cast<bool>(assets_->mark_published()), "private assets could not be committed");
        // The owner publishes the one FeatureState, then signals terminal
        // admission. Catalogue success cannot skip these adapter/resource steps.
    }

    bool rollback() noexcept override {
        if (boot_text_) boot_text_->stop();
        if (rollback_done_) return rollback_clean_;
        rollback_done_ = true;
        const bool catalogue = !arming_started_ || (hook_ && hook_->intact());
        // Adapter rollback always retires its independent callback owners,
        // even if the catalogue patch cannot prove ownership. Only then decide
        // whether releasing private native assets is safe.
        const bool adapters = !adapters_ || adapters_->rollback();
        if (!catalogue || !adapters) return false;
        if (assets_ && !assets_->rollback()) return false;
        rollback_clean_ = true;
        return true;
    }

    void retire_observer() noexcept override {
        if (boot_text_) boot_text_->stop();
        // Startup observation owns creation/activation of adapters_. Drain it
        // first so shutdown cannot race that pointer or an in-progress arm.
        if (hook_) hook_->retire_observer();
        if (adapters_) adapters_->retire_callbacks();
    }

private:
    const NativeSetup setup_;
    FeatureState& state_;
    MemoryView memory_ = current_process_memory();
    std::vector<ModuleLease> leases_;
    NativeModules modules_;
    CatalogContract initial_;
    std::string prefix_;
    std::unique_ptr<CatalogHook> hook_;
    std::unique_ptr<BootText> boot_text_;
    std::unique_ptr<NativeAdapters> adapters_;
    std::unique_ptr<AssetOwner> assets_;
    bool arming_started_{}, rollback_done_{}, rollback_clean_{};

    ImageInfo hold(std::wstring_view name) {
        leases_.emplace_back(name);
        return leases_.back().image();
    }
    std::optional<ImageInfo> optional(const wchar_t* name) {
        if (!GetModuleHandleW(name)) return {};
        return hold(name);
    }
    void preparing() const {
        require(state_.view().phase == FeaturePhase::preparing, "native admission stopped before publication");
    }
    void check_read() const {
        check_avs_read_contract(memory_, modules_.game, modules_.avs);
    }
};
} // namespace

std::unique_ptr<NativeDriver> make_native_driver(const NativeSetup& setup, native::FeatureState& state) {
    return std::make_unique<GameDriver>(setup, state);
}
} // namespace point_zero::core
