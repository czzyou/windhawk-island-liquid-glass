struct GlassMenuItem {
    MSAAMENUINFO accessible{};
    std::wstring text;
    UINT command = 0;
    bool submenu = false;
    bool separator = false;
};

struct GlassMenu {
    int dpi = 96;
    int maxWidth = 640;
    HBRUSH background = nullptr;
    HBRUSH highlight = nullptr;
    HFONT font = nullptr;
    HFONT symbols = nullptr;
    std::vector<std::unique_ptr<GlassMenuItem>> items;

    GlassMenu(HWND owner, POINT point) {
        const UINT windowDpi = owner ? GetDpiForWindow(owner) : 96;
        dpi = windowDpi ? static_cast<int>(windowDpi) : 96;
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor))
            maxWidth = (std::max)(Scale(180), static_cast<int>(monitor.rcWork.right - monitor.rcWork.left) - Scale(30));
        background = CreateSolidBrush(RGB(23, 24, 27));
        highlight = CreateSolidBrush(RGB(61, 62, 68));
        font = CreateFontW(-Scale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, L"Segoe UI");
        symbols = CreateFontW(-Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH, L"Segoe MDL2 Assets");
    }
    ~GlassMenu() {
        if (font) DeleteObject(font);
        if (symbols) DeleteObject(symbols);
        if (highlight) DeleteObject(highlight);
        if (background) DeleteObject(background);
    }
    int Scale(int value) const { return MulDiv(value, dpi, 96); }

    void Prepare(HMENU menu) {
        MENUINFO appearance{sizeof(appearance)};
        appearance.fMask = MIM_STYLE | MIM_BACKGROUND;
        GetMenuInfo(menu, &appearance);
        appearance.fMask = MIM_STYLE | MIM_BACKGROUND;
        appearance.dwStyle |= MNS_NOCHECK;
        appearance.hbrBack = background ? background : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        SetMenuInfo(menu, &appearance);
        const int count = GetMenuItemCount(menu);
        for (int index = 0; index < count; ++index) {
            MENUITEMINFOW info{sizeof(info)};
            info.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
            if (!GetMenuItemInfoW(menu, index, TRUE, &info)) continue;
            auto entry = std::make_unique<GlassMenuItem>();
            entry->separator = (info.fType & MFT_SEPARATOR) != 0;
            entry->submenu = info.hSubMenu != nullptr;
            entry->command = info.wID;
            if (entry->submenu) {
                entry->command = GetMenuItemID(info.hSubMenu, 0);
                if (entry->command == 0) entry->command = GetMenuItemID(info.hSubMenu, 1);
                Prepare(info.hSubMenu);
            }
            if (info.cch) {
                entry->text.resize(info.cch + 1);
                info.dwTypeData = entry->text.data();
                info.cch = static_cast<UINT>(entry->text.size());
                if (GetMenuItemInfoW(menu, index, TRUE, &info)) entry->text.resize(info.cch);
                else entry->text.clear();
            }
            entry->accessible.dwMSAASignature = MSAA_MENU_SIG;
            entry->accessible.cchWText = static_cast<DWORD>(entry->text.size());
            entry->accessible.pszWText = entry->text.data();
            MENUITEMINFOW styled{sizeof(styled)};
            styled.fMask = MIIM_FTYPE | MIIM_DATA;
            styled.fType = info.fType | MFT_OWNERDRAW;
            styled.dwItemData = reinterpret_cast<ULONG_PTR>(entry.get());
            items.push_back(std::move(entry));
            if (!SetMenuItemInfoW(menu, index, TRUE, &styled)) items.pop_back();
        }
    }

    GlassMenuItem* Find(ULONG_PTR data) const {
        for (const auto& item : items)
            if (reinterpret_cast<ULONG_PTR>(item.get()) == data) return item.get();
        return nullptr;
    }
    bool Measure(MEASUREITEMSTRUCT* measure) const {
        if (!measure || measure->CtlType != ODT_MENU) return false;
        const auto item = Find(measure->itemData);
        if (!item) return false;
        HDC dc = GetDC(nullptr);
        SIZE size{};
        if (dc) {
            HGDIOBJ previous = SelectObject(dc, font ? static_cast<HGDIOBJ>(font) : GetStockObject(DEFAULT_GUI_FONT));
            GetTextExtentPoint32W(dc, item->text.c_str(), static_cast<int>(item->text.size()), &size);
            SelectObject(dc, previous);
            ReleaseDC(nullptr, dc);
        }
        measure->itemWidth = (std::min)(maxWidth, (std::max)(Scale(220), static_cast<int>(size.cx) + Scale(88)));
        measure->itemHeight = Scale(item->separator ? 9 : 32);
        return true;
    }
    wchar_t Icon(const GlassMenuItem& item) const {
        const UINT id = item.command;
        if (id >= kMediaSourceMenuBase && id < kMediaSourceMenuLimit) return 0xE8D6;
        if ((id >= 30 && id <= 34) || (id >= 2010 && id <= 2016)) return 0xE916;
        switch (id) {
            case 1: return 0xE711;
            case 2: case 2002: return 0xE718;
            case 3: return 0xE7FC;
            case 9: return 0xE713;
            case 11: return 0xE74A;
            case 41: case 2004: return 0xE74D;
            case 46: return 0xE787;
            case 47: case 48: return 0xEB52;
            case 49: return 0xE72C;
            case 2001: return 0xE8D6;
            case 2020: return 0xE721;
            case 2030: return 0xE793;
            default: return 0;
        }
    }
    bool Draw(DRAWITEMSTRUCT* draw) const {
        if (!draw || draw->CtlType != ODT_MENU) return false;
        const auto item = Find(draw->itemData);
        if (!item) return false;
        const int saved = SaveDC(draw->hDC);
        FillRect(draw->hDC, &draw->rcItem, background ? background : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        const RECT box = draw->rcItem;
        if (item->separator) {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(70, 71, 76));
            if (pen) {
                SelectObject(draw->hDC, pen);
                const int y = (box.top + box.bottom) / 2;
                MoveToEx(draw->hDC, box.left + Scale(14), y, nullptr);
                LineTo(draw->hDC, box.right - Scale(14), y);
                SelectObject(draw->hDC, GetStockObject(BLACK_PEN));
                DeleteObject(pen);
            }
        } else {
            const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
            if ((draw->itemState & ODS_SELECTED) && !disabled && highlight) {
                SelectObject(draw->hDC, highlight);
                SelectObject(draw->hDC, GetStockObject(NULL_PEN));
                RoundRect(draw->hDC, box.left + Scale(4), box.top + Scale(2),
                          box.right - Scale(4), box.bottom - Scale(2), Scale(12), Scale(12));
            }
            SetBkMode(draw->hDC, TRANSPARENT);
            SetTextColor(draw->hDC, disabled ? RGB(133, 134, 140) : RGB(241, 242, 246));
            SelectObject(draw->hDC, font ? static_cast<HGDIOBJ>(font) : GetStockObject(DEFAULT_GUI_FONT));
            RECT text = {box.left + Scale(28), box.top, box.right - Scale(item->submenu ? 65 : 42), box.bottom};
            DrawTextW(draw->hDC, item->text.c_str(), static_cast<int>(item->text.size()), &text,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (symbols) SelectObject(draw->hDC, symbols);
            if (draw->itemState & ODS_CHECKED) {
                const wchar_t check = 0xE73E;
                RECT area = {box.left + Scale(7), box.top, box.left + Scale(24), box.bottom};
                DrawTextW(draw->hDC, &check, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            const wchar_t icon = Icon(*item);
            if (icon) {
                RECT area = {box.right - Scale(item->submenu ? 53 : 33), box.top,
                             box.right - Scale(item->submenu ? 34 : 14), box.bottom};
                DrawTextW(draw->hDC, &icon, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            if (item->submenu) {
                const wchar_t arrow = 0xE76C;
                RECT area = {box.right - Scale(25), box.top, box.right - Scale(8), box.bottom};
                DrawTextW(draw->hDC, &arrow, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
        if (saved) RestoreDC(draw->hDC, saved);
        return true;
    }
};

thread_local GlassMenu* g_activeGlassMenu = nullptr;

LRESULT CALLBACK GlassMenuWindowHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_CREATEWND && g_activeGlassMenu) {
        HWND window = reinterpret_cast<HWND>(wParam);
        wchar_t className[32]{};
        if (GetClassNameW(window, className, ARRAYSIZE(className)) && wcscmp(className, L"#32768") == 0) {
            SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(window, 0, 210, LWA_ALPHA);
            const int rounded = 2;
            DwmSetWindowAttribute(window, static_cast<DWMWINDOWATTRIBUTE>(33), &rounded, sizeof(rounded));
            if (const auto setAttribute = Backdrop::Resolve()) {
                Backdrop::AccentPolicy policy{Backdrop::ACCENT_ENABLE_ACRYLICBLURBEHIND, 2, 0x9C202020, 0};
                Backdrop::CompositionAttributeData data{Backdrop::WCA_ACCENT_POLICY, &policy, sizeof(policy)};
                setAttribute(window, &data);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

UINT TrackGlassMenu(HMENU menu, POINT point, HWND owner) {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    BOOL screenReader = FALSE;
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    SystemParametersInfoW(SPI_GETSCREENREADER, 0, &screenReader, 0);
    if (screenReader || (contrast.dwFlags & HCF_HIGHCONTRASTON)) {
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
        DestroyMenu(menu);
        return command;
    }
    GlassMenu context(owner, point);
    g_activeGlassMenu = &context;
    context.Prepare(menu);
    HHOOK hook = SetWindowsHookExW(WH_CBT, GlassMenuWindowHook, nullptr, GetCurrentThreadId());
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
    if (hook) UnhookWindowsHookEx(hook);
    g_activeGlassMenu = nullptr;
    DestroyMenu(menu);
    return command;
}
