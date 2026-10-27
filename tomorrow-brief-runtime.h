void LoadBriefLogos(const TomorrowBrief& brief) {
    for (size_t row = 0; row < brief.games.size(); ++row) {
        const std::wstring urls[] = {brief.games[row].awayLogoUrl,brief.games[row].homeLogoUrl};
        for (int side=0; side<2; ++side) {
            const size_t index=row*2+side;
            {
                std::lock_guard lock(g_nbaLogoMutex);
                if (g_briefLogos[index].url==urls[side] && !g_briefLogos[index].pixels.bgra.empty()) continue;
            }
            auto pixels = FetchNbaLogo(urls[side]);
            std::lock_guard lock(g_nbaLogoMutex);
            static uint64_t generation=0;
            pixels.generation=++generation;
            g_briefLogos[index]={urls[side],std::move(pixels)};
        }
    }
}

bool BriefMayAppear() {
    if (!g_hwnd || g_importantDaysOpen.load() || g_manuallyHidden.load() || g_isFullscreen.load() ||
        Wh_GetIntValue(L"PinnedExpanded", 0) || (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return false;
    LASTINPUTINFO input{sizeof(input)};
    if (!GetLastInputInfo(&input) || GetTickCount() - input.dwTime > 300000) return false;
    POINT cursor{}; RECT bounds{};
    if (GetCursorPos(&cursor) && GetWindowRect(g_hwnd, &bounds) && PtInRect(&bounds, cursor)) return false;
    // A locked or disconnected session must not consume the daily reminder.
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop) return false;
    wchar_t name[64]{}; DWORD bytes = 0;
    const bool interactive = GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &bytes) &&
        _wcsicmp(name, L"Default") == 0;
    CloseDesktop(desktop);
    return interactive;
}

DWORD WINAPI TomorrowBriefThreadProc(void*) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) return 0;
    Wh_SetIntValue(L"CalendarDaysThread",1);
    TomorrowBrief weather, nba, showing;
    double nextFetch = NowSeconds() + 20, nextShow = nextFetch;
    int targetDate = 0, weatherDate = 0, visibleSeconds = 0;
    Wh_SetIntValue(L"TomorrowBriefActive", 0);
    while (WaitForSingleObject(g_stopEvent, 1000) == WAIT_TIMEOUT) {
        SYSTEMTIME today{}; GetLocalTime(&today);
        const auto tomorrow = BriefTomorrow(today);
        const int target = BriefDateKey(tomorrow);
        const bool weatherTomorrow = BriefWeatherTomorrow(today);
        const int weatherTarget = BriefDateKey(weatherTomorrow ? tomorrow : today);
        auto settings = GetSettingsCopy();
        double now = NowSeconds();
        if (targetDate && target != targetDate) {weather = {}; nba = {}; nextFetch = now;}
        targetDate = target;
        if (weatherDate && weatherTarget != weatherDate) {weather = {}; nextFetch = now;}
        weatherDate = weatherTarget;
        const auto weatherKey = (weatherTomorrow ? L"" : L"today:") + std::to_wstring(weatherTarget) + L":" + settings.weatherCity;
        const auto nbaKey = std::to_wstring(target) + L":" + settings.nbaTeam;
        if (!settings.weather || (!weather.key.empty() && weather.key != weatherKey)) weather = {};
        if (!settings.nbaScores || (!nba.key.empty() && nba.key != nbaKey)) nba = {};
        wchar_t weatherSeen[1024]{}, nbaSeen[1024]{};
        Wh_GetStringValue(L"TomorrowWeatherShown", weatherSeen, ARRAYSIZE(weatherSeen));
        Wh_GetStringValue(L"TomorrowNbaShown", nbaSeen, ARRAYSIZE(nbaSeen));
        const std::wstring weatherShown = weatherSeen, nbaShown = nbaSeen;
        auto meal = MakeMealReminder(today, Wh_GetIntValue(L"MealLunchShown", 0), Wh_GetIntValue(L"MealDinnerShown", 0));
        meal.fetchedAt = now;
        TomorrowBrief important;
        std::vector<ImportantDay> days;
        const bool daysValid=ReadImportantDays(days);
        Wh_SetIntValue(L"CalendarDaysValid",daysValid);
        Wh_SetIntValue(L"CalendarDaysLoaded",static_cast<int>(days.size()));
        if (daysValid) {
            std::stable_sort(days.begin(),days.end(),[](const auto& a,const auto& b){return a.date<b.date;});
            for (const auto& day:days) {
                important=MakeImportantReminder(day,today,Wh_GetIntValue((L"ImportantDayShown:"+day.id).c_str(),0));
                if (!important.key.empty()) break;
            }
        }
        important.fetchedAt=now;
        if (now >= nextFetch && !showing.active && meal.key.empty() && important.key.empty()) {
            if (settings.weather && weatherShown != weatherKey) {
                const auto city = ChinaWeatherCityCode(settings.weatherCity);
                if (!city.empty()) {
                    weather = ParseTomorrowWeather(HttpGet(L"d1.weather.com.cn",
                        (L"/weather_index/" + city + L".html?_=" + std::to_wstring(GetTickCount64())).c_str()),
                        today, settings.weatherCity, settings.weatherFahrenheit);
                    weather.fetchedAt = NowSeconds();
                }
            }
            if (settings.nbaScores && nbaShown != nbaKey) {
                const auto day = GetNbaLocalDay(tomorrow);
                std::vector<NbaSnapshot> games;
                bool valid = day.start != 0 && day.end != 0;
                for (const auto& date : day.dates) {
                    if (!valid || WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) {valid = false; break;}
                    std::vector<NbaSnapshot> parsed;
                    const auto path = L"/apis/site/v2/sports/basketball/nba/scoreboard?dates=" + date;
                    valid = ParseNbaGames(HttpGet(L"site.api.espn.com", path.c_str(), true, nullptr), &parsed);
                    games.insert(games.end(), parsed.begin(), parsed.end());
                }
                nba = valid ? MakeTomorrowNba(games, today, settings.nbaTeam) : TomorrowBrief{};
                nba.fetchedAt = NowSeconds();
                LoadBriefLogos(nba);
                {
                    std::lock_guard lock(g_nbaLogoMutex);
                    int count=0;
                    for (const auto& logo : g_briefLogos) count += !logo.pixels.bgra.empty();
                    Wh_SetIntValue(L"TomorrowNbaLogosReady",count);
                }
            }
            now = NowSeconds();
            nextFetch = now + 1800;
            Wh_SetIntValue(L"TomorrowWeatherReady", weather.date == weatherTarget);
            Wh_SetIntValue(L"TomorrowNbaReady", nba.date == target);
            Wh_SetStringValue(L"TomorrowWeatherText", weather.body.c_str());
            Wh_SetStringValue(L"TomorrowNbaText", nba.body.c_str());
        }
        bool changed = false;
        {
            std::lock_guard lock(g_stateMutex);
            if (g_state.importantDays!=days) {
                g_state.importantDays=days;
                g_layoutDirty=true;
            }
            if (showing.active) {
                const auto kinds = ChooseActivities(g_state, settings, now);
                if (BriefMayAppear() && kinds[0] == IslandKind::TomorrowBrief) ++visibleSeconds;
                const bool enabled = showing.meal || showing.important || (showing.weatherCode ? settings.weather : settings.nbaScores);
                const bool dateValid = showing.important
                    ? showing.date==BriefDateKey(today) && std::any_of(days.begin(),days.end(),[&](const auto& day){return day.id==showing.key;})
                    : showing.meal ? showing.date == BriefDateKey(today) && MealReminderSlot(today) == showing.meal
                    : showing.date == (showing.weatherCode ? weatherTarget : target);
                if (visibleSeconds >= 10 || !enabled || !dateValid ||
                    now - showing.fetchedAt >= 3600) {
                    if (visibleSeconds >= 10) {
                        if (showing.important) Wh_SetIntValue((L"ImportantDayShown:"+showing.key).c_str(),showing.date);
                        else if (showing.meal) Wh_SetIntValue(showing.meal == 1 ? L"MealLunchShown" : L"MealDinnerShown", showing.date);
                        else Wh_SetStringValue(showing.weatherCode ? L"TomorrowWeatherShown" : L"TomorrowNbaShown", showing.key.c_str());
                    }
                    showing = {}; g_state.tomorrowBrief = {}; changed = true;
                    nextShow = now + 90;
                }
            } else if (now >= nextShow && BriefMayAppear()) {
                const auto kinds = ChooseActivities(g_state, settings, now);
                const auto first = kinds[0];
                if (first == IslandKind::Idle || first == IslandKind::Media || first == IslandKind::NBAScore ||
                    (first == IslandKind::Timer && !g_state.timer.justFinished)) {
                    if (!meal.key.empty()) showing = meal;
                    else if (!important.key.empty()) showing = important;
                    else if (BriefPending(weather, weatherTarget, now, weatherShown)) showing = weather;
                    else if (BriefPending(nba, target, now, nbaShown)) showing = nba;
                    if (!showing.key.empty()) {
                        showing.active = true; visibleSeconds = 0;
                        g_state.tomorrowBrief = showing; changed = true;
                    }
                }
            }
        }
        if (changed) {
            Wh_SetIntValue(L"TomorrowBriefActive", showing.active ? 1 : 0);
            g_layoutDirty = true;
            TriggerNudge();
        }
    }
    CoUninitialize();
    return 0;
}
