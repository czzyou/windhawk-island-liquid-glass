from pathlib import Path


def apply_release(src, version, author):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    change('// @id              dynamic-island-for-windows-yasb', '// @id              dynamic-island-liquid-glass')
    change('// @name            Dynamic Island for Windows - YASB integration', '// @name            灵动岛 · 液态玻璃')
    change('// @version         1.3.1-local.60', '// @version         ' + version)
    change('// @author          Himanshu', '// @author          ' + author)
    change('// @github          https://github.com/devcode90', '// @github          https://github.com/' + author)
    change('-ld3dcompiler -ldxguid\n', '-ld3dcompiler -ldxguid -lcomctl32 -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008 -D__USE_MINGW_ANSI_STDIO=0\n')
    start = src.index('// ==WindhawkModReadme==')
    end = src.index('// ==/WindhawkModReadme==', start)
    src = src[:start] + '''// ==WindhawkModReadme==
/*
# 灵动岛 · 液态玻璃

默认启用液态玻璃，包含媒体控制、日历、天气、硬件信息、时钟、计时器和 NBA 实时比分。

- 天气：留空城市时，向中国天气网查询公网 IP 所在城市；不需要密钥。
  代理或 VPN 可能改变定位结果。可以在设置中填写九位中国天气网城市编号覆盖自动定位。
  数据源覆盖中国天气网支持的地区，定位或网络失败时不会假装显示沈阳天气。
- NBA：首次启动选择主队；支持全部 30 支球队、多主队及优先顺序。
  暂不开启后不再弹窗；右键灵动岛 → NBA 主队设置，可随时重新选择。
  比分来自 ESPN，无直播时回到普通时钟。
- 重要日子：右键添加自己的生日、考试等日程；新安装没有预置个人日程。
- 吃饭提醒：默认关闭，可在设置中开启并修改午饭、晚饭时间。
- 液态玻璃默认参数已内置。没有 SF Pro Text 字体时使用系统字体回退，无需另装字体。

安装：在 Windhawk 中新建模组，用此文件的完整内容替换模板，点击编译并启用。
使用本模组前请关闭其他灵动岛模组，避免两个浮窗重叠。
本文件包含所需实现，可独立编译；不需要 Python、YASB、额外文件或个人配置备份。

基于 Himanshu 的 Dynamic Island for Windows（MIT）；液态玻璃后端来自
hicccc77/electron-liquid-glass（MIT，完整许可保留在源码中）。
*/
''' + src[end:]

    defaults = {
        'ThemePreset': 'custom', 'PillOpacity': '100', 'LiquidGlass': 'true',
        'TintIntensity': '0', 'BackdropFillOpacity': '15', 'BackdropTint': '14',
        'MaterialDepth': 'false', 'DropShadow': 'false', 'AccentBloom': '0',
        'ClockAccentGlow': 'false', 'FontFamily': '"SF Pro Text"',
        'ContourBorderHex': '"#E0E0E069"', 'PillBgColor': '"#10101024"',
        'TextPrimaryColor': '"#F2F2F7"', 'TextSecondaryColor': '"#D1D1D6"',
        'NBAScores': 'true',
    }
    import re
    for key, value in defaults.items():
        src, count = re.subn(r'(?m)^(  - ' + key + r': )[^\n]+$', lambda m: m[1] + value, src)
        assert count == 1, (key, count)
    change('''  - NBATeam: ""
    $name: 主队（按优先顺序）
    $description: 多支球队用逗号分隔，例如 CLE,PHI（骑士、76人）；只显示这些球队的实时比赛，同时比赛时按填写顺序选择。留空自动显示直播。
''', '')
    change('    $description: ESPN 比分每10秒刷新，默认收起；主队没有直播时隐藏。右键菜单可查看本地今日赛程统计。',
           '    $description: 首次启动选择主队，之后可从右键菜单修改。此项是 NBA 总开关；无直播时显示普通时钟。')
    change('    $name: Weather City (Optional)', '    $name: 天气城市（留空按公网 IP 定位）')
    change("    $description: '中国天气网城市：沈阳，或九位城市编号（沈阳 101070101）。其他城市请填写编号。' ",
           "    $description: '留空自动定位；代理或 VPN 可能影响结果。手动覆盖可填九位中国天气网城市编号，例如北京 101010100。'")
    change('    if (city.empty() || city == L"沈阳"', '    if (city == L"沈阳"')
    change('    // shortcut: name lookup covers Shenyang; use a 9-digit weather.com.cn code for other cities.',
           '    // A blank setting is resolved by the IP lookup, never a fixed city.')
    change('DWORD WINAPI WeatherThreadProc(void*) {', (root/'release-weather.h').read_text(encoding='utf-8') + '\nDWORD WINAPI WeatherThreadProc(void*) {')
    change('    size_t failures = 0;', '    double nextIPLookup = 0;\n    size_t failures = 0;')
    change('''        const auto code = ChinaWeatherCityCode(settings.weatherCity);''', '''        if (settings.weatherCity.empty() && NowSeconds() >= nextIPLookup) {
            const auto detected = ParseWeatherIPCity(HttpGet(L"wgeo.weather.com.cn",
                (L"/ip/?_=" + std::to_wstring(GetTickCount64())).c_str()));
            if (!detected.empty()) {
                std::lock_guard lock(g_weatherIPMutex);
                g_weatherIPCity = detected;
            }
            nextIPLookup = NowSeconds() + (detected.empty() ? 300 : 21600);
        }
        const auto code = ResolvedWeatherCity(settings.weatherCity);''')
    change('        const auto weatherKey = std::to_wstring(target) + L":" + settings.weatherCity;',
           '        const auto weatherCity = ResolvedWeatherCity(settings.weatherCity);\n        const auto weatherKey = std::to_wstring(target) + L":" + weatherCity;')
    change('                const auto city = ChinaWeatherCityCode(settings.weatherCity);', '                const auto city = weatherCity;')
    change('                        today, settings.weatherCity, settings.weatherFahrenheit);', '''                        today, weatherCity, settings.weatherFahrenheit);
                    std::wstring cityName;
                    { std::lock_guard lock(g_stateMutex); cityName = g_state.weather.city; }
                    weather.title = L"明天天气 · " + (cityName.empty() ? weatherCity : cityName);''')
    change('(city.empty() ? std::wstring(L"沈阳") : city)', '(city.empty() ? std::wstring(L"自动定位") : city)')

    change('inline std::wstring NormalizeNbaTeams(std::wstring value) {', (root/'nba-teams.h').read_text(encoding='utf-8') + '\ninline std::wstring NormalizeNbaTeams(std::wstring value) {')
    change('''        if (token.find(L"骑士") != std::wstring::npos) token = L"CLE";
        if (token.find(L"76人") != std::wstring::npos) token = L"PHI";
        token = NormalizeNbaTeam(token);''', '        token = CanonicalNbaTeam(token);')
    change('''        if (_wcsicmp(team.c_str(), game.awayAbbr.c_str()) == 0 ||
            _wcsicmp(team.c_str(), game.homeAbbr.c_str()) == 0) return rank;''',
           '''        if (team == CanonicalNbaTeam(game.awayAbbr) ||
            team == CanonicalNbaTeam(game.homeAbbr)) return rank;''')
    start = src.index('inline std::wstring NbaFavoritesLabel(')
    end = src.index('\ninline bool ParseNbaGames(', start)
    src = src[:start] + '''inline std::wstring NbaFavoritesLabel(const std::wstring& favorites) {
    std::wstring result;
    size_t begin = 0;
    while (begin < favorites.size()) {
        const auto end = favorites.find(L',', begin);
        const auto code = favorites.substr(begin, end == std::wstring::npos ? end : end - begin);
        for (const auto& team : kNbaTeams) if (code == team.code) {
            if (!result.empty()) result += L"、";
            result += team.name;
        }
        if (end == std::wstring::npos) break;
        begin = end + 1;
    }
    return result.empty() ? L"全部球队" : result;
}

std::wstring ReadNbaSetup() {
    wchar_t value[256]{};
    Wh_GetStringValue(L"NBASetup", value, ARRAYSIZE(value));
    const std::wstring saved = value;
    if (saved == L"off" || saved == L"all") return saved;
    const auto teams = NormalizeNbaTeams(saved);
    return teams.empty() ? L"pending" : teams;
}
''' + src[end:]
    change('''    next.nbaScores = Wh_GetIntSetting(L"Modules.NBAScores") != 0;
    next.nbaTeam = NormalizeNbaTeams(GetStringSettingCopy(L"Modules.NBATeam"));''', '''    const auto nbaSetup = ReadNbaSetup();
    next.nbaScores = Wh_GetIntSetting(L"Modules.NBAScores") != 0 && nbaSetup != L"off" && nbaSetup != L"pending";
    next.nbaTeam = (nbaSetup == L"all" || nbaSetup == L"off" || nbaSetup == L"pending") ? L"" : nbaSetup;''')
    change('void ShowContextMenu(HWND hwnd, POINT screenPoint) {', (root/'nba-setup-ui.h').read_text(encoding='utf-8') + '\nvoid ShowContextMenu(HWND hwnd, POINT screenPoint) {')
    change('    AppendMenuW(menu, MF_STRING, 46, L"重要日子…");', '    AppendMenuW(menu, MF_STRING, 46, L"重要日子…");\n    AppendMenuW(menu, MF_STRING, 48, L"NBA 主队设置…");')
    change('        case 46: ShowImportantDays(hwnd); break;', '        case 48: ShowNbaSetup(hwnd); break;\n        case 46: ShowImportantDays(hwnd); break;')
    change('''        case WM_CREATE:
            AddClipboardFormatListener(hwnd);''', '''        case WM_APP + 0x449:
            if (ReadNbaSetup() == L"pending") ShowNbaSetup(hwnd, true);
            return 0;
        case WM_CREATE:
            PostMessageW(hwnd, WM_APP + 0x449, 0, 0);
            AddClipboardFormatListener(hwnd);''')

    # Keep optional personal reminders configurable without exporting user data.
    change('  - WeatherFahrenheit: false', '''  - MealReminders: false
    $name: 吃饭提醒
    $description: 按本机时间提醒午饭和晚饭，默认关闭。
  - LunchTime: "11:00"
    $name: 午饭时间
    $description: 使用 24 小时制 HH:mm，留空或格式错误则不提醒。
  - DinnerTime: "17:00"
    $name: 晚饭时间
    $description: 使用 24 小时制 HH:mm，留空或格式错误则不提醒。
  - WeatherFahrenheit: false''')
    change('        auto meal = MakeMealReminder(today, Wh_GetIntValue(L"MealLunchShown", 0), Wh_GetIntValue(L"MealDinnerShown", 0));', '''        TomorrowBrief meal;
        if (Wh_GetIntSetting(L"Modules.MealReminders")) {
            const auto lunch = GetStringSettingCopy(L"Modules.LunchTime");
            const auto dinner = GetStringSettingCopy(L"Modules.DinnerTime");
            const int minute = today.wHour * 60 + today.wMinute;
            for (int slot = 1; slot <= 2; ++slot) {
                const auto& time = slot == 1 ? lunch : dinner;
                const int targetMinute = ParseMealTime(time);
                if (targetMinute < 0 || minute < targetMinute - 10 || minute >= targetMinute + 15 ||
                    Wh_GetIntValue(slot == 1 ? L"MealLunchShown" : L"MealDinnerShown", 0) == BriefDateKey(today)) continue;
                meal.meal = slot; meal.date = BriefDateKey(today);
                meal.key = std::to_wstring(meal.date) + L":" + std::to_wstring(slot);
                meal.title = (slot == 1 ? L"午饭提醒 · " : L"晚饭提醒 · ") + time;
                meal.body = slot == 1 ? L"该吃午饭了" : L"该吃晚饭了";
                meal.detail = L"歇一会儿，按时吃饭";
                break;
            }
        }''')
    change('inline int MealReminderSlot(const SYSTEMTIME& local) {', '''inline int ParseMealTime(const std::wstring& text) {
    if (text.size() != 5 || text[2] != L':' ||
        text[0] < L'0' || text[0] > L'2' || text[1] < L'0' || text[1] > L'9' ||
        text[3] < L'0' || text[3] > L'5' || text[4] < L'0' || text[4] > L'9') return -1;
    const int hour = (text[0] - L'0') * 10 + text[1] - L'0';
    return hour < 24 ? hour * 60 + (text[3] - L'0') * 10 + text[4] - L'0' : -1;
}

inline int MealReminderSlot(const SYSTEMTIME& local) {''')
    change('showing.meal ? showing.date == BriefDateKey(today) && MealReminderSlot(today) == showing.meal',
           'showing.meal ? Wh_GetIntSetting(L"Modules.MealReminders") && meal.key == showing.key && showing.date == BriefDateKey(today)')
    return src
