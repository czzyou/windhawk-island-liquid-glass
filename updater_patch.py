"""Release-only update badge, health reporting and manual updater handoff."""

from pathlib import Path


def apply_updater(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:90], src.count(old))
        src = src.replace(old, new, 1)

    change('void ShowContextMenu(HWND hwnd, POINT screenPoint) {', r'''std::atomic<bool> g_updateAvailable{false};

void CheckIslandUpdates(HWND hwnd) {
    wchar_t programFiles[MAX_PATH]{};
    DWORD count = GetEnvironmentVariableW(L"ProgramW6432", programFiles, ARRAYSIZE(programFiles));
    if (!count) count = GetEnvironmentVariableW(L"ProgramFiles", programFiles, ARRAYSIZE(programFiles));
    if (!count || count >= ARRAYSIZE(programFiles)) {
        MessageBoxW(hwnd, L"无法找到更新工具目录。", L"检查更新", MB_OK | MB_ICONERROR);
        return;
    }
    const std::wstring script = std::wstring(programFiles) + L"\\Windhawk-Island-Updater\\check.ps1";
    if (GetFileAttributesW(script.c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd, L"尚未安装更新工具，请先运行安装包中的 install.cmd。", L"检查更新", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t windows[MAX_PATH]{};
    count = GetWindowsDirectoryW(windows, ARRAYSIZE(windows));
    if (!count || count >= ARRAYSIZE(windows)) return;
    BOOL wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &wow64);
    const std::wstring executable = std::wstring(windows) + (wow64
        ? L"\\Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe"
        : L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
    const std::wstring arguments = L"-NoProfile -WindowStyle Hidden -File \"" + script + L"\" -Mode Manual";
    if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", executable.c_str(), arguments.c_str(), nullptr, SW_HIDE)) <= 32)
        MessageBoxW(hwnd, L"无法启动更新检查，请稍后重试。", L"检查更新", MB_OK | MB_ICONERROR);
}

void ShowContextMenu(HWND hwnd, POINT screenPoint) {''')
    change('    AppendMenuW(menu, MF_STRING, 9, L"Open Windhawk settings");',
           '    AppendMenuW(menu, MF_STRING, 49, g_updateAvailable.load() ? L"安装可用更新…" : L"检查更新…");\n    AppendMenuW(menu, MF_STRING, 9, L"Open Windhawk settings");')
    change('        case 48: ShowNbaSetup(hwnd); break;',
           '        case 49: CheckIslandUpdates(hwnd); break;\n        case 48: ShowNbaSetup(hwnd); break;')

    for marker in ('void StartFocusTimer(int minutes, bool isBreak) {',
                   'void ShowContextMenu(HWND hwnd, POINT screenPoint) {',
                   'void ShowNbaSetup(HWND owner, bool firstRun = false) {',
                   'void ShowImportantDays(HWND owner) {'):
        # ShowImportantDays uses hwnd in the current source.
        if marker not in src and marker.startswith('void ShowImportantDays'):
            marker = 'void ShowImportantDays(HWND hwnd) {'
        change(marker, marker + '\n    if (Wh_GetIntValue(L"UpdateRequested", 0)) return;')
    badge = (Path(__file__).parent / 'update-badge.h').read_text(encoding='utf-8')
    change('class Renderer {', badge + '\nclass Renderer {')
    change('    ComPtr<ID2D1SolidColorBrush> scratchColorBrush_;',
           '    ComPtr<ID2D1SolidColorBrush> scratchColorBrush_;\n    ComPtr<ID2D1SolidColorBrush> updateBadgeBrush_;')
    change('        scratchColorBrush_.Reset();', '        scratchColorBrush_.Reset();\n        updateBadgeBrush_.Reset();')
    change('        nativeGlass_.EndFrame();', '''        if (g_updateAvailable.load() && width >= 20.0f && height >= 16.0f) {
            if (!updateBadgeBrush_)
                target_->CreateSolidColorBrush(D2D1::ColorF(0xFFD60A), &updateBadgeBrush_);
            if (updateBadgeBrush_) {
                const auto badge = UpdateBadgeBounds(left, top, width, height, settings.sizeScale);
                const float unit = badge.radiusX / 6.0f;
                updateBadgeBrush_->SetColor(D2D1::ColorF(0xFFD60A));
                target_->FillEllipse(badge, updateBadgeBrush_.Get());
                updateBadgeBrush_->SetColor(D2D1::ColorF(0x171717));
                target_->DrawLine(D2D1::Point2F(badge.point.x, badge.point.y - 3.0f * unit),
                    D2D1::Point2F(badge.point.x, badge.point.y + 0.7f * unit), updateBadgeBrush_.Get(), 1.6f * unit);
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(badge.point.x, badge.point.y + 3.0f * unit),
                    0.85f * unit, 0.85f * unit), updateBadgeBrush_.Get());
            }
        }
        nativeGlass_.EndFrame();''')

    # The render thread also owns the modal dialogs: stale heartbeats defer updates.
    change('    double nextProgressPoll = 0.0;', '    double nextProgressPoll = 0.0;\n    double nextUpdaterPoll = 0.0;')
    marker = '        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);'
    change(marker, '''        if (now >= nextUpdaterPoll) {
            nextUpdaterPoll = now + 1.0;
            const bool available = Wh_GetIntValue(L"UpdateAvailable", 0) != 0;
            if (g_updateAvailable.exchange(available) != available) g_layoutDirty = true;
            const bool busy = snapshot.timer.active || g_importantDaysOpen.load() || g_lookupUi.open || g_mediaHttpActive.load() != 0;
            Wh_SetStringValue(L"UpdateVersion", WH_MOD_VERSION);
            Wh_SetIntValue(L"UpdatePid", GetCurrentProcessId());
            Wh_SetIntValue(L"UpdateHeartbeat", GetTickCount64() / 1000);
            Wh_SetIntValue(L"UpdateBusy", busy);
            Wh_SetIntValue(L"UpdateRequiresGlass", Renderer::UsesNativeGlass(g_settings));
            Wh_SetIntValue(L"UpdateReady", !busy && Wh_GetIntValue(L"UpdateRequested", 0));
        }

''' + marker)
    return src
