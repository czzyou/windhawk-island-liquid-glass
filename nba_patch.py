from pathlib import Path


def apply_nba(src):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:120], src.count(old))
        src = src.replace(old, new, 1)

    # ---- Parser (kept in its own header so it can be tested standalone) ----
    parser = (root / 'nba-parser.h').read_text(encoding='utf-8')
    parser = '\n'.join(line for line in parser.splitlines()
                       if not line.startswith('#pragma once') and line.strip() != '')
    change('struct Settings {', parser + '\n\nstruct Settings {')

    # ---- Settings ----------------------------------------------------------
    change('''  - TimerModule: true
    $name: Focus Timer module
    $description: Enables a Pomodoro-style focus/break timer, startable from the island's right-click menu.''',
           '''  - TimerModule: true
    $name: Focus Timer module
    $description: Enables a Pomodoro-style focus/break timer, startable from the island's right-click menu.
  - NBAScores: false
    $name: NBA live scores
    $description: ESPN 比分每10秒刷新，默认收起；主队没有直播时隐藏。右键菜单可查看本地今日赛程统计。
  - NBATeam: ""
    $name: 主队（按优先顺序）
    $description: 多支球队用逗号分隔，例如 CLE,PHI（骑士、76人）；只显示这些球队的实时比赛，同时比赛时按填写顺序选择。留空自动显示直播。''')

    change('''    bool timerEnabled = true;
    bool hideShowHotkeyEnabled = true;''',
           '''    bool timerEnabled = true;
    bool nbaScores = false;
    std::wstring nbaTeam;
    bool hideShowHotkeyEnabled = true;''')

    change('''    next.timerEnabled = Wh_GetIntSetting(L"Modules.TimerModule") != 0;''',
           '''    next.timerEnabled = Wh_GetIntSetting(L"Modules.TimerModule") != 0;
    next.nbaScores = Wh_GetIntSetting(L"Modules.NBAScores") != 0;
    next.nbaTeam = NormalizeNbaTeams(GetStringSettingCopy(L"Modules.NBATeam"));''')

    # ---- Shared state ------------------------------------------------------
    change('''    TimerSnapshot timer;
    DeviceSnapshot device;''', '''    TimerSnapshot timer;
    NbaSnapshot nba;
    std::wstring nbaSchedule = L"NBA：赛程待查询";
    DeviceSnapshot device;''')

    # ---- Activity kind -----------------------------------------------------
    change('''    Timer,
    DoNotDisturb,
    Split,''', '''    Timer,
    NBAScore,
    DoNotDisturb,
    Split,''')

    change('''        case IslandKind::Timer:
            activity.width = 190.0f;
            activity.height = 36.0f;
            break;''', '''        case IslandKind::Timer:
            activity.width = 190.0f;
            activity.height = 36.0f;
            break;
        case IslandKind::NBAScore:
            activity.width = 300.0f;
            activity.height = 36.0f;
            break;''')

    # Timer first, then the live game, then media. A transient alert still wins
    # because it is pushed earlier and ChooseActivities only shows two pills.
    change('''    if (settings.media && state.media.available) {
        activities.push_back(IslandKind::Media);
    }''', '''    if (settings.nbaScores && state.nba.active) {
        activities.push_back(Wh_GetIntValue(L"NBAShowClock", 0) ? IslandKind::Idle : IslandKind::NBAScore);
    }
    if (settings.media && state.media.available) {
        activities.push_back(IslandKind::Media);
    }''')

    logo_helper = (root / 'nba-logo.h').read_text(encoding='utf-8').replace('#pragma once', '')
    change('struct MediaSnapshot {', logo_helper + r'''

struct NbaLogoCache {
    std::wstring url;
    BitmapPixels pixels;
};
std::mutex g_nbaLogoMutex;
NbaLogoCache g_nbaLogos[2];

struct MediaSnapshot {''')
    change('    ComPtr<ID2D1Bitmap> fileTrayIconBitmap_;', '''    ComPtr<ID2D1Bitmap> nbaLogoBitmaps_[2];
    uint64_t nbaLogoGenerations_[2]{};
    ComPtr<ID2D1Bitmap> fileTrayIconBitmap_;''')
    change('        artBitmap_.Reset();', '''        for (auto& bitmap : nbaLogoBitmaps_) bitmap.Reset();
        artBitmap_.Reset();''')

    # ---- Polling thread ----------------------------------------------------
    change('''std::string HttpGet(const wchar_t* host, const wchar_t* path, bool https = true) {''',
           '''std::string HttpGet(const wchar_t* host, const wchar_t* path, bool https = true,
                    const wchar_t* referer = L"https://www.weather.com.cn/") {''')

    change('''            const wchar_t* headers = L"Referer: https://www.weather.com.cn/\\r\\nCache-Control: no-cache\\r\\n";''',
           '''            std::wstring headerText;
            if (referer && *referer) headerText = std::wstring(L"Referer: ") + referer + L"\\r\\n";
            headerText += L"Cache-Control: no-cache\\r\\n";
            const wchar_t* headers = headerText.c_str();''')

    change('''bool StartThreads() {''', '''void UpdateNbaLogos(const NbaSnapshot& nba) {
    const std::wstring urls[] = {nba.awayLogoUrl, nba.homeLogoUrl};
    for (int i = 0; i < 2; ++i) {
        {
            std::lock_guard lock(g_nbaLogoMutex);
            if (g_nbaLogos[i].url == urls[i] && !g_nbaLogos[i].pixels.bgra.empty()) continue;
        }
        BitmapPixels pixels;
        const std::wstring prefix = L"https://a.espncdn.com/";
        if (urls[i].starts_with(prefix)) {
            const std::wstring path = urls[i].substr(prefix.size() - 1);
            const std::string body = HttpGet(L"a.espncdn.com", path.c_str(), true, nullptr);
            BitmapPixels decoded;
            if (DecodeImageBytesToPixels(std::vector<uint8_t>(body.begin(), body.end()), &decoded)) {
                pixels = ResizeNbaLogo(decoded);
            }
        }
        {
            std::lock_guard lock(g_nbaLogoMutex);
            // A failed refresh must not replace an already decoded logo.
            if (!pixels.bgra.empty() || g_nbaLogos[i].url != urls[i]) {
                static uint64_t generation = 0;
                pixels.generation = ++generation;
                g_nbaLogos[i] = {urls[i], std::move(pixels)};
            }
        }
    }
    std::lock_guard lock(g_nbaLogoMutex);
    Wh_SetIntValue(L"NBALogosReady", !g_nbaLogos[0].pixels.bgra.empty() +
        static_cast<int>(!g_nbaLogos[1].pixels.bgra.empty()));
}

DWORD WINAPI NbaThreadProc(void*) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    WaitForSingleObject(g_stopEvent, 5000);
    Wh_SetIntValue(L"NBAActive", 0);
    Wh_SetStringValue(L"NBAStatus", L"thread started");
    NbaSnapshot last;
    std::wstring lastFavorites;
    bool failures = false;
    bool wasDemo = false;
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        const Settings settings = GetSettingsCopy();
        if (!settings.nbaScores) {
            Wh_SetStringValue(L"NBAStatus", L"disabled");
            bool had = false;
            {
                std::lock_guard lock(g_stateMutex);
                had = g_state.nba.active;
                if (had) g_state.nba = {};
            }
            if (had) g_layoutDirty = true;
            Wh_SetIntValue(L"NBAActive", 0);
            wasDemo = false;
            last = {};
            failures = false;
            WaitForSingleObject(g_stopEvent, 5000);
            continue;
        }

        // Hidden preview hook used by the verification script; it is not part of
        // the settings UI. Set LocalStorage NBADemo=1 to preview the layout
        // without waiting for a real game.
        if (Wh_GetIntValue(L"NBADemo", 0) != 0) {
            Wh_SetStringValue(L"NBAStatus", L"demo");
            NbaSnapshot demo;
            demo.active = true;
            demo.live = true;
            demo.awayAbbr = L"CLE";
            demo.homeAbbr = L"PHI";
            demo.awayName = L"Cavaliers";
            demo.homeName = L"76ers";
            demo.awayRecord = L"3-0";
            demo.homeRecord = L"2-1";
            demo.awayScore = 98;
            demo.homeScore = 102;
            demo.period = 4;
            demo.clock = L"5:12";
            demo.status = L"第4节 5:12";
            demo.matchup = L"CLE @ PHI";
            demo.gameUrl = L"https://www.espn.com/nba/";
            demo.awayLogoUrl = L"https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/cle.png";
            demo.homeLogoUrl = L"https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/phi.png";
            UpdateNbaLogos(demo);
            wasDemo = true;
            {
                std::lock_guard lock(g_stateMutex);
                g_state.nba = demo;
            }
            Wh_SetIntValue(L"NBAActive", 1);
            g_layoutDirty = true;
            WaitForSingleObject(g_stopEvent, 5000);
            continue;
        }

        if (wasDemo) {
            {
                std::lock_guard lock(g_stateMutex);
                g_state.nba = {};
            }
            Wh_SetIntValue(L"NBAActive", 0);
            g_layoutDirty = true;
            wasDemo = false;
        }
        if (settings.nbaTeam != lastFavorites) {
            last = {};
            lastFavorites = settings.nbaTeam;
        }
        SYSTEMTIME localNow{};
        GetLocalTime(&localNow);
        const NbaLocalDay day = GetNbaLocalDay(localNow);
        std::vector<NbaSnapshot> games;
        failures = day.start == 0 || day.end == 0;
        for (const auto& date : day.dates) {
            if (failures) break;
            const std::wstring path = L"/apis/site/v2/sports/basketball/nba/scoreboard?dates=" + date;
            const std::string body = HttpGet(L"site.api.espn.com", path.c_str(), true, nullptr);
            std::vector<NbaSnapshot> parsed;
            if (!ParseNbaGames(body, &parsed)) {
                failures = true;
                break;
            }
            for (auto& game : parsed) {
                if (std::none_of(games.begin(), games.end(), [&](const auto& other) {
                    return !game.gameId.empty() && other.gameId == game.gameId;
                })) games.push_back(std::move(game));
            }
        }
        const double now = NowSeconds();
        last = AdvanceNbaSnapshot(last, games, settings.nbaTeam, !failures, now);
        std::wstring schedule = L"NBA：赛程暂不可用（请求失败）";
        if (!failures) {
            const auto summary = SummarizeNbaDay(games, day.start, day.end, settings.nbaTeam);
            schedule = L"NBA " + day.label + L"：" + std::to_wstring(summary.total) + L"场 · " +
                std::to_wstring(summary.live) + L"进行中 · " + std::to_wstring(summary.scheduled) +
                L"未开始 · " + std::to_wstring(summary.finished) + L"已结束";
            Wh_SetIntValue(L"NBATodayGames", summary.total);
            Wh_SetIntValue(L"NBATodayLive", summary.live);
            Wh_SetIntValue(L"NBATodayFavorites", summary.favorites);
        }
        Wh_SetIntValue(L"NBAScheduleKnown", failures ? 0 : 1);
        Wh_SetStringValue(L"NBASchedule", schedule.c_str());
        Wh_SetStringValue(L"NBAFavorites", NbaFavoritesLabel(settings.nbaTeam).c_str());
        const std::wstring status = last.active
            ? (last.stale ? L"stale" : (last.live ? L"live" : L"final"))
            : (failures ? L"request failed" : L"no favorite live game");
        Wh_SetStringValue(L"NBAStatus", status.c_str());
        if (last.active) UpdateNbaLogos(last);
        {
            std::lock_guard lock(g_stateMutex);
            g_state.nba = last;
            g_state.nbaSchedule = schedule;
        }
        Wh_SetIntValue(L"NBAActive", last.active ? 1 : 0);
        g_layoutDirty = true;
        const DWORD waitMs = last.live || last.stale || failures ? 10000 : 60000;
        Wh_SetIntValue(L"NBAPollMs", waitMs);
        for (DWORD waited = 0; waited < waitMs; waited += 1000) {
            if (WaitForSingleObject(g_stopEvent, 1000) != WAIT_TIMEOUT ||
                Wh_GetIntValue(L"NBADemo", 0) != 0 ||
                GetSettingsCopy().nbaTeam != settings.nbaTeam) break;
        }
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return 0;
}

bool StartThreads() {''')

    change('''HANDLE g_weatherThread = nullptr;''', '''HANDLE g_weatherThread = nullptr;
HANDLE g_nbaThread = nullptr;''')

    change('''    g_weatherThread = CreateThread(nullptr, 0, WeatherThreadProc, nullptr, 0, nullptr);''',
           '''    g_weatherThread = CreateThread(nullptr, 0, WeatherThreadProc, nullptr, 0, nullptr);
    g_nbaThread = CreateThread(nullptr, 0, NbaThreadProc, nullptr, 0, nullptr);''')

    change('''    HANDLE handles[] = {g_renderThread, g_mediaThread, g_audioThread, g_weatherThread, g_keyboardThread, g_mouseThread, g_bluetoothThread};''',
           '''    HANDLE handles[] = {g_renderThread, g_mediaThread, g_audioThread, g_weatherThread, g_nbaThread, g_keyboardThread, g_mouseThread, g_bluetoothThread};''')

    change('''    g_weatherThread = nullptr;
''', '''    g_weatherThread = nullptr;
    g_nbaThread = nullptr;
''')

    change('    ComPtr<IDWriteTextFormat> timerHeroFormat_;',
           '    ComPtr<IDWriteTextFormat> timerHeroFormat_;\n    ComPtr<IDWriteTextFormat> nbaAuxFormat_;')
    change('        timerHeroFormat_ = nullptr;',
           '        timerHeroFormat_ = nullptr;\n        nbaAuxFormat_ = nullptr;')
    change('        if (timerHeroFormat_) {', '''        createFormat(smallFamily, defaultSmall,
                     DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 8.5f * ts, nbaAuxFormat_);
        if (nbaAuxFormat_) {
            nbaAuxFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            nbaAuxFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            nbaAuxFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (timerHeroFormat_) {''')
    change('        clockFormat_.Reset();', '        clockFormat_.Reset();\n        nbaAuxFormat_.Reset();')
    change('    HMENU menu = CreatePopupMenu();', '''    HMENU menu = CreatePopupMenu();
    {
        std::wstring schedule;
        {
            std::lock_guard lock(g_stateMutex);
            schedule = g_state.nbaSchedule;
        }
        const auto nbaSettings = GetSettingsCopy();
        if (nbaSettings.nbaScores) {
            AppendMenuW(menu, MF_STRING, 47, Wh_GetIntValue(L"NBAShowClock", 0)
                ? L"切换到 NBA 比分" : L"切换到时间");
            AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, schedule.c_str());
            const auto favorites = L"主队：" + NbaFavoritesLabel(nbaSettings.nbaTeam) + L"（优先顺序）";
            AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, favorites.c_str());
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        }
    }''')

    change('    switch (cmd) {', '''    switch (cmd) {
        case 47:
            Wh_SetIntValue(L"NBAShowClock", !Wh_GetIntValue(L"NBAShowClock", 0));
            g_idleTab = 0;
            g_layoutDirty = true;
            TriggerNudge();
            break;''')

    # ---- Drawing -----------------------------------------------------------
    change('''            case IslandKind::Timer:
                if (activity.width < 60.0f) DrawTimerOrb(state, unscaledRect);
                else DrawTimer(state, unscaledRect);
                break;''', '''            case IslandKind::Timer:
                if (activity.width < 60.0f) DrawTimerOrb(state, unscaledRect);
                else DrawTimer(state, unscaledRect);
                break;
            case IslandKind::NBAScore:
                DrawNba(state, unscaledRect, settings);
                break;''')

    change('''    void DrawTimer(const SharedState& state, D2D1_RECT_F rect) {''',
           '''    void DrawNba(const SharedState& state, D2D1_RECT_F rect, const Settings& settings) {
        const auto& nba = state.nba;
        const float width = rect.right - rect.left;
        const float height = rect.bottom - rect.top;
        if (width < 140.0f || height < 30.0f) return;

        const float expandedAlpha = Clamp((height - 60.0f) / 50.0f, 0.0f, 1.0f);
        const float compactAlpha = 1.0f - expandedAlpha;
        const float radius = ContentIslandRadius(height);
        ComPtr<ID2D1Geometry> mask = CreateIslandMaskGeometry(rect, radius, settings.notchStyle);
        ComPtr<ID2D1Layer> layer;
        target_->CreateLayer(&layer);
        const bool canLayer = mask && layer;

        auto pushContent = [&](float alpha) {
            if (canLayer) {
                target_->PushLayer(D2D1::LayerParameters(rect, mask.Get(),
                    D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), alpha, nullptr,
                    D2D1_LAYER_OPTIONS_NONE), layer.Get());
            } else {
                target_->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            }
        };
        auto popContent = [&]() {
            if (canLayer) target_->PopLayer();
            else target_->PopAxisAlignedClip();
        };

        const auto oldSmallAlign = smallTextFormat_ ? smallTextFormat_->GetTextAlignment()
                                                    : DWRITE_TEXT_ALIGNMENT_LEADING;
        const auto oldSmallPara = smallTextFormat_ ? smallTextFormat_->GetParagraphAlignment()
                                                   : DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
        const auto oldTextAlign = textFormat_ ? textFormat_->GetTextAlignment()
                                              : DWRITE_TEXT_ALIGNMENT_LEADING;
        const auto oldTextPara = textFormat_ ? textFormat_->GetParagraphAlignment()
                                             : DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
        const auto oldClockAlign = clockFormat_ ? clockFormat_->GetTextAlignment()
                                                : DWRITE_TEXT_ALIGNMENT_CENTER;
        const auto oldClockPara = clockFormat_ ? clockFormat_->GetParagraphAlignment()
                                               : DWRITE_PARAGRAPH_ALIGNMENT_CENTER;

        BitmapPixels logos[2];
        {
            std::lock_guard lock(g_nbaLogoMutex);
            if (g_nbaLogos[0].url == nba.awayLogoUrl) logos[0] = g_nbaLogos[0].pixels;
            if (g_nbaLogos[1].url == nba.homeLogoUrl) logos[1] = g_nbaLogos[1].pixels;
        }
        auto drawLogo = [&](int index, D2D1_RECT_F bounds) {
            DrawBitmapPixels(logos[index], bounds, nbaLogoBitmaps_[index],
                nbaLogoGenerations_[index], 0.95f);
        };
        const std::wstring status = nba.stale ? L"更新中断" : nba.finished ? L"已结束"
            : (nba.status.empty() ? L"进行中" : nba.status);
        const auto oldHeroAlign = timerHeroFormat_ ? timerHeroFormat_->GetTextAlignment() : DWRITE_TEXT_ALIGNMENT_CENTER;
        const auto oldHeroPara = timerHeroFormat_ ? timerHeroFormat_->GetParagraphAlignment() : DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
        wchar_t awayScore[16]{};
        wchar_t homeScore[16]{};
        swprintf_s(awayScore, L"%d", nba.awayScore);
        swprintf_s(homeScore, L"%d", nba.homeScore);

        if (compactAlpha > 0.01f) {
            pushContent(compactAlpha);
            textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            clockFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            drawLogo(0, D2D1::RectF(rect.left + 12, rect.top + 9, rect.left + 30, rect.top + 27));
            drawLogo(1, D2D1::RectF(rect.left + 202, rect.top + 9, rect.left + 220, rect.top + 27));
            textBrush_->SetOpacity(0.92f);
            target_->DrawTextW(nba.awayAbbr.c_str(), static_cast<UINT32>(nba.awayAbbr.size()),
                textFormat_.Get(), D2D1::RectF(rect.left + 34, rect.top + 3, rect.left + 70, rect.top + 33),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            clockFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            target_->DrawTextW(awayScore, static_cast<UINT32>(wcslen(awayScore)),
                clockFormat_.Get(), D2D1::RectF(rect.left + 70, rect.top + 3, rect.left + 110, rect.top + 33),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            clockFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            target_->DrawTextW(homeScore, static_cast<UINT32>(wcslen(homeScore)),
                clockFormat_.Get(), D2D1::RectF(rect.left + 124, rect.top + 3, rect.left + 164, rect.top + 33),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            mutedBrush_->SetOpacity(0.58f);
            target_->DrawTextW(L":", 1, textFormat_.Get(),
                D2D1::RectF(rect.left + 110, rect.top + 3, rect.left + 124, rect.top + 33),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            target_->DrawTextW(nba.homeAbbr.c_str(), static_cast<UINT32>(nba.homeAbbr.size()),
                textFormat_.Get(), D2D1::RectF(rect.left + 164, rect.top + 3, rect.left + 200, rect.top + 33),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            const std::wstring compactStatus = nba.stale ? L"更新中断" : nba.finished ? L"终场"
                : ((nba.period > 4 ? L"OT" : L"Q" + std::to_wstring(nba.period)) + L" " + nba.clock);
            mutedBrush_->SetOpacity(0.46f);
            target_->DrawTextW(compactStatus.c_str(), static_cast<UINT32>(compactStatus.size()),
                nbaAuxFormat_.Get(), D2D1::RectF(rect.left + 226, rect.top + 3, rect.right - 14, rect.top + 33),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            popContent();
        }

        if (expandedAlpha > 0.01f) {
            pushContent(expandedAlpha);
            const bool awayWins = nba.finished && nba.awayScore > nba.homeScore;
            const bool homeWins = nba.finished && nba.homeScore > nba.awayScore;

            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            mutedBrush_->SetOpacity(0.64f);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            const std::wstring header = L"NBA";
            target_->DrawTextW(header.c_str(), static_cast<UINT32>(header.size()),
                smallTextFormat_.Get(),
                D2D1::RectF(rect.left + 24, rect.top + 10, rect.right - 24, rect.top + 30),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            target_->DrawTextW(status.c_str(), static_cast<UINT32>(status.size()),
                smallTextFormat_.Get(),
                D2D1::RectF(rect.left + 100, rect.top + 10, rect.right - 24, rect.top + 30),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            drawLogo(0, D2D1::RectF(rect.left + 24, rect.top + 46, rect.left + 54, rect.top + 76));
            drawLogo(1, D2D1::RectF(rect.left + 24, rect.top + 94, rect.left + 54, rect.top + 124));
            auto drawTeamRow = [&](float rowTop, const std::wstring& abbr,
                                   const std::wstring& name, const std::wstring& record,
                                   const wchar_t* score, bool winner) {
                textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                textBrush_->SetOpacity(winner ? 1.0f : 0.86f);
                target_->DrawTextW(abbr.c_str(), static_cast<UINT32>(abbr.size()),
                    textFormat_.Get(),
                    D2D1::RectF(rect.left + 64, rowTop + 8, rect.left + 110, rowTop + 32),
                    textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

                smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                mutedBrush_->SetOpacity(0.72f);
                target_->DrawTextW(name.c_str(), static_cast<UINT32>(name.size()),
                    smallTextFormat_.Get(),
                    D2D1::RectF(rect.left + 114, rowTop + 8, rect.right - 145, rowTop + 32),
                    mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                mutedBrush_->SetOpacity(0.52f);
                target_->DrawTextW(record.c_str(), static_cast<UINT32>(record.size()),
                    smallTextFormat_.Get(),
                    D2D1::RectF(rect.right - 148, rowTop + 9, rect.right - 96, rowTop + 31),
                    mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

                IDWriteTextFormat* scoreFormat = timerHeroFormat_
                    ? timerHeroFormat_.Get() : clockFormat_.Get();
                scoreFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                scoreFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                textBrush_->SetOpacity(winner ? 1.0f : 0.90f);
                target_->DrawTextW(score, static_cast<UINT32>(wcslen(score)), scoreFormat,
                    D2D1::RectF(rect.right - 100, rowTop + 2, rect.right - 24, rowTop + 38),
                    textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            };

            drawTeamRow(rect.top + 42, nba.awayAbbr, nba.awayName, nba.awayRecord,
                        awayScore, awayWins);
            drawTeamRow(rect.top + 90, nba.homeAbbr, nba.homeName, nba.homeRecord,
                        homeScore, homeWins);

            ComPtr<ID2D1SolidColorBrush> divider;
            target_->CreateSolidColorBrush(
                WithAlpha(material_.hairline, material_.hairline.a * settingsOpacity_ * 0.9f),
                &divider);
            if (divider) {
                target_->FillRectangle(
                    D2D1::RectF(rect.left + 24, rect.top + 86, rect.right - 24, rect.top + 87),
                    divider.Get());
            }

            popContent();
        }

        if (smallTextFormat_) {
            smallTextFormat_->SetTextAlignment(oldSmallAlign);
            smallTextFormat_->SetParagraphAlignment(oldSmallPara);
        }
        if (textFormat_) {
            textFormat_->SetTextAlignment(oldTextAlign);
            textFormat_->SetParagraphAlignment(oldTextPara);
        }
        if (clockFormat_) {
            clockFormat_->SetTextAlignment(oldClockAlign);
            clockFormat_->SetParagraphAlignment(oldClockPara);
        }
        if (timerHeroFormat_) {
            timerHeroFormat_->SetTextAlignment(oldHeroAlign);
            timerHeroFormat_->SetParagraphAlignment(oldHeroPara);
        }
        textBrush_->SetOpacity(0.90f);
        mutedBrush_->SetOpacity(0.58f);
    }

    void DrawTimer(const SharedState& state, D2D1_RECT_F rect) {''')

    # ---- Expanded geometry -------------------------------------------------
    change('''        if (primary.kind == IslandKind::Media) {
            if (!isFullscreen && (isHoverExpanded || pinned || recentTrackChange)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;
            }
        }''', '''        if (primary.kind == IslandKind::Media) {
            if (!isFullscreen && (isHoverExpanded || pinned || recentTrackChange)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;
            }
        }
        if (primary.kind == IslandKind::NBAScore) {
            if (!isFullscreen && (isHoverExpanded || pinned)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = 150.0f * g_settings.sizeScale;
            }
        }''')

    # ---- Click opens the game ---------------------------------------------
    change('''                if (g_mediaCompanionExpanded.load() &&
                    MediaCompanionHitTest(MediaContentFromClient(xPos, yPos, true)) == 1) {''',
           '''                if (!kinds.empty() && kinds[0] == IslandKind::NBAScore) {
                    std::wstring url;
                    bool hasGame = false;
                    {
                        std::lock_guard lock(g_stateMutex);
                        url = g_state.nba.gameUrl;
                        hasGame = g_state.nba.active;
                    }
                    if (hasGame && !url.empty() &&
                        std::abs(xPos - s_touchStart.x) <= 8 &&
                        std::abs(yPos - s_touchStart.y) <= 8) {
                        ShellExecuteW(hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        g_layoutDirty = true;
                    }
                    return 0;
                }

                if (g_mediaCompanionExpanded.load() &&
                    MediaCompanionHitTest(MediaContentFromClient(xPos, yPos, true)) == 1) {''')

    # Media transport and scrubber must only react when media really owns the
    # primary pill; an NBA/timer pill must not forward clicks to hidden media.
    change('''                {
                    std::lock_guard lock(g_stateMutex);
                    mediaActive = g_settings.media && g_state.media.available;
                    kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                }''', '''                {
                    std::lock_guard lock(g_stateMutex);
                    kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                    mediaActive = g_settings.media && g_state.media.available &&
                        !kinds.empty() && kinds[0] == IslandKind::Media;
                }''')

    return src
