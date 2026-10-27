#include <winrt/Windows.Foundation.Collections.h>

std::wstring WeatherTemperatureText(float value) {
    wchar_t text[16]{};
    swprintf_s(text, L"%.0f", value);
    return wcscmp(text, L"-0") == 0 ? L"0" : text;
}

std::wstring ChinaWeatherCityCode(const std::wstring& city) {
    // shortcut: name lookup covers Shenyang; use a 9-digit weather.com.cn code for other cities.
    if (city.empty() || city == L"沈阳" || city == L"Shenyang" || city == L"shenyang") return L"101070101";
    if (city.size() == 9 && std::all_of(city.begin(), city.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
        return city;
    return L"";
}

std::string ChinaWeatherObject(const std::string& script, const std::string& name) {
    size_t pos = script.find("var " + name);
    if (pos == std::string::npos) return {};
    pos = script.find('=', pos + 4 + name.size());
    if (pos == std::string::npos) return {};
    pos = script.find_first_not_of(" \r\n\t", pos + 1);
    if (pos == std::string::npos || script[pos] != '{') return {};
    int depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = pos; i < script.size(); ++i) {
        const char ch = script[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') quoted = false;
        } else if (ch == '"') quoted = true;
        else if (ch == '{') ++depth;
        else if (ch == '}' && --depth == 0) return script.substr(pos, i - pos + 1);
    }
    return {};
}

SYSTEMTIME ChinaWeatherClock() {
    FILETIME utc{}; GetSystemTimeAsFileTime(&utc);
    ULARGE_INTEGER ticks{}; ticks.LowPart = utc.dwLowDateTime; ticks.HighPart = utc.dwHighDateTime;
    ticks.QuadPart += 8ULL * 60 * 60 * 10000000;
    FILETIME local{ticks.LowPart, ticks.HighPart};
    SYSTEMTIME result{}; FileTimeToSystemTime(&local, &result);
    return result;
}

bool ChinaWeatherTemperature(const std::wstring& text, float& value) {
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    const float parsed = wcstof(text.c_str(), &end);
    if (end == text.c_str() || *end || !std::isfinite(parsed) || parsed < -90 || parsed > 65) return false;
    value = parsed;
    return true;
}

int ChinaWeatherVisualCode(const std::wstring& code) {
    if (code.size() < 2) return 0;
    const std::wstring digits = (code[0] == L'd' || code[0] == L'n') ? code.substr(1) : code;
    if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return 0;
    switch (_wtoi(digits.c_str())) {
        case 0: return 113;
        case 1: return 116;
        case 2: return 122;
        case 4: case 5: return 200;
        case 6: return 182;
        case 3: case 7: case 8: case 9: case 10: case 11: case 12:
        case 19: case 21: case 22: case 23: case 24: case 25: case 301: return 296;
        case 13: case 14: case 15: case 16: case 17: case 26: case 27: case 28: return 326;
        case 18: case 20: case 29: case 30: case 31: case 53: return 248;
        default: return 0;
    }
}

bool ParseChinaWeather(const std::string& currentScript, const std::string& forecastScript,
                       const SYSTEMTIME& now, bool fahrenheit, WeatherSnapshot& result) {
    using namespace winrt::Windows::Data::Json;
    try {
        const auto current = JsonObject::Parse(winrt::to_hstring(ChinaWeatherObject(currentScript, "dataSK")));
        const auto forecast = JsonObject::Parse(winrt::to_hstring(ChinaWeatherObject(forecastScript, "fc")));
        auto field = [](const JsonObject& object, const wchar_t* name) {
            return std::wstring(object.GetNamedString(name, L""));
        };
        const std::wstring today = std::to_wstring(now.wMonth) + L"月" + std::to_wstring(now.wDay) + L"日";
        if (!field(current, L"date").starts_with(today)) return false;
        const auto observation = field(current, L"time");
        int hour = -1, minute = -1;
        if (observation.size() != 5 || observation[2] != L':' ||
            swscanf_s(observation.c_str(), L"%d:%d", &hour, &minute) != 2 ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
        const int age = now.wHour * 60 + now.wMinute - (hour * 60 + minute);
        if (age < -10 || age > 120) return false;
        WeatherSnapshot parsed;
        if (!ChinaWeatherTemperature(field(current, L"temp"), parsed.temperature)) return false;
        parsed.city = field(current, L"cityname");
        parsed.weatherDesc = field(current, L"weather");
        if (parsed.city.empty() || parsed.weatherDesc.empty()) return false;
        parsed.weatherCode = ChinaWeatherVisualCode(field(current, L"weathercode"));
        parsed.observationTime = observation;
        parsed.windSpeed = field(current, L"wse");
        const auto unit = parsed.windSpeed.find(L"km/h");
        if (unit != std::wstring::npos) parsed.windSpeed.resize(unit);
        parsed.windDir = field(current, L"wde");
        parsed.humidity = field(current, L"SD");
        if (!parsed.humidity.empty() && parsed.humidity.back() == L'%') parsed.humidity.pop_back();
        wchar_t day[8]{}; swprintf_s(day, L"%02d/%02d", now.wMonth, now.wDay);
        const auto days = forecast.GetNamedArray(L"f");
        for (const auto& value : days) {
            const auto item = value.GetObject();
            if (field(item, L"fi") != day) continue;
            if (!ChinaWeatherTemperature(field(item, L"fd"), parsed.lowTemperature) ||
                !ChinaWeatherTemperature(field(item, L"fc"), parsed.highTemperature) ||
                parsed.lowTemperature > parsed.highTemperature) return false;
            parsed.hasDailyRange = true;
            break;
        }
        if (!parsed.hasDailyRange) return false;
        if (fahrenheit) {
            parsed.temperature = parsed.temperature * 1.8f + 32;
            parsed.lowTemperature = parsed.lowTemperature * 1.8f + 32;
            parsed.highTemperature = parsed.highTemperature * 1.8f + 32;
            const float mph = _wtof(parsed.windSpeed.c_str()) / 1.609344f;
            wchar_t speed[16]{}; swprintf_s(speed, L"%.0f", mph); parsed.windSpeed = speed;
        }
        parsed.hasData = true;
        result = std::move(parsed);
        return true;
    } catch (const winrt::hresult_error&) {
        return false;
    }
}
