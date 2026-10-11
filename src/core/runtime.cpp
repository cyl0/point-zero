#include "runtime.hpp"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace point_zero::core {
namespace {
PZ_Bytes bytes(std::string_view value) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}
void diagnostic(PZ_Diagnostic out, const char* message) noexcept {
    if (out.data && out.capacity) std::snprintf(out.data, out.capacity, "%s", message);
}
template<class Callback, class... Args>
void host_call(Callback callback, Args... args) noexcept {
    try { callback(args...); } catch (...) {}
}
template<class Callback, class... Args>
void guarded_host_call(Callback callback, Args... args) noexcept {
#if defined(_MSC_VER)
    // Catch host SEH in this frame, before returning to any caller holding a
    // mutex. An outer observer handler cannot unwind that scope under /EHsc.
    __try { host_call(callback, args...); }
    __except (1) {}
#else
    host_call(callback, args...);
#endif
}

class Runtime final {
public:
    Runtime(std::unique_ptr<PZ_Prepared> prepared, NativeRequest request)
        : prepared_(std::move(prepared)), request_(std::move(request)) {
        // The ABI requires the loader to copy this before initialize transfers
        // ownership. Keep ratings/provenance, not their serialized duplicate.
        std::string{}.swap(prepared_->cache);
    }

    int start(PZ_Diagnostic out) noexcept {
        try {
            NativeSetup setup{request_.runtime_path, request_.generation_path,
                prepared_->generation_id, {request_.host.avs, this, host_log,
                    request_.host.notice ? host_notice : nullptr,
                    request_.host.ready ? host_ready : nullptr}, prepared_->release_version};
            driver_ = make_native_driver(setup, state_);
            if (!driver_) throw std::runtime_error("native adapter owner unavailable");
            driver_->install(observed, this);
            return PZ_OK; // The original native loader has not necessarily run.
        } catch (const std::exception& e) {
            diagnostic(out, e.what()); reject(e.what());
        } catch (...) {
            diagnostic(out, "unexpected native initialization failure");
            reject("unexpected native initialization failure");
        }
        return PZ_NATIVE_FAILED;
    }

    void stop() noexcept {
        // The fixed loader serializes this after start returns. Draw/native
        // callback backing is retained, so making the shared state terminal does
        // not free anything beneath an executing native callback.
        {
            // Completion and cancellation have one order. Do not retain this
            // lock while draining the observer: it may be finishing admission
            // or sending its last host log on the native thread.
            std::lock_guard lock(host_mutex_);
            stopping_.store(true, std::memory_order_release);
            state_.stop();
        }
        if (driver_) driver_->retire_observer();
        finish(PZ_CANCELLED);
        std::lock_guard lock(host_mutex_);
        host_retired_ = true;
        request_.host.context = nullptr;
        request_.host.log = nullptr;
        request_.host.notice = nullptr;
        request_.host.ready = nullptr;
        request_.completion_context = nullptr;
        request_.completion = nullptr;
    }

private:
    std::unique_ptr<PZ_Prepared> prepared_;
    NativeRequest request_;
    native::FeatureState state_;
    native::Catalog catalog_;
    std::unique_ptr<NativeDriver> driver_;
    std::atomic_bool stopping_{false};
    std::mutex host_mutex_;
    bool completed_{}, host_retired_{};

    static void PZ_CALL host_log(void* context, int32_t level, PZ_Bytes message) noexcept {
        auto& self = *static_cast<Runtime*>(context);
        std::lock_guard lock(self.host_mutex_);
        if (!self.host_retired_ && self.request_.host.log) {
            guarded_host_call(self.request_.host.log, self.request_.host.context, level, message);
        }
    }
    static void PZ_CALL host_notice(void* context, PZ_Bytes message) noexcept {
        auto& self = *static_cast<Runtime*>(context);
        std::lock_guard lock(self.host_mutex_);
        if (!self.host_retired_ && self.request_.host.notice) {
            guarded_host_call(self.request_.host.notice, self.request_.host.context, message);
        }
    }
    static void PZ_CALL host_ready(void* context) noexcept {
        auto& self = *static_cast<Runtime*>(context);
        std::lock_guard lock(self.host_mutex_);
        if (!self.host_retired_ && !self.stopping_.load(std::memory_order_acquire) && self.request_.host.ready) {
            guarded_host_call(self.request_.host.ready, self.request_.host.context);
        }
    }
    void log(const char* message) noexcept { host_log(this, PZ_LOG_ERROR, bytes(message)); }

    void finish(int status) noexcept {
        std::lock_guard lock(host_mutex_);
        if (completed_ || host_retired_) return;
        completed_ = true;
        // Cancellation that already won may never report positive admission.
        if (stopping_.load(std::memory_order_acquire)) status = PZ_CANCELLED;
        if (status == PZ_OK) {
            // Binding copied all runtime values into catalog_; native adapters
            // never resolve its assignment indices back to prepared provenance.
            prepared_->ratings = {};
        }
        if (request_.completion) {
            guarded_host_call(request_.completion, request_.completion_context, status);
        }
    }
    void reject(const char* message) noexcept {
        // Stop owns callback draining and retains native backing. A checkpoint
        // throwing after stop is cancellation, not evidence of lost ownership.
        if (stopping_.load(std::memory_order_acquire)) { finish(PZ_CANCELLED); return; }
        const bool clean = !driver_ || driver_->rollback();
        state_.fail(clean);
        if (stopping_.load(std::memory_order_acquire)) { finish(PZ_CANCELLED); return; }
        log(message);
        if (!clean) log("native ownership uncertain; custom work stopped; restart required");
        finish(PZ_NATIVE_FAILED);
    }
    void loaded(int result) noexcept {
        try {
            if (stopping_.load(std::memory_order_acquire)) { finish(PZ_CANCELLED); return; }
            if (result != 0) throw std::runtime_error("native catalogue loader failed");
            const auto captured = driver_->capture();
            catalog_ = native::bind_catalog(captured.charts, prepared_->ratings);
            if (stopping_.load(std::memory_order_acquire)) { finish(PZ_CANCELLED); return; }
            driver_->activate(catalog_);
            // Activation makes all required callbacks/resources ready while
            // the shared gate remains closed. A concurrent stop cannot revive it.
            if (!state_.publish()) {
                if (stopping_.load(std::memory_order_acquire)) finish(PZ_CANCELLED);
                else reject("custom feature publication refused");
                return;
            }
            finish(PZ_OK);
        } catch (const std::exception& e) { reject(e.what()); }
        catch (...) { reject("unexpected native catalogue admission failure"); }
    }
    static bool guarded_loaded(Runtime* self, int result) noexcept {
#if defined(_MSC_VER)
        __try { self->loaded(result); return true; }
        __except (1) { return false; }
#else
        self->loaded(result); return true;
#endif
    }
    static void observed(void* context, int result) noexcept {
        auto& self = *static_cast<Runtime*>(context);
        if (!guarded_loaded(&self, result)) {
            // Native SEH is separate from ordinary rejection/cancellation:
            // retain every potentially exposed native owner, never release via
            // rollback. Shutdown still drains callbacks with backing retained.
            self.state_.fail(false);
            self.log("native admission fault; ownership uncertain; restart required");
            self.finish(PZ_NATIVE_FAILED);
        }
    }
};

// No static Runtime destructor. Once native initialization starts, the fixed
// loader retains core code and this owner retains all callback/data/module
// backing until process exit. Host callbacks are separately retired by stop.
std::atomic<Runtime*> runtime{};
std::atomic_bool attempted{};
} // namespace

int32_t initialize_native(std::unique_ptr<PZ_Prepared> prepared, NativeRequest request,
                          PZ_Diagnostic out) noexcept {
    diagnostic(out, "");
    if (attempted.exchange(true, std::memory_order_acq_rel)) {
        diagnostic(out, "native initialization already attempted");
        if (request.completion) {
            guarded_host_call(request.completion, request.completion_context, PZ_NATIVE_FAILED);
        }
        return PZ_NATIVE_FAILED;
    }
    try {
        if (!prepared) throw std::runtime_error("prepared native input missing");
        // Preserve the original request until allocation/copy succeeds, so even
        // allocation failure can deliver the accepted call's terminal result.
        auto owner = std::make_unique<Runtime>(std::move(prepared), request);
        const auto retained = owner.release();
        runtime.store(retained, std::memory_order_release);
        return retained->start(out);
    } catch (const std::exception& e) { diagnostic(out, e.what()); }
    catch (...) { diagnostic(out, "native owner allocation failed"); }
    if (request.completion) {
        guarded_host_call(request.completion, request.completion_context, PZ_NATIVE_FAILED);
    }
    return PZ_NATIVE_FAILED;
}

void shutdown_native() noexcept {
    if (const auto owner = runtime.load(std::memory_order_acquire)) owner->stop();
}
} // namespace point_zero::core
