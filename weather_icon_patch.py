from pathlib import Path


def apply_weather_icons(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    change('struct MarqueeLayoutCache {',
           Path(__file__).with_name('weather-emoji.h').read_text(encoding='utf-8') + '\nstruct MarqueeLayoutCache {')
    start = src.index('    void DrawCloudShape(')
    end = src.index('    // Small vector glyphs', start)
    src = src[:start] + '''    void DrawWeatherIcon(D2D1_POINT_2F center, float size, WeatherVisual visual,
                         ID2D1Brush* strong, ID2D1Brush* soft, bool night = false,
                         float temperature = NAN, bool fahrenheit = false) {
        const auto emoji = WeatherEmoji(visual, night, temperature, fahrenheit);
        const auto count = std::count_if(emoji.begin(), emoji.end(), [](wchar_t c) {
            return c != 0xFE0F && (c < 0xDC00 || c > 0xDFFF);
        });
        const float fontSize = size / static_cast<float>(std::max<ptrdiff_t>(1, count));
        if (!weatherEmojiFormat_ || weatherEmojiFontSize_ != fontSize) {
            weatherEmojiFormat_.Reset();
            if (FAILED(dwriteFactory_->CreateTextFormat(L"Segoe UI Emoji", nullptr,
                    DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    fontSize, L"", &weatherEmojiFormat_))) return;
            weatherEmojiFontSize_ = fontSize;
            weatherEmojiFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            weatherEmojiFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            weatherEmojiFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        target_->DrawTextW(emoji.c_str(), static_cast<UINT32>(emoji.size()), weatherEmojiFormat_.Get(),
            D2D1::RectF(center.x - size * 0.6f, center.y - size * 0.6f,
                        center.x + size * 0.6f, center.y + size * 0.6f),
            strong, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }

''' + src[end:]
    change('    ComPtr<IDWriteTextFormat> weatherDescFormat_;', '''    ComPtr<IDWriteTextFormat> weatherEmojiFormat_;
    float weatherEmojiFontSize_ = -1.0f;
    ComPtr<IDWriteTextFormat> weatherDescFormat_;''')
    change('WeatherVisualFromCode(page == 0 ? state.weather.weatherCode : forecast.weatherCode),\n                            iconStrong.Get(), iconSoft.Get());',
           'WeatherVisualFromCode(page == 0 ? state.weather.weatherCode : forecast.weatherCode),\n                            iconStrong.Get(), iconSoft.Get(), page == 0 && ChinaWeatherNight(ChinaWeatherClock().wHour),\n                            page == 0 ? state.weather.temperature : forecast.high, settings.weatherFahrenheit);')

    start = src.index('            // Mirrors the label DrawIdleDashboard builds')
    end = src.index('\n        }', start)
    src = src[:start] + '''            const bool hasData = state.weather.hasData && (now - state.weather.lastUpdated < 3600.0);
            const auto emoji = WeatherEmoji(hasData ? WeatherVisualFromCode(state.weather.weatherCode) : WeatherVisual::Unknown,
                ChinaWeatherNight(ChinaWeatherClock().wHour), hasData ? state.weather.temperature : NAN, settings.weatherFahrenheit);
            const auto label = emoji + L" " + (hasData ? WeatherTemperatureText(state.weather.temperature) : L"--") + L"°";
            metrics.weatherWidth = MeasureTextWidthCached(WidestDigitForm(label), fmt, idleWeatherWidthCache_);''' + src[end:]

    start = src.index('                wchar_t weatherLabel[32] = {};')
    end = src.index('\n            }', start)
    src = src[:start] + '''                const auto emoji = WeatherEmoji(hasWeather ? WeatherVisualFromCode(state.weather.weatherCode) : WeatherVisual::Unknown,
                    ChinaWeatherNight(ChinaWeatherClock().wHour), hasWeather ? state.weather.temperature : NAN, settings.weatherFahrenheit);
                const auto weatherLabel = emoji + L" " + (hasWeather ? WeatherTemperatureText(state.weather.temperature) : L"--") + L"°";
                const D2D1_RECT_F wRect =
                    D2D1::RectF(slotX, rect.top, slotX + idleMetrics.weatherWidth, rect.bottom);
                target_->DrawTextW(weatherLabel.c_str(), static_cast<UINT32>(weatherLabel.size()), idleFmt,
                                   wRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);''' + src[end:]
    return src
