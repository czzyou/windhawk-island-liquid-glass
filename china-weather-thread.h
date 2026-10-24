std::string HttpGet(const wchar_t* host, const wchar_t* path, bool https = true) {
    std::string response;
    if (!https) return response;
    HINTERNET session = WinHttpOpen(L"DynamicIsland/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return response;
    WinHttpSetTimeouts(session, 5000, 5000, 8000, 8000);
    HINTERNET connection = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection) {
        HINTERNET request = WinHttpOpenRequest(connection, L"GET", path, nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (request) {
            const wchar_t* headers = L"Referer: https://www.weather.com.cn/\r\nCache-Control: no-cache\r\n";
            bool valid = WinHttpSendRequest(request, headers, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr);
            DWORD status = 0, statusSize = sizeof(status);
            valid = valid && WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) && status == 200;
            while (valid) {
                DWORD available = 0, downloaded = 0;
                if (!WinHttpQueryDataAvailable(request, &available)) { valid = false; break; }
                if (!available) break;
                if (available > 512 * 1024 || response.size() + available > 512 * 1024) { valid = false; break; }
                std::vector<char> chunk(available);
                if (!WinHttpReadData(request, chunk.data(), available, &downloaded) || !downloaded) {
                    valid = false; break;
                }
                response.append(chunk.data(), downloaded);
            }
            if (!valid) response.clear();
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connection);
    }
    WinHttpCloseHandle(session);
    return response;
}

DWORD WINAPI WeatherThreadProc(void*) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) return 0;
    WaitForSingleObject(g_stopEvent, 3000);
    size_t failures = 0;
    const DWORD backoff[] = {60000, 120000, 300000, 900000};
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        const Settings settings = GetSettingsCopy();
        if (!settings.weather) {
            WaitForSingleObject(g_stopEvent, 2000);
            continue;
        }
        const auto code = ChinaWeatherCityCode(settings.weatherCity);
        WeatherSnapshot weather;
        bool success = false;
        if (!code.empty()) {
            const auto stamp = L"?_=" + std::to_wstring(GetTickCount64());
            const auto current = HttpGet(L"d1.weather.com.cn", (L"/sk_2d/" + code + L".html" + stamp).c_str());
            const auto forecast = HttpGet(L"d1.weather.com.cn", (L"/weather_index/" + code + L".html" + stamp).c_str());
            success = ParseChinaWeather(current, forecast, ChinaWeatherClock(), settings.weatherFahrenheit, weather);
        }
        if (success) {
            weather.lastUpdated = NowSeconds();
            Wh_SetStringValue(L"WeatherSource", L"中国天气网");
            Wh_SetStringValue(L"WeatherObservation", weather.observationTime.c_str());
            Wh_SetIntValue(L"WeatherCurrentCentiDegrees", static_cast<int>(std::lround(weather.temperature * 100)));
            Wh_SetIntValue(L"WeatherLowCentiDegrees", static_cast<int>(std::lround(weather.lowTemperature * 100)));
            Wh_SetIntValue(L"WeatherHighCentiDegrees", static_cast<int>(std::lround(weather.highTemperature * 100)));
            {
                std::lock_guard lock(g_stateMutex);
                g_state.weather = std::move(weather);
            }
            g_layoutDirty = true;
            failures = 0;
        }
        Wh_SetStringValue(L"WeatherFetchStatus", success ? L"Updated" : L"Unavailable; preserving previous reading");
        const DWORD waitMs = success ? 900000 : backoff[(std::min)(failures++, size_t{3})];
        HANDLE events[] = {g_stopEvent, g_settingsChangedEvent};
        const DWORD result = WaitForMultipleObjects(2, events, FALSE, waitMs);
        if (result == WAIT_OBJECT_0) break;
        if (result == WAIT_OBJECT_0 + 1) failures = 0;
    }
    CoUninitialize();
    return 0;
}
