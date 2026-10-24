from pathlib import Path


def apply_notifications(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    change('struct NotificationSnapshot {', 'constexpr double kNotificationVisibleSeconds = 10.0;\n\nstruct NotificationSnapshot {')
    start = src.index('DWORD WINAPI NotificationThreadProc(')
    end = src.index('\n#endif', start)
    listener = src[start:end]
    assert listener.count('snapshot.expiresAt = NowSeconds() + 4.0;') == 2
    src = src[:start] + listener.replace('snapshot.expiresAt = NowSeconds() + 4.0;',
        'snapshot.expiresAt = NowSeconds() + kNotificationVisibleSeconds;') + src[end:]
    change('notification.expiresAt = NowSeconds() + 4.0;',
           'notification.expiresAt = NowSeconds() + kNotificationVisibleSeconds;')
    change('''    std::thread([hwnd]() {
        Sleep(400);''', '''    double notificationExpiry = 0.0;
    { std::lock_guard lock(g_stateMutex); notificationExpiry = g_state.notification.expiresAt; }
    std::thread([hwnd, notificationExpiry]() {
        Sleep(400);''')
    change('''                            if (g_state.notification.active) {''', '''                            if (g_state.notification.active &&
                                g_state.notification.expiresAt == notificationExpiry) {''')
    start = src.index('    void DrawNotification(')
    end = src.index('    void DrawVolume(', start)
    display = src[start:end]
    assert display.count('const float ttl = 4.0f;') == 1
    src = src[:start] + display.replace('const float ttl = 4.0f;',
        'const float ttl = static_cast<float>(kNotificationVisibleSeconds);') + src[end:]

    start = src.index('bool OpenNotificationSource(')
    end = src.index('void HandleStatusClickAtPoint(', start)
    mapping = (Path(__file__).parent / 'notification-mapping.h').read_text(encoding='utf-8')
    src = src[:start] + mapping + src[end:]
    change('    static double notificationExpiry = 0.0;',
           '    static double notificationExpiry = 0.0;\n    static std::wstring notificationAppName;')
    change('                        notificationExpiry = g_state.notification.expiresAt;',
           '                        notificationExpiry = g_state.notification.expiresAt;\n                        notificationAppName = g_state.notification.app;')
    change('if (tap && OpenNotificationSource(notificationAppId))',
           'if (tap && OpenNotificationSource(notificationAppId, notificationAppName))')
    change('    AppendMenuW(menu, MF_STRING, 1, L"Dismiss");',
           '    AppendMenuW(menu, MF_STRING, 1, L"Dismiss");\n    AppendMenuW(menu, MF_STRING, 45, L"Edit notification app mappings");')
    change('''    switch (cmd) {
        case 1:''', '''    switch (cmd) {
        case 45: {
            const auto path = NotificationMappingsPath();
            const auto arguments = L"\\\"" + path + L"\\\"";
            ShellExecuteW(hwnd, L"open", L"notepad.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
            break;
        }
        case 1:''')
    assert 'g_inbox' not in src and 'NotificationInbox' not in src
    assert 'Recent notifications (' not in src
    assert src.count('expiresAt = NowSeconds() + kNotificationVisibleSeconds;') == 3
    return src
