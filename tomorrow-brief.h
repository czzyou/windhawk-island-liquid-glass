#pragma once

struct TomorrowBrief {
    bool active = false;
    int date = 0;
    int weatherCode = 0;
    int meal = 0;
    bool important = false;
    std::wstring title, body, detail, key;
    std::vector<NbaSnapshot> games;
    double fetchedAt = 0;
};

inline int BriefDateKey(const SYSTEMTIME& day) {
    return day.wYear * 10000 + day.wMonth * 100 + day.wDay;
}

inline int MealReminderSlot(const SYSTEMTIME& local) {
    const int minute = local.wHour * 60 + local.wMinute;
    if (minute >= 10*60+50 && minute < 11*60+15) return 1;
    if (minute >= 16*60+50 && minute < 17*60+15) return 2;
    return 0;
}

inline TomorrowBrief MakeMealReminder(const SYSTEMTIME& local, int lunchShown, int dinnerShown) {
    TomorrowBrief result;
    const int slot = MealReminderSlot(local);
    const int date = BriefDateKey(local);
    if (!slot || (slot == 1 ? lunchShown : dinnerShown) == date) return result;
    result.meal = slot;
    result.date = date;
    result.key = std::to_wstring(date) + L":" + std::to_wstring(slot);
    result.title = slot == 1 ? L"午饭提醒 · 11:00" : L"晚饭提醒 · 17:00";
    result.body = slot == 1 ? L"该吃午饭了" : L"该吃晚饭了";
    result.detail = L"歇一会儿，按时吃饭";
    return result;
}

inline SYSTEMTIME BriefTomorrow(SYSTEMTIME today) {
    return ChinaWeatherDay(today, 1);
}

inline bool BriefPending(const TomorrowBrief& brief, int tomorrow, double now,
                         const std::wstring& shown) {
    return brief.date == tomorrow && !brief.key.empty() && brief.key != shown &&
        now >= brief.fetchedAt && now - brief.fetchedAt < 3600;
}

inline bool BriefWeatherTomorrow(const SYSTEMTIME& local) { return local.wHour >= 18; }

inline TomorrowBrief ParseTomorrowWeather(const std::string& script, const SYSTEMTIME& today,
                                         const std::wstring& city, bool fahrenheit) {
    using namespace winrt::Windows::Data::Json;
    TomorrowBrief result;
    const auto tomorrow = BriefTomorrow(today);
    const bool showTomorrow = BriefWeatherTomorrow(today);
    const auto target = showTomorrow ? tomorrow : today;
    try {
        const auto object = JsonObject::Parse(winrt::to_hstring(ChinaWeatherObject(script, "fc")));
        const auto days = object.GetNamedArray(L"f");
        bool current = false;
        for (const auto& value : days) {
            if (ChinaWeatherDateMatches(std::wstring(value.GetObject().GetNamedString(L"fi", L"")), today)) current = true;
        }
        if (!current) return {};
        for (const auto& value : days) {
            const auto item = value.GetObject();
            if (!ChinaWeatherDateMatches(std::wstring(item.GetNamedString(L"fi", L"")), target)) continue;
            float low = 0, high = 0;
            if (!ChinaWeatherTemperature(std::wstring(item.GetNamedString(L"fd", L"")), low) ||
                !ChinaWeatherTemperature(std::wstring(item.GetNamedString(L"fc", L"")), high) || low > high) return {};
            const std::wstring code = item.GetNamedString(L"fa", L"").c_str();
            result.weatherCode = ChinaWeatherVisualCode(code);
            if (!result.weatherCode) return {};
            const std::wstring description = ChinaWeatherDescription(code);
            if (fahrenheit) { low = low * 1.8f + 32; high = high * 1.8f + 32; }
            wchar_t range[64]{};
            swprintf_s(range, L"%s°～%s°%s", WeatherTemperatureText(low).c_str(),
                WeatherTemperatureText(high).c_str(), fahrenheit ? L"F" : L"C");
            result.date = BriefDateKey(target);
            result.title = std::wstring(showTomorrow ? L"明天天气 · " : L"今天天气 · ") + (city.empty() ? std::wstring(L"沈阳") : city);
            result.body = description + L"  " + range;
            result.detail = std::wstring(item.GetNamedString(L"fe", L"")) + L" " +
                std::wstring(item.GetNamedString(L"fg", L""));
            if (result.detail.size() > 80) return {};
            result.key = (showTomorrow ? L"" : L"today:") + std::to_wstring(result.date) + L":" + city;
            return result;
        }
    } catch (const winrt::hresult_error&) {}
    return {};
}

inline TomorrowBrief MakeTomorrowNba(const std::vector<NbaSnapshot>& games, const SYSTEMTIME& today,
                                    const std::wstring& favorites) {
    TomorrowBrief result;
    const auto tomorrow = BriefTomorrow(today);
    const auto day = GetNbaLocalDay(tomorrow);
    if (!day.start || !day.end) return {};
    std::vector<NbaSnapshot> selected;
    for (const auto& game : games) {
        if (!game.scheduled || game.startTime < day.start || game.startTime >= day.end ||
            NbaFavoriteRank(game, favorites) == (std::numeric_limits<int>::max)()) continue;
        if (std::none_of(selected.begin(), selected.end(), [&](const auto& other) {return other.gameId == game.gameId;}))
            selected.push_back(game);
    }
    std::sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) {return a.startTime < b.startTime;});
    if (selected.empty()) return {};
    // shortcut: the popup lists two games; expand this layout if more favorites are configured.
    auto label = [](const NbaSnapshot& game) {
        const time_t start = static_cast<time_t>(game.startTime);
        tm local{};
        if (localtime_s(&local, &start) != 0) return std::wstring{};
        wchar_t time[16]{}; swprintf_s(time, L"%02d:%02d", local.tm_hour, local.tm_min);
        return std::wstring(time) + L"  " + game.awayAbbr + L" vs " + game.homeAbbr;
    };
    result.date = BriefDateKey(tomorrow);
    result.title = L"明天 NBA · " + std::to_wstring(selected.size()) + L" 场关注比赛";
    result.body = label(selected[0]);
    result.detail = selected.size() > 1 ? label(selected[1]) : L"开赛时间为本地时间";
    result.key = std::to_wstring(result.date) + L":" + favorites;
    result.games.assign(selected.begin(), selected.begin() + (std::min)(selected.size(), size_t{2}));
    return result;
}
