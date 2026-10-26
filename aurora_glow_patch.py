"""Add Ambient Aurora Glow to the island."""
from pathlib import Path


def apply_aurora_glow(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    # 1. Add YAML settings under Themes
    settings_yaml = '''  - AmbientAuroraGlow: true
    $name: Ambient aurora glow
    $description: Casts a soft, breathing ambient glow around the island driven by the playing album art accent color.
  - AmbientGlowIntensity: 100
    $name: Ambient glow intensity
    $description: 0 to 200. Controls how brightly the ambient aurora glow radiates around the island.
'''
    change('''  - DropShadow: true
    $name: Soft drop shadow
    $description: Casts a soft shadow beneath the island to lift it off the desktop.''',
           '''  - DropShadow: true
    $name: Soft drop shadow
    $description: Casts a soft shadow beneath the island to lift it off the desktop.
''' + settings_yaml)

    # 2. Add Settings struct fields
    change('''    bool dropShadow = true;         // soft shadow under the island''',
           '''    bool dropShadow = true;         // soft shadow under the island
    bool ambientAuroraGlow = true;  // soft ambient glow around the island driven by album accent
    float ambientGlowIntensity = 1.0f;''')

    # 3. Load settings in LoadSettings()
    change('''    next.dropShadow = Wh_GetIntSetting(L"Themes.DropShadow") != 0;''',
           '''    next.dropShadow = Wh_GetIntSetting(L"Themes.DropShadow") != 0;
    next.ambientAuroraGlow = Wh_GetIntSetting(L"Themes.AmbientAuroraGlow") != 0;
    next.ambientGlowIntensity = Clamp(Wh_GetIntSetting(L"Themes.AmbientGlowIntensity") / 100.0f, 0.0f, 2.0f);''')

    # 4. Add Renderer member variables
    change('''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    float settingsOpacity_ = 0.96f;''',
           '''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    float auroraGlowBlend_ = 0.0f;
    double lastGlowTime_ = -1.0;
    float settingsOpacity_ = 0.96f;''')

    # 5. Add DrawAmbientAuroraGlow method right before DrawSoftShadow
    glow_method = '''    // Ambient Aurora Glow.
    // Driven by album art accent, this projects a soft breathing aura around
    // the island on the desktop, creating an organic physical backlight bloom.
    void DrawAmbientAuroraGlow(D2D1_RECT_F rect, float radius,
                               const SharedState& state, const Settings& settings,
                               IslandKind kind, double now) {
        if (settings.backdropMaterial != BackdropMaterial::None) {
            return;
        }

        const bool isMedia = (kind == IslandKind::Media);
        const float targetBlend = (settings.ambientAuroraGlow && isMedia) ? (state.media.playing ? 1.0f : 0.65f) : 0.0f;

        if (lastGlowTime_ < 0.0) {
            auroraGlowBlend_ = targetBlend;
            lastGlowTime_ = now;
        } else {
            const double dt = (std::max)(0.0, (std::min)(now - lastGlowTime_, 0.1));
            lastGlowTime_ = now;
            const float k = 1.0f - static_cast<float>(std::exp(-dt / 0.18));
            auroraGlowBlend_ += (targetBlend - auroraGlowBlend_) * k;
        }

        if (auroraGlowBlend_ <= 0.005f || settings.ambientGlowIntensity <= 0.005f) {
            return;
        }

        D2D1_COLOR_F glowColor = currentAccent_;
        if (glowColor.a <= 0.001f) {
            return;
        }

        // Lift luminance slightly so dark/muted cover accents bloom vibrantly
        const float maxC = (std::max)({glowColor.r, glowColor.g, glowColor.b});
        if (maxC < 0.35f && maxC > 0.001f) {
            const float boost = 0.35f / maxC;
            glowColor.r = (std::min)(1.0f, glowColor.r * boost);
            glowColor.g = (std::min)(1.0f, glowColor.g * boost);
            glowColor.b = (std::min)(1.0f, glowColor.b * boost);
        } else if (maxC <= 0.001f) {
            glowColor = D2D1::ColorF(0.40f, 0.60f, 0.90f, 1.0f);
        }

        // Calm breathing pulsation (~2.6s cycle) when music is actively playing
        const float pulse = state.media.playing ?
            (0.86f + 0.14f * std::sin(static_cast<float>(now * 2.4))) : 0.85f;

        const float totalIntensity = settings.ambientGlowIntensity * auroraGlowBlend_ * pulse * settingsOpacity_;
        if (totalIntensity <= 0.005f) {
            return;
        }

        // Keep spread within render padding bounds (kRenderPadY = 22.0f, kRenderPadX = 28.0f)
        const float spreadY = Clamp(16.0f * settings.sizeScale, 6.0f, kRenderPadY - 3.0f);
        const float spreadX = Clamp(spreadY * 1.25f, 6.0f, kRenderPadX - 3.0f);
        const float yOffset = spreadY * 0.20f;

        constexpr int kSteps = 10;
        ComPtr<ID2D1SolidColorBrush> brush;
        if (FAILED(target_->CreateSolidColorBrush(glowColor, &brush)) || !brush) {
            return;
        }

        const float compactFade = 1.0f - g_idleCompactBlend.load();
        for (int i = kSteps; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            const float growX = spreadX * t;
            const float growY = spreadY * t;

            // Concentric smooth falloff
            const float falloff = (1.0f - t) * (1.0f - t);
            const float alpha = Clamp(0.09f * falloff * totalIntensity * compactFade, 0.0f, 1.0f);
            if (alpha <= 0.0015f) {
                continue;
            }

            brush->SetOpacity(alpha);
            const D2D1_RECT_F glowRect = D2D1::RectF(
                rect.left - growX,
                rect.top - growY * 0.70f + yOffset,
                rect.right + growX,
                rect.bottom + growY + yOffset
            );
            FillIslandShape(glowRect, radius + growY, settings.w11Style,
                            settings.notchStyle, brush.Get());
        }
    }

'''
    change('''    // A real soft shadow, approximated by stacking concentric rounded shapes''',
           glow_method + '''    // A real soft shadow, approximated by stacking concentric rounded shapes''')

    # 6. Call DrawAmbientAuroraGlow in DrawPill
    change('''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawSoftShadow(rect, radius);''',
           '''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawAmbientAuroraGlow(rect, radius, state, settings, activity.kind, now);
        DrawSoftShadow(rect, radius);''')

    return src
