#include "build_bindings.hpp"
#include "core/runtime.hpp"
#include "adapters.hpp"
#include "assets.hpp"
#include "asset_contract.hpp"
#include "catalog_contract.hpp"
#include "catalog_hook.hpp"
#include "boot_text.hpp"
#include "game_build.hpp"

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
class GameDriver final : public NativeDriver {
public:
    GameDriver(const NativeSetup& setup, FeatureState& state) : setup_(setup), state_(state) {
        require(std::strcmp(setup.host.avs.model, "LDJ") == 0,
                "unsupported game; POINT ZERO requires IIDX (LDJ)");
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
        const auto& build = select_game_build(memory_, modules_.game);
        modules_.game.range.bindings = build.bindings;
        const auto tested_datecode = build.datecode;
        const auto identity_note = std::string("tested_game_build=LDJ") + std::string(tested_datecode) +
            " reported_datecode=" + setup.host.avs.datecode;
        setup.host.log(setup.host.context, PZ_LOG_INFO,
            {reinterpret_cast<const std::uint8_t*>(identity_note.data()), identity_note.size()});
        modules_.afp = hold(L"afp-core.dll");
        modules_.utils = hold(L"afp-utils.dll");
    }

    void install(Observer observer, void* context) override {
        require(!hook_ && observer, "catalogue startup installation already attempted or invalid");
        initial_ = check_catalog_contract(memory_, modules_.game.range);
        std::uint8_t graphics{};
        require(read_value(memory_, modules_.game.range.base + game_bindings(modules_.game.range).game_driver.graphics_started_rva, graphics) && graphics == 0,
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
        const auto prefix = read_code(memory_, modules_.game.range,
            game_bindings(modules_.game.range).catalog_contract.catalog_load_rva, 32);
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
        require(read_value(memory_, base + game_bindings(modules_.game.range).game_driver.graphics_started_rva, graphics) && graphics == 1 &&
                read_value(memory_, base + game_bindings(modules_.game.range).game_driver.catalog_mutex_rva, mutex) && mutex,
                "native graphics/resource boundary is not ready");
        return read_current_catalog(initial_.getters);
    }

    void activate(const Catalog& catalog) override {
        preparing();
        require(!adapters_ && !assets_, "native adapters already prepared");
        adapters_ = make_native_adapters({modules_, memory_, catalog, state_, setup_.host});
        require(adapters_ != nullptr, "required native adapters unavailable");
        adapters_->prepare();
        const auto families = adapters_->asset_families();
        std::vector<LoadedAsset> loaded;
        loaded.reserve(families.size());
        try {
            const auto calls = admit_asset_calls(memory_, modules_.game);
            assets_ = std::make_unique<AssetOwner>(prefix_, families, calls, memory_);
            for (const auto family : families) {
                preparing();
                const auto asset = assets_->load(family);
                if (asset) loaded.push_back({family, asset.slot});
                else warning(std::string("asset unavailable: ") + std::string(family));
            }
        } catch (const std::exception& e) {
            warning(std::string("assets unavailable: ") + e.what());
        }
        preparing();
        adapters_->activate(loaded);
        preparing();
        if (assets_ && !loaded.empty())
            require(static_cast<bool>(assets_->mark_published()), "private assets could not be committed");
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
    void warning(const std::string& message) const {
        setup_.host.log(setup_.host.context, PZ_LOG_WARNING,
            {reinterpret_cast<const std::uint8_t*>(message.data()), message.size()});
    }
    void preparing() const {
        require(state_.view().phase == FeaturePhase::preparing, "native admission stopped before publication");
    }
};
} // namespace

std::unique_ptr<NativeDriver> make_native_driver(const NativeSetup& setup, native::FeatureState& state) {
    return std::make_unique<GameDriver>(setup, state);
}
} // namespace point_zero::core
