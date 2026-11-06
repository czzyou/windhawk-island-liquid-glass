from pathlib import Path


def apply_glass_menu(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    change('#include <shellapi.h>', '#include <shellapi.h>\n#include <oleacc.h>')
    helper = (Path(__file__).parent / 'glass-menu.h').read_text(encoding='utf-8')
    change('std::vector<MediaSourceInfo> AppendMediaSources(HMENU menu) {',
           helper + '\nstd::vector<MediaSourceInfo> AppendMediaSources(HMENU menu) {')
    change('''        const UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,hwnd,nullptr);
        DestroyMenu(menu); PostMessageW(hwnd,WM_NULL,0,0);''',
           '''        const UINT cmd=TrackGlassMenu(menu, point, hwnd);
        PostMessageW(hwnd,WM_NULL,0,0);''')
    change('''    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                   screenPoint.x, screenPoint.y, 0, hwnd, nullptr);
    DestroyMenu(menu);''', '''    const UINT cmd = TrackGlassMenu(menu, screenPoint, hwnd);''')
    change('''    AppendLyricsMenu(menu);
    AppendMenuW(menu, MF_STRING, 1,''', '''    AppendLyricsMenu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1,''')
    start = src.index('LRESULT CALLBACK OverlayWndProc(')
    switch = src.index('    switch (msg) {', start)
    src = src[:switch] + src[switch:].replace('''    switch (msg) {''', '''    switch (msg) {
        case WM_MEASUREITEM:
            if (g_activeGlassMenu && g_activeGlassMenu->Measure(reinterpret_cast<MEASUREITEMSTRUCT*>(lParam))) return TRUE;
            break;
        case WM_DRAWITEM:
            if (g_activeGlassMenu && g_activeGlassMenu->Draw(reinterpret_cast<DRAWITEMSTRUCT*>(lParam))) return TRUE;
            break;''', 1)
    return src
