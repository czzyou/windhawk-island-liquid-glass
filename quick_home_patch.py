from pathlib import Path


def apply_quick_home(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    helper = (Path(__file__).parent/'quick-home.h').read_text(encoding='utf-8')
    change('// The conversion and hit-test helpers live further down, just after Clamp().',
           helper + '\n// The conversion and hit-test helpers live further down, just after Clamp().')
    change('MediaContentPoint MediaContentFromClient(int clientX, int clientY) {',
           'MediaContentPoint MediaContentFromClient(int clientX, int clientY, bool allowCached = false) {')
    change('if (stamp == 0 || GetTickCount64() - stamp > 500) {',
           'if (stamp == 0 || (!allowCached && GetTickCount64() - stamp > 500)) {')
    start = src.index('    void DrawTimeDashboard(')
    end = src.index('        const bool micActive =', start)
    src = src[:start] + '''    void DrawQuickHomeDashboard(const SharedState& state, D2D1_RECT_F rect, const Settings& settings, double now, float scale, SYSTEMTIME& local) {
        (void)now;
        const float cx = (rect.left + rect.right) * 0.5f;
        wchar_t date[64]{};
        GetDateFormatEx(L"en-US", 0, &local, L"ddd, MMM d", date, ARRAYSIZE(date), nullptr);
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.55f);
        target_->DrawTextW(date, static_cast<UINT32>(wcslen(date)), smallTextFormat_.Get(),
            D2D1::RectF(rect.left + 24, rect.top + 14, rect.right - 30, rect.top + 34),
            mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        const wchar_t* titles[4] = {L"截图", state.system.volumeMuted ? L"声音" : L"静音", L"专注 25′", L"休息 5′"};
        const wchar_t glyphs[4] = {0xE722, state.system.volumeMuted ? wchar_t(0xE74F) : wchar_t(0xE767), 0xE916, 0xE708};
        for (int i = 0; i < 4; ++i) {
            auto box = QuickHomeButtonBounds(i, rect.right - rect.left);
            box.left += rect.left; box.right += rect.left;
            box.top += rect.top; box.bottom += rect.top;
            const bool hovered = g_hoveredQuickAction.load() == i;
            const D2D1_POINT_2F center{(box.left + box.right) * 0.5f, rect.top + 64};
            ComPtr<ID2D1SolidColorBrush> disc;
            target_->CreateSolidColorBrush(WithAlpha(hovered ? material_.accent : material_.textPrimary,
                (hovered ? 0.15f : 0.055f) * settingsOpacity_), &disc);
            if (disc) target_->FillEllipse(D2D1::Ellipse(center, 21, 21), disc.Get());
            if (iconFormat_) {
                iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                textBrush_->SetOpacity(hovered ? 1.0f : 0.82f);
                target_->DrawTextW(&glyphs[i], 1, iconFormat_.Get(),
                    D2D1::RectF(center.x - 20, center.y - 20, center.x + 20, center.y + 20),
                    textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            }
            mutedBrush_->SetOpacity(hovered ? 0.95f : 0.72f);
            target_->DrawTextW(titles[i], static_cast<UINT32>(wcslen(titles[i])), smallTextFormat_.Get(),
                D2D1::RectF(box.left - 2, rect.top + 90, box.right + 2, rect.top + 111),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        }
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textBrush_->SetOpacity(0.98f);

''' + src[end:]
    change('if (activeTabId == 3) DrawTimeDashboard(', 'if (activeTabId == 3) DrawQuickHomeDashboard(')
    change('''        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen && (pinned || isHoverExpanded)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;''', '''        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen && (pinned || isHoverExpanded)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                const bool quickHome = NormalizedTabIndex(g_settings) == 0 && !privacyActive;
                primary.height = (quickHome ? 124.0f : MediaLayout::kExpandedHeight) * g_settings.sizeScale;''')
    change('''const float expandedAlpha = IdleDashboardExpandedAlpha(dashHeight);''',
           '''const float expandedAlpha = IdleDashboardExpandedAlpha(dashHeight, NormalizedTabIndex(settings) == 0 ? 124.0f : 144.0f);''')
    change('''    static bool notificationPressed = false;''', '''    static bool notificationPressed = false;
    static int quickHomePressed = -1;''')
    change('''                if (notificationPressed) return 0;''', '''                if (notificationPressed) return 0;
                quickHomePressed = -1;
                {
                    std::lock_guard lock(g_stateMutex);
                    const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                    if (!kinds.empty() && kinds[0] == IslandKind::Idle && NormalizedTabIndex(g_settings) == 0 &&
                        !g_settings.gameOverlay && Wh_GetIntValue(L"GameOverlayPinned", 0) == 0)
                        quickHomePressed = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
                }
                if (quickHomePressed >= 0) { g_layoutDirty = true; return 0; }''')
    change('''                // File Tray: clicking a row opens that file with its default app.''', '''                if (!kinds.empty() && kinds[0] == IslandKind::Idle && currentTab == 0 && !gameMetricsPresent) {
                    const int action = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
                    const bool tap = action >= 0 && action == quickHomePressed &&
                        std::abs(xPos - s_touchStart.x) <= 8 && std::abs(yPos - s_touchStart.y) <= 8;
                    quickHomePressed = -1;
                    if (tap) {
                        if (action == 0) ShellExecuteW(hwnd, L"open", L"ms-screenclip:", nullptr, nullptr, SW_SHOWNORMAL);
                        else if (action == 1) ToggleEndpointMute();
                        else if (g_settings.timerEnabled) StartFocusTimer(action == 2 ? 25 : 5, action == 3);
                        g_layoutDirty = true;
                    }
                    return 0;
                }

                // File Tray: clicking a row opens that file with its default app.''')
    change('''        case WM_MOUSELEAVE:
''', '''        case WM_MOUSELEAVE:
            if (g_hoveredQuickAction.exchange(-1) != -1) g_layoutDirty = true;
''')
    change('''            const int xPos = GET_X_LPARAM(lParam);
            const int yPos = GET_Y_LPARAM(lParam);

            if (g_scrubbing.load())''', '''            const int xPos = GET_X_LPARAM(lParam);
            const int yPos = GET_Y_LPARAM(lParam);
            int quickHovered = -1;
            {
                std::lock_guard lock(g_stateMutex);
                const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                if (!kinds.empty() && kinds[0] == IslandKind::Idle && NormalizedTabIndex(g_settings) == 0 &&
                    !g_settings.gameOverlay && Wh_GetIntValue(L"GameOverlayPinned", 0) == 0)
                    quickHovered = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
            }
            if (g_hoveredQuickAction.exchange(quickHovered) != quickHovered) g_layoutDirty = true;

            if (g_scrubbing.load())''')
    change('''                // A File Tray row under the cursor is clickable''', '''                if (g_hoveredQuickAction.load() >= 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }

                // A File Tray row under the cursor is clickable''')
    assert 'DrawTimeDashboard' not in src
    return src
