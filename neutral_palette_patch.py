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
    panel = panel.replace('''        for (int i = 0; i < 6; ++i) {''', '''        ComPtr<ID2D1SolidColorBrush> cardFill;
        target_->CreateSolidColorBrush(
            WithAlpha(material_.raised, material_.raised.a * 0.70f), &cardFill);
        for (int i = 0; i < 6; ++i) {''')
    panel = panel.replace('            DrawCard(card, 9.0f * scale);', '''            if (cardFill) target_->FillRoundedRectangle(
                D2D1::RoundedRect(card, 9.0f * scale, 9.0f * scale), cardFill.Get());''')
    panel = panel.replace('const D2D1_COLOR_F tint = hasBar ? LoadStateColor(m.fraction) : material_.accent;',
                          'const D2D1_COLOR_F tint = D2D1::ColorF(0xE1E1E6);')
    panel = panel.replace('WithAlpha(tint, 0.85f * settingsOpacity_)',
                          'WithAlpha(tint, 0.78f * settingsOpacity_)')
    panel = panel.replace('CreateSolidColorBrush(material_.raisedStrong, &trackBrush)',
                          'CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF, 0.10f * settingsOpacity_), &trackBrush)')
    panel = panel.replace('WithAlpha(tint, 0.95f)', 'WithAlpha(tint, 0.72f * settingsOpacity_)')
    src = src[:start] + panel + src[end:]

    assert src.count('hovered ? material_.accent : material_.textPrimary') == 3
    src = src.replace('hovered ? material_.accent : material_.textPrimary', 'material_.textPrimary')
    return src
