#pragma once

#include "http.hpp"
#include "transaction.hpp"
#include "updater_protocol.hpp"

#include <functional>
#include <memory>
#include <optional>

namespace point_zero::loader {
enum class StartupNotice { updated, failed };
struct StartupOptions {
    std::filesystem::path runtime;
    // Called once, after recovery, under the existing transaction lock. It may
    // stage a complete generation; it must not activate anything.
    std::function<std::optional<std::string>(const Release&, Channel, const Stop&)> update;
    std::function<void(std::string_view)> log;
    // Fetch once after transaction recovery, before config/update/activation.
    // Returned metadata is a value; callback/context storage must survive the
    // core's shutdown even when a bounded wait_finished call times out.
    std::function<PZ_NativeHost()> native_host;
    // Typed outcome, not log parsing. Optional and best-effort; the SDK owner
    // defers visible delivery until the native ready signal. No ACK on a toast.
    std::function<void(StartupNotice)> notice;
    // Before any core load, transfer a verified selection to its updater. A
    // true result returns updater_handoff after releasing this transaction.
    std::function<bool(const VerifiedGeneration&, bool offered)> handoff;
    // Local running-game evidence, supplied by the SDK owner after native_host.
    // Absent in generic contract hosts, which retain their installed major.
    std::function<std::uint32_t()> game_major;
    // Handoffs must detect again; only the initial updater may recover.
    bool recover_detection{};
};

// Entry thread owns preparation through initialize_prepared. A single worker
// then owns ACK/cache and shutdown. Completion callbacks only publish an atomic
// result and event. Destroy never races a core call or destroys a live context.
// The enclosing updater DLL must remain resident while this owner can execute.
class Startup {
public:
    explicit Startup(StartupOptions);
    ~Startup();
    Startup(const Startup&) = delete;
    Startup& operator=(const Startup&) = delete;
    int run() noexcept; // repeated calls share the first attempt/result
    void request_stop() noexcept;
    bool wait_finished(std::uint32_t milliseconds) const noexcept;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace point_zero::loader
