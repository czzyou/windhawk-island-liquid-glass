from pathlib import Path


def apply_weather_glass_fx(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    root = Path(__file__).parent

    # 1. Load persisted weather glass FX tuning parameters on settings reload.
    change(
        '''void LoadSettings() {
    Settings next;''',
        '''void LoadSettings() {
    g_weatherFxOverride.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxMode", 0), -1, 6), std::memory_order_relaxed);
    g_weatherFxScope.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxScope", 0), 0, 2), std::memory_order_relaxed);
    g_weatherFxIntensityPct.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxIntensity", 100), 0, 150), std::memory_order_relaxed);
    g_weatherFxSpeedPct.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxSpeed", 100), 20, 250), std::memory_order_relaxed);
    g_weatherFxScalePct.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxScale", 100), 50, 200), std::memory_order_relaxed);
    g_weatherFxFogPct.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxFog", 75), 0, 100), std::memory_order_relaxed);
    g_weatherFxRefractPct.store(ClampInt(Wh_GetIntValue(L"WeatherGlassFxRefract", 100), 0, 200), std::memory_order_relaxed);
    Settings next;''',
    )

    # 2. Embed live Weather Glass FX parameter tuner dialog.
    tuner_ui = (root / 'weather-glass-fx-ui.h').read_text(encoding='utf-8')
    change(
        'void ShowContextMenu(HWND hwnd, POINT screenPoint) {',
        tuner_ui + '\nvoid ShowContextMenu(HWND hwnd, POINT screenPoint) {',
    )

    # 3. Reorganize context menu into grouped sections and add Weather glass FX submenu.
    has_updater = 'g_updateAvailable.load()' in src
    has_nba_setup = 'void ShowNbaSetup(' in src
    update_line = (
        '    AppendMenuW(menu, MF_STRING, 49, g_updateAvailable.load() ? '
        'UiText(L"Install available update…", L"安装可用更新…") : '
        'UiText(L"Check for updates…", L"检查更新…"));\n'
        if has_updater else ''
    )
    nba_fav_item = (
        '        AppendMenuW(nbaMenu, MF_STRING, 48, UiText(L"NBA favorites…", L"NBA 主队设置…"));\n'
        if has_nba_setup else ''
    )
    nba_else_block = (
        '''    } else {
        AppendMenuW(menu, MF_STRING, 48, UiText(L"NBA favorites…", L"NBA 主队设置…"));
    }'''
        if has_nba_setup else '    }'
    )

    menu_start = src.index('void ShowContextMenu(HWND hwnd, POINT screenPoint) {')
    menu_build_start = src.index('    HMENU menu = CreatePopupMenu();', menu_start)
    menu_build_end = src.index('    SetForegroundWindow(hwnd);', menu_build_start)
    new_menu_build = f'''    HMENU menu = CreatePopupMenu();
    const auto menuSettings = GetSettingsCopy();

    // ── Section 1: Core Modules (NBA / Media & Lyrics / Weather FX) ──
    if (menuSettings.nbaScores) {{
        std::wstring schedule;
        {{
            std::lock_guard lock(g_stateMutex);
            schedule = g_state.nbaSchedule;
            if (schedule == L"NBA：赛程待查询") schedule = UiText(L"NBA: schedule pending", L"NBA：赛程待查询");
        }}
        HMENU nbaMenu = CreatePopupMenu();
        AppendMenuW(nbaMenu, MF_STRING, 47, Wh_GetIntValue(L"NBAShowClock", 0)
            ? UiText(L"Show NBA scores", L"切换到 NBA 比分") : UiText(L"Show clock", L"切换到时间"));
{nba_fav_item}        AppendMenuW(nbaMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(nbaMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, schedule.c_str());
        const auto favorites = UiText(L"Favorites: ", L"主队：") + NbaFavoritesLabel(menuSettings.nbaTeam);
        AppendMenuW(nbaMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, favorites.c_str());
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(nbaMenu), UiText(L"NBA Scores", L"NBA 比分"));
{nba_else_block}

    std::vector<MediaSourceInfo> sourceMenuItems;
    {{
        HMENU mediaMenu = CreatePopupMenu();
        sourceMenuItems = AppendMediaSources(mediaMenu);
        AppendMenuW(mediaMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(mediaMenu, MF_STRING | (menuSettings.collapsedLyrics ? MF_CHECKED : 0), 2001,
                    UiText(L"Show lyrics when collapsed", L"收起时显示歌词"));
        AppendMenuW(mediaMenu, MF_STRING | (menuSettings.lyricsPinnedFirst ? MF_CHECKED : 0), 2002,
                    UiText(L"Pin lyrics page first", L"歌词页置顶"));
        HMENU syncMenu = CreatePopupMenu();
        const bool bt = g_defaultOutputIsBluetooth.load();
        const auto offsetLabel = std::wstring(bt ? UiText(L"Bluetooth offset: ", L"蓝牙偏移：")
                                                 : UiText(L"Speaker / wired offset: ", L"扬声器／有线偏移："))
            + std::to_wstring(bt ? g_lyricsEffectiveOffsetBtMs.load() : g_lyricsEffectiveOffsetMs.load()) + L" ms";
        AppendMenuW(syncMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, offsetLabel.c_str());
        AppendMenuW(syncMenu, MF_SEPARATOR, 0, nullptr);
        const wchar_t* steps[] = {{
            UiText(L"10 ms earlier", L"提前 10 ms"), UiText(L"50 ms earlier", L"提前 50 ms"),
            UiText(L"250 ms earlier", L"提前 250 ms"), UiText(L"10 ms later", L"延后 10 ms"),
            UiText(L"50 ms later", L"延后 50 ms"), UiText(L"250 ms later", L"延后 250 ms"),
            UiText(L"Reset current device adjustment", L"重置当前设备微调")
        }};
        for (int i = 0; i < 7; ++i) AppendMenuW(syncMenu, MF_STRING, 2010 + i, steps[i]);
        AppendMenuW(mediaMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(syncMenu), UiText(L"Lyrics sync", L"歌词同步"));
        AppendMenuW(mediaMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(mediaMenu, MF_STRING, 2004, UiText(L"Clear lyrics cache", L"清除歌词缓存"));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(mediaMenu), UiText(L"Media & Lyrics", L"媒体与歌词"));
    }}

    {{
        const int fxMode = g_weatherFxOverride.load(std::memory_order_relaxed);
        HMENU weatherFxMenu = CreatePopupMenu();
        AppendMenuW(weatherFxMenu, MF_STRING, 2048, UiText(L"Customize parameters…", L"自定义参数调节…"));
        AppendMenuW(weatherFxMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 0 ? MF_CHECKED : 0), 2040, UiText(L"Follow live weather", L"跟随实时天气"));
        AppendMenuW(weatherFxMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 1 ? MF_CHECKED : 0), 2041, UiText(L"Preview: Rain (Glass Drops)", L"预览：下雨（玻璃水珠）"));
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 2 ? MF_CHECKED : 0), 2042, UiText(L"Preview: Thunderstorm", L"预览：雷暴闪电"));
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 3 ? MF_CHECKED : 0), 2043, UiText(L"Preview: Snow", L"预览：下雪"));
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 4 ? MF_CHECKED : 0), 2044, UiText(L"Preview: Cloudy / Fog", L"预览：多云 / 薄雾"));
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 5 ? MF_CHECKED : 0), 2045, UiText(L"Preview: Clear Day (Sun Glow)", L"预览：晴天光晕"));
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode == 6 ? MF_CHECKED : 0), 2046, UiText(L"Preview: Clear Night (Stars)", L"预览：晴夜星空"));
        AppendMenuW(weatherFxMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(weatherFxMenu, MF_STRING | (fxMode < 0 ? MF_CHECKED : 0), 2047, UiText(L"Off", L"关闭天气动效"));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(weatherFxMenu), UiText(L"Weather glass FX", L"天气动效预览"));
    }}

    // ── Section 2: Productivity Tools ──
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    {{
        HMENU timerMenu = CreatePopupMenu();
        AppendMenuW(timerMenu, MF_STRING, 30, UiText(L"Start 25 min Focus", L"开始 25 分钟专注"));
        AppendMenuW(timerMenu, MF_STRING, 31, UiText(L"Start 50 min Focus", L"开始 50 分钟专注"));
        AppendMenuW(timerMenu, MF_STRING, 32, UiText(L"Start 5 min Break", L"开始 5 分钟休息"));
        if (timerActive) {{
            AppendMenuW(timerMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(timerMenu, MF_STRING, 33, timerRunning ? UiText(L"Pause Timer", L"暂停计时") : UiText(L"Resume Timer", L"继续计时"));
            AppendMenuW(timerMenu, MF_STRING, 34, UiText(L"Stop Timer", L"结束计时"));
        }}
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(timerMenu), UiText(L"Focus Timer", L"专注计时器"));
    }}
    if (menuSettings.quickLookup) {{
        AppendMenuW(menu, MF_STRING, 2020, UiText(L"Quick lookup…", L"快捷查词…"));
    }}
    AppendMenuW(menu, MF_STRING, 46, UiText(L"Important dates…", L"重要日子…"));
    if (menuSettings.fileTrayModule) {{
        size_t trayCount = 0;
        {{
            std::lock_guard lock(g_stateMutex);
            trayCount = g_state.fileTrayItems.size();
        }}
        if (trayCount > 0) {{
            std::wstring label = std::wstring(Loc(L"File Tray")) + L" · " + UiText(L"Clear ", L"清空 ") + std::to_wstring(trayCount);
            AppendMenuW(menu, MF_STRING, 41, label.c_str());
        }}
    }}

    // ── Section 3: Appearance & Behavior Toggles ──
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    {{
        HMENU displayMenu = CreatePopupMenu();
        const bool pinnedExpanded = Wh_GetIntValue(L"PinnedExpanded", 0) != 0;
        const bool gamePinned = Wh_GetIntValue(L"GameOverlayPinned", 0) != 0;
        const int activeExpandOnHover = Wh_GetIntValue(L"ExpandOnHoverOverride", -1) >= 0
                              ? Wh_GetIntValue(L"ExpandOnHoverOverride", 0)
                              : (Wh_GetIntSetting(L"Behavior.ExpandOnHover") != 0);
        AppendMenuW(displayMenu, MF_STRING | (pinnedExpanded ? MF_CHECKED : 0), 2, UiText(L"Pin expanded", L"固定展开"));
        AppendMenuW(displayMenu, MF_STRING | (activeExpandOnHover ? MF_CHECKED : 0), 11, UiText(L"Expand on Hover", L"悬停自动展开"));
        AppendMenuW(displayMenu, MF_STRING | (gamePinned ? MF_CHECKED : 0), 3, UiText(L"Game overlay", L"游戏性能悬浮窗"));
        AppendMenuW(displayMenu, MF_STRING | (menuSettings.ambientAuroraGlow ? MF_CHECKED : 0), 2030, UiText(L"Album ambient glow", L"专辑氛围背光"));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(displayMenu), UiText(L"Display & Interaction", L"显示与交互"));
    }}

    // ── Section 4: System & Dismiss ──
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 9, UiText(L"Open Windhawk settings", L"打开 Windhawk 设置"));
{update_line}    AppendMenuW(menu, MF_STRING, 1, UiText(L"Dismiss", L"关闭当前状态"));

'''
    src = src[:menu_build_start] + new_menu_build + src[menu_build_end:]

    change(
        '''        case 30:
            StartFocusTimer(25, false);''',
        '''        case 2040: case 2041: case 2042: case 2043:
        case 2044: case 2045: case 2046: case 2047: {
            const int newMode = (cmd == 2047) ? -1 : static_cast<int>(cmd - 2040);
            Wh_SetIntValue(L"WeatherGlassFxMode", newMode);
            g_weatherFxOverride.store(newMode, std::memory_order_relaxed);
            g_layoutDirty = true;
            TriggerNudge();
            break;
        }
        case 2048:
            ShowWeatherFxSetup(hwnd);
            break;
        case 30:
            StartFocusTimer(25, false);''',
    )

    # 4. Sync live weather condition & display scope into the native glass renderer.
    change(
        '''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);''',
        '''        if (!activity.companion) {
            const int fxScope = g_weatherFxScope.load(std::memory_order_relaxed);
            const bool islandExpanded = (rect.bottom - rect.top) > 62.0f * settings.sizeScale;
            g_weatherFxIsExpanded.store(islandExpanded, std::memory_order_relaxed);
            const bool scopeOk = (fxScope == 0)
                || (fxScope == 1 && islandExpanded)
                || (fxScope == 2 && islandExpanded && CurrentDashboardTab(settings) == 2);
            g_weatherFxScopeAllowed.store(scopeOk, std::memory_order_relaxed);

            int liveWeatherFx = 0;
            if (state.weather.hasData && state.weather.weatherCode != 0) {
                const bool isNight = ChinaWeatherNight(ChinaWeatherClock().wHour);
                switch (WeatherVisualFromCode(state.weather.weatherCode)) {
                    case WeatherVisual::Rain: liveWeatherFx = 1; break;
                    case WeatherVisual::Storm: liveWeatherFx = 2; break;
                    case WeatherVisual::Snow: liveWeatherFx = 3; break;
                    case WeatherVisual::PartlyCloudy:
                    case WeatherVisual::Cloudy:
                    case WeatherVisual::Fog: liveWeatherFx = 4; break;
                    case WeatherVisual::Clear: liveWeatherFx = isNight ? 6 : 5; break;
                    default: liveWeatherFx = 0; break;
                }
            }
            g_weatherFxLiveMode.store(liveWeatherFx, std::memory_order_relaxed);
        }
        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);''',
    )

    # 5. Apple Weather-style colorful daily temperature range bar on the Weather tab.
    change(
        '''        if (hasWeather && state.weather.hasDailyRange) {
            wchar_t range[64]{};
            swprintf_s(range, UiText(L"Today %s°–%s°", L"今日 %s°～%s°"), WeatherTemperatureText(state.weather.lowTemperature).c_str(),
                WeatherTemperatureText(state.weather.highTemperature).c_str());
            desc = std::wstring(range) + L"\\n" + WeatherCurrentDescriptionForUi(state.weather);
        }
        if (page > 0) {
            desc = hasForecast ? WeatherTextForUi(forecast.description) + L"\\n" + WeatherTemperatureText(forecast.low) + L"°～" +
                WeatherTemperatureText(forecast.high) + L"°" : std::wstring(UiText(L"Forecast unavailable", L"暂无预报"));
        }''',
        '''        bool hasTempRange = false;
        float rangeLow = 0.0f, rangeHigh = 0.0f, rangeCur = NAN;
        if (page == 0 && hasWeather && state.weather.hasDailyRange) {
            hasTempRange = true;
            rangeLow = state.weather.lowTemperature;
            rangeHigh = state.weather.highTemperature;
            rangeCur = state.weather.temperature;
            desc = WeatherCurrentDescriptionForUi(state.weather);
        }
        if (page > 0) {
            if (hasForecast) {
                hasTempRange = true;
                rangeLow = forecast.low;
                rangeHigh = forecast.high;
                desc = WeatherTextForUi(forecast.description);
            } else {
                desc = std::wstring(UiText(L"Forecast unavailable", L"暂无预报"));
            }
        }''',
    )
    change(
        '''        const float descTop = rect.top + 120.0f * scale;
        const float descBottom = rect.top + 162.0f * scale;''',
        '''        const float descTop = rect.top + 116.0f * scale;
        const float descBottom = hasTempRange ? (rect.top + 138.0f * scale) : (rect.top + 162.0f * scale);''',
    )
    change(
        '''        textBrush_->SetOpacity(0.82f);
        target_->DrawTextW(desc.c_str(), static_cast<UINT32>(desc.length()),
                           weatherDescFormat_ ? weatherDescFormat_.Get() : textFormat_.Get(),
                           D2D1::RectF(heroLeft, descTop, heroRight, descBottom),
                           textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);''',
        '''        textBrush_->SetOpacity(0.86f);
        target_->DrawTextW(desc.c_str(), static_cast<UINT32>(desc.length()),
                           weatherDescFormat_ ? weatherDescFormat_.Get() : textFormat_.Get(),
                           D2D1::RectF(heroLeft, descTop, heroRight, descBottom),
                           textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);

        // Apple Weather style colorful daily temperature range bar + live temperature dot
        if (hasTempRange && smallTextFormat_) {
            const float barRowTop = rect.top + 140.0f * scale;
            const float barRowBot = rect.top + 158.0f * scale;
            const float barCenterY = (barRowTop + barRowBot) * 0.5f;
            const float labelW = 28.0f * scale;
            const std::wstring lowTxt = WeatherTemperatureText(rangeLow) + L"°";
            const std::wstring highTxt = WeatherTemperatureText(rangeHigh) + L"°";

            smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            mutedBrush_->SetOpacity(0.76f);
            target_->DrawTextW(lowTxt.c_str(), static_cast<UINT32>(lowTxt.size()), smallTextFormat_.Get(),
                               D2D1::RectF(heroLeft, barRowTop, heroLeft + labelW, barRowBot),
                               mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);

            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
            textBrush_->SetOpacity(0.95f);
            target_->DrawTextW(highTxt.c_str(), static_cast<UINT32>(highTxt.size()), smallTextFormat_.Get(),
                               D2D1::RectF(heroRight - labelW, barRowTop, heroRight, barRowBot),
                               textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

            const float trackL = heroLeft + labelW + 3.0f * scale;
            const float trackR = heroRight - labelW - 3.0f * scale;
            const float trackH = 5.0f * scale;
            const float trackRds = trackH * 0.5f;
            if (trackR > trackL + 10.0f * scale) {
                const D2D1_RECT_F trackRect = D2D1::RectF(trackL, barCenterY - trackRds, trackR, barCenterY + trackRds);
                D2D1_GRADIENT_STOP tStops[4] = {
                    { 0.00f, D2D1::ColorF(0x38BDF8, 0.92f * settingsOpacity_) },
                    { 0.36f, D2D1::ColorF(0x34D399, 0.92f * settingsOpacity_) },
                    { 0.72f, D2D1::ColorF(0xFACC15, 0.95f * settingsOpacity_) },
                    { 1.00f, D2D1::ColorF(0xFB923C, 0.96f * settingsOpacity_) }
                };
                ComPtr<ID2D1GradientStopCollection> tColl;
                ComPtr<ID2D1LinearGradientBrush> tBrush;
                if (SUCCEEDED(target_->CreateGradientStopCollection(tStops, 4, &tColl)) && tColl &&
                    SUCCEEDED(target_->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(trackL, barCenterY), D2D1::Point2F(trackR, barCenterY)),
                        tColl.Get(), &tBrush)) && tBrush) {
                    target_->FillRoundedRectangle(D2D1::RoundedRect(trackRect, trackRds, trackRds), tBrush.Get());
                }
                if (!std::isnan(rangeCur)) {
                    const float span = rangeHigh - rangeLow;
                    const float frac = (span > 0.1f) ? Clamp((rangeCur - rangeLow) / span, 0.0f, 1.0f) : 0.5f;
                    const float dotX = trackL + trackRds + frac * std::max(0.0f, (trackR - trackL) - trackH);
                    ComPtr<ID2D1SolidColorBrush> ringBrush, dotBrush;
                    target_->CreateSolidColorBrush(D2D1::ColorF(0x0F172A, 0.55f * settingsOpacity_), &ringBrush);
                    target_->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF, 0.98f * settingsOpacity_), &dotBrush);
                    if (ringBrush) target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, barCenterY), 4.2f * scale, 4.2f * scale), ringBrush.Get());
                    if (dotBrush) target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, barCenterY), 2.8f * scale, 2.8f * scale), dotBrush.Get());
                }
            }
        }''',
    )

    # 6. Fix Timer <-> Idle(GameOverlay) oscillation when both timer and game overlay are active.
    change(
        '''        activities.push_back(g_timerDashboardExpanded.load() || Wh_GetIntValue(L"PinnedExpanded", 0) != 0
            ? IslandKind::Idle : IslandKind::Timer);''',
        '''        // Prevent 60Hz Timer <-> Idle(GameOverlay) ping-pong oscillation when both
        // a focus timer and the game performance overlay are active simultaneously.
        const bool gameOverlayOn = settings.gameOverlay || Wh_GetIntValue(L"GameOverlayPinned", 0) != 0;
        activities.push_back((!gameOverlayOn && (g_timerDashboardExpanded.load() || Wh_GetIntValue(L"PinnedExpanded", 0) != 0))
            ? IslandKind::Idle : IslandKind::Timer);''',
    )

    # 7. Freeze hover state & defer topmost reassertion while the right-click context menu is open.
    change(
        '''        bool hover = false;
        if (widthSpring.value > 1.0f && heightSpring.value > 1.0f) {
            const float topPad = (g_settings.notchStyle || g_settings.borderMergedMode) ? 0.0f : kRenderPadY;
            RECT pillRect = {
                windowRect.left + static_cast<int>(std::round(kRenderPadX)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value)),
                windowRect.left + static_cast<int>(std::round(kRenderPadX + widthSpring.value)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value + heightSpring.value))
            };
            hover = PtInRect(&pillRect, cursor) != FALSE;
        } else if (g_settings.unhideOnHover) {
            RECT dockRect = GetIslandDockRect();
            hover = PtInRect(&dockRect, cursor) != FALSE;
        }''',
        '''        static bool s_lastStableHover = false;
        bool hover = false;
        if (g_contextMenuOpen.load(std::memory_order_relaxed)) {
            // Freeze hover state while right-click menu is open so the island never expands/collapses mid-menu
            hover = s_lastStableHover;
        } else if (widthSpring.value > 1.0f && heightSpring.value > 1.0f) {
            const float topPad = (g_settings.notchStyle || g_settings.borderMergedMode) ? 0.0f : kRenderPadY;
            RECT pillRect = {
                windowRect.left + static_cast<int>(std::round(kRenderPadX)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value)),
                windowRect.left + static_cast<int>(std::round(kRenderPadX + widthSpring.value)),
                windowRect.top + static_cast<int>(std::round(topPad + nudgeSpring.value + heightSpring.value))
            };
            hover = PtInRect(&pillRect, cursor) != FALSE;
            s_lastStableHover = hover;
        } else if (g_settings.unhideOnHover) {
            RECT dockRect = GetIslandDockRect();
            hover = PtInRect(&dockRect, cursor) != FALSE;
            s_lastStableHover = hover;
        } else {
            s_lastStableHover = false;
        }''',
    )
    change(
        '''        if (!isFullscreen && !g_manuallyHidden.load() && !g_autoHiddenParked.load() &&
            now - lastTopmostCheck > 1.0) {''',
        '''        if (!isFullscreen && !g_manuallyHidden.load() && !g_autoHiddenParked.load() &&
            !g_contextMenuOpen.load(std::memory_order_relaxed) &&
            now - lastTopmostCheck > 1.0) {''',
    )
    return src
