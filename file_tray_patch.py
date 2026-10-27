from pathlib import Path


def apply_file_tray(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:80], src.count(old))
        src = src.replace(old, new, 1)

    change('#include <shellapi.h>', '#include <shellapi.h>\n#include <shlobj.h>')
    marker = 'int FileTrayRowAtContentPoint('
    assert src.count(marker) == 1
    helpers = '\n'.join(Path(__file__).with_name(name).read_text(encoding='utf-8')
                        for name in ('file-tray-drag.h', 'file-tray-delete.h'))
    src = src.replace(marker, helpers + '\n' + marker)
    change('        case WM_CREATE:\n', '''        case WM_TIMER:
            if (wParam == 0x44A) { ProcessFileTrayDeletes(); return 0; }
            break;
        case WM_CREATE:
            LoadFileTrayDeletes();
            SetTimer(hwnd, 0x44A, 60000, nullptr);
            PostMessageW(hwnd, WM_TIMER, 0x44A, 0);
''')
    change('                added.push_back(std::move(item));', '''                if (!CancelFileTrayDelete(item.path)) {
                    MessageBoxW(hwnd, L"无法取消待删除计划，文件未重新加入托盘。", L"文件托盘", MB_OK | MB_ICONERROR);
                    continue;
                }
                added.push_back(std::move(item));''')
    change('    static ULONGLONG s_touchStartTime = 0;',
           '    static std::wstring fileTrayPressed;\n    static bool fileTrayRemovePressed = false;\n    static POINT fileTrayDragStart{};\n    static ULONGLONG s_touchStartTime = 0;')
    change('                if (notificationPressed) return 0;', '''                if (notificationPressed) return 0;
                if (g_settings.fileTrayModule && NormalizedTabIndex(g_settings) == FileTrayTabIndex(g_settings)) {
                    {
                        std::lock_guard lock(g_stateMutex);
                        const auto point = MediaContentFromClient(xPos, yPos, true);
                        const int row = FileTrayRowAtContentPoint(point,
                            static_cast<int>(g_state.fileTrayItems.size()));
                        if (row >= 0) {
                            fileTrayPressed = g_state.fileTrayItems[g_state.fileTrayItems.size() - 1 - row].path;
                            fileTrayRemovePressed = FileTrayRemoveHit(point.x, point.width - FileTrayLayout::kPadX);
                        }
                    }
                    if (!fileTrayPressed.empty()) {
                        fileTrayDragStart = {xPos, yPos};
                        SetCapture(hwnd);
                        return 0;
                    }
                }''')
    change('        case WM_MOUSEMOVE: {', '''        case WM_MOUSEMOVE: {
            if (!fileTrayPressed.empty()) {
                if (!(wParam & MK_LBUTTON)) {
                    fileTrayPressed.clear();
                    ReleaseCapture();
                } else if (!fileTrayRemovePressed && FileTrayDragThreshold(fileTrayDragStart,
                    {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                    GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG))) {
                    const auto path = std::move(fileTrayPressed);
                    fileTrayPressed.clear();
                    s_touchStartTime = 0;
                    ReleaseCapture();
                    DragFileTrayItem(hwnd, path, g_stopEvent);
                    g_layoutDirty = true;
                }
                return 0;
            }''')
    change('        case WM_LBUTTONUP:', '''        case WM_LBUTTONUP:
            if (!fileTrayPressed.empty()) {
                const auto path = std::move(fileTrayPressed);
                fileTrayPressed.clear();
                ReleaseCapture();
                s_touchStartTime = 0;
                if (fileTrayRemovePressed) {
                    const auto point = MediaContentFromClient(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true);
                    bool remove = false;
                    {
                        std::lock_guard lock(g_stateMutex);
                        const int row = FileTrayRowAtContentPoint(point, static_cast<int>(g_state.fileTrayItems.size()));
                        remove = row >= 0 && FileTrayRemoveHit(point.x, point.width - FileTrayLayout::kPadX) &&
                            g_state.fileTrayItems[g_state.fileTrayItems.size() - 1 - row].path == path;
                    }
                    if (remove && ScheduleFileTrayDelete(path)) {
                        std::lock_guard lock(g_stateMutex);
                        std::erase_if(g_state.fileTrayItems, [&](const auto& item) { return item.path == path; });
                        g_hoveredFileTrayRow = -1;
                        g_layoutDirty = true;
                    } else if (remove) {
                        MessageBoxW(hwnd, L"无法保存七天删除计划，文件仍保留在托盘中。仅支持本机普通文件。",
                            L"文件托盘", MB_OK | MB_ICONERROR);
                    }
                    return 0;
                }
                if (FileTrayDragThreshold(fileTrayDragStart,
                    {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)},
                    GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG))) return 0;
                ShellExecuteW(hwnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }''')
    change('        case WM_CAPTURECHANGED:', '''        case WM_CAPTURECHANGED:
            fileTrayPressed.clear();''')
    change('hoveredRow = FileTrayRowAtContentPoint(MediaContentFromClient(xPos, yPos),',
           'hoveredRow = FileTrayRowAtContentPoint(MediaContentFromClient(xPos, yPos, true),')
    change('const wchar_t* title = Loc(L"File Tray");',
           'const wchar_t* title = L"文件托盘 · × 后7天删原文件";')
    change('            const float sizeW = 66.0f * scale;', '''            const float removeW = kFileTrayRemoveWidth * scale;
            const float centerX = row.right - removeW * 0.5f;
            const float centerY = (row.top + row.bottom) * 0.5f;
            textBrush_->SetOpacity(hovered ? 0.95f : 0.65f);
            target_->DrawLine(D2D1::Point2F(centerX - 3.5f * scale, centerY - 3.5f * scale),
                D2D1::Point2F(centerX + 3.5f * scale, centerY + 3.5f * scale), textBrush_.Get(), 1.5f * scale);
            target_->DrawLine(D2D1::Point2F(centerX + 3.5f * scale, centerY - 3.5f * scale),
                D2D1::Point2F(centerX - 3.5f * scale, centerY + 3.5f * scale), textBrush_.Get(), 1.5f * scale);
            const float sizeW = 66.0f * scale;''')
    change('row.right - sizeW - 10.0f * scale, row.bottom);',
           'row.right - removeW - sizeW - 10.0f * scale, row.bottom);')
    change('D2D1::RectF(row.right - sizeW - 8.0f * scale, row.top,\n                                                          row.right - 10.0f * scale, row.bottom);',
           'D2D1::RectF(row.right - removeW - sizeW - 8.0f * scale, row.top,\n                                                          row.right - removeW - 10.0f * scale, row.bottom);')
    return src
