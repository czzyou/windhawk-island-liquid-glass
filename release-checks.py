"""Checks the emitted release helpers and actual dialog with in-memory storage."""
from pathlib import Path
import re
import subprocess
import sys
import json

root = Path(__file__).parent.resolve()
source = (root/'dynamic-island-liquid-glass.wh.cpp').read_text(encoding='utf-8')
release = json.loads((root/'release.json').read_text(encoding='utf-8-sig'))
assert re.search(r'^// @version\s+' + re.escape(release['version']) + r'$', source, re.M)
assert re.search(r'^// @id\s+dynamic-island-liquid-glass$', source, re.M)
assert 'Wh_SetIntValue(L"UpdateHeartbeat"' in source
assert 'Wh_SetIntValue(L"UpdateReady"' in source
assert 'SetTimer(hwnd, 0x449, 45000, nullptr)' in source
assert not re.search(r'#include\s+"', source), 'Release must be self contained'
assert '-lcomctl32' in source.split('// ==/WindhawkMod==')[0]
assert '  - LiquidGlass: true' in source and '  - ThemePreset: custom' in source
assert '  - PillBgColor: "#10101024"' in source
assert '  - WeatherCity: ""' in source
assert not re.search(r'(?:CzzY|D:\\\\CzzY|E:\\\\math)', source)

test = r'''#include <windows.h>
#include <winhttp.h>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <mutex>
#include <cstdio>
#include <limits>
#include <atomic>
std::wstring saved;
int settingsLoads = 0;
size_t Wh_GetStringValue(const wchar_t*, wchar_t* buffer, size_t size) {
    if (saved.size() >= size) { buffer[0] = 0; return 0; }
    wcscpy_s(buffer, size, saved.c_str()); return saved.size();
}
BOOL Wh_SetStringValue(const wchar_t*, const wchar_t* value) { saved = value; return TRUE; }
void LoadSettings() { ++settingsLoads; }
std::atomic<bool> g_importantDaysOpen = false;
'''
test += source[source.index('struct NbaSnapshot {'):source.index('inline bool ParseNbaGames(')]
test += source[source.index('std::wstring ChinaWeatherCityCode('):source.index('std::string ChinaWeatherObject(')]
test += source[source.index('std::string ChinaWeatherObject('):source.index('SYSTEMTIME ChinaWeatherClock(')]
test += source[source.index('std::string HttpGet('):source.index('DWORD WINAPI WeatherThreadProc(')]
test += source[source.index('inline int ParseMealTime('):source.index('inline int MealReminderSlot(')]
test += (root/'nba-setup-ui.h').read_text(encoding='utf-8')
test += r'''
int scenario = 0;
BOOL CALLBACK ExerciseDialog(HWND hwnd, LPARAM) {
    wchar_t cls[64]{}; GetClassNameW(hwnd, cls, 64);
    if (std::wstring(cls) != L"#32770" || !GetDlgItem(hwnd, 101)) return TRUE;
    assert(SendDlgItemMessageW(hwnd, 101, LB_GETCOUNT, 0, 0) == 30);
    auto click = [&](int id) { SendMessageW(hwnd, WM_COMMAND, id, 0); };
    auto add = [&](int index) { SendDlgItemMessageW(hwnd, 101, LB_SETCURSEL, index, 0); click(103); };
    if (scenario == 0) {
        add(9); add(13); add(9);
        assert(SendDlgItemMessageW(hwnd, 102, LB_GETCOUNT, 0, 0) == 2);
        SendDlgItemMessageW(hwnd, 102, LB_SETCURSEL, 1, 0); click(105);
        auto* state = reinterpret_cast<NbaSetupDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
        assert(state->teams[0] == L"LAL" && state->teams[1] == L"GS");
        click(105); assert(state->teams[0] == L"LAL");
        click(106); assert(state->teams[0] == L"GS");
        click(105); click(IDOK);
    } else if (scenario == 1) {
        assert(SendDlgItemMessageW(hwnd, 102, LB_GETCOUNT, 0, 0) == 2);
        SendDlgItemMessageW(hwnd, 102, LB_SETCURSEL, 0, 0); click(104);
        click(IDCANCEL);
    } else if (scenario == 2) {
        assert(SendDlgItemMessageW(hwnd, 102, LB_GETCOUNT, 0, 0) == 0);
        click(IDOK);
    } else if (scenario == 3) {
        SendMessageW(hwnd, WM_CLOSE, 0, 0);
    } else {
        assert(SendDlgItemMessageW(hwnd, 100, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
        SendDlgItemMessageW(hwnd, 100, BM_SETCHECK, BST_CHECKED, 0);
        add(10); click(IDOK);
    }
    return FALSE;
}
void CALLBACK ExerciseTimer(HWND, UINT, UINT_PTR, DWORD) {
    EnumThreadWindows(GetCurrentThreadId(), ExerciseDialog, 0);
}
int main(int argc, char** argv) {
    SetProcessDPIAware();
    assert(std::size(kNbaTeams) == 30);
    for (const auto& team : kNbaTeams) {
        assert(CanonicalNbaTeam(team.code) == team.code);
        assert(CanonicalNbaTeam(team.name) == team.code);
        assert(NbaFavoritesLabel(team.code) == team.name);
        NbaSnapshot game; game.awayAbbr = team.code; game.homeAbbr = L"BOS";
        assert(NbaFavoriteRank(game, team.code) == 0);
    }
    assert(NormalizeNbaTeams(L" 湖人，勇士;NYK;SAS;NOP;UTA;WAS ") == L"LAL,GS,NY,SA,NO,UTAH,WSH");
    assert(CanonicalNbaTeam(L"invalid").empty());
    assert(ChinaWeatherCityCode(L"").empty());
    assert(ChinaWeatherCityCode(L"101010100") == L"101010100");
    assert(ParseWeatherIPCity("var ip=\"0.0.0.0\";var id=\"101010100\";var addr=\"x\";") == L"101010100");
    for (auto bad : {"", "<html>error</html>", "var id=\"1010101000\";", "var id=\"000000000\";", "var id=\"10101010x\";", "var id=\"101\";"})
        assert(ParseWeatherIPCity(bad).empty());
    assert(ResolvedWeatherCity(L"").empty());
    g_weatherIPCity = L"101320101";
    assert(ResolvedWeatherCity(L"") == L"101320101");
    assert(ResolvedWeatherCity(L"101010100") == L"101010100");
    assert(ParseMealTime(L"00:00") == 0 && ParseMealTime(L"23:59") == 1439);
    for (auto bad : {L"24:00", L"9:00", L"12:60", L"", L"aa:bb"}) assert(ParseMealTime(bad) == -1);
    assert(ReadNbaSetup() == L"pending");
    saved = L"bogus"; assert(ReadNbaSetup() == L"pending");
    saved.clear();
    if (argc > 1 && std::string(argv[1]) == "--preview") {
        ShowNbaSetup(nullptr, true); return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--live") {
        const auto code = ParseWeatherIPCity(HttpGet(L"wgeo.weather.com.cn", L"/ip/"));
        if (code.empty()) { puts("Live IP lookup unavailable"); return 1; }
        const auto current = HttpGet(L"d1.weather.com.cn", (L"/sk_2d/" + code + L".html").c_str());
        const auto forecast = HttpGet(L"d1.weather.com.cn", (L"/weather_index/" + code + L".html").c_str());
        assert(!ChinaWeatherObject(current, "dataSK").empty());
        assert(!ChinaWeatherObject(forecast, "fc").empty());
        puts("Live WinHTTP IP lookup and both weather endpoints passed"); return 0;
    }
    const auto timer = SetTimer(nullptr, 0, 100, ExerciseTimer);
    assert(timer);
    for (scenario = 0; scenario < 5; ++scenario) {
        if (scenario == 2 || scenario == 3) saved.clear();
        ShowNbaSetup(nullptr, scenario == 0 || scenario == 2 || scenario == 3);
        assert(!g_importantDaysOpen.load());
        if (scenario == 0 || scenario == 1) assert(saved == L"LAL,GS");
        if (scenario == 2) assert(saved == L"all");
        if (scenario == 3) assert(saved == L"off");
        if (scenario == 4) assert(saved == L"HOU");
    }
    KillTimer(nullptr, timer);
    assert(settingsLoads == 4);
    puts("Release checks passed: IP parser, manual override, 30 teams, aliases, priority, save/reopen/cancel/skip/re-enable, reminder times");
}
'''
cpp = root/'release-checks.cpp'
cpp.write_text(test, encoding='utf-8')
compiler = Path(r'C:\Program Files\Windhawk\Compiler')
subprocess.run([str(compiler/'bin/clang++.exe'), '-std=c++23', '-static', '-DUNICODE', '-D_UNICODE', '-D_WIN32_WINNT=0x0A00',
                str(cpp), '-luser32', '-lgdi32', '-lwinhttp', '-o', str(root/'release-checks.exe')], cwd=compiler, check=True)
subprocess.run([str(root/'release-checks.exe'), *sys.argv[1:]], check=True, timeout=120 if '--preview' in sys.argv else 60)
