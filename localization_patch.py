def apply_localization(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    if '// @name            灵动岛 · 液态玻璃' in src:
        change('// @name            灵动岛 · 液态玻璃', '''// @name            Dynamic Island · Liquid Glass
// @name:zh-CN      灵动岛 · 液态玻璃
// @name:zh-TW      灵动岛 · 液态玻璃''')
    change('// @description     A living, breathing pill overlay inspired by iPhone\'s Dynamic Island. Reacts to media, downloads, clipboard, battery, and more.', '''// @description     A liquid-glass island for media, weather, calendar, timers and system status.
// @description:zh-CN 液态玻璃灵动岛，显示媒体、天气、日历、计时器和系统状态。
// @description:zh-TW 液态玻璃灵动岛，显示媒体、天气、日历、计时器和系统状态。''')
    change('namespace {\n\nconstexpr wchar_t kWindowClass', '''namespace {
bool UiChinese();
const wchar_t* UiText(const wchar_t* english, const wchar_t* chinese);

constexpr wchar_t kWindowClass''')
    change('''    std::wstring city;
    std::wstring weatherDesc;''', '''    std::wstring city;
    std::wstring cityEnglish;
    std::wstring weatherDesc;
    std::wstring weatherDescEnglish;''')
    change('''// Resolves the "auto" language setting from the user's Windows UI language.
UiLanguage DetectSystemLanguage() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, ARRAYSIZE(name)) > 0) {
        return LanguageFromTag(name);
    }
    return UiLanguage::English;
}''', '''bool IsChineseUiLanguage(LANGID language) {
    return PRIMARYLANGID(language) == LANG_CHINESE;
}

UiLanguage DetectSystemLanguage() {
    return IsChineseUiLanguage(GetUserDefaultUILanguage())
        ? UiLanguage::ChineseSimplified : UiLanguage::English;
}

bool UiChinese() {
    return g_uiLanguage.load(std::memory_order_relaxed) == static_cast<int>(UiLanguage::ChineseSimplified);
}

const wchar_t* UiText(const wchar_t* english, const wchar_t* chinese) {
    return UiChinese() ? chinese : english;
}

std::wstring WeatherTextForUi(std::wstring text) {
    if (UiChinese()) return text;
    struct Translation { const wchar_t* chinese; const wchar_t* english; };
    static constexpr Translation words[] = {
        {L"雷阵雨伴冰雹", L"Thundershowers with hail"}, {L"大暴雨到特大暴雨", L"Very heavy to extreme rain"},
        {L"暴雨到大暴雨", L"Heavy to very heavy rain"}, {L"大到暴雨", L"Heavy rain to rainstorm"},
        {L"中到大雨", L"Moderate to heavy rain"}, {L"小到中雨", L"Light to moderate rain"},
        {L"小到中雪", L"Light to moderate snow"}, {L"中到大雪", L"Moderate to heavy snow"},
        {L"大到暴雪", L"Heavy snow to blizzard"}, {L"特大暴雨", L"Extreme rainstorm"},
        {L"大暴雨", L"Very heavy rain"}, {L"雷阵雨", L"Thundershowers"}, {L"雨夹雪", L"Sleet"},
        {L"强沙尘暴", L"Severe dust storm"}, {L"沙尘暴", L"Dust storm"}, {L"阵雪", L"Snow showers"},
        {L"小雪", L"Light snow"}, {L"中雪", L"Moderate snow"}, {L"大雪", L"Heavy snow"},
        {L"暴雪", L"Blizzard"}, {L"阵雨", L"Showers"}, {L"小雨", L"Light rain"},
        {L"中雨", L"Moderate rain"}, {L"大雨", L"Heavy rain"}, {L"暴雨", L"Rainstorm"},
        {L"冻雨", L"Freezing rain"}, {L"多云", L"Cloudy"}, {L"浮尘", L"Dust"}, {L"扬沙", L"Blowing sand"},
        {L"东北风", L"NE wind"}, {L"东南风", L"SE wind"}, {L"西北风", L"NW wind"}, {L"西南风", L"SW wind"},
        {L"东风", L"E wind"}, {L"南风", L"S wind"}, {L"西风", L"W wind"}, {L"北风", L"N wind"},
        {L"无持续风向", L"Variable wind"}, {L"小于", L"<"}, {L"级", L" level"},
        {L"晴", L"Clear"}, {L"阴", L"Overcast"}, {L"雾", L"Fog"}, {L"霾", L"Haze"},
        {L"雨", L"Rain"}, {L"雪", L"Snow"}, {L"转", L" to "}, {L"未知", L"Unknown"}
    };
    for (const auto& word : words) {
        size_t position = 0;
        while ((position = text.find(word.chinese, position)) != std::wstring::npos) {
            text.replace(position, wcslen(word.chinese), word.english);
            position += wcslen(word.english);
        }
    }
    return text;
}''')
    change('enum class WeatherVisual {', '''std::wstring WeatherCityForUi(const WeatherSnapshot& weather) {
    if (UiChinese() || weather.cityEnglish.empty()) return weather.city;
    std::wstring city = weather.cityEnglish;
    if (city[0] >= L'a' && city[0] <= L'z') city[0] -= L'a' - L'A';
    return city;
}

std::wstring WeatherCurrentDescriptionForUi(const WeatherSnapshot& weather) {
    return !UiChinese() && !weather.weatherDescEnglish.empty()
        ? weather.weatherDescEnglish : WeatherTextForUi(weather.weatherDesc);
}

enum class WeatherVisual {''')
    change('''        std::wstring langTag = GetStringSettingCopy(L"Modules.Language");
        if (langTag.empty()) {
            langTag = L"auto";
        }
        next.language = langTag;
        const UiLanguage resolved = EqualsNoCase(langTag, L"auto") ? DetectSystemLanguage()
                                                                  : LanguageFromTag(langTag);
        g_uiLanguage.store(static_cast<int>(resolved), std::memory_order_relaxed);''', '''        next.language = L"auto";
        g_uiLanguage.store(static_cast<int>(DetectSystemLanguage()), std::memory_order_relaxed);''')

    change('        label = L"第" + std::to_wstring(period) + L"节";',
           '        label = std::wstring(UiText(L"Q", L"第")) + std::to_wstring(period) + UiText(L"", L"节");')
    change('        label = period == 5 ? L"加时" : L"加时" + std::to_wstring(period - 4);',
           '        label = period == 5 ? UiText(L"OT", L"加时") : UiText(L"OT", L"加时") + std::to_wstring(period - 4);')
    change('candidate.status = candidate.live ? NbaPeriodText(period, clock) : L"已结束";',
           'candidate.status = candidate.live ? NbaPeriodText(period, clock) : UiText(L"Finished", L"已结束");')
    change('inline std::wstring NbaFavoritesLabel(const std::wstring& favorites) {', '''inline std::wstring NbaDisplayTeam(const std::wstring& code, const std::wstring& fallback = L"") {
    static constexpr const wchar_t* names[] = {
        L"Hawks", L"Celtics", L"Nets", L"Hornets", L"Bulls", L"Cavaliers", L"Mavericks", L"Nuggets",
        L"Pistons", L"Warriors", L"Rockets", L"Pacers", L"Clippers", L"Lakers", L"Grizzlies", L"Heat",
        L"Bucks", L"Timberwolves", L"Pelicans", L"Knicks", L"Thunder", L"Magic", L"76ers", L"Suns",
        L"Trail Blazers", L"Kings", L"Spurs", L"Raptors", L"Jazz", L"Wizards"
    };
    static_assert(std::size(names) == std::size(kNbaTeams));
    const auto canonical = CanonicalNbaTeam(code);
    for (size_t i = 0; i < std::size(kNbaTeams); ++i)
        if (canonical == kNbaTeams[i].code) return UiChinese() ? kNbaTeams[i].name : names[i];
    return fallback.empty() ? code : fallback;
}

inline std::wstring NbaFavoritesLabel(const std::wstring& favorites) {''')
    change('            result += team.name;', '            result += NbaDisplayTeam(code);')
    change('    return result.empty() ? L"全部球队" : result;',
           '    return result.empty() ? UiText(L"All teams", L"全部球队") : result;')
    change('const auto label = std::wstring(team.name) + L" (" + team.code + L")";',
           'const auto label = NbaDisplayTeam(team.code) + L" (" + team.code + L")";')
    change('drawTeamRow(rect.top + 52, nba.awayAbbr, nba.awayName, nba.awayRecord,',
           'drawTeamRow(rect.top + 52, nba.awayAbbr, NbaDisplayTeam(nba.awayAbbr, nba.awayName), nba.awayRecord,')
    change('drawTeamRow(rect.top + 114, nba.homeAbbr, nba.homeName, nba.homeRecord,',
           'drawTeamRow(rect.top + 114, nba.homeAbbr, NbaDisplayTeam(nba.homeAbbr, nba.homeName), nba.homeRecord,')
    change('return kind==1 ? L"考试" : kind==2 ? L"生日" : L"其他";',
           'return kind==1 ? UiText(L"Exam", L"考试") : kind==2 ? UiText(L"Birthday", L"生日") : UiText(L"Other", L"其他");')
    change('''    result.title=L"重要日子 · "+(remaining==0 ? std::wstring(L"今天") : remaining==1 ? std::wstring(L"明天") : L"还有 "+std::to_wstring(remaining)+L" 天");''',
           '''    result.title=std::wstring(UiText(L"Important date · ", L"重要日子 · "))+
        (remaining==0 ? std::wstring(UiText(L"Today", L"今天")) : remaining==1 ? std::wstring(UiText(L"Tomorrow", L"明天")) :
         std::wstring(UiText(L"In ", L"还有 "))+std::to_wstring(remaining)+UiText(L" days", L" 天"));''')
    change('swprintf_s(date,L"%04d 年 %02d 月 %02d 日",day.date/10000,(day.date/100)%100,day.date%100);',
           'swprintf_s(date,UiText(L"%04d-%02d-%02d", L"%04d 年 %02d 月 %02d 日"),day.date/10000,(day.date/100)%100,day.date%100);')
    change('''(day.advance ? L"提前 "+std::to_wstring(day.advance)+L" 天及当天" : std::wstring(L"仅当天"))''',
           '''(day.advance ? std::wstring(UiText(L"", L"提前 "))+std::to_wstring(day.advance)+UiText(L" days before and on the day", L" 天及当天") : std::wstring(L"仅当天"))''')
    change('''    result.title = L"明天 NBA · " + std::to_wstring(selected.size()) + L" 场关注比赛";''',
           '''    result.title = std::wstring(UiText(L"Tomorrow's NBA · ", L"明天 NBA · ")) + std::to_wstring(selected.size()) + UiText(L" favorite games", L" 场关注比赛");''')
    change('''    result.detail = selected.size() > 1 ? label(selected[1]) : L"开赛时间为本地时间";''',
           '''    result.detail = selected.size() > 1 ? label(selected[1]) : UiText(L"Times shown in local time", L"开赛时间为本地时间");''')
    change('''            result.body = description + L"  " + range;''',
           '''            result.body = WeatherTextForUi(description) + L"  " + range;''')
    change('''            result.detail = std::wstring(item.GetNamedString(L"fe", L"")) + L" " +
                std::wstring(item.GetNamedString(L"fg", L""));''',
           '''            result.detail = WeatherTextForUi(std::wstring(item.GetNamedString(L"fe", L"")) + L" " +
                std::wstring(item.GetNamedString(L"fg", L"")));''')
    change('''        std::wstring schedule = L"NBA：赛程暂不可用（请求失败）";''',
           '''        std::wstring schedule = UiText(L"NBA: schedule unavailable", L"NBA：赛程暂不可用（请求失败）");''')
    change('''            schedule = L"NBA " + day.label + L"：" + std::to_wstring(summary.total) + L"场 · " +
                std::to_wstring(summary.live) + L"进行中 · " + std::to_wstring(summary.scheduled) +
                L"未开始 · " + std::to_wstring(summary.finished) + L"已结束";''',
           '''            schedule = L"NBA " + day.label + UiText(L": ", L"：") + std::to_wstring(summary.total) + UiText(L" games · ", L"场 · ") +
                std::to_wstring(summary.live) + UiText(L" live · ", L"进行中 · ") + std::to_wstring(summary.scheduled) +
                UiText(L" scheduled · ", L"未开始 · ") + std::to_wstring(summary.finished) + UiText(L" finished", L"已结束");''')
    change('g_state.volume.deviceName=L"系统音量";', 'g_state.volume.deviceName=UiText(L"System volume", L"系统音量");')
    change('''    result.title = slot == 1 ? L"午饭提醒 · 11:00" : L"晚饭提醒 · 17:00";
    result.body = slot == 1 ? L"该吃午饭了" : L"该吃晚饭了";
    result.detail = L"歇一会儿，按时吃饭";''', '''    result.title = slot == 1 ? UiText(L"Lunch reminder · 11:00", L"午饭提醒 · 11:00") : UiText(L"Dinner reminder · 17:00", L"晚饭提醒 · 17:00");
    result.body = slot == 1 ? UiText(L"Time for lunch", L"该吃午饭了") : UiText(L"Time for dinner", L"该吃晚饭了");
    result.detail = UiText(L"Take a break and have a meal", L"歇一会儿，按时吃饭");''')
    change('''result.title = std::wstring(showTomorrow ? L"明天天气 · " : L"今天天气 · ") + (city.empty() ? std::wstring(L"自动定位") : city);''',
           '''result.title = std::wstring(showTomorrow ? UiText(L"Tomorrow's weather · ", L"明天天气 · ") : UiText(L"Today's weather · ", L"今天天气 · ")) + (city.empty() ? std::wstring(UiText(L"Automatic location", L"自动定位")) : city);''')
    change('demo.status = L"第4节 5:12";', 'demo.status = NbaPeriodText(4, L"5:12");')
    change('schedule = g_state.nbaSchedule;', '''schedule = g_state.nbaSchedule;
            if (schedule == L"NBA：赛程待查询") schedule = UiText(L"NBA: schedule pending", L"NBA：赛程待查询");''')

    # Translate user-facing literals only, leaving persisted keys and provider data untouched.
    text = {
        '重要日子': 'Important dates', '仅当天': 'On the day',
        '选择日期和提前量；临近日期时主动展开，每个提醒阶段只展示一次。': 'Choose a date and advance notice. Each reminder is shown once.',
        '名称：': 'Name:', '日期：': 'Date:', '提醒：': 'Remind:', '类型：': 'Type:',
        '提前 1 天及当天': '1 day before and on the day', '提前 3 天及当天': '3 days before and on the day',
        '提前 7 天及当天': '7 days before and on the day', '其他 · 小圆点': 'Other · dot',
        '考试 · 手绘圈': 'Exam · circle', '生日 · 蛋糕': 'Birthday · cake',
        '添加': 'Add', '保存选中': 'Save selected', '删除选中': 'Delete selected', '完成': 'Done',
        '每次添加或删除立即保存。饭点：11:00、17:00，提前 10 分钟提醒。': 'Changes are saved immediately. Meal reminders use the configured times.',
        '无法创建日期管理控件，已有日程未修改。': 'Unable to create date controls. Existing dates are unchanged.',
        '请填写有效名称和日期；最多保存 32 个重要日子。': 'Enter a valid name and date; up to 32 dates are supported.',
        '这个名称和日期已经添加。': 'This name and date have already been added.',
        '保存失败，已有日程未修改。': 'Unable to save. Existing dates are unchanged.',
        '保存失败，日程未删除。': 'Unable to save. The date was not deleted.',
        '已有日程数据无法读取，已保留原数据。': 'Unable to read saved dates. The original data has been preserved.',
        '无法打开日期管理窗口，已有日程未修改。': 'Unable to open date settings. Existing dates are unchanged.',
        '自动跟随': 'Automatic', '媒体来源（选择后锁定）': 'Media source (select to lock)',
        '收起时显示歌词': 'Show lyrics when collapsed', '歌词页置顶': 'Pin lyrics page first',
        '清除歌词缓存': 'Clear lyrics cache', '蓝牙偏移：': 'Bluetooth offset: ',
        '扬声器／有线偏移：': 'Speaker / wired offset: ',
        '提前 10 ms': '10 ms earlier', '提前 50 ms': '50 ms earlier', '提前 250 ms': '250 ms earlier',
        '延后 10 ms': '10 ms later', '延后 50 ms': '50 ms later', '延后 250 ms': '250 ms later',
        '重置当前设备微调': 'Reset current device adjustment', '歌词同步': 'Lyrics sync',
        '快捷查词…': 'Quick lookup…', '有道词典': 'Youdao Dictionary',
        '英 → 中': 'English → Chinese', '中 → 英': 'Chinese → English',
        '  英 /': '  UK /', '  美 /': '  US /',
        '保存失败，请重试。原来的选择已保留。': 'Unable to save. Your previous selection was preserved.',
        'NBA 主队': 'NBA favorites', '欢迎使用灵动岛 · 选择 NBA 主队': 'Welcome · Choose NBA favorites',
        'NBA 主队设置': 'NBA favorites', '选择后保存，以后开机不再询问；右键灵动岛可随时修改。': 'Save your selection. Change it later from the island menu.',
        '开启 NBA 实时比分': 'Enable live NBA scores', '全部球队': 'All teams',
        '我的主队（从上到下优先）': 'Favorites (highest priority first)', '添加 →': 'Add →',
        '移除': 'Remove', '上移': 'Move up', '下移': 'Move down',
        '开启后不添加主队 = 显示全部球队的直播。': 'With no favorites selected, show all live games.',
        '保存': 'Save', '暂不开启': 'Not now', '取消': 'Cancel',
        '无法打开主队设置，请从右键菜单重试。': 'Unable to open favorites. Retry from the island menu.',
        '无法找到更新工具目录。': 'The updater folder could not be found.',
        '尚未安装更新工具，请先运行安装包中的 install.cmd。': 'The updater is not installed. Run install.cmd from the setup package.',
        '无法启动更新检查，请稍后重试。': 'Unable to start the update check. Try again later.',
        '检查更新': 'Check for updates', '切换到 NBA 比分': 'Show NBA scores', '切换到时间': 'Show clock',
        '主队：': 'Favorites: ', '（优先顺序）': ' (priority order)', '重要日子…': 'Important dates…',
        'NBA 主队设置…': 'NBA favorites…', '专辑氛围背光': 'Album ambient glow',
        '安装可用更新…': 'Install available update…', '检查更新…': 'Check for updates…',
        '%d 月 %d 日': '%d/%d', '今天': 'Today', '明天': 'Tomorrow', '后天': 'Day after',
        '今日 %s°～%s°': 'Today %s°–%s°', '暂无预报': 'Forecast unavailable', '最高': ' high',
        '风况': 'Wind', '预报日期': 'Forecast date',
        '文件托盘 · × 后7天删原文件': 'File tray · × deletes the original in 7 days',
        '截图': 'Screenshot', '音量': 'Volume', '专注 25′': 'Focus 25′', '休息 5′': 'Break 5′',
        '暂停': 'Pause', '继续': 'Resume', '结束': 'Stop',
        '摄像头': 'Camera', ' · 摄像头': ' · Camera', '麦克风': 'Microphone', ' · 麦克风': ' · Microphone',
        ' 分钟 · ': ' min · ', '休息': 'Break', '专注': 'Focus', ' · 进行中': ' · Running',
        ' · 已暂停': ' · Paused', '已暂停': 'Paused', '休息结束': 'Break finished',
        '专注完成': 'Focus complete', '休息 5 分钟': 'Break 5 min', '再来一轮': 'Start again',
        '正在播放': 'Now playing', '歌词查询失败，稍后重试': 'Lyrics lookup failed',
        '正在查找歌词…': 'Finding lyrics…', '未找到匹配歌词': 'No matching lyrics', '等待歌词…': 'Waiting for lyrics…',
        '输入英文或中文，支持拼写候选': 'Enter English or Chinese', '输入后按 Enter 查询': 'Press Enter to look up',
        '最近查询': 'Recent lookups', '清空': 'Clear', '查询': 'Look up',
        '正在查询…': 'Looking up…', '查询失败，请检查网络后重试': 'Lookup failed; check your connection',
        '未找到结果': 'No results', '本地时间': 'Local time', '时钟': 'Clock',
        '更新中断': 'Update interrupted', '已结束': 'Finished', '进行中': 'Live', '终场': 'Final',
        '无法取消待删除计划，文件未重新加入托盘。': 'Could not cancel deletion. The file was not added back.',
        '无法保存七天删除计划，文件仍保留在托盘中。仅支持本机普通文件。': 'Could not schedule deletion. The file remains in the tray. Only local files are supported.',
        '文件托盘': 'File tray', '午饭提醒 · ': 'Lunch reminder · ', '晚饭提醒 · ': 'Dinner reminder · ',
        '该吃午饭了': 'Time for lunch', '该吃晚饭了': 'Time for dinner',
        '歇一会儿，按时吃饭': 'Take a break and have a meal',
        '明天天气 · ': 'Tomorrow’s weather · ', '今天天气 · ': 'Today’s weather · ',
    }
    start = src.index('bool ReadImportantDays(')
    head, ui = src[:start], src[start:]
    for chinese, english in text.items():
        old = 'L"' + chinese + '"'
        if old in ui:
            ui = ui.replace(old, 'UiText(L"' + english + '", ' + old + ')')
    src = head + ui

    src = src.replace('wText = state.weather.weatherDesc;', 'wText = WeatherCurrentDescriptionForUi(state.weather);')
    change('hasWeather ? state.weather.city : std::wstring(Loc(L"Locating..."))',
           'hasWeather ? WeatherCityForUi(state.weather) : std::wstring(Loc(L"Locating..."))')
    change('cityName = g_state.weather.city;', 'cityName = WeatherCityForUi(g_state.weather);')
    change('desc = std::wstring(range) + L"\\n" + state.weather.weatherDesc;',
           'desc = std::wstring(range) + L"\\n" + WeatherCurrentDescriptionForUi(state.weather);')
    change('desc = hasForecast ? forecast.description + L"\\n" + WeatherTemperatureText(forecast.low)',
           'desc = hasForecast ? WeatherTextForUi(forecast.description) + L"\\n" + WeatherTemperatureText(forecast.low)')
    change('hasForecast ? forecast.wind : std::wstring(L"--")',
           'hasForecast ? WeatherTextForUi(forecast.wind) : std::wstring(L"--")')
    change('WindDirToArrow(state.weather.windDir)', 'WeatherTextForUi(WindDirToArrow(state.weather.windDir))')

    # The selected language also controls dates and labels supplied by the upstream UI.
    src = src.replace('GetDateFormatEx(L"en-US", 0, &local, L"ddd, MMM d",',
                      'GetDateFormatEx(UiText(L"en-US", L"zh-CN"), 0, &local, UiText(L"ddd, MMM d", L"M月d日 ddd"),')
    for pattern in ('MMMM', 'dddd'):
        src = src.replace('GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &local, L"' + pattern + '",',
                          'GetDateFormatEx(UiText(L"en-US", L"zh-CN"), 0, &local, L"' + pattern + '",')
    change('const wchar_t* days[] = {L"S", L"M", L"T", L"W", L"T", L"F", L"S"};',
           'const wchar_t* days[] = {UiText(L"S", L"日"), UiText(L"M", L"一"), UiText(L"T", L"二"), UiText(L"W", L"三"), UiText(L"T", L"四"), UiText(L"F", L"五"), UiText(L"S", L"六")};')
    change('target_->DrawTextW(UiText(L"Clock", L"时钟"), 2, smallTextFormat_.Get(),',
           'target_->DrawTextW(UiText(L"Clock", L"时钟"), static_cast<UINT32>(wcslen(UiText(L"Clock", L"时钟"))), smallTextFormat_.Get(),')

    english = {
        'Dismiss': '关闭', 'Start 25 min Focus': '开始 25 分钟专注', 'Start 50 min Focus': '开始 50 分钟专注',
        'Start 5 min Break': '开始 5 分钟休息', 'Pause Timer': '暂停计时', 'Resume Timer': '继续计时',
        'Stop Timer': '结束计时', 'Focus Timer': '专注计时器', 'Clear ': '清空 ',
        'Pin expanded': '固定展开', 'Hide game overlay': '隐藏游戏悬浮窗', 'Show game overlay': '显示游戏悬浮窗',
        'Expand on Click': '点击展开', 'Expand on Hover': '悬停展开', 'Open Windhawk settings': '打开 Windhawk 设置',
        'NET UP': '上传', 'NET DOWN': '下载', 'DISK': '磁盘', 'Image copied': '图片已复制',
        'Clipboard': '剪贴板', 'Device Connected': '设备已连接', 'Device Removed': '设备已移除',
        'USB Device': 'USB 设备', 'Bluetooth Device': '蓝牙设备', 'Power Connected': '电源已连接',
        'Battery Alert': '电池提醒', 'Progress %d%%': '进度 %d%%',
    }
    start = src.index('void ShowContextMenu(')
    head, ui = src[:start], src[start:]
    for original, chinese in english.items():
        old = 'L"' + original + '"'
        ui = ui.replace(old, 'UiText(' + old + ', L"' + chinese + '")')
    src = head + ui
    from localization_settings import localize_settings
    settings_start = src.index('/*', src.index('// ==WindhawkModSettings==')) + 2
    settings_end = src.index('*/', settings_start)
    src = src[:settings_start] + localize_settings(src[settings_start:settings_end]) + src[settings_end:]
    return src
