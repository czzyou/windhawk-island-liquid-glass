from pathlib import Path


def apply_weather(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    root = Path(__file__).parent
    change('#include <winhttp.h>', '#include <winhttp.h>\n#include <winrt/Windows.Data.Json.h>')
    change('''struct WeatherSnapshot {
    bool hasData = false;''', '''struct WeatherSnapshot {
    struct ForecastDay {
        bool available = false;
        int date = 0, weatherCode = 0;
        float low = 0, high = 0;
        std::wstring description, wind;
    };
    ForecastDay forecast[3];
    bool hasDailyRange = false;
    float lowTemperature = 0.0f;
    float highTemperature = 0.0f;
    std::wstring observationTime;
    bool hasData = false;''')
    start = src.index('// --- Weather Fetching Helpers ---')
    end = src.index('DWORD WINAPI AudioThreadProc(', start)
    src = src[:start] + (root/'china-weather.h').read_text(encoding='utf-8') + '\n' + \
        (root/'china-weather-thread.h').read_text(encoding='utf-8') + '\n' + src[end:]
    change('''    $description: Enter your city (e.g. London). Leave blank to use auto IP geolocation.''',
           '''    $description: '中国天气网城市：沈阳，或九位城市编号（沈阳 101070101）。其他城市请填写编号。' ''')
    change('swprintf_s(wTemp, L"%.0f\\x00B0", state.weather.temperature);',
           'swprintf_s(wTemp, L"%s\\x00B0", WeatherTemperatureText(state.weather.temperature).c_str());')
    change('''        std::wstring desc = wText;''', '''        std::wstring desc = wText;
        if (hasWeather && state.weather.hasDailyRange) {
            wchar_t range[64]{};
            swprintf_s(range, L"今日 %s°～%s°", WeatherTemperatureText(state.weather.lowTemperature).c_str(),
                WeatherTemperatureText(state.weather.highTemperature).c_str());
            desc = std::wstring(range) + L"\\n" + state.weather.weatherDesc;
        }
        if (hasWeather) city += L" · 中国天气网";''')
    change('''        const Metric metrics[3] = {''', '''        const Metric metrics[2] = {''')
    change('''            {1, Loc(L"Feels Like"),
             hasWeather ? state.weather.feelsLike + L"\\x00B0" : std::wstring(L"--")},
''', '')
    change('''        const float stackH = cardH * 3.0f + cardGap * 2.0f;''',
           '''        const float stackH = cardH * 2.0f + cardGap;''')

    marker = 'int FileTrayRowAtContentPoint('
    change(marker, (root/'weather-page.h').read_text(encoding='utf-8') + '\n' + marker)
    start = src.index('    void DrawWeatherDashboard(')
    end = src.index('\n    void ', start + 5)
    weather = src[start:end]
    weather = weather.replace('''        wchar_t wTemp[32] = {};''', '''        const int page = g_weatherPage.load();
        const auto target = ChinaWeatherDay(ChinaWeatherClock(), page);
        const auto& forecast = state.weather.forecast[page];
        const bool hasForecast = hasWeather && forecast.available && forecast.date == ChinaWeatherDateKey(target);
        const bool hasReading = hasWeather && (page == 0 || hasForecast);
        const wchar_t* dayLabels[] = {L"今天", L"明天", L"后天"};
        wchar_t date[16]{}; swprintf_s(date, L"%02u/%02u", target.wMonth, target.wDay);
        wchar_t wTemp[32] = {};''')
    weather = weather.replace('if (hasWeather) swprintf_s(wTemp,', 'if (hasReading) swprintf_s(wTemp,')
    weather = weather.replace('WeatherTemperatureText(state.weather.temperature).c_str()',
                              'WeatherTemperatureText(page == 0 ? state.weather.temperature : forecast.high).c_str()')
    weather = weather.replace('''        if (hasWeather) city += L" · 中国天气网";''', '''        if (page > 0) {
            desc = hasForecast ? forecast.description + L"\\n" + WeatherTemperatureText(forecast.low) + L"°～" +
                WeatherTemperatureText(forecast.high) + L"°" : std::wstring(L"暂无预报");
        }
        if (hasWeather) city += L" · " + std::wstring(dayLabels[page]) + (page > 0 ? L"最高" : L"");
        const auto button = WeatherPageButtonBounds((rect.right - rect.left) / scale);
        const auto buttonRect = D2D1::RectF(rect.left + button.left * scale, rect.top + button.top * scale,
            rect.left + button.right * scale, rect.top + button.bottom * scale);
        DrawCard(buttonRect, 10.0f * scale);
        const auto buttonText = std::wstring(dayLabels[page]) + L"  ›";
        if (smallTextFormat_) {
            textBrush_->SetOpacity(0.9f);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            target_->DrawTextW(buttonText.c_str(), static_cast<UINT32>(buttonText.size()), smallTextFormat_.Get(),
                buttonRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }''')
    weather = weather.replace('if (hasWeather && iconStrong && iconSoft)', 'if (hasReading && iconStrong && iconSoft)')
    weather = weather.replace('WeatherVisualFromCode(state.weather.weatherCode)',
                              'WeatherVisualFromCode(page == 0 ? state.weather.weatherCode : forecast.weatherCode)')
    weather = weather.replace('''{0, Loc(L"Wind"),
             hasWeather ?''', '''{0, page > 0 ? L"风况" : Loc(L"Wind"),
             page > 0 ? (hasForecast ? forecast.wind : std::wstring(L"--")) : hasWeather ?''')
    weather = weather.replace('''{2, Loc(L"Humidity"),
             hasWeather ?''', '''{2, page > 0 ? L"预报日期" : Loc(L"Humidity"),
             page > 0 ? std::wstring(date) : hasWeather ?''')
    src = src[:start] + weather + src[end:]
    marker = 'LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {'
    change(marker, '''bool WeatherPageVisible() {
    if (!g_settings.weather || CurrentDashboardTab(g_settings) != 2 || g_lookupUi.open) return false;
    std::lock_guard lock(g_stateMutex);
    const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
    if (kinds.empty()) return false;
    if (kinds[0] == IslandKind::Media) return true;
    return kinds[0] == IslandKind::Idle && !g_settings.gameOverlay &&
        !Wh_GetIntValue(L"GameOverlayPinned", 0) && !g_timerDetailVisible.load();
}

''' + marker)
    change('    static ULONGLONG s_touchStartTime = 0;',
           '    static bool weatherPagePressed = false;\n    static ULONGLONG s_touchStartTime = 0;')
    change('''                s_touchStartTime = GetTickCount64();''', '''                s_touchStartTime = GetTickCount64();
                if (WeatherPageVisible() && WeatherPageButtonHit(MediaContentFromClient(xPos, yPos, true))) {
                    weatherPagePressed = true;
                    SetCapture(hwnd);
                    return 0;
                }''')
    change('''        case WM_LBUTTONUP:
            {''', '''        case WM_LBUTTONUP:
            {
            if (weatherPagePressed) {
                weatherPagePressed = false;
                ReleaseCapture();
                s_touchStartTime = 0;
                const int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
                if (WeatherPageVisible() && WeatherPageButtonHit(MediaContentFromClient(x, y, true)) &&
                    std::abs(x - s_touchStart.x) <= 8 && std::abs(y - s_touchStart.y) <= 8) {
                    g_weatherPage = NextWeatherPage(g_weatherPage.load());
                    g_layoutDirty = true;
                }
                return 0;
            }''')
    change('''        case WM_CAPTURECHANGED:''', '''        case WM_CAPTURECHANGED:
            weatherPagePressed = false;''')
    change('''        case WM_SETCURSOR:
''', '''        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && WeatherPageVisible()) {
                POINT point{}; GetCursorPos(&point); ScreenToClient(hwnd, &point);
                if (WeatherPageButtonHit(MediaContentFromClient(point.x, point.y, true))) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE;
                }
            }
''')
    return src
