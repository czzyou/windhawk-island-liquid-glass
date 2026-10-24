def apply_weather_icons(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    start = src.index('    void DrawWeatherIcon(')
    end = src.index('    // Small vector glyphs', start)
    icon = src[start:end]
    icon = icon.replace('        switch (visual) {', '''        // Opaque cloud stops keep overlapping primitives free of internal seams.
        auto gradient = [&](UINT32 top, UINT32 bottom) {
            D2D1_GRADIENT_STOP stops[] = {
                {0.0f, D2D1::ColorF(top)}, {1.0f, D2D1::ColorF(bottom)}};
            ComPtr<ID2D1GradientStopCollection> collection;
            ComPtr<ID2D1LinearGradientBrush> brush;
            if (SUCCEEDED(target_->CreateGradientStopCollection(stops, 2, &collection))) {
                target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                    D2D1::Point2F(center.x, center.y - 0.5f * s),
                    D2D1::Point2F(center.x, center.y + 0.5f * s)), collection.Get(), &brush);
            }
            if (brush) brush->SetOpacity(settingsOpacity_);
            return brush;
        };
        auto cloud = gradient(0xFFFFFF, 0xCDD9ED);
        auto shade = gradient(0xC5D0E2, 0x98ABC7);
        auto sun = gradient(0xFFE578, 0xFFAA32);
        auto rain = gradient(0x9CDEFF, 0x55B4F6);
        if (cloud) strong = cloud.Get();
        if (shade) soft = shade.Get();
        ID2D1Brush* warm = sun ? sun.Get() : strong;
        ID2D1Brush* cool = rain ? rain.Get() : soft;

        switch (visual) {''', 1)
    icon = icon.replace('DrawSunShape(center, s, strong, 0.28f)',
                        'DrawSunShape(center, s, warm, 0.28f)')
    icon = icon.replace('s * 0.62f, soft, 0.30f)', 's * 0.62f, warm, 0.30f)')
    icon = icon.replace('DrawLightningShape(center, s, strong)', 'DrawLightningShape(center, s, warm)')
    icon = icon.replace('y + 0.26f * s), soft, stroke)', 'y + 0.26f * s), cool, stroke)')
    icon = icon.replace('soft, std::max(1.0f, 0.035f * s)', 'strong, std::max(1.0f, 0.035f * s)')
    src = src[:start] + icon + src[end:]
    change('(coreRadius + 0.09f) * s', '(coreRadius + 0.07f) * s')
    change('(coreRadius + 0.21f) * s', '(coreRadius + 0.15f) * s')

    start = src.index('            // Mirrors the label DrawIdleDashboard builds')
    end = src.index('\n        }', start)
    src = src[:start] + '''            const bool hasData = state.weather.hasData && (now - state.weather.lastUpdated < 3600.0);
            wchar_t label[32] = {};
            if (hasData) swprintf_s(label, L"%.0f\\x00B0", state.weather.temperature);
            else wcscpy_s(label, ARRAYSIZE(label), L"--\\x00B0");
            metrics.weatherWidth = 24.0f * settings.sizeScale +
                MeasureTextWidthCached(WidestDigitForm(label), fmt, idleWeatherWidthCache_);''' + src[end:]

    start = src.index('                wchar_t weatherLabel[32] = {};')
    end = src.index('\n            }', start)
    src = src[:start] + '''                wchar_t weatherLabel[32] = {};
                if (hasWeather) swprintf_s(weatherLabel, L"%.0f\\x00B0", state.weather.temperature);
                else wcscpy_s(weatherLabel, ARRAYSIZE(weatherLabel), L"--\\x00B0");
                const float iconSlot = 24.0f * scale;
                DrawWeatherIcon(D2D1::Point2F(slotX + 10.0f * scale, (rect.top + rect.bottom) * 0.5f),
                                20.0f * scale,
                                hasWeather ? WeatherVisualFromCode(state.weather.weatherCode) : WeatherVisual::Unknown,
                                textBrush_.Get(), textBrush_.Get());
                const D2D1_RECT_F wRect =
                    D2D1::RectF(slotX + iconSlot, rect.top, slotX + idleMetrics.weatherWidth, rect.bottom);
                target_->DrawTextW(weatherLabel, static_cast<UINT32>(wcslen(weatherLabel)), idleFmt,
                                   wRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);''' + src[end:]
    return src
