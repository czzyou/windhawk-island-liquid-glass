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
        (void)strong; (void)soft; (void)temperature; (void)fahrenheit;
        if (!target_ || !d2dFactory_) return;
        const float op = settingsOpacity_;
        const float s = size;

        ComPtr<ID2D1StrokeStyle> roundStroke;
        D2D1_STROKE_STYLE_PROPERTIES ssp = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        d2dFactory_->CreateStrokeStyle(ssp, nullptr, 0, &roundStroke);

        auto makeLinGrad = [&](D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_COLOR_F c0, D2D1_COLOR_F c1) {
            ComPtr<ID2D1LinearGradientBrush> brush;
            D2D1_GRADIENT_STOP stops[2] = {
                { 0.0f, WithAlpha(c0, c0.a * op) },
                { 1.0f, WithAlpha(c1, c1.a * op) }
            };
            ComPtr<ID2D1GradientStopCollection> coll;
            if (SUCCEEDED(target_->CreateGradientStopCollection(stops, 2, &coll)) && coll) {
                target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(p0, p1), coll.Get(), &brush);
            }
            return brush;
        };

        auto drawSun = [&](float cx, float cy, float r, bool withRays) {
            // Soft outer warm halo
            D2D1_GRADIENT_STOP hStops[2] = {
                { 0.0f, D2D1::ColorF(0xFFD54F, 0.36f * op) },
                { 1.0f, D2D1::ColorF(0xFF9F1C, 0.0f) }
            };
            ComPtr<ID2D1GradientStopCollection> hColl;
            ComPtr<ID2D1RadialGradientBrush> haloBrush;
            if (SUCCEEDED(target_->CreateGradientStopCollection(hStops, 2, &hColl)) && hColl &&
                SUCCEEDED(target_->CreateRadialGradientBrush(
                    D2D1::RadialGradientBrushProperties(D2D1::Point2F(cx, cy), D2D1::Point2F(0, 0), r * 1.9f, r * 1.9f),
                    hColl.Get(), &haloBrush)) && haloBrush) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r * 1.9f, r * 1.9f), haloBrush.Get());
            }
            if (withRays) {
                ComPtr<ID2D1SolidColorBrush> rayBrush;
                if (SUCCEEDED(target_->CreateSolidColorBrush(D2D1::ColorF(0xFFE07D, 0.82f * op), &rayBrush)) && rayBrush) {
                    const float innerR = r * 1.32f;
                    const float outerR = r * 1.68f;
                    for (int i = 0; i < 8; ++i) {
                        const float ang = i * 0.78539816f;
                        const float ca = std::cos(ang), sa = std::sin(ang);
                        target_->DrawLine(D2D1::Point2F(cx + ca * innerR, cy + sa * innerR),
                                          D2D1::Point2F(cx + ca * outerR, cy + sa * outerR),
                                          rayBrush.Get(), std::max(1.4f, r * 0.17f), roundStroke.Get());
                    }
                }
            }
            auto sunBrush = makeLinGrad(D2D1::Point2F(cx - r * 0.5f, cy - r), D2D1::Point2F(cx + r * 0.5f, cy + r),
                                        D2D1::ColorF(0xFFF099, 1.0f), D2D1::ColorF(0xFF9500, 1.0f));
            if (sunBrush) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), sunBrush.Get());
            }
        };

        auto drawMoon = [&](float cx, float cy, float r, bool withStars) {
            ComPtr<ID2D1PathGeometry> moonGeom;
            if (SUCCEEDED(d2dFactory_->CreatePathGeometry(&moonGeom)) && moonGeom) {
                ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(moonGeom->Open(&sink)) && sink) {
                    // Elegant crescent moon formed by two arcs
                    const D2D1_POINT_2F pTop = D2D1::Point2F(cx + r * 0.18f, cy - r * 0.96f);
                    const D2D1_POINT_2F pBot = D2D1::Point2F(cx + r * 0.88f, cy + r * 0.45f);
                    sink->BeginFigure(pTop, D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddArc(D2D1::ArcSegment(pBot, D2D1::SizeF(r, r), 0.0f,
                        D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                    sink->AddArc(D2D1::ArcSegment(pTop, D2D1::SizeF(r * 0.82f, r * 0.82f), 0.0f,
                        D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                }
                auto moonBrush = makeLinGrad(D2D1::Point2F(cx - r, cy - r), D2D1::Point2F(cx + r, cy + r),
                                             D2D1::ColorF(0xF8FAFC, 0.98f), D2D1::ColorF(0x93C5FD, 0.96f));
                if (moonBrush) target_->FillGeometry(moonGeom.Get(), moonBrush.Get());
            }
            if (withStars) {
                ComPtr<ID2D1SolidColorBrush> starBrush;
                if (SUCCEEDED(target_->CreateSolidColorBrush(D2D1::ColorF(0xFEF3C7, 0.90f * op), &starBrush)) && starBrush) {
                    auto spark = [&](float sx, float sy, float sr) {
                        target_->DrawLine(D2D1::Point2F(sx - sr, sy), D2D1::Point2F(sx + sr, sy),
                                          starBrush.Get(), std::max(1.0f, sr * 0.36f), roundStroke.Get());
                        target_->DrawLine(D2D1::Point2F(sx, sy - sr), D2D1::Point2F(sx, sy + sr),
                                          starBrush.Get(), std::max(1.0f, sr * 0.36f), roundStroke.Get());
                    };
                    spark(cx + r * 0.65f, cy - r * 0.58f, r * 0.24f);
                    spark(cx + r * 0.98f, cy - r * 0.12f, r * 0.16f);
                }
            }
        };

        auto drawCloud = [&](float cx, float cy, float w, bool stormy, float alphaMul = 1.0f) {
            const float h = w * 0.56f;
            const D2D1_COLOR_F topCol = stormy ? D2D1::ColorF(0xDCE4F0, 0.95f * alphaMul)
                                               : D2D1::ColorF(0xFFFFFF, 0.97f * alphaMul);
            const D2D1_COLOR_F botCol = stormy ? D2D1::ColorF(0x8EA2BC, 0.94f * alphaMul)
                                               : D2D1::ColorF(0xCFDCEB, 0.93f * alphaMul);
            auto cBrush = makeLinGrad(D2D1::Point2F(cx, cy - h * 0.65f), D2D1::Point2F(cx, cy + h * 0.50f), topCol, botCol);
            if (!cBrush) return;

            // Soft drop shadow beneath cloud base for Apple-style depth
            ComPtr<ID2D1SolidColorBrush> shBrush;
            if (SUCCEEDED(target_->CreateSolidColorBrush(D2D1::ColorF(0x0B1220, 0.20f * alphaMul * op), &shBrush)) && shBrush) {
                target_->FillRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(cx - w * 0.44f, cy + h * 0.12f, cx + w * 0.44f, cy + h * 0.48f),
                    h * 0.22f, h * 0.22f), shBrush.Get());
            }

            // Plump 3-puff Apple cloud silhouette
            const float rLeft = w * 0.22f;
            const float rMid  = w * 0.29f;
            const float rRight = w * 0.20f;
            target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - w * 0.23f, cy + h * 0.08f), rLeft, rLeft), cBrush.Get());
            target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - w * 0.02f, cy - h * 0.12f), rMid, rMid), cBrush.Get());
            target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + w * 0.24f, cy + h * 0.10f), rRight, rRight), cBrush.Get());
            target_->FillRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(cx - w * 0.45f, cy + h * 0.04f, cx + w * 0.44f, cy + h * 0.46f),
                h * 0.22f, h * 0.22f), cBrush.Get());
        };

        switch (visual) {
            case WeatherVisual::Clear:
                if (night) drawMoon(center.x - s * 0.04f, center.y, s * 0.36f, true);
                else drawSun(center.x, center.y, s * 0.25f, true);
                break;

            case WeatherVisual::PartlyCloudy:
                if (night) drawMoon(center.x + s * 0.14f, center.y - s * 0.12f, s * 0.24f, true);
                else drawSun(center.x + s * 0.16f, center.y - s * 0.14f, s * 0.19f, true);
                drawCloud(center.x - s * 0.05f, center.y + s * 0.10f, s * 0.78f, false);
                break;

            case WeatherVisual::Cloudy:
                drawCloud(center.x + s * 0.12f, center.y - s * 0.10f, s * 0.62f, true, 0.78f);
                drawCloud(center.x - s * 0.06f, center.y + s * 0.08f, s * 0.80f, false);
                break;

            case WeatherVisual::Fog: {
                // Airy translucent cloud + 3 sleek horizontal mist ribbons
                drawCloud(center.x, center.y - s * 0.14f, s * 0.72f, false, 0.82f);
                const float yBase = center.y + s * 0.16f;
                const float gap = s * 0.115f;
                const float strokeW = std::max(1.8f, s * 0.062f);
                const float lens[3] = { 0.68f, 0.56f, 0.44f };
                const float xOff[3] = { -0.02f, 0.06f, -0.05f };
                const float alphas[3] = { 0.82f, 0.68f, 0.52f };
                for (int i = 0; i < 3; ++i) {
                    const float x0 = center.x + (xOff[i] - lens[i] * 0.5f) * s;
                    const float x1 = center.x + (xOff[i] + lens[i] * 0.5f) * s;
                    const float yy = yBase + i * gap;
                    auto fogBrush = makeLinGrad(D2D1::Point2F(x0, yy), D2D1::Point2F(x1, yy),
                        D2D1::ColorF(0xE2E8F0, alphas[i] * 0.45f), D2D1::ColorF(0xF8FAFC, alphas[i]));
                    if (fogBrush) {
                        target_->DrawLine(D2D1::Point2F(x0, yy), D2D1::Point2F(x1, yy),
                                          fogBrush.Get(), strokeW, roundStroke.Get());
                    }
                }
                break;
            }

            case WeatherVisual::Rain: {
                drawCloud(center.x, center.y - s * 0.11f, s * 0.78f, true);
                auto dropBrush = makeLinGrad(D2D1::Point2F(center.x, center.y + s * 0.12f),
                                             D2D1::Point2F(center.x, center.y + s * 0.42f),
                                             D2D1::ColorF(0x93C5FD, 0.96f), D2D1::ColorF(0x38BDF8, 0.98f));
                if (dropBrush) {
                    const float dx[3] = { -0.22f, 0.0f, 0.22f };
                    const float dy[3] = { 0.0f, 0.06f, 0.0f };
                    const float sw = std::max(1.8f, s * 0.065f);
                    for (int i = 0; i < 3; ++i) {
                        const float x0 = center.x + dx[i] * s;
                        const float y0 = center.y + (0.19f + dy[i]) * s;
                        target_->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x0 - s * 0.06f, y0 + s * 0.16f),
                                          dropBrush.Get(), sw, roundStroke.Get());
                    }
                }
                break;
            }

            case WeatherVisual::Storm: {
                drawCloud(center.x, center.y - s * 0.13f, s * 0.78f, true);
                // Bright golden lightning bolt
                ComPtr<ID2D1PathGeometry> boltGeom;
                if (SUCCEEDED(d2dFactory_->CreatePathGeometry(&boltGeom)) && boltGeom) {
                    ComPtr<ID2D1GeometrySink> sink;
                    if (SUCCEEDED(boltGeom->Open(&sink)) && sink) {
                        const float bx = center.x + s * 0.01f;
                        const float by = center.y + s * 0.07f;
                        sink->BeginFigure(D2D1::Point2F(bx + s * 0.05f, by), D2D1_FIGURE_BEGIN_FILLED);
                        sink->AddLine(D2D1::Point2F(bx - s * 0.11f, by + s * 0.18f));
                        sink->AddLine(D2D1::Point2F(bx - s * 0.01f, by + s * 0.18f));
                        sink->AddLine(D2D1::Point2F(bx - s * 0.08f, by + s * 0.36f));
                        sink->AddLine(D2D1::Point2F(bx + s * 0.11f, by + s * 0.14f));
                        sink->AddLine(D2D1::Point2F(bx + s * 0.01f, by + s * 0.14f));
                        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                        sink->Close();
                    }
                    auto boltBrush = makeLinGrad(D2D1::Point2F(center.x, center.y + s * 0.06f),
                                                 D2D1::Point2F(center.x, center.y + s * 0.42f),
                                                 D2D1::ColorF(0xFEF08A, 1.0f), D2D1::ColorF(0xF59E0B, 1.0f));
                    if (boltBrush) target_->FillGeometry(boltGeom.Get(), boltBrush.Get());
                }
                // Flanking rain droplets
                ComPtr<ID2D1SolidColorBrush> rBrush;
                if (SUCCEEDED(target_->CreateSolidColorBrush(D2D1::ColorF(0x60A5FA, 0.90f * op), &rBrush)) && rBrush) {
                    const float sw = std::max(1.5f, s * 0.055f);
                    target_->DrawLine(D2D1::Point2F(center.x - s * 0.22f, center.y + s * 0.18f),
                                      D2D1::Point2F(center.x - s * 0.27f, center.y + s * 0.30f), rBrush.Get(), sw, roundStroke.Get());
                    target_->DrawLine(D2D1::Point2F(center.x + s * 0.23f, center.y + s * 0.18f),
                                      D2D1::Point2F(center.x + s * 0.18f, center.y + s * 0.30f), rBrush.Get(), sw, roundStroke.Get());
                }
                break;
            }

            case WeatherVisual::Snow: {
                drawCloud(center.x, center.y - s * 0.12f, s * 0.78f, false);
                ComPtr<ID2D1SolidColorBrush> snowBrush;
                if (SUCCEEDED(target_->CreateSolidColorBrush(D2D1::ColorF(0xE0F2FE, 0.95f * op), &snowBrush)) && snowBrush) {
                    const float fx[3] = { -0.22f, 0.0f, 0.22f };
                    const float fy[3] = { 0.25f, 0.32f, 0.25f };
                    const float fr = s * 0.075f;
                    const float sw = std::max(1.2f, s * 0.036f);
                    for (int i = 0; i < 3; ++i) {
                        const float x = center.x + fx[i] * s;
                        const float y = center.y + fy[i] * s;
                        for (int k = 0; k < 3; ++k) {
                            const float ang = k * 1.04719755f;
                            const float ca = std::cos(ang) * fr, sa = std::sin(ang) * fr;
                            target_->DrawLine(D2D1::Point2F(x - ca, y - sa), D2D1::Point2F(x + ca, y + sa),
                                              snowBrush.Get(), sw, roundStroke.Get());
                        }
                    }
                }
                break;
            }

            default: {
                // Clean minimalist thermometer fallback
                ComPtr<ID2D1SolidColorBrush> bodyBrush, mercBrush;
                target_->CreateSolidColorBrush(D2D1::ColorF(0xE2E8F0, 0.90f * op), &bodyBrush);
                target_->CreateSolidColorBrush(D2D1::ColorF(0xFB7185, 0.95f * op), &mercBrush);
                if (bodyBrush && mercBrush) {
                    const float sw = std::max(1.5f, s * 0.055f);
                    target_->DrawRoundedRectangle(D2D1::RoundedRect(
                        D2D1::RectF(center.x - s * 0.08f, center.y - s * 0.34f, center.x + s * 0.08f, center.y + s * 0.14f),
                        s * 0.08f, s * 0.08f), bodyBrush.Get(), sw);
                    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y + s * 0.22f), s * 0.14f, s * 0.14f), mercBrush.Get());
                    target_->DrawLine(D2D1::Point2F(center.x, center.y - s * 0.08f), D2D1::Point2F(center.x, center.y + s * 0.18f),
                                      mercBrush.Get(), sw, roundStroke.Get());
                }
                break;
            }
        }
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
