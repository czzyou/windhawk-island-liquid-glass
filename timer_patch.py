from pathlib import Path


def apply_timer(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    change('std::atomic<bool> g_clickExpanded = false;', 'std::atomic<bool> g_clickExpanded = false;\nstd::atomic<bool> g_timerDashboardExpanded = false;')
    change('activities.push_back(IslandKind::Timer);', '''activities.push_back(g_timerDashboardExpanded.load() || Wh_GetIntValue(L"PinnedExpanded", 0) != 0
            ? IslandKind::Idle : IslandKind::Timer);''')
    change('''        bool isHidden = false;''', '''        // Share the timer's visible surface with all pointer hit tests.
        if (g_timerDashboardExpanded.exchange(!g_isFullscreen.load() && (pinned || isHoverExpanded)) !=
            (!g_isFullscreen.load() && (pinned || isHoverExpanded))) needsRender = true;

        bool isHidden = false;''')
    change('''        case IslandKind::Timer:
            activity.width = 260.0f;
            activity.height = 54.0f;''', '''        case IslandKind::Timer:
            activity.width = 190.0f;
            activity.height = 36.0f;''')
    start = src.index('    void DrawTimer(')
    end = src.index('    void DrawCapsLock(', start)
    timer = src[start:end]
    timer = timer.replace('const float badgeSz = (rect.bottom - rect.top) - 16.0f;', 'const float badgeSz = 24.0f;')
    a = timer.index('        const float br = badgeSz * 0.35f;')
    b = timer.index('        const float ringR', a)
    timer = timer[:a] + timer[b:]
    timer = timer.replace('const float ringR = badgeSz * 0.30f;', 'const float ringR = 9.0f;')
    timer = timer.replace('material_.raisedStrong, &ringTrack', 'WithAlpha(material_.textPrimary, 0.16f * settingsOpacity_), &ringTrack')
    a = timer.index('        D2D1_COLOR_F fgColor =')
    b = timer.index('        target_->CreateSolidColorBrush(fgColor', a)
    timer = timer[:a] + '        D2D1_COLOR_F fgColor = WithAlpha(material_.textPrimary, 0.88f * settingsOpacity_);\n' + timer[b:]
    a = timer.index('        const float cy =')
    b = timer.index('        ComPtr<ID2D1SolidColorBrush> ringTrack;', a)
    timer = timer[:a] + '''        const float cy = (rect.top + rect.bottom) * 0.5f;
        const wchar_t* label = state.timer.justFinished ? L"完成" :
            (!state.timer.running ? L"已暂停" : (state.timer.isBreak ? L"休息" : L"专注"));
        wchar_t value[32]{};
        const int seconds = static_cast<int>(std::ceil(remaining));
        swprintf_s(value, L"%d:%02d", seconds / 60, seconds % 60);
        const float labelWidth = MeasureTextWidthCached(label, smallTextFormat_.Get(), timerLabelWidthCache_);
        const float valueWidth = MeasureTextWidthCached(value, textFormat_.Get(), timerValueWidthCache_);
        const float groupWidth = 18 + 12 + labelWidth + 12 + valueWidth;
        const float left = (rect.left + rect.right - groupWidth) * 0.5f;
        const float tx = left + 30;
        const float ringR = 9.0f;
        const D2D1_POINT_2F ringCenter = D2D1::Point2F(left + ringR, cy);
''' + timer[b:]
    a = timer.index('        const float tx = badge.right + 14;')
    timer = timer[:a] + '''        const auto labelAlign = smallTextFormat_->GetTextAlignment();
        const auto labelParagraph = smallTextFormat_->GetParagraphAlignment();
        const auto valueAlign = textFormat_->GetTextAlignment();
        const auto valueParagraph = textFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.78f);
        textBrush_->SetOpacity(0.95f);
        target_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), smallTextFormat_.Get(),
            D2D1::RectF(tx, rect.top, tx + labelWidth + 1, rect.bottom), mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        target_->DrawTextW(value, static_cast<UINT32>(wcslen(value)), textFormat_.Get(),
            D2D1::RectF(tx + labelWidth + 12, rect.top, tx + labelWidth + 13 + valueWidth, rect.bottom),
            textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        smallTextFormat_->SetTextAlignment(labelAlign);
        smallTextFormat_->SetParagraphAlignment(labelParagraph);
        textFormat_->SetTextAlignment(valueAlign);
        textFormat_->SetParagraphAlignment(valueParagraph);
        textBrush_->SetOpacity(0.90f);
        mutedBrush_->SetOpacity(0.58f);
    }

'''
    src = src[:start] + timer + src[end:]
    change('        idleWeatherWidthCache_.valid = false;', '        idleWeatherWidthCache_.valid = false;\n        timerLabelWidthCache_.valid = false;\n        timerValueWidthCache_.valid = false;')
    change('    TextWidthCache idleWeatherWidthCache_;', '    TextWidthCache idleWeatherWidthCache_;\n    TextWidthCache timerLabelWidthCache_;\n    TextWidthCache timerValueWidthCache_;')
    change('''        (void)now;
        const float cx''', '''        const float cx''')
    change('''        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.55f);''', '''        if (settings.timerEnabled && state.timer.active) {
            const double remaining = state.timer.running ? std::max(0.0, state.timer.endsAt - now) : state.timer.remainingAtPause;
            const int seconds = static_cast<int>(std::ceil(remaining));
            swprintf_s(date, L"%s · %d:%02d%s", state.timer.isBreak ? L"休息" : L"专注",
                seconds / 60, seconds % 60, state.timer.running ? L"" : L" · 已暂停");
        }
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.78f);''')
    change('''        const wchar_t glyphs[4] =''', '''        if (settings.timerEnabled && state.timer.active) {
            titles[2] = state.timer.running ? L"暂停" : L"继续";
            titles[3] = L"结束";
        }
        const wchar_t glyphs[4] =''')
    change('''0xE916, 0xE708};''', '''state.timer.active ? (state.timer.running ? wchar_t(0xE769) : wchar_t(0xE768)) : wchar_t(0xE916),
            state.timer.active ? wchar_t(0xE71A) : wchar_t(0xE708)};''')
    change('''                        else if (g_settings.timerEnabled) StartFocusTimer(action == 2 ? 25 : 5, action == 3);''', '''                        else if (g_settings.timerEnabled) {
                            bool active;
                            { std::lock_guard lock(g_stateMutex); active = g_state.timer.active; }
                            if (!active) StartFocusTimer(action == 2 ? 25 : 5, action == 3);
                            else if (action == 2) ToggleTimerPause();
                            else StopFocusTimer();
                        }''')
    change('''        if (timerShown && snapshot.timer.running) {''', '''        if ((timerShown || (primary.kind == IslandKind::Idle && g_settings.timerEnabled && snapshot.timer.active)) && snapshot.timer.running) {''')
    change('''        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.78f);''', '''        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.78f);''')
    change('D2D1::RectF(rect.left + 24, rect.top + 14, rect.right - 30, rect.top + 34)',
           'D2D1::RectF(rect.left + 24, rect.top + 14, rect.right - 24, rect.top + 34)')
    change('''        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textBrush_->SetOpacity(0.98f);''', '''        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        textBrush_->SetOpacity(0.98f);''')
    return src
