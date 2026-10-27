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
    today.wHour = today.wMinute = today.wSecond = today.wMilliseconds = 0;
    FILETIME file{};
    if (!SystemTimeToFileTime(&today, &file)) return {};
    ULARGE_INTEGER ticks{};
    ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    ticks.QuadPart += 864000000000ULL;
    file = {ticks.LowPart, ticks.HighPart};
    SYSTEMTIME result{};
    FileTimeToSystemTime(&file, &result);
    return result;
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
    wchar_t currentDate[8]{}, nextDate[8]{};
    swprintf_s(currentDate, L"%02u/%02u", today.wMonth, today.wDay);
    swprintf_s(nextDate, L"%02u/%02u", target.wMonth, target.wDay);
    try {
        const auto object = JsonObject::Parse(winrt::to_hstring(ChinaWeatherObject(script, "fc")));
        const auto days = object.GetNamedArray(L"f");
        bool current = false;
        for (const auto& value : days) {
            if (value.GetObject().GetNamedString(L"fi", L"") == currentDate) current = true;
        }
        if (!current) return {};
        for (const auto& value : days) {
            const auto item = value.GetObject();
            if (item.GetNamedString(L"fi", L"") != nextDate) continue;
            float low = 0, high = 0;
            if (!ChinaWeatherTemperature(std::wstring(item.GetNamedString(L"fd", L"")), low) ||
                !ChinaWeatherTemperature(std::wstring(item.GetNamedString(L"fc", L"")), high) || low > high) return {};
            const std::wstring code = item.GetNamedString(L"fa", L"").c_str();
            result.weatherCode = ChinaWeatherVisualCode(code);
            if (!result.weatherCode) return {};
            const wchar_t* descriptions[] = {L"晴", L"多云", L"阴", L"阵雨", L"雷阵雨", L"雷阵雨伴冰雹",
                L"雨夹雪", L"小雨", L"中雨", L"大雨", L"暴雨", L"大暴雨", L"特大暴雨",
                L"阵雪", L"小雪", L"中雪", L"大雪", L"暴雪", L"雾", L"冻雨", L"沙尘暴",
                L"小到中雨", L"中到大雨", L"大到暴雨", L"暴雨到大暴雨", L"大暴雨到特大暴雨",
                L"小到中雪", L"中到大雪", L"大到暴雪", L"浮尘", L"扬沙", L"强沙尘暴"};
            const int index = _wtoi(code.c_str());
            const std::wstring description = index >= 0 && index < 32 ? descriptions[index] : L"霾";
            if (fahrenheit) { low = low * 1.8f + 32; high = high * 1.8f + 32; }
            wchar_t range[64]{};
            swprintf_s(range, L"%.0f°～%.0f°%s", low, high, fahrenheit ? L"F" : L"C");
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
