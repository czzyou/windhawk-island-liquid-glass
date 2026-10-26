"""Add Ambient Aurora Glow to the island with rich full-spectrum album color extraction."""
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

    # 4. Enhance BitmapPixels default accent: Pure soft frosted white instead of cyan
    change('''    D2D1_COLOR_F sampledAccent = D2D1::ColorF(0x4cc9f0);''',
           '''    D2D1_COLOR_F sampledAccent = D2D1::ColorF(0.92f, 0.94f, 0.98f, 1.0f);''')

    # 5. Expand album color extraction spectrum (unlock moody darks, soft pastels, warm earthy & vivid tones)
    old_color_filter = '''        // Reject near-black, near-white, and near-gray pixels.
        // Raise luminance floor slightly (36 instead of 28) so very dark-but-colorful
        // pixels don't swamp vibrancy scoring on dark-mood album art.
        if (luminance < 36 || luminance > 220 || saturation < 28) {
            continue;
        }'''
    new_color_filter = '''        // Full spectrum color extraction: capture deep moody tones, delicate pastels, earthy hues and rich vivid colors
        if (luminance < 14 || luminance > 246 || saturation < 12) {
            continue;
        }'''
    change(old_color_filter, new_color_filter)

    # 6. Balance dominant area presence with visual vibrancy in candidate selection
    old_scoring = '''    // Among those top-3, pick the one with the highest avg saturation.
    const Bucket* best = nullptr;
    size_t bestIdx = 0;
    float bestVibrancy = -1.0f;
    for (int s = 0; s < 3; ++s) {
        if (!top[s].b) break;
        const float vibrancy = static_cast<float>(top[s].b->satSum) /
                               static_cast<float>(top[s].b->count);
        if (vibrancy > bestVibrancy) {
            bestVibrancy = vibrancy;
            best = top[s].b;
            bestIdx = top[s].idx;
        }
    }'''
    new_scoring = '''    // Balance dominant area presence with visual color vibrancy so the true aesthetic theme wins
    const Bucket* best = nullptr;
    size_t bestIdx = 0;
    float bestScore = -1.0f;
    for (int s = 0; s < 3; ++s) {
        if (!top[s].b) break;
        const float vibrancy = static_cast<float>(top[s].b->satSum) /
                               static_cast<float>(top[s].b->count);
        const float score = static_cast<float>(top[s].b->count) * (0.8f + (std::min)(vibrancy / 48.0f, 2.5f));
        if (score > bestScore) {
            bestScore = score;
            best = top[s].b;
            bestIdx = top[s].idx;
        }
    }'''
    change(old_scoring, new_scoring)

    # 7. Fallback for black & white / monochrome album art (e.g. Ariana Grande vinyl, monochrome photography)
    old_fallback = '''        // fallback for fully-grayscale/near-black-and-white album art.
    }'''
    new_fallback = '''        // fallback for fully-grayscale/near-black-and-white album art.
    } else {
        // True black & white / grayscale album art: use clean, elegant frosted soft white
        pixels.sampledAccent = D2D1::ColorF(0.92f, 0.94f, 0.98f, 1.0f);
    }'''
    change(old_fallback, new_fallback)

    # 8. Add Renderer member variables & device release
    change('''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    float settingsOpacity_ = 0.96f;''',
           '''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    ComPtr<ID2D1SolidColorBrush> auroraGlowBrush_;
    float auroraGlowBlend_ = 0.0f;
    double lastGlowTime_ = -1.0;
    float settingsOpacity_ = 0.96f;''')

    change('''        glassRimBrush_.Reset();
        glassRimStops_.Reset();''',
           '''        glassRimBrush_.Reset();
        glassRimStops_.Reset();
        auroraGlowBrush_.Reset();''')

    # 9. Add DrawAmbientAuroraGlow method right before DrawSoftShadow
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

        const float maxC = (std::max)({glowColor.r, glowColor.g, glowColor.b});
        const float minC = (std::min)({glowColor.r, glowColor.g, glowColor.b});
        const float sat = maxC - minC;

        // If the album art is monochrome, neutral, dark, or low-saturation (e.g. gray/black/white covers),
        // gracefully cast a clean, elegant frosted soft pale white glow instead of a muddy tint.
        if (sat < 0.12f || maxC <= 0.001f) {
            glowColor = D2D1::ColorF(0.92f, 0.94f, 0.98f, 1.0f);
        } else if (maxC < 0.35f && maxC > 0.001f) {
            // Lift luminance slightly so dark but saturated cover accents bloom vibrantly
            const float boost = 0.35f / maxC;
            glowColor.r = (std::min)(1.0f, glowColor.r * boost);
            glowColor.g = (std::min)(1.0f, glowColor.g * boost);
            glowColor.b = (std::min)(1.0f, glowColor.b * boost);
        }

        // Calm breathing pulsation (~2.6s cycle) when music is actively playing
        const float pulse = state.media.playing ?
            (0.86f + 0.14f * std::sin(static_cast<float>(now * 2.4))) : 0.85f;

        const float totalIntensity = settings.ambientGlowIntensity * auroraGlowBlend_ * pulse * settingsOpacity_;
        if (totalIntensity <= 0.005f) {
            return;
        }

        // Compute strict safe margins to prevent any outer glow clipping on all DPI scales
        // (100% ~ 250%), ultra-wide aspect ratios, notch/docked modes, and spring bounce nudges.
        const float availBottom = (std::max)(4.0f, static_cast<float>(bitmapHeight_) - rect.bottom - 3.5f);
        const float availLeft = (std::max)(4.0f, rect.left - 4.0f);
        const float maxSafeSpreadY = (std::min)(16.5f, availBottom / 1.20f);
        const float spreadY = Clamp(13.5f * settings.sizeScale, 5.0f, maxSafeSpreadY);
        const float yOffset = (settings.notchStyle || settings.borderMergedMode) ? (spreadY * 0.32f) : (spreadY * 0.16f);
        const float spreadX = Clamp(spreadY * 1.30f, 6.0f, (std::min)(availLeft, kRenderPadX - 4.0f));

        if (!auroraGlowBrush_) {
            if (FAILED(target_->CreateSolidColorBrush(glowColor, &auroraGlowBrush_)) || !auroraGlowBrush_) {
                return;
            }
        } else {
            auroraGlowBrush_->SetColor(glowColor);
        }

        constexpr int kSteps = 10;
        const float compactFade = 1.0f - g_idleCompactBlend.load();
        for (int i = kSteps; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            const float growX = spreadX * t;
            const float growY = spreadY * t;

            // Concentric smooth falloff
            const float falloff = (1.0f - t) * (1.0f - t);
            const float alpha = Clamp(0.085f * falloff * totalIntensity * compactFade, 0.0f, 1.0f);
            if (alpha <= 0.0015f) {
                continue;
            }

            auroraGlowBrush_->SetOpacity(alpha);
            const float glowTop = (settings.notchStyle || settings.borderMergedMode) ?
                rect.top : (rect.top - growY * 0.70f + yOffset);
            const D2D1_RECT_F glowRect = D2D1::RectF(
                rect.left - growX,
                glowTop,
                rect.right + growX,
                rect.bottom + growY + yOffset
            );
            FillIslandShape(glowRect, radius + growY, settings.w11Style,
                            settings.notchStyle, auroraGlowBrush_.Get());
        }
    }

'''
    change('''    // A real soft shadow, approximated by stacking concentric rounded shapes''',
           glow_method + '''    // A real soft shadow, approximated by stacking concentric rounded shapes''')

    # 10. Call DrawAmbientAuroraGlow in DrawPill
    change('''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawSoftShadow(rect, radius);''',
           '''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawAmbientAuroraGlow(rect, radius, state, settings, activity.kind, now);
        DrawSoftShadow(rect, radius);''')

    return src
