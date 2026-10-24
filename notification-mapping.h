std::wstring NotificationMappingsPath() {
    wchar_t path[MAX_PATH]{};
    if (!ExpandEnvironmentStringsW(L"%APPDATA%\\Windhawk-Island\\notification-apps.ini", path, ARRAYSIZE(path)))
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
    if (target.empty()) target = NotificationLaunchTarget(appId);
    SHELLEXECUTEINFOW launch{sizeof(launch)};
    launch.fMask = SEE_MASK_FLAG_NO_UI;
    launch.lpFile = target.c_str();
    launch.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&launch)) return true;
    if (target == L"ms-actioncenter:") return false;
    launch.lpFile = L"ms-actioncenter:";
    return ShellExecuteExW(&launch) != FALSE;
}
