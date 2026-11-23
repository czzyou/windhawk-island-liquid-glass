def apply_neutral_palette(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    start = src.index('        // The accent is the album-art / system accent like everywhere else.')
    end = src.index('        if (calendarCachedDate_', start)
    src = src[:start] + '''        ComPtr<ID2D1SolidColorBrush> accent;
        target_->CreateSolidColorBrush(
            D2D1::ColorF(0xF2F2F7, 0.80f * settingsOpacity_), &accent);
        ComPtr<ID2D1SolidColorBrush> selectedText;
        target_->CreateSolidColorBrush(
            D2D1::ColorF(0x202126, settingsOpacity_), &selectedText);

''' + src[end:]
    change('''                               accent.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);''',
           '''                               textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);''')
    start = src.index("                // Today's figure is drawn against the accent fill")
    end = src.index('            } else if (col == 0', start)
    src = src[:start] + '''                target_->DrawTextW(dayText.c_str(), static_cast<UINT32>(dayText.size()), gridFmt,
                                   cell, selectedText ? selectedText.Get() : textBrush_.Get(),
                                   D2D1_DRAW_TEXT_OPTIONS_NONE);
''' + src[end:]

    start = src.index('    void DrawHardwareMonitorDashboard(')
    end = src.index('    void DrawQuickHomeDashboard(', start)
    panel = src[start:end]
    panel = panel.replace(
        'const D2D1_COLOR_F tint = hasBar ? LoadStateColor(m.fraction) : material_.accent;',
        '''// Dynamic load color palette: cool emerald/sky (<60%) -> warm amber (60-85%) -> coral red (>85%)
            D2D1_COLOR_F tint = D2D1::ColorF(0x38BDF8);
            D2D1_COLOR_F tintStart = D2D1::ColorF(0x34D399);
            if (!hasBar) {
                tint = (m.glyph == 6) ? D2D1::ColorF(0x60A5FA) : D2D1::ColorF(0x34D399);
                tintStart = tint;
            } else if (m.fraction >= 0.85f) {
                tint = D2D1::ColorF(0xF87171);
                tintStart = D2D1::ColorF(0xFB923C);
            } else if (m.fraction >= 0.60f) {
                tint = D2D1::ColorF(0xFB923C);
                tintStart = D2D1::ColorF(0xFACC15);
            } else {
                tint = D2D1::ColorF(0x38BDF8);
                tintStart = D2D1::ColorF(0x34D399);
            }''')
    panel = panel.replace('WithAlpha(tint, 0.85f * settingsOpacity_)',
                          'WithAlpha(tint, 0.88f * settingsOpacity_)')
    panel = panel.replace('CreateSolidColorBrush(material_.raisedStrong, &trackBrush)',
                          'CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF, 0.10f * settingsOpacity_), &trackBrush)')
    panel = panel.replace(
        '''                    ComPtr<ID2D1SolidColorBrush> fillBrush;
                    if (SUCCEEDED(target_->CreateSolidColorBrush(WithAlpha(tint, 0.95f), &fillBrush)) && fillBrush) {''',
        '''                    D2D1_GRADIENT_STOP bStops[2] = {
                        { 0.0f, WithAlpha(tintStart, 0.88f * settingsOpacity_) },
                        { 1.0f, WithAlpha(tint, 0.94f * settingsOpacity_) }
                    };
                    ComPtr<ID2D1GradientStopCollection> bColl;
                    ComPtr<ID2D1LinearGradientBrush> fillBrush;
                    if (SUCCEEDED(target_->CreateGradientStopCollection(bStops, 2, &bColl)) && bColl &&
                        SUCCEEDED(target_->CreateLinearGradientBrush(
                            D2D1::LinearGradientBrushProperties(D2D1::Point2F(track.left, track.top),
                                                                D2D1::Point2F(track.left + span, track.top)),
                            bColl.Get(), &fillBrush)) && fillBrush) {''')
    src = src[:start] + panel + src[end:]

    assert src.count('hovered ? material_.accent : material_.textPrimary') == 2
    src = src.replace('hovered ? material_.accent : material_.textPrimary', 'material_.textPrimary')
    return src
