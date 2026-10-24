struct NbaSetupDialog {
    HWND available = nullptr, selected = nullptr, enabled = nullptr;
    std::vector<std::wstring> teams;
    bool firstRun = false;
};

void RefreshNbaSetup(NbaSetupDialog& state, int selection = -1) {
    SendMessageW(state.selected, LB_RESETCONTENT, 0, 0);
    for (const auto& code : state.teams) {
        const auto label = NbaFavoritesLabel(code) + L" (" + code + L")";
        SendMessageW(state.selected, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    SendMessageW(state.selected, LB_SETCURSEL, selection, 0);
}

bool SaveNbaSetup(HWND hwnd, NbaSetupDialog& state, bool enabled) {
    std::wstring value;
    for (const auto& code : state.teams) {
        if (!value.empty()) value += L",";
        value += code;
    }
    if (!enabled) value = L"off";
    else if (value.empty()) value = L"all";
    if (!Wh_SetStringValue(L"NBASetup", value.c_str())) {
        MessageBoxW(hwnd, L"保存失败，请重试。原来的选择已保留。", L"NBA 主队", MB_OK | MB_ICONERROR);
        return false;
    }
    LoadSettings();
    EndDialog(hwnd, IDOK);
    return true;
}

INT_PTR CALLBACK NbaSetupDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<NbaSetupDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    if (message == WM_INITDIALOG) {
        state = reinterpret_cast<NbaSetupDialog*>(lParam);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(state));
        SetWindowTextW(hwnd, state->firstRun ? L"欢迎使用灵动岛 · 选择 NBA 主队" : L"NBA 主队设置");
        const float scale = GetDpiForWindow(hwnd) / 96.0f;
        SetWindowPos(hwnd, nullptr, 0, 0, int(600 * scale), int(450 * scale), SWP_NOMOVE | SWP_NOZORDER);
        auto control = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                           int x, int y, int w, int h, int id) {
            HWND item = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                int(x * scale), int(y * scale), int(w * scale), int(h * scale), hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
            SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            return item;
        };
        control(L"STATIC", L"选择后保存，以后开机不再询问；右键灵动岛可随时修改。", 0, 18, 14, 550, 24, 0);
        state->enabled = control(L"BUTTON", L"开启 NBA 实时比分", BS_AUTOCHECKBOX | WS_TABSTOP, 18, 44, 300, 26, 100);
        SendMessageW(state->enabled, BM_SETCHECK, ReadNbaSetup() == L"off" ? BST_UNCHECKED : BST_CHECKED, 0);
        control(L"STATIC", L"全部球队", 0, 18, 82, 220, 24, 0);
        control(L"STATIC", L"我的主队（从上到下优先）", 0, 300, 82, 260, 24, 0);
        const auto listStyle = WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
        state->available = control(L"LISTBOX", L"", listStyle, 18, 108, 220, 222, 101);
        state->selected = control(L"LISTBOX", L"", listStyle, 300, 108, 260, 190, 102);
        for (const auto& team : kNbaTeams) {
            const auto label = std::wstring(team.name) + L" (" + team.code + L")";
            SendMessageW(state->available, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        control(L"BUTTON", L"添加 →", WS_TABSTOP, 242, 148, 54, 32, 103);
        control(L"BUTTON", L"移除", WS_TABSTOP, 242, 190, 54, 32, 104);
        control(L"BUTTON", L"上移", WS_TABSTOP, 300, 304, 80, 28, 105);
        control(L"BUTTON", L"下移", WS_TABSTOP, 390, 304, 80, 28, 106);
        control(L"STATIC", L"开启后不添加主队 = 显示全部球队的直播。", 0, 18, 342, 540, 24, 0);
        control(L"BUTTON", L"保存", WS_TABSTOP | BS_DEFPUSHBUTTON, 300, 374, 110, 30, IDOK);
        control(L"BUTTON", state->firstRun ? L"暂不开启" : L"取消", WS_TABSTOP, 430, 374, 130, 30, IDCANCEL);
        const auto saved = ReadNbaSetup();
        size_t begin = 0;
        while (begin < saved.size()) {
            const auto end = saved.find(L',', begin);
            const auto code = CanonicalNbaTeam(saved.substr(begin, end == std::wstring::npos ? end : end - begin));
            if (!code.empty()) state->teams.push_back(code);
            if (end == std::wstring::npos) break;
            begin = end + 1;
        }
        RefreshNbaSetup(*state);
        return TRUE;
    }
    if (!state) return FALSE;
    if (message == WM_COMMAND) {
        const int command = LOWORD(wParam);
        if (command == 103 || (command == 101 && HIWORD(wParam) == LBN_DBLCLK)) {
            const int index = static_cast<int>(SendMessageW(state->available, LB_GETCURSEL, 0, 0));
            if (index >= 0 && index < static_cast<int>(std::size(kNbaTeams))) {
                const std::wstring code = kNbaTeams[index].code;
                if (std::find(state->teams.begin(), state->teams.end(), code) == state->teams.end()) {
                    state->teams.push_back(code);
                    RefreshNbaSetup(*state, static_cast<int>(state->teams.size()) - 1);
                }
            }
            return TRUE;
        }
        const int index = static_cast<int>(SendMessageW(state->selected, LB_GETCURSEL, 0, 0));
        if (command >= 104 && command <= 106 && index >= 0 && index < static_cast<int>(state->teams.size())) {
            if (command == 104) state->teams.erase(state->teams.begin() + index);
            else {
                const int next = index + (command == 105 ? -1 : 1);
                if (next >= 0 && next < static_cast<int>(state->teams.size())) {
                    std::swap(state->teams[index], state->teams[next]);
                    RefreshNbaSetup(*state, next);
                    return TRUE;
                }
            }
            RefreshNbaSetup(*state, (std::min)(index, static_cast<int>(state->teams.size()) - 1));
            return TRUE;
        }
        if (command == IDOK) {
            SaveNbaSetup(hwnd, *state, SendMessageW(state->enabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return TRUE;
        }
        if (command == IDCANCEL) {
            if (state->firstRun) SaveNbaSetup(hwnd, *state, false);
            else EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
    }
    if (message == WM_CLOSE) {
        SendMessageW(hwnd, WM_COMMAND, IDCANCEL, 0);
        return TRUE;
    }
    return FALSE;
}

void ShowNbaSetup(HWND owner, bool firstRun = false) {
    NbaSetupDialog state;
    state.firstRun = firstRun;
    std::vector<DWORD> storage((sizeof(DLGTEMPLATE) + 6 + 3) / 4);
    auto* dialog = reinterpret_cast<DLGTEMPLATE*>(storage.data());
    dialog->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    dialog->cx = 380; dialog->cy = 260;
    g_importantDaysOpen = true;
    const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), dialog, owner,
        NbaSetupDialogProc, reinterpret_cast<LPARAM>(&state));
    g_importantDaysOpen = false;
    if (result == -1) MessageBoxW(owner, L"无法打开主队设置，请从右键菜单重试。", L"NBA 主队", MB_OK | MB_ICONERROR);
}
