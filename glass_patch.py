from pathlib import Path


def apply_glass(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    rim = (Path(__file__).parent/'liquid-glass-rim.h').read_text(encoding='utf-8')
    change('class Renderer {', rim + '\nclass Renderer {')
    change('''    float settingsOpacity_ = 0.96f;''', '''    ComPtr<ID2D1GradientStopCollection> glassRimStops_;
    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    float settingsOpacity_ = 0.96f;''')
    change('''        target_.Reset();
        textFormat_.Reset();''', '''        glassRimBrush_.Reset();
        glassRimStops_.Reset();
        target_.Reset();
        textFormat_.Reset();''')
    change('''    // Rounded inner card used by every dashboard, so panels across the media,
    // calendar, weather, hardware and file-tray surfaces match exactly.
    //
    // Fill only, no outline. Each card used to also get a hairline ring, and six
    // of those side by side in the hardware grid turned into a mesh of bright
    // edges competing with the content. The fill alpha in BuildMaterialTokens
    // was raised to carry the separation on its own.
    void DrawCard(D2D1_RECT_F rect, float radius, bool active = false) {
        ComPtr<ID2D1SolidColorBrush> fill;
        if (SUCCEEDED(target_->CreateSolidColorBrush(
                active ? material_.raisedStrong : material_.raised, &fill)) && fill) {
            target_->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), fill.Get());
        }
    }''', '''    void DrawCard(D2D1_RECT_F rect, float radius, bool active = false) {
        const auto card = D2D1::RoundedRect(rect, radius, radius);
        const float oldOpacity = textBrush_->GetOpacity();
        if (active) {
            textBrush_->SetOpacity(0.04f * settingsOpacity_);
            target_->FillRoundedRectangle(card, textBrush_.Get());
        }
        if (glassRimBrush_ && material_.onDark) {
            glassRimBrush_->SetStartPoint(D2D1::Point2F(0, rect.top));
            glassRimBrush_->SetEndPoint(D2D1::Point2F(0, rect.bottom));
            glassRimBrush_->SetOpacity((active ? 0.90f : 0.60f) * settingsOpacity_);
            target_->DrawRoundedRectangle(card, glassRimBrush_.Get(), 0.8f);
        } else {
            textBrush_->SetOpacity((active ? 0.22f : 0.14f) * settingsOpacity_);
            target_->DrawRoundedRectangle(card, textBrush_.Get(), 0.8f);
        }
        textBrush_->SetOpacity(oldOpacity);
    }''')
    change('''            ComPtr<ID2D1SolidColorBrush> border;
            target_->CreateSolidColorBrush(borderColor, &border);''', '''            if (settings.backdropMaterial != BackdropMaterial::None || settings.themePreset == ThemePreset::Custom) {
                // Cache the glass rim; resizing only updates its gradient endpoints.
                if (!glassRimStops_) {
                    const auto stops = LiquidGlassRimStops();
                    target_->CreateGradientStopCollection(stops.data(), static_cast<UINT32>(stops.size()), &glassRimStops_);
                }
                if (glassRimStops_ && !glassRimBrush_)
                    target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
                        D2D1::Point2F(0, rect.top), D2D1::Point2F(0, rect.bottom)), glassRimStops_.Get(), &glassRimBrush_);
                if (glassRimBrush_) {
                    glassRimBrush_->SetStartPoint(D2D1::Point2F(0, rect.top));
                    glassRimBrush_->SetEndPoint(D2D1::Point2F(0, rect.bottom));
                    glassRimBrush_->SetOpacity(settingsOpacity_);
                    const float glassStroke = settings.sizeScale;
                    const float inset = glassStroke * 0.5f;
                    const auto rim = D2D1::RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset);
                    DrawIslandShape(rim, (std::max)(0.0f, radius - inset), settings.w11Style, settings.notchStyle,
                        glassRimBrush_.Get(), glassStroke);
                    return;
                }
            }
            ComPtr<ID2D1SolidColorBrush> border;
            target_->CreateSolidColorBrush(borderColor, &border);''')
    return src
