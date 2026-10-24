#include "http.hpp"

#include <Windows.h>
#include <winhttp.h>
#include <shlwapi.h>
#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace point_zero::loader {
namespace {
void require(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
[[noreturn]] void failure(const char* why) {
    throw std::runtime_error(std::string(why) + ": Windows error " + std::to_string(GetLastError()));
}
void check_stop(const Stop& stop) { if (stop.requested()) throw std::runtime_error("startup cancelled"); }
struct Internet {
    HINTERNET value{};
    ~Internet() { if (value) WinHttpCloseHandle(value); }
    Internet() = default;
    Internet(const Internet&) = delete;
    Internet& operator=(const Internet&) = delete;
};
std::wstring wide(std::string_view s) {
    require(!s.empty() && s.size() <= 8192 && s.find('\0') == s.npos, "invalid download URL");
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    require(size > 0, "invalid UTF-8 URL");
    std::wstring out(size, L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), out.data(), size) == size,
            "URL conversion failed");
    return out;
}
struct Url { std::wstring host, path; INTERNET_PORT port{}; bool secure{}; };
Url parse_url(const std::wstring& url) {
    require(!url.empty() && url.size() <= 4096 && url.find(L'\0') == url.npos &&
            url.find_first_of(L"\r\n\t #") == url.npos, "invalid download URL");
    URL_COMPONENTS c{};
    c.dwStructSize = sizeof(c);
    c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength = c.dwUserNameLength = c.dwPasswordLength = DWORD(-1);
    require(WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.size()), 0, &c) != FALSE && c.dwHostNameLength &&
            !c.dwUserNameLength && !c.dwPasswordLength, "download URL must not contain credentials");
    Url out{{c.lpszHostName, c.dwHostNameLength}, {}, c.nPort, c.nScheme == INTERNET_SCHEME_HTTPS};
    bool permitted = out.secure;
#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
    permitted = permitted || (c.nScheme == INTERNET_SCHEME_HTTP && out.host == L"127.0.0.1");
#endif
    require(permitted, "HTTPS is required for updates");
    if (c.dwUrlPathLength) out.path.assign(c.lpszUrlPath, c.dwUrlPathLength);
    if (out.path.empty()) out.path = L"/";
    if (c.dwExtraInfoLength) out.path.append(c.lpszExtraInfo, c.dwExtraInfoLength);
    return out;
}
std::optional<std::wstring> header(HINTERNET request, DWORD query) {
    DWORD size{};
    if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX)) {
        if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND) return {};
        require(GetLastError() == ERROR_INSUFFICIENT_BUFFER && size <= 32768, "response header too large");
    }
    std::wstring result(size / sizeof(wchar_t) + 1, L'\0');
    if (!WinHttpQueryHeaders(request, query, WINHTTP_HEADER_NAME_BY_INDEX, result.data(), &size, WINHTTP_NO_HEADER_INDEX))
        failure("response header query");
    result.resize(size / sizeof(wchar_t));
    while (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}
std::wstring redirect(const std::wstring& base, const std::wstring& location) {
    require(!location.empty() && location.size() <= 4096, "invalid redirect location");
    std::array<wchar_t, 8192> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    require(SUCCEEDED(UrlCombineW(base.c_str(), location.c_str(), buffer.data(), &size, URL_DONT_ESCAPE_EXTRA_INFO)),
            "redirect URL combination failed");
    std::wstring result(buffer.data(), size);
    if (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}

#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
std::atomic_uint32_t live_contexts{}, retired_contexts{};
#endif
struct Context {
    std::atomic_uint refs{1}; // owner, plus one reference held by the request
    HANDLE changed{}, closed{};
    std::atomic<DWORD> notices{}, error{}, read_bytes{}, available_bytes{};
    std::atomic<ULONGLONG> completion_tick{};
    std::array<char, 65536> buffer{};
    Context() {
        changed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        closed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!changed || !closed) {
            if (changed) CloseHandle(changed);
            if (closed) CloseHandle(closed);
            failure("network events");
        }
#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
        ++live_contexts;
#endif
    }
    ~Context() {
        CloseHandle(changed); CloseHandle(closed);
#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
        --live_contexts;
#endif
    }
    void release() noexcept { if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this; }
};
void CALLBACK status(HINTERNET, DWORD_PTR value, DWORD notice, void* data, DWORD size) noexcept {
    auto* context = reinterpret_cast<Context*>(value);
    if (!context) return;
    if (notice == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
        SetEvent(context->closed);
#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
        ++retired_contexts;
#endif
        // Documented final callback, not concurrent with another callback for
        // this request. Release last: it may destroy the entire buffer/context.
        context->release();
        return;
    }
    if (notice == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) context->read_bytes.store(size, std::memory_order_release);
    if (notice == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE && data && size == sizeof(DWORD))
        context->available_bytes.store(*static_cast<DWORD*>(data), std::memory_order_release);
    if (notice == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE || notice == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
        notice == WINHTTP_CALLBACK_STATUS_READ_COMPLETE || notice == WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE)
        context->completion_tick.store(GetTickCount64(), std::memory_order_relaxed);
    if (notice == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR && data && size == sizeof(WINHTTP_ASYNC_RESULT))
        context->error.store(static_cast<WINHTTP_ASYNC_RESULT*>(data)->dwError, std::memory_order_release);
    context->notices.fetch_or(notice, std::memory_order_release);
    SetEvent(context->changed);
}
struct Request {
    Context* context = new Context;
    Internet connection, request;
    const Stop& stop;
    HttpTimeouts timeouts;
    bool callback_owns_reference = false;
    Request(const Stop& s, HttpTimeouts t) : stop(s), timeouts(t) {}
    ~Request() {
        if (request.value) {
            const auto handle = std::exchange(request.value, nullptr);
            const bool closing = WinHttpCloseHandle(handle) != FALSE;
            if (closing && callback_owns_reference)
                (void)WaitForSingleObject(context->closed, timeouts.retirement_ms);
            // On late retirement, the request reference still owns its context.
            // No further API call uses this request, including on close failure.
        }
        context->release();
    }
    void open(HINTERNET session, const Url& url) {
        check_stop(stop);
        connection.value = WinHttpConnect(session, url.host.c_str(), url.port, 0);
        if (!connection.value) failure("WinHTTP connection");
        request.value = WinHttpOpenRequest(connection.value, L"GET", url.path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           url.secure ? WINHTTP_FLAG_SECURE : 0);
        if (!request.value) failure("WinHTTP request");
        DWORD disabled = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_COOKIES;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) failure("request features");
        DWORD policy = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_AUTOLOGON_POLICY, &policy, sizeof(policy))) failure("request credential policy");
        DWORD_PTR value = reinterpret_cast<DWORD_PTR>(context);
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_CONTEXT_VALUE, &value, sizeof(value))) failure("request context");
        if (WinHttpSetStatusCallback(request.value, status,
            WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
            failure("request callback");
        context->refs.fetch_add(1, std::memory_order_relaxed);
        callback_owns_reference = true;
    }
    void wait(DWORD wanted, ULONGLONG deadline) {
        for (;;) {
            check_stop(stop);
            if (const auto error = context->error.load(std::memory_order_acquire))
                throw std::runtime_error("WinHTTP request failed: " + std::to_string(error));
            if (context->notices.load(std::memory_order_acquire) & wanted) {
                require(context->completion_tick.load(std::memory_order_relaxed) <= deadline, "download progress deadline exceeded");
                context->notices.fetch_and(~wanted, std::memory_order_acq_rel); return;
            }
            const auto now = GetTickCount64();
            require(now < deadline, "download progress deadline exceeded");
            HANDLE handles[]{static_cast<HANDLE>(stop.event()), context->changed};
            const auto woke = WaitForMultipleObjects(2, handles, FALSE, static_cast<DWORD>(deadline - now));
            require(woke == WAIT_OBJECT_0 || woke == WAIT_OBJECT_0 + 1 || woke == WAIT_TIMEOUT, "download wait failed");
        }
    }
    DWORD send() {
        check_stop(stop);
        const auto deadline = GetTickCount64() + timeouts.connect_ms + timeouts.headers_ms;
        if (!WinHttpSendRequest(request.value, L"Cache-Control: no-cache\r\n", DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0,
                                reinterpret_cast<DWORD_PTR>(context))) failure("send update request");
        wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, deadline);
        check_stop(stop);
        const auto headers_deadline = GetTickCount64() + timeouts.headers_ms;
        if (!WinHttpReceiveResponse(request.value, nullptr)) failure("receive update headers");
        wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, headers_deadline);
        DWORD code{}, size = sizeof(code);
        if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX)) failure("response status");
        return code;
    }
    std::string body(size_t limit) {
        std::optional<size_t> expected;
        if (const auto raw = header(request.value, WINHTTP_QUERY_CONTENT_LENGTH)) {
            require(!raw->empty(), "invalid Content-Length");
            size_t length = 0;
            for (const auto c : *raw) {
                require(c >= L'0' && c <= L'9', "invalid Content-Length");
                const auto digit = static_cast<size_t>(c - L'0');
                require(digit <= limit && length <= (limit - digit) / 10,
                        "download size limit exceeded");
                length = length * 10 + digit;
                require(length <= limit, "download size limit exceeded");
            }
            expected = length;
        }
        std::string result;
        if (expected) result.reserve(*expected);
        auto deadline = GetTickCount64() + timeouts.body_idle_ms;
        for (;;) {
            check_stop(stop);
            if (!WinHttpQueryDataAvailable(request.value, nullptr)) failure("query update body");
            wait(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, deadline);
            const auto available = context->available_bytes.load(std::memory_order_acquire);
            // An unbounded-size read can wait to fill its buffer despite small
            // healthy chunks arriving. Querying does not itself renew progress.
            const auto requested = available ? std::min<DWORD>(available, static_cast<DWORD>(context->buffer.size()))
                                             : static_cast<DWORD>(context->buffer.size());
            context->read_bytes.store(0, std::memory_order_relaxed);
            if (!WinHttpReadData(request.value, context->buffer.data(), requested, nullptr))
                failure("read update body");
            wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, deadline);
            const auto count = context->read_bytes.load(std::memory_order_acquire);
            require(count <= context->buffer.size() && count <= limit - result.size(), "download size limit exceeded");
            if (!count) break; // EOF does not extend the progress deadline.
            result.append(context->buffer.data(), count);
            deadline = context->completion_tick.load(std::memory_order_relaxed) + timeouts.body_idle_ms;
        }
        check_stop(stop);
        require(!expected || result.size() == *expected, "truncated update response");
        return result;
    }
};
} // namespace

Stop::Stop() : event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) { if (!event_) failure("startup stop event"); }
Stop::~Stop() { CloseHandle(static_cast<HANDLE>(event_)); }
void Stop::request() noexcept { stopped_.store(true, std::memory_order_release); SetEvent(static_cast<HANDLE>(event_)); }
bool Stop::requested() const noexcept { return stopped_.load(std::memory_order_acquire); }
const std::atomic_bool& Stop::flag() const noexcept { return stopped_; }
void* Stop::event() const noexcept { return event_; }

struct Http::Impl {
    const Stop& stop;
    HttpTimeouts timeouts;
    Internet session;
    Impl(const Stop& s, HttpTimeouts t) : stop(s), timeouts(t) {
        check_stop(s);
        for (const auto n : {t.connect_ms, t.headers_ms, t.body_idle_ms})
            require(n && n <= 60000, "invalid HTTP timeout");
        require(t.retirement_ms <= 1000, "invalid callback retirement wait");
        DWORD access = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
        access = WINHTTP_ACCESS_TYPE_NO_PROXY;
#endif
        session.value = WinHttpOpen(L"POINT ZERO", access, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
        if (!session.value) failure("WinHTTP session");
        if (!WinHttpSetTimeouts(session.value, static_cast<int>(t.connect_ms), static_cast<int>(t.connect_ms),
                                static_cast<int>(t.headers_ms), static_cast<int>(t.body_idle_ms))) failure("WinHTTP timeouts");
    }
};
Http::Http(const Stop& stop, HttpTimeouts timeouts) : impl_(std::make_unique<Impl>(stop, timeouts)) {}
Http::~Http() = default;
std::string Http::get(std::string_view address, size_t maximum_bytes) {
    require(maximum_bytes <= 512ULL * 1024 * 1024, "download bound too large");
    auto url = wide(address);
    for (int hop = 0; hop <= 3; ++hop) {
        check_stop(impl_->stop);
        const auto parsed = parse_url(url);
        Request request(impl_->stop, impl_->timeouts);
        request.open(impl_->session.value, parsed);
        const auto code = request.send();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            require(hop < 3, "update redirect limit exceeded");
            const auto location = header(request.request.value, WINHTTP_QUERY_LOCATION);
            require(location.has_value(), "redirect Location missing");
            const auto next = redirect(url, *location);
            const auto target = parse_url(next);
            require(!parsed.secure || target.secure, "HTTPS downgrade redirect refused");
            url = next;
            continue;
        }
        require(code == 200, "update server returned non-success status");
        return request.body(maximum_bytes);
    }
    throw std::runtime_error("update redirect limit exceeded");
}

#ifdef POINT_ZERO_TEST_HTTP_LOOPBACK
namespace testing {
std::uint32_t live_http_contexts() noexcept { return live_contexts.load(); }
std::uint32_t retired_http_contexts() noexcept { return retired_contexts.load(); }
}
#endif
} // namespace point_zero::loader
