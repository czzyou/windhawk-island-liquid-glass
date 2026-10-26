// Async WinHTTP keeps network waits cancelable during module shutdown.
struct HttpResult {
    bool ok = false;
    DWORD status = 0;
    std::string body;
    bool IsDefinitive() const {
        return ok && ((status == 200 && !body.empty()) || status == 404);
    }
};

std::atomic<int> g_mediaHttpActive{0};
struct MediaHttpContext {
    HANDLE completed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE closed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::atomic<DWORD> notification{0}, bytes{0};
    ~MediaHttpContext() {
        if (completed) CloseHandle(completed);
        if (closed) CloseHandle(closed);
    }
};

void CALLBACK MediaHttpCallback(HINTERNET, DWORD_PTR context, DWORD status, void*, DWORD size) {
    auto* state = reinterpret_cast<MediaHttpContext*>(context);
    if (!state) return;
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
        SetEvent(state->closed);
    } else if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE ||
               status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
               status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE ||
               status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
        state->bytes = size;
        state->notification = status;
        SetEvent(state->completed);
    }
}

HttpResult HttpGetEx(const wchar_t* host, const wchar_t* path, bool https, const wchar_t* agent,
                     const wchar_t* headers = nullptr, DWORD timeoutMs = 30000) {
    HttpResult result;
    if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) return result;
    MediaHttpContext state;
    if (!state.completed || !state.closed) return result;
    HINTERNET session = WinHttpOpen(agent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (!session) return result;
    ++g_mediaHttpActive;
    WinHttpSetTimeouts(session, (std::min<DWORD>)(8000, timeoutMs), (std::min<DWORD>)(8000, timeoutMs),
        (std::min<DWORD>)(10000, timeoutMs), (std::min<DWORD>)(15000, timeoutMs));
    HINTERNET connection = WinHttpConnect(session, host,
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    bool callbackInstalled = false;
    char buffer[16384];
    if (request) {
        DWORD_PTR context = reinterpret_cast<DWORD_PTR>(&state);
        callbackInstalled = WinHttpSetOption(request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)) &&
            WinHttpSetStatusCallback(request, MediaHttpCallback,
                WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) != WINHTTP_INVALID_STATUS_CALLBACK;
        auto completed = [&](BOOL started, DWORD wanted) {
            if (!started && GetLastError() != ERROR_IO_PENDING) return false;
            HANDLE events[] = {g_stopEvent, state.completed};
            return WaitForMultipleObjects(2, events, FALSE, timeoutMs) == WAIT_OBJECT_0 + 1 &&
                   state.notification.load() == wanted;
        };
        if (callbackInstalled && completed(WinHttpSendRequest(request, headers,
                headers ? static_cast<DWORD>(-1L) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, context), WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE) &&
            completed(WinHttpReceiveResponse(request, nullptr), WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE)) {
            DWORD size = sizeof(result.status);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &result.status, &size, WINHTTP_NO_HEADER_INDEX)) {
                while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT &&
                       completed(WinHttpReadData(request, buffer, sizeof(buffer), nullptr),
                                 WINHTTP_CALLBACK_STATUS_READ_COMPLETE)) {
                    const DWORD bytes = state.bytes.load();
                    if (!bytes) { result.ok = true; break; }
                    if (result.body.size() + bytes > 4u * 1024u * 1024u) break;
                    result.body.append(buffer, bytes);
                }
            }
        }
        WinHttpCloseHandle(request);
        // HANDLE_CLOSING is the last callback; its context and read buffer stay alive until then.
        if (callbackInstalled) WaitForSingleObject(state.closed, INFINITE);
    }
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    --g_mediaHttpActive;
    return result;
}
