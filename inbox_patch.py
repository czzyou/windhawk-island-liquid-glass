from pathlib import Path


def apply_inbox(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    header = (Path(__file__).parent / 'notification-inbox.h').read_text(encoding='utf-8')
    change('    std::wstring sourceAppId;\n    std::wstring title;', '''    std::wstring sourceAppId;
    std::wstring fullText;
    std::wstring inboxLabel;
    uint32_t systemId = 0;
    uint64_t token = 0;
    SYSTEMTIME receivedTime{};
    std::wstring title;''')
    change('struct VolumeSnapshot {', header + '\nstruct VolumeSnapshot {')
    change('SharedState g_state;', '''SharedState g_state;
NotificationInbox g_inbox; // Guarded by g_stateMutex; notification text stays in memory.
std::atomic<size_t> g_inboxCount{0};
std::atomic<bool> g_inboxBrowsing{false};
bool g_inboxSuppressHover = false; // Render/UI thread only.
double g_inboxOpenUntil = 0.0;

void RememberNotification(NotificationSnapshot& item) {
    GetLocalTime(&item.receivedTime);
    if (item.fullText.empty()) item.fullText = item.title;
    if (item.fullText.size() > 4096) item.fullText.resize(4096);
    g_inbox.Push(item, g_inboxBrowsing.load());
    g_inboxCount.store(g_inbox.items.size());
}

void ShowInboxSelection(double now) {
    if (g_inbox.items.empty()) {
        g_inboxBrowsing = false;
        g_state.notification.active = false;
        return;
    }
    g_state.notification = g_inbox.items[g_inbox.selected];
    auto& item = g_state.notification;
    item.active = true;
    item.expiresAt = now + 3600.0;
    wchar_t stamp[16]{};
    swprintf_s(stamp, L"%02d:%02d", item.receivedTime.wHour, item.receivedTime.wMinute);
    item.inboxLabel = item.app + L"  ·  " + stamp + L"  ·  " +
        std::to_wstring(g_inbox.selected + 1) + L"/" + std::to_wstring(g_inbox.items.size());
}''')
    change('snapshot.expiresAt = NowSeconds() + 4.0;\n                                g_state.notification = std::move(snapshot);', '''snapshot.expiresAt = NowSeconds() + 4.0;
                                snapshot.systemId = id;
                                RememberNotification(snapshot);
                                if (!g_inboxBrowsing.load()) g_state.notification = std::move(snapshot);''')
    change('        g_state.notification = std::move(notification);', '''        RememberNotification(notification);
        if (!g_inboxBrowsing.load()) g_state.notification = std::move(notification);''')
    change('''                            if (snapshot.title.size() > 120) {''', '''                            snapshot.fullText = snapshot.title;
                            if (snapshot.title.size() > 120) {''')
    # Keep the accessible toast fallback from editing whichever newer notification arrived meanwhile.
    change('''    std::thread([hwnd]() {
        Sleep(400);''', '''    double notificationExpiry = 0.0;
    { std::lock_guard lock(g_stateMutex); notificationExpiry = g_state.notification.expiresAt; }
    std::thread([hwnd, notificationExpiry]() {
        Sleep(400);''')
    change('''                            if (g_state.notification.active) {''', '''                            if (g_state.notification.active && !g_inboxBrowsing.load() &&
                                g_state.notification.expiresAt == notificationExpiry) {''')
    change('''                                g_state.notification.title = fullText;''', '''                                g_state.notification.title = fullText;
                                g_state.notification.fullText = fullText;
                                for (auto& entry : g_inbox.items) {
                                    if (entry.token == g_state.notification.token) {
                                        entry = g_state.notification;
                                        break;
                                    }
                                }''')
    change('''        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);''', '''        static double inboxHoverSince = 0.0;
        RECT inboxWindow{}; POINT inboxCursor{};
        GetWindowRect(hwnd, &inboxWindow); GetCursorPos(&inboxCursor);
        RECT inboxHit{inboxWindow.left + static_cast<LONG>(kRenderPadX),
            inboxWindow.top + static_cast<LONG>(kRenderPadY),
            inboxWindow.left + static_cast<LONG>(kRenderPadX + widthSpring.value),
            inboxWindow.top + static_cast<LONG>(kRenderPadY + heightSpring.value)};
        const bool inboxHover = (PtInRect(&inboxHit, inboxCursor) || now < g_inboxOpenUntil) &&
            !(GetAsyncKeyState(VK_CONTROL) & 0x8000) && !g_isFullscreen.load();
        if (!inboxHover) {
            inboxHoverSince = 0.0;
            g_inboxSuppressHover = false;
            if (g_inboxBrowsing.exchange(false)) {
                std::lock_guard lock(g_stateMutex);
                g_state.notification.active = false;
                snapshot.notification.active = false;
                g_layoutDirty = true;
            }
        } else if (!g_inboxSuppressHover && g_inboxCount.load() &&
            (g_inboxBrowsing.load() || snapshot.notification.active || !snapshot.media.available)) {
            if (inboxHoverSince == 0.0) inboxHoverSince = now;
            if (now - inboxHoverSince >= 0.25 || g_inboxBrowsing.load()) {
                if (!g_inboxBrowsing.exchange(true)) g_layoutDirty = true;
                std::lock_guard lock(g_stateMutex);
                ShowInboxSelection(now);
                snapshot.notification = g_state.notification;
            }
        }
        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);''')
    change('''            activity.width = 360.0f;
            activity.height = 58.0f;''', '''            activity.width = g_inboxBrowsing.load() ? 420.0f : 360.0f;
            activity.height = g_inboxBrowsing.load() ? 144.0f : 58.0f;''')
    change('''    void DrawNotification(const SharedState& state, D2D1_RECT_F rect) {
''', '''    void DrawNotification(const SharedState& state, D2D1_RECT_F rect) {
        if (g_inboxBrowsing.load() && rect.bottom - rect.top >= 100.0f) {
            const auto& item = state.notification;
            D2D1_RECT_F heading{rect.left + 20, rect.top + 10, rect.right - 20, rect.top + 30};
            target_->DrawTextW(item.inboxLabel.c_str(), static_cast<UINT32>(item.inboxLabel.size()),
                smallTextFormat_.Get(), heading, mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            const auto& body = item.fullText.empty() ? item.title : item.fullText;
            ComPtr<IDWriteTextLayout> layout;
            if (SUCCEEDED(dwriteFactory_->CreateTextLayout(body.c_str(), static_cast<UINT32>(body.size()),
                textFormat_.Get(), rect.right - rect.left - 40, rect.bottom - rect.top - 64, &layout))) {
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                target_->DrawTextLayout(D2D1::Point2F(rect.left + 20, rect.top + 34), layout.Get(),
                    textBrush_.Get(), static_cast<D2D1_DRAW_TEXT_OPTIONS>(D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_NO_SNAP));
            }
            const wchar_t* hint = L"Scroll to browse · Click to open · Right-click to remove";
            D2D1_RECT_F footer{rect.left + 20, rect.bottom - 25, rect.right - 20, rect.bottom - 7};
            target_->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), smallTextFormat_.Get(),
                footer, mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            return;
        }
''')
    change('''        case WM_MOUSEWHEEL: {
''', '''        case WM_MOUSEWHEEL: {
            if (g_inboxBrowsing.load()) {
                std::lock_guard lock(g_stateMutex);
                g_inbox.Move(GET_WHEEL_DELTA_WPARAM(wParam) < 0 ? 1 : -1);
                ShowInboxSelection(NowSeconds());
                g_layoutDirty = true;
                return 0;
            }
''')
    change('''        case WM_RBUTTONUP: {
''', '''        case WM_RBUTTONUP: {
            if (g_inboxBrowsing.load()) {
                std::lock_guard lock(g_stateMutex);
                g_inbox.RemoveSelected();
                g_inboxCount = g_inbox.items.size();
                ShowInboxSelection(NowSeconds());
                if (g_inbox.items.empty()) g_inboxSuppressHover = true;
                g_layoutDirty = true;
                return 0;
            }
''')
    change('''    AppendMenuW(menu, MF_STRING, 1, L"Dismiss");''', '''    AppendMenuW(menu, MF_STRING, 1, L"Dismiss");
    const auto inboxLabel = L"Recent notifications (" + std::to_wstring(g_inboxCount.load()) + L")";
    AppendMenuW(menu, MF_STRING | (g_inboxCount.load() ? 0 : MF_GRAYED), 42, inboxLabel.c_str());
    AppendMenuW(menu, MF_STRING, 44, L"Clear notification inbox");
    AppendMenuW(menu, MF_STRING, 45, L"Edit notification app mappings");''')
    change('''    switch (cmd) {
        case 1:''', '''    switch (cmd) {
        case 42: {
            std::lock_guard lock(g_stateMutex);
            g_inboxSuppressHover = false;
            g_inboxBrowsing = true;
            g_inboxOpenUntil = NowSeconds() + 2.0;
            ShowInboxSelection(NowSeconds());
            g_layoutDirty = true;
            break;
        }
        case 44: {
            std::lock_guard lock(g_stateMutex);
            g_inbox.items.clear(); g_inbox.selected = 0; g_inboxCount = 0;
            g_state.notification.active = false; g_inboxBrowsing = false;
            g_inboxSuppressHover = true; g_layoutDirty = true;
            break;
        }
        case 45: {
            const auto path = NotificationMappingsPath();
            const auto arguments = L"\\\"" + path + L"\\\"";
            ShellExecuteW(hwnd, L"open", L"notepad.exe", arguments.c_str(), nullptr, SW_SHOWNORMAL);
            break;
        }
        case 1:''')
    change('''    static double notificationExpiry = 0.0;''', '''    static uint64_t notificationToken = 0;
    static std::wstring notificationAppName;''')
    change('''                        notificationExpiry = g_state.notification.expiresAt;''', '''                        notificationToken = g_state.notification.token;
                        notificationAppName = g_state.notification.app;''')
    change('''if (tap && OpenNotificationSource(notificationAppId))''', '''if (tap && OpenNotificationSource(notificationAppId, notificationAppName))''')
    change('''                        if (g_state.notification.expiresAt == notificationExpiry)
                            g_state.notification.active = false;''', '''                        if (g_state.notification.token == notificationToken)
                            g_state.notification.active = false;
                        g_inboxBrowsing = false;
                        g_inboxSuppressHover = true;''')
    change('''bool OpenNotificationSource(const std::wstring& appId) {
    const auto target = NotificationLaunchTarget(appId);''', '''std::wstring NotificationMappingsPath() {
    wchar_t path[MAX_PATH]{};
    if (!ExpandEnvironmentStringsW(L"%APPDATA%\\\\Windhawk-Island\\\\notification-apps.ini", path, ARRAYSIZE(path)))
        return L"";
    return path;
}

bool OpenNotificationSource(const std::wstring& appId, const std::wstring& appName) {
    std::wstring target;
    const auto mappingPath = NotificationMappingsPath();
    wchar_t mapped[2048]{};
    if (!appId.empty()) GetPrivateProfileStringW(L"Apps", appId.c_str(), L"", mapped, ARRAYSIZE(mapped), mappingPath.c_str());
    if (!mapped[0] && !appName.empty()) GetPrivateProfileStringW(L"Apps", appName.c_str(), L"", mapped, ARRAYSIZE(mapped), mappingPath.c_str());
    if (mapped[0]) {
        target = mapped;
        if (target.starts_with(L"app:")) target = NotificationLaunchTarget(target.substr(4));
        else {
            wchar_t expanded[2048]{};
            const DWORD length = ExpandEnvironmentStringsW(target.c_str(), expanded, ARRAYSIZE(expanded));
            if (!length || length > ARRAYSIZE(expanded)) target.clear();
            else target = expanded;
            const DWORD attributes = GetFileAttributesW(target.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) ||
                !EqualsNoCase(target.substr(target.size() > 4 ? target.size() - 4 : 0), L".exe"))
                target = NotificationLaunchTarget(appId);
            struct FocusSearch { const std::wstring* path; HWND found = nullptr; } search{&target};
            EnumWindows([](HWND window, LPARAM data) WINAPI -> BOOL {
                auto& search = *reinterpret_cast<FocusSearch*>(data);
                if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
                std::wstring image;
                if (ProcessImageNameForWindow(window, &image) && EqualsNoCase(image, *search.path)) {
                    search.found = window; return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&search));
            if (search.found) {
                if (IsIconic(search.found)) ShowWindow(search.found, SW_RESTORE);
                if (SetForegroundWindow(search.found)) return true;
            }
        }
    }
    if (target.empty()) target = NotificationLaunchTarget(appId);''')
    return src
