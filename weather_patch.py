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
    change('''        std::wstring desc = wText;''', '''        std::wstring desc = wText;
        if (hasWeather && state.weather.hasDailyRange) {
            wchar_t range[64]{};
            swprintf_s(range, L"今日 %.0f°～%.0f°", state.weather.lowTemperature, state.weather.highTemperature);
            desc = std::wstring(range) + L"\\n" + state.weather.weatherDesc;
        }
        if (hasWeather) city += L" · 中国天气网";''')
    change('''        const Metric metrics[3] = {''', '''        const Metric metrics[2] = {''')
    change('''            {1, Loc(L"Feels Like"),
             hasWeather ? state.weather.feelsLike + L"\\x00B0" : std::wstring(L"--")},
''', '')
    change('''        const float stackH = cardH * 3.0f + cardGap * 2.0f;''',
           '''        const float stackH = cardH * 2.0f + cardGap;''')
    return src
