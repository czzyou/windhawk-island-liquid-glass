inline std::atomic<bool> g_contextMenuOpen{false};

struct GlassMenuItem {
    MSAAMENUINFO accessible{};
    std::wstring text;
    UINT command = 0;
    bool submenu = false;
    bool separator = false;
    bool infoRow = false;
};

struct GlassMenu {
    int dpi = 96;
    int maxWidth = 640;
    HBRUSH background = nullptr;
    HBRUSH highlight = nullptr;
    HPEN highlightBorder = nullptr;
    HPEN separatorPen = nullptr;
    HFONT font = nullptr;
    HFONT subFont = nullptr;
    HFONT symbols = nullptr;
    HFONT smallSymbols = nullptr;
    std::vector<std::unique_ptr<GlassMenuItem>> items;

    GlassMenu(HWND owner, POINT point) {
        const UINT windowDpi = owner ? GetDpiForWindow(owner) : 96;
        dpi = windowDpi ? static_cast<int>(windowDpi) : 96;
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor))
            maxWidth = (std::max)(Scale(200), static_cast<int>(monitor.rcWork.right - monitor.rcWork.left) - Scale(30));
        background = CreateSolidBrush(RGB(36, 37, 41));
        highlight = CreateSolidBrush(RGB(68, 71, 78));
        highlightBorder = CreatePen(PS_SOLID, 1, RGB(66, 68, 76)); // reused as subtle outer rounded-menu border
        separatorPen = CreatePen(PS_SOLID, 1, RGB(54, 56, 62));
        font = CreateFontW(-Scale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        subFont = CreateFontW(-Scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        symbols = CreateFontW(-Scale(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              DEFAULT_PITCH, L"Segoe MDL2 Assets");
        smallSymbols = CreateFontW(-Scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                   DEFAULT_PITCH, L"Segoe MDL2 Assets");
    }
    ~GlassMenu() {
        if (font) DeleteObject(font);
        if (subFont) DeleteObject(subFont);
        if (symbols) DeleteObject(symbols);
        if (smallSymbols) DeleteObject(smallSymbols);
        if (highlight) DeleteObject(highlight);
        if (highlightBorder) DeleteObject(highlightBorder);
        if (separatorPen) DeleteObject(separatorPen);
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
            info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
            if (!GetMenuItemInfoW(menu, index, TRUE, &info)) continue;
            auto entry = std::make_unique<GlassMenuItem>();
            entry->separator = (info.fType & MFT_SEPARATOR) != 0;
            entry->submenu = info.hSubMenu != nullptr;
            entry->command = info.wID;
            entry->infoRow = !entry->separator && !entry->submenu && entry->command == 0
                && ((info.fState & (MFS_DISABLED | MFS_GRAYED)) != 0);
            if (entry->submenu) {
                // Find first meaningful command ID inside submenu to resolve the parent icon
                UINT subCmd = 0;
                const int subCount = GetMenuItemCount(info.hSubMenu);
                for (int k = 0; k < subCount && subCmd == 0; ++k) {
                    const UINT candidate = GetMenuItemID(info.hSubMenu, k);
                    if (candidate != 0 && candidate != static_cast<UINT>(-1)) subCmd = candidate;
                }
                if (subCmd == 0 && subCount > 0) {
                    HMENU nested = GetSubMenu(info.hSubMenu, 0);
                    if (nested) subCmd = GetMenuItemID(nested, 0);
                }
                entry->command = subCmd;
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
            HFONT f = item->infoRow && subFont ? subFont : font;
            HGDIOBJ previous = SelectObject(dc, f ? static_cast<HGDIOBJ>(f) : GetStockObject(DEFAULT_GUI_FONT));
            GetTextExtentPoint32W(dc, item->text.c_str(), static_cast<int>(item->text.size()), &size);
            SelectObject(dc, previous);
            ReleaseDC(nullptr, dc);
        }
        const int extraW = item->infoRow ? Scale(36) : Scale(96);
        measure->itemWidth = (std::min)(maxWidth, (std::max)(Scale(216), static_cast<int>(size.cx) + extraW));
        measure->itemHeight = Scale(item->separator ? 11 : (item->infoRow ? 24 : 34));
        return true;
    }
    wchar_t Icon(const GlassMenuItem& item) const {
        const UINT id = item.command;
        if (id >= kMediaSourceMenuBase && id < kMediaSourceMenuLimit) return 0xE8D6;
        if ((id >= 30 && id <= 34) || (id >= 2010 && id <= 2016)) return 0xE916;
        if (id >= 2040 && id <= 2048) {
            if (!item.submenu && id == 2048) return 0xE9E9; // Sliders icon for parameter tuner
            return 0xE753; // Weather sun/cloud icon for weather submenu & previews
        }
        switch (id) {
            case 1: return 0xE711;       // Dismiss
            case 2: return item.submenu ? 0xE790 : 0xE718; // Appearance submenu or Pin
            case 3: return 0xE7FC;       // Game overlay
            case 9: return 0xE713;       // Settings
            case 11: return 0xE8B0;      // Touch / hover pointer
            case 41: case 2004: return 0xE74D; // Trash / clear
            case 46: return 0xE787;      // Calendar / important dates
            case 47: case 48: return 0xE734; // Star / NBA favorites & scores
            case 49: return 0xE72C;      // Update refresh
            case 2001: return 0xE8D6;    // Media & lyrics
            case 2002: return 0xE718;    // Pin lyrics
            case 2020: return 0xE721;    // Quick lookup search
            case 2030: return 0xE793;    // Ambient glow
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
            if (separatorPen) {
                SelectObject(draw->hDC, separatorPen);
                const int y = (box.top + box.bottom) / 2;
                MoveToEx(draw->hDC, box.left + Scale(16), y, nullptr);
                LineTo(draw->hDC, box.right - Scale(16), y);
            }
        } else {
            const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
            const bool selected = (draw->itemState & ODS_SELECTED) != 0 && !disabled;
            const bool checked = (draw->itemState & ODS_CHECKED) != 0;
            const bool isDismiss = !item->submenu && item->command == 1;

            if (selected && highlight) {
                SelectObject(draw->hDC, highlight);
                SelectObject(draw->hDC, GetStockObject(NULL_PEN));
                RoundRect(draw->hDC, box.left + Scale(7), box.top + Scale(2),
                          box.right - Scale(7), box.bottom - Scale(2), Scale(20), Scale(20));
            }

            SetBkMode(draw->hDC, TRANSPARENT);

            if (item->infoRow) {
                SetTextColor(draw->hDC, RGB(148, 152, 162));
                SelectObject(draw->hDC, subFont ? static_cast<HGDIOBJ>(subFont) : GetStockObject(DEFAULT_GUI_FONT));
                RECT text = {box.left + Scale(18), box.top, box.right - Scale(16), box.bottom};
                DrawTextW(draw->hDC, item->text.c_str(), static_cast<int>(item->text.size()), &text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            } else {
                // Left: soft balanced icon
                const wchar_t icon = Icon(*item);
                if (icon && symbols) {
                    SelectObject(draw->hDC, symbols);
                    const COLORREF iconCol = disabled ? RGB(114, 117, 125)
                        : (checked ? RGB(120, 186, 248)
                        : (isDismiss && selected ? RGB(244, 134, 134)
                        : (selected ? RGB(242, 244, 248) : RGB(186, 190, 198))));
                    SetTextColor(draw->hDC, iconCol);
                    RECT iconArea = {box.left + Scale(14), box.top, box.left + Scale(36), box.bottom};
                    DrawTextW(draw->hDC, &icon, 1, &iconArea, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }

                // Middle: warm soft-white text
                const COLORREF textCol = disabled ? RGB(126, 130, 138)
                    : (isDismiss && selected ? RGB(250, 174, 174)
                    : (selected ? RGB(248, 249, 252) : RGB(232, 234, 239)));
                SetTextColor(draw->hDC, textCol);
                SelectObject(draw->hDC, font ? static_cast<HGDIOBJ>(font) : GetStockObject(DEFAULT_GUI_FONT));
                RECT text = {box.left + Scale(42), box.top, box.right - Scale(34), box.bottom};
                DrawTextW(draw->hDC, item->text.c_str(), static_cast<int>(item->text.size()), &text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                // Right: soft checkmark OR subtle submenu chevron
                if (item->submenu && smallSymbols) {
                    SelectObject(draw->hDC, smallSymbols);
                    SetTextColor(draw->hDC, selected ? RGB(214, 218, 226) : RGB(142, 146, 156));
                    const wchar_t arrow = 0xE76C;
                    RECT area = {box.right - Scale(28), box.top, box.right - Scale(11), box.bottom};
                    DrawTextW(draw->hDC, &arrow, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                } else if (checked && smallSymbols) {
                    SelectObject(draw->hDC, smallSymbols);
                    SetTextColor(draw->hDC, RGB(120, 186, 248));
                    const wchar_t check = 0xE73E;
                    RECT area = {box.right - Scale(28), box.top, box.right - Scale(11), box.bottom};
                    DrawTextW(draw->hDC, &check, 1, &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
            }
        }
        if (saved) RestoreDC(draw->hDC, saved);
        return true;
    }
};

thread_local GlassMenu* g_activeGlassMenu = nullptr;

constexpr UINT_PTR kGlassMenuSubclassId = 0x474D4E55;

struct GlassMenuPopupState {
    int dpi = 96;
    int lastW = 0;
    int lastH = 0;
    bool updatingRgn = false;
};

void ApplyGlassMenuRoundRegion(HWND hwnd, GlassMenuPopupState* st) {
    if (!st || st->updatingRgn) return;
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) return;
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    if (w <= 4 || h <= 4 || (w == st->lastW && h == st->lastH)) return;
    st->lastW = w;
    st->lastH = h;
    st->updatingRgn = true;
    const int diam = MulDiv(26, st->dpi, 96); // ~13px Apple-style smooth corner radius
    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, diam, diam);
    if (rgn) {
        if (!SetWindowRgn(hwnd, rgn, FALSE)) DeleteObject(rgn);
    }
    st->updatingRgn = false;
}

void PaintGlassMenuFrame(HWND hwnd, HDC dc, int dpi) {
    if (!dc) return;
    RECT wr{}, cr{};
    if (!GetWindowRect(hwnd, &wr) || !GetClientRect(hwnd, &cr)) return;
    const int w = wr.right - wr.left;
    const int h = wr.bottom - wr.top;
    if (w <= 0 || h <= 0) return;

    POINT tl{0, 0};
    ClientToScreen(hwnd, &tl);
    const int offX = tl.x - wr.left;
    const int offY = tl.y - wr.top;
    const RECT clientInWin{ offX, offY, offX + (cr.right - cr.left), offY + (cr.bottom - cr.top) };

    HBRUSH bg = (g_activeGlassMenu && g_activeGlassMenu->background)
        ? g_activeGlassMenu->background : nullptr;
    HBRUSH tempBg = bg ? nullptr : CreateSolidBrush(RGB(36, 37, 41));
    HBRUSH fillBrush = bg ? bg : tempBg;

    // Fill only the non-client margin (wipes out Win32 #32768's hard-coded 3px white/gray 3D border)
    const int saved = SaveDC(dc);
    ExcludeClipRect(dc, clientInWin.left, clientInWin.top, clientInWin.right, clientInWin.bottom);
    RECT full{0, 0, w, h};
    FillRect(dc, &full, fillBrush);
    if (saved) RestoreDC(dc, saved);

    // Draw a subtle, soft-contrast rounded rim around the outer edge
    const int diam = MulDiv(26, dpi, 96);
    HPEN borderPen = (g_activeGlassMenu && g_activeGlassMenu->highlightBorder)
        ? g_activeGlassMenu->highlightBorder : nullptr;
    HPEN tempPen = borderPen ? nullptr : CreatePen(PS_SOLID, 1, RGB(66, 68, 76));
    const int saved2 = SaveDC(dc);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    SelectObject(dc, borderPen ? borderPen : tempPen);
    RoundRect(dc, 0, 0, w, h, diam, diam);
    if (saved2) RestoreDC(dc, saved2);

    if (tempPen) DeleteObject(tempPen);
    if (tempBg) DeleteObject(tempBg);
}

LRESULT CALLBACK GlassMenuPopupSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                            UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto* st = reinterpret_cast<GlassMenuPopupState*>(dwRefData);
    const int dpi = st ? st->dpi : 96;
    if (msg == WM_NCPAINT) {
        HDC dc = GetWindowDC(hwnd);
        if (dc) {
            PaintGlassMenuFrame(hwnd, dc, dpi);
            ReleaseDC(hwnd, dc);
        }
        return 0;
    }
    if (msg == WM_PRINT) {
        const LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        if (wParam) PaintGlassMenuFrame(hwnd, reinterpret_cast<HDC>(wParam), dpi);
        return res;
    }
    if (msg == WM_ERASEBKGND) {
        if (wParam && g_activeGlassMenu && g_activeGlassMenu->background) {
            RECT cr{};
            GetClientRect(hwnd, &cr);
            FillRect(reinterpret_cast<HDC>(wParam), &cr, g_activeGlassMenu->background);
            return 1;
        }
    }
    if (msg == WM_SIZE || msg == WM_SHOWWINDOW) {
        const LRESULT res = DefSubclassProc(hwnd, msg, wParam, lParam);
        ApplyGlassMenuRoundRegion(hwnd, st);
        return res;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, GlassMenuPopupSubclassProc, uIdSubclass);
        delete st;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK GlassMenuWindowHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HCBT_CREATEWND && g_activeGlassMenu) {
        HWND window = reinterpret_cast<HWND>(wParam);
        wchar_t className[32]{};
        if (GetClassNameW(window, className, ARRAYSIZE(className)) && wcscmp(className, L"#32768") == 0) {
            SetWindowLongPtrW(window, GWL_EXSTYLE, GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(window, 0, 232, LWA_ALPHA);
            const int rounded = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(window, static_cast<DWMWINDOWATTRIBUTE>(33), &rounded, sizeof(rounded));
            const COLORREF noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
            DwmSetWindowAttribute(window, static_cast<DWMWINDOWATTRIBUTE>(34), &noBorder, sizeof(noBorder));
            auto* st = new GlassMenuPopupState{ g_activeGlassMenu->dpi, 0, 0, false };
            if (!SetWindowSubclass(window, GlassMenuPopupSubclassProc, kGlassMenuSubclassId,
                                   reinterpret_cast<DWORD_PTR>(st))) {
                delete st;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

UINT TrackGlassMenu(HMENU menu, POINT point, HWND owner) {
    struct ContextMenuGuard {
        ContextMenuGuard() { g_contextMenuOpen.store(true, std::memory_order_relaxed); }
        ~ContextMenuGuard() { g_contextMenuOpen.store(false, std::memory_order_relaxed); }
    } menuGuard;
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
