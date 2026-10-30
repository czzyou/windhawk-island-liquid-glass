from pathlib import Path


def apply_expanded_layout(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    # Calendar/media dimensions are the shared expanded size.
    change('''            activity.width = 260.0f;
            activity.height = 84.0f;''', '''            activity.width = MediaLayout::kExpandedWidth;
            activity.height = MediaLayout::kExpandedHeight;''')
    change('''                const bool timerDetail = g_settings.timerEnabled &&
                    (snapshot.timer.active || snapshot.timer.justFinished);
                const bool quickHome = !timerDetail && NormalizedTabIndex(g_settings) == 0 && !privacyActive;
                primary.height = (timerDetail ? TimerDetailPanelHeight(snapshot, g_settings)
                    : (quickHome ? 124.0f : MediaLayout::kExpandedHeight)) * g_settings.sizeScale;''',
           '''                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;''')
    change('''                primary.height = 150.0f * g_settings.sizeScale;''',
           '''                primary.height = MediaLayout::kExpandedHeight * g_settings.sizeScale;''')
    change('''        const bool timerDetail = settings.timerEnabled &&
            (state.timer.active || state.timer.justFinished);
        const float expandedFullHeight = timerDetail
            ? TimerDetailPanelHeight(state, settings)
            : (NormalizedTabIndex(settings) == 0 ? 124.0f : 144.0f);''',
           '''        const float expandedFullHeight = MediaLayout::kExpandedHeight;''')
    # This variable-height helper no longer has callers.
    change('''float TimerDetailPanelHeight(const SharedState& state, const Settings& settings) {
    return TimerDetailLayout::kHeight +
        (PrivacyIndicatorsVisible(state, settings) ? TimerDetailLayout::kPrivacyExtra : 0.0f);
}

''', '')
    change('''    constexpr float kHeight = 164.0f;
    constexpr float kPrivacyExtra = 16.0f;''', '''    constexpr float kHeight = 184.0f;''')
    for key, old, new in [('HeaderTop',6,14),('HeaderBottom',26,36),('HeroTop',28,36),
                          ('HeroBottom',80,100),('ProgressY',86,104),('ButtonCenterY',113,125),
                          ('LabelTop',136,146),('LabelBottom',156,162),('PrivacyRowCenterY',168,172)]:
        change(f'    constexpr float k{key} = {old}.0f;', f'    constexpr float k{key} = {new}.0f;')
    # Move the quick actions and the matching hit boxes down together.
    start = src.index('    void DrawQuickHomeDashboard(')
    end = src.index('        const bool micActive =', start)
    body = src[start:end]
    for old, new in [('rect.top + 14','rect.top + 34'),('rect.top + 34','rect.top + 54'),
                     ('rect.top + 64','rect.top + 94'),('rect.top + 90','rect.top + 120'),
                     ('rect.top + 111','rect.top + 141')]:
        # Header bounds are replaced together below to avoid chained replacements.
        if old in ('rect.top + 14','rect.top + 34'): continue
        body = body.replace(old, new)
    body = body.replace('rect.top + 14, rect.right - 24, rect.top + 34',
                        'rect.top + 14, rect.right - 24, rect.top + 34')
    src = src[:start] + body + src[end:]
    # One strong numeric face, following the calendar's large-number hierarchy.
    change('    ComPtr<IDWriteTextFormat> timerHeroFormat_;',
           '    ComPtr<IDWriteTextFormat> timerHeroFormat_;\n    ComPtr<IDWriteTextFormat> hourlyTimeFormat_;')
    change('        timerHeroFormat_ = nullptr;',
           '        timerHeroFormat_ = nullptr;\n        hourlyTimeFormat_ = nullptr;')
    change('        clockFormat_.Reset();',
           '        clockFormat_.Reset();\n        hourlyTimeFormat_.Reset();')
    change('''        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 42.0f * ts, hugeTextFormat_);''', '''        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 42.0f * ts, hugeTextFormat_);
        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 60.0f * ts, hourlyTimeFormat_);
        if (hourlyTimeFormat_) {
            hourlyTimeFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            hourlyTimeFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            hourlyTimeFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }''')
    start = src.index('    void DrawHourlyClock(')
    end = src.index('    void DrawNba(', start)
    src = src[:start] + r'''    void DrawHourlyClock(const HourlyClockSnapshot& clock, D2D1_RECT_F rect) {
        IDWriteTextFormat* format = hourlyTimeFormat_ ? hourlyTimeFormat_.Get() : hugeTextFormat_.Get();
        if (!format) return;
        wchar_t label[16]{};
        swprintf_s(label, L"%02d:%02d", clock.hour, clock.minute);
        const float textWidth = MeasureTextWidthCached(label, format, hourlyClockWidthCache_);
        const float groupWidth = 80.0f + 20.0f + textWidth;
        const float cx = (rect.left + rect.right) * 0.5f;
        const float cy = rect.top + 104.0f;
        const float left = cx - groupWidth * 0.5f;
        const auto center = D2D1::Point2F(left + 40.0f, cy);
        const auto oldSmallAlign = smallTextFormat_->GetTextAlignment();
        const auto oldSmallPara = smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        mutedBrush_->SetOpacity(0.66f);
        target_->DrawTextW(L"时钟", 2, smallTextFormat_.Get(),
            D2D1::RectF(rect.left + 24, rect.top + 14, rect.right - 24, rect.top + 34),
            mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        smallTextFormat_->SetTextAlignment(oldSmallAlign);
        smallTextFormat_->SetParagraphAlignment(oldSmallPara);
        D2D1_MATRIX_3X2_F oldTransform;
        target_->GetTransform(&oldTransform);
        const float fit = (std::min)(1.0f, (rect.right - rect.left - 48.0f) / groupWidth);
        target_->SetTransform(D2D1::Matrix3x2F::Scale(fit, fit, D2D1::Point2F(cx, cy)) * oldTransform);
        ComPtr<ID2D1SolidColorBrush> face, ink;
        target_->CreateSolidColorBrush(D2D1::ColorF(0xF2F2F7, 0.96f * settingsOpacity_), &face);
        target_->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.09f, settingsOpacity_), &ink);
        if (face && ink) {
            target_->FillEllipse(D2D1::Ellipse(center, 40, 40), face.Get());
            for (int tick = 0; tick < 12; ++tick) {
                const float angle = tick * 3.14159265f / 6.0f;
                const float inner = tick % 3 == 0 ? 30.0f : 33.0f;
                target_->DrawLine(D2D1::Point2F(center.x + std::sin(angle) * inner,
                                             center.y - std::cos(angle) * inner),
                                  D2D1::Point2F(center.x + std::sin(angle) * 36.0f,
                                             center.y - std::cos(angle) * 36.0f), ink.Get(), tick % 3 == 0 ? 2.6f : 1.6f);
            }
            const float hourAngle = (clock.hour % 12 + clock.minute / 60.0f) * 3.14159265f / 6.0f;
            const float minuteAngle = clock.minute * 3.14159265f / 30.0f;
            auto hand = [&](float angle, float length, float thickness) {
                const auto end = D2D1::Point2F(center.x + std::sin(angle) * length,
                                             center.y - std::cos(angle) * length);
                target_->DrawLine(center, end, ink.Get(), thickness);
                target_->FillEllipse(D2D1::Ellipse(end, thickness * 0.5f, thickness * 0.5f), ink.Get());
            };
            hand(hourAngle, 20.0f, 5.0f);
            hand(minuteAngle, 29.0f, 3.5f);
            target_->FillEllipse(D2D1::Ellipse(center, 3.5f, 3.5f), ink.Get());
        }
        textBrush_->SetOpacity(0.97f);
        target_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), format,
            D2D1::RectF(left + 100, cy - 56, left + 102 + textWidth, cy + 56),
            textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        target_->SetTransform(oldTransform);
        textBrush_->SetOpacity(0.90f);
        mutedBrush_->SetOpacity(0.58f);
    }

''' + src[end:]
    # Space NBA rows across the common panel without changing the compact row.
    start = src.index('        if (expandedAlpha > 0.01f) {', src.index('    void DrawNba('))
    end = src.index('    void DrawTimer(', start)
    body = src[start:end]
    for old,new in [('rect.top + 46, rect.left + 54, rect.top + 76','rect.top + 54, rect.left + 60, rect.top + 90'),
                    ('rect.top + 94, rect.left + 54, rect.top + 124','rect.top + 116, rect.left + 60, rect.top + 152'),
                    ('drawTeamRow(rect.top + 42','drawTeamRow(rect.top + 52'),
                    ('drawTeamRow(rect.top + 90','drawTeamRow(rect.top + 114'),
                    ('rect.top + 86, rect.right - 24, rect.top + 87','rect.top + 104, rect.right - 24, rect.top + 105')]:
        assert old in body, old
        body=body.replace(old,new)
    src=src[:start]+body+src[end:]
    # The media companion keeps its existing play/pause action in the full panel.
    change('''    constexpr float kExpandedWidth = 200.0f;
    constexpr float kExpandedHeight = 44.0f;
    constexpr float kButtonInset = 22.0f;''', '''    constexpr float kExpandedWidth = 380.0f;
    constexpr float kExpandedHeight = 184.0f;''')
    change('''    if (!point.valid || point.height > 60.0f || point.width < 140.0f) return -1;
    const float dx = point.x - (point.width - MediaCompanionLayout::kButtonInset);
    const float dy = point.y - point.height * 0.5f;''', '''    if (!point.valid || point.height < 140.0f || point.width < 300.0f) return -1;
    const float dx = point.x - point.width * 0.5f;
    const float dy = point.y - (point.height - 36.0f);''')
    start=src.index('    void DrawMediaCompanion(')
    end=src.index('    void DrawIdleDashboard(', start)
    body=src[start:end]
    body=body.replace('''        const float artSize = height - 12.0f;''', '''        if (height < 120.0f) return;
        const float artSize = 64.0f;''')
    body=body.replace('''D2D1::RectF(rect.left + 6.0f, rect.top + 6.0f,
                                 rect.left + 6.0f + artSize, rect.bottom - 6.0f)''', '''D2D1::RectF(rect.left + 32.0f, rect.top + 40.0f,
                                 rect.left + 32.0f + artSize, rect.top + 40.0f + artSize)''')
    body=body.replace('rect.right - MediaCompanionLayout::kButtonInset','(rect.left + rect.right) * 0.5f')
    body=body.replace('(rect.top + rect.bottom) * 0.5f','rect.bottom - 36.0f')
    body=body.replace('rect.left + 6.0f + artSize + 9.0f','rect.left + 116.0f')
    body=body.replace('buttonCx - 20.0f','rect.right - 32.0f')
    body=body.replace('rect.top + 5.0f, textRight, rect.top + 23.0f','rect.top + 42.0f, textRight, rect.top + 70.0f')
    body=body.replace('rect.top + 21.0f, textRight, rect.top + 39.0f','rect.top + 74.0f, textRight, rect.top + 100.0f')
    src=src[:start]+body+src[end:]
    change('    constexpr float kButtonRadius = 13.0f;', '    constexpr float kButtonRadius = 21.0f;')
    # A numeric format must not inherit alignment left behind by another panel.
    start = src.index('    void DrawTimerDetail(')
    end = src.index('    void DrawTimerOrb(', start)
    body = src[start:end]
    body = body.replace('''        IDWriteTextFormat* heroFormat = timerHeroFormat_
            ? timerHeroFormat_.Get()
            : (hugeTextFormat_ ? hugeTextFormat_.Get() : textFormat_.Get());''', '''        IDWriteTextFormat* heroFormat = hourlyTimeFormat_ ? hourlyTimeFormat_.Get() : hugeTextFormat_.Get();
        const auto oldHeroAlign = heroFormat->GetTextAlignment();
        const auto oldHeroPara = heroFormat->GetParagraphAlignment();
        heroFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        heroFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);''')
    body = body.replace('''        const float barWidth = TimerDetailLayout::kProgressWidth;''', '''        heroFormat->SetTextAlignment(oldHeroAlign);
        heroFormat->SetParagraphAlignment(oldHeroPara);
        const float barWidth = TimerDetailLayout::kProgressWidth;''')
    body = body.replace('mutedBrush_->SetOpacity(0.66f);', 'mutedBrush_->SetOpacity(0.82f);')
    body = body.replace('''        target_->DrawTextW(header.c_str(), static_cast<UINT32>(header.size()),
            smallTextFormat_.Get(),''', '''        const auto oldHeaderAlign = textFormat_->GetTextAlignment();
        const auto oldHeaderPara = textFormat_->GetParagraphAlignment();
        textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        target_->DrawTextW(header.c_str(), static_cast<UINT32>(header.size()),
            textFormat_.Get(),''')
    body = body.replace('''        wchar_t hero[32]{};''', '''        textFormat_->SetTextAlignment(oldHeaderAlign);
        textFormat_->SetParagraphAlignment(oldHeaderPara);
        wchar_t hero[32]{};''')
    body = body.replace('(hovered ? 0.16f : 0.06f)', '(hovered ? 0.10f : 0.035f)')
    body = body.replace('D2D1::Ellipse(center, TimerDetailLayout::kButtonRadius,\n                                  TimerDetailLayout::kButtonRadius)',
                        'D2D1::Ellipse(center, 16.0f, 16.0f)')
    body = body.replace('center.x - 20, center.y - 20, center.x + 20, center.y + 20',
                        'center.x - 16, center.y - 16, center.x + 16, center.y + 16')
    body = body.replace('hovered ? 1.0f : 0.84f', 'hovered ? 0.94f : 0.72f')
    body = body.replace('hovered ? 0.95f : 0.68f', 'hovered ? 0.86f : 0.58f')
    body = body.replace('material_.textPrimary, 0.16f * settingsOpacity_', 'material_.textPrimary, 0.12f * settingsOpacity_')
    body = body.replace('material_.textPrimary, 0.88f * settingsOpacity_', 'material_.textPrimary, 0.76f * settingsOpacity_')
    src = src[:start] + body + src[end:]
    change('    constexpr float kProgressWidth = 180.0f;', '    constexpr float kProgressWidth = 160.0f;')
    change('    constexpr float kProgressThickness = 3.5f;', '    constexpr float kProgressThickness = 2.0f;')
    # Clock is a permanent card before the optional file tray in both contexts.
    change('int count = 2;  // primary (media or clock) + calendar',
           'int count = 3;  // primary + calendar + clock')
    change('''int FileTrayTabIndex(const Settings& settings) {''', '''int ClockTabIndex(const Settings& settings) {
    return 2 + (settings.weather ? 1 : 0) + (settings.hardwareMonitorModule ? 1 : 0);
}

int FileTrayTabIndex(const Settings& settings) {''')
    change('''    return 2 + (settings.weather ? 1 : 0) + (settings.hardwareMonitorModule ? 1 : 0);
}

// ── Localization''',
           '''    return ClockTabIndex(settings) + 1;
}

// ── Localization''')
    old = '            if (settings.fileTrayModule) activeTabs.push_back(4);'
    assert src.count(old) == 2
    src = src.replace(old, '            activeTabs.push_back(5); // Clock\n' + old)
    change('''            else if (activeTabId == 4) DrawFileTrayDashboard(state, rect, settings, scale);''', '''            else if (activeTabId == 5) DrawHourlyClock({true, local.wHour, local.wMinute, 0}, rect);
            else if (activeTabId == 4) DrawFileTrayDashboard(state, rect, settings, scale);''')
    change('''            } else if (activeTabId == 4) {
                DrawFileTrayDashboard(state, rect, settings, 1.0f);''', '''            } else if (activeTabId == 5) {
                SYSTEMTIME local{}; GetLocalTime(&local);
                DrawHourlyClock({true, local.wHour, local.wMinute, 0}, rect);
            } else if (activeTabId == 4) {
                DrawFileTrayDashboard(state, rect, settings, 1.0f);''')
    change('''        if (primary.kind == IslandKind::Idle && !isHidden) {''', '''        if ((primary.kind == IslandKind::Idle ||
             (primary.kind == IslandKind::Media && NormalizedTabIndex(g_settings) == ClockTabIndex(g_settings))) && !isHidden) {''')
    return src
