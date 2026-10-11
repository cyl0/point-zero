#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace point_zero::loader {
// Shared by startup and SDK shutdown. Its event wakes network waits immediately;
// the atomic is also consumed by the existing file/hash/preparation owners.
class Stop {
public:
    Stop();
    ~Stop();
    Stop(const Stop&) = delete;
    Stop& operator=(const Stop&) = delete;
    void request() noexcept;
    bool requested() const noexcept;
    const std::atomic_bool& flag() const noexcept;
    void* event() const noexcept;
private:
    std::atomic_bool stopped_{false};
    void* event_{};
};

struct HttpTimeouts {
    std::uint32_t connect_ms = 5000;
    std::uint32_t headers_ms = 10000;
    std::uint32_t body_idle_ms = 15000;
    std::uint32_t retirement_ms = 250;
};

// One synchronous owner using asynchronous WinHTTP underneath. Only the owner
// makes WinHTTP calls. Callback buffers remain alive through HANDLE_CLOSING,
// even when bounded retirement waiting ends first. HTTPS and certificate checks
// are mandatory; only a separately compiled test target permits HTTP loopback.
class Http {
public:
    explicit Http(const Stop&, HttpTimeouts = {});
    ~Http();
    Http(const Http&) = delete;
    Http& operator=(const Http&) = delete;
    std::string get(std::string_view url, std::size_t maximum_bytes);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace point_zero::loader
