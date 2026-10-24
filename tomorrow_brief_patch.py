from pathlib import Path


def apply_tomorrow_brief(src):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    header = (root / 'tomorrow-brief.h').read_text(encoding='utf-8').replace('#pragma once', '')
    declarations, helpers = header.split('inline int BriefDateKey', 1)
    important = (root / 'important-days.h').read_text(encoding='utf-8').replace('#pragma once', '')
    important_types, important_helpers = important.split('inline int InferImportantKind', 1)
    declarations += important_types
    change('struct SharedState {', declarations + '\nstruct SharedState {\n    std::vector<ImportantDay> importantDays;\n    TomorrowBrief tomorrowBrief;')
    helpers = 'inline int BriefDateKey' + helpers
    helpers += '\ninline int InferImportantKind' + important_helpers
    change('DWORD WINAPI AudioThreadProc(', helpers + '\nDWORD WINAPI AudioThreadProc(')
    src = src.replace('#include <windows.h>', '#include <windows.h>\n#include <commctrl.h>', 1)
    change('void ShowContextMenu(HWND hwnd, POINT screenPoint) {', (root / 'important-days-ui.h').read_text(encoding='utf-8') + '\nvoid ShowContextMenu(HWND hwnd, POINT screenPoint) {')
    change('    AppendMenuW(menu, MF_STRING, 45, L"Edit notification app mappings");', '    AppendMenuW(menu, MF_STRING, 45, L"Edit notification app mappings");\n    AppendMenuW(menu, MF_STRING, 46, L"重要日子…");')
    change('        case 45: {', '        case 46: ShowImportantDays(hwnd); break;\n        case 45: {')
    change('    HourlyClock,\n    DoNotDisturb,', '    HourlyClock,\n    TomorrowBrief,\n    DoNotDisturb,')
    change('''        case IslandKind::HourlyClock:
            activity.width''', '''        case IslandKind::TomorrowBrief:
            activity.width = 340.0f;
            activity.height = 152.0f;
            break;
        case IslandKind::HourlyClock:
            activity.width''')
    change('''    if (settings.progress && state.progress.active) {''', '''    if (activities.empty() && state.tomorrowBrief.active && !g_isFullscreen.load() &&
        !g_manuallyHidden.load() && !Wh_GetIntValue(L"PinnedExpanded", 0) &&
        !(settings.timerEnabled && state.timer.justFinished && now < state.timer.finishedExpiresAt)) {
        return {IslandKind::TomorrowBrief};
    }
    if (settings.progress && state.progress.active) {''')
    change('''            case IslandKind::HourlyClock:
                DrawHourlyClock''', '''            case IslandKind::TomorrowBrief:
                DrawTomorrowBrief(state.tomorrowBrief, unscaledRect);
                break;
            case IslandKind::HourlyClock:
                DrawHourlyClock''')
    change('''                                 primary.kind == IslandKind::HourlyClock ||''', '''                                 primary.kind == IslandKind::HourlyClock ||
                                 primary.kind == IslandKind::TomorrowBrief ||''')
    change('    void DrawHourlyClock(', (root / 'tomorrow-brief-draw.h').read_text(encoding='utf-8') + '\n    void DrawHourlyClock(')
    change('NbaLogoCache g_nbaLogos[2];', 'NbaLogoCache g_nbaLogos[2];\nNbaLogoCache g_briefLogos[4];')
    change('    ComPtr<ID2D1Bitmap> nbaLogoBitmaps_[2];', '    ComPtr<ID2D1Bitmap> briefLogoBitmaps_[4];\n    uint64_t briefLogoGenerations_[4]{};\n    ComPtr<ID2D1Bitmap> nbaLogoBitmaps_[2];')
    change('        for (auto& bitmap : nbaLogoBitmaps_) bitmap.Reset();', '        for (auto& bitmap : briefLogoBitmaps_) bitmap.Reset();\n        for (auto& bitmap : nbaLogoBitmaps_) bitmap.Reset();')
    change('HANDLE g_hourlyClockThread = nullptr;', 'HANDLE g_hourlyClockThread = nullptr;\nHANDLE g_tomorrowBriefThread = nullptr;')
    start = src.index('void UpdateNbaLogos(')
    a = src.index('        BitmapPixels pixels;', start)
    b = src.index('        {\n            std::lock_guard lock(g_nbaLogoMutex);', a)
    download = src[a:b].replace('urls[i]', 'url')
    src = src[:a] + '        BitmapPixels pixels = FetchNbaLogo(urls[i]);\n' + src[b:]
    change('void UpdateNbaLogos(', 'BitmapPixels FetchNbaLogo(const std::wstring& url) {\n' + download + '    return pixels;\n}\n\nvoid UpdateNbaLogos(')
    change('bool StartThreads() {', (root / 'tomorrow-brief-runtime.h').read_text(encoding='utf-8') + '\nbool StartThreads() {')
    change('''    g_hourlyClockThread = CreateThread(nullptr, 0, HourlyClockThreadProc, nullptr, 0, nullptr);''', '''    g_hourlyClockThread = CreateThread(nullptr, 0, HourlyClockThreadProc, nullptr, 0, nullptr);
    g_tomorrowBriefThread = CreateThread(nullptr, 0, TomorrowBriefThreadProc, nullptr, 0, nullptr);''')
    change('g_nbaThread, g_hourlyClockThread, g_volumeControlThread', 'g_nbaThread, g_hourlyClockThread, g_tomorrowBriefThread, g_volumeControlThread')
    change('    g_hourlyClockThread = nullptr;', '    g_hourlyClockThread = nullptr;\n    g_tomorrowBriefThread = nullptr;')
    return src
