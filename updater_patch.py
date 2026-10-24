"""Release-only health reporting and safe handoff to the external updater."""


def apply_updater(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:90], src.count(old))
        src = src.replace(old, new, 1)

    for marker in ('void StartFocusTimer(int minutes, bool isBreak) {',
                   'void ShowContextMenu(HWND hwnd, POINT screenPoint) {',
                   'void ShowNbaSetup(HWND owner, bool firstRun = false) {',
                   'void ShowImportantDays(HWND owner) {'):
        # ShowImportantDays uses hwnd in the current source.
        if marker not in src and marker.startswith('void ShowImportantDays'):
            marker = 'void ShowImportantDays(HWND hwnd) {'
        change(marker, marker + '\n    if (Wh_GetIntValue(L"UpdateRequested", 0)) return;')
    # Let the first installation finish its startup check before the modal setup dialog.
    change('            PostMessageW(hwnd, WM_APP + 0x449, 0, 0);',
           '            SetTimer(hwnd, 0x449, 45000, nullptr);')
    change('        case WM_CREATE:', '''        case WM_TIMER:
            if (wParam == 0x449) {
                KillTimer(hwnd, 0x449);
                PostMessageW(hwnd, WM_APP + 0x449, 0, 0);
                return 0;
            }
            break;
        case WM_CREATE:''')
    # The render thread also owns the modal dialogs: stale heartbeats defer updates.
    change('    double nextProgressPoll = 0.0;', '    double nextProgressPoll = 0.0;\n    double nextUpdaterPoll = 0.0;')
    marker = '        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);'
    change(marker, '''        if (now >= nextUpdaterPoll) {
            nextUpdaterPoll = now + 1.0;
            const bool busy = snapshot.timer.active || g_importantDaysOpen.load();
            Wh_SetStringValue(L"UpdateVersion", WH_MOD_VERSION);
            Wh_SetIntValue(L"UpdatePid", GetCurrentProcessId());
            Wh_SetIntValue(L"UpdateHeartbeat", GetTickCount64() / 1000);
            Wh_SetIntValue(L"UpdateBusy", busy);
            Wh_SetIntValue(L"UpdateRequiresGlass", Renderer::UsesNativeGlass(g_settings));
            Wh_SetIntValue(L"UpdateReady", !busy && Wh_GetIntValue(L"UpdateRequested", 0));
        }

''' + marker)
    return src
