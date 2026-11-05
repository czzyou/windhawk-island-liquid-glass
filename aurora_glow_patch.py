"""Add area-weighted album glow with slow liquid gradient motion."""
from pathlib import Path


def apply_aurora_glow(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    # 1. Add YAML settings under Themes
    settings_yaml = '''  - AmbientAuroraGlow: true
    $name: 专辑氛围背光
    $description: Casts a soft, breathing ambient glow around the island blending the main album colors in proportion to their visible area.
  - AmbientGlowIntensity: 100
    $name: 氛围背光强度
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
    const int glowSetting = Wh_GetIntSetting(L"Themes.AmbientAuroraGlow") != 0;
    int glowOverride = Wh_GetIntValue(L"AmbientAuroraGlowOverride", -1);
    if (glowOverride >= 0 && Wh_GetIntValue(L"AmbientAuroraGlowOverrideBase", -1) != glowSetting) {
        Wh_SetIntValue(L"AmbientAuroraGlowOverride", -1);
        glowOverride = -1;
    }
    next.ambientAuroraGlow = glowOverride >= 0 ? glowOverride != 0 : glowSetting != 0;
    next.ambientGlowIntensity = Clamp(Wh_GetIntSetting(L"Themes.AmbientGlowIntensity") / 100.0f, 0.0f, 2.0f);''')

    # 4. Enhance BitmapPixels default accent: Pure soft frosted white instead of cyan
    change('''    D2D1_COLOR_F sampledAccent = D2D1::ColorF(0x4cc9f0);''',
           '''    D2D1_COLOR_F sampledAccent = D2D1::ColorF(0.92f, 0.94f, 0.98f, 1.0f);
    bool accentIsNeutral = true;
    std::array<D2D1_COLOR_F, 5> glowColors{};
    std::array<float, 5> glowWeights{};
    int glowColorCount = 0;''')

    change('''        if (poolCount > 0) {''', '''        if (poolCount > 0) {
            pixels.accentIsNeutral = false;''')
    change('''            targetAccent = state.media.art.sampledAccent;''',
           '''            targetAccent = EnsureContrastAgainstBackground(
                state.media.art.sampledAccent, settings.pillBgColor);''')

    change('bool DecodeImageBytesToPixels(',
           (Path(__file__).parent / 'album-glow-palette.h').read_text(encoding='utf-8') + '\n\nbool DecodeImageBytesToPixels(')
    decode = src.index('bool DecodeImageBytesToPixels(')
    sample = src.index('    pixels.generation = ++g_artGenerationCounter;', decode)
    src = src[:sample] + '    SampleAlbumGlow(pixels);\n' + src[sample:]

    change('class Renderer {',
           (Path(__file__).parent / 'album-glow-motion.h').read_text(encoding='utf-8') + '\n\n' +
           (Path(__file__).parent / 'album-glow-fluid.h').read_text(encoding='utf-8') + '\n\nclass Renderer {')

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

    # 8. Add Renderer member variables & device release
    change('''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    float settingsOpacity_ = 0.96f;''',
           '''    ComPtr<ID2D1LinearGradientBrush> glassRimBrush_;
    ComPtr<ID2D1BitmapBrush> auroraGlowBrush_;
    ComPtr<ID2D1Bitmap> auroraGlowBitmap_;
    double lastGlowFieldTime_ = -1.0;
    float lastGlowFieldPhase_ = -1.0f;
    float lastGlowEnergy_ = -1.0f;
    float lastGlowWidth_ = 0.0f, lastGlowHeight_ = 0.0f, lastGlowRadius_ = 0.0f;
    uint64_t lastGlowArt_ = 0;
    D2D1_COLOR_F lastGlowAccent_{};
    bool lastGlowAuto_ = true;
    std::array<D2D1_COLOR_F, 2> glowColors_{};
    std::array<D2D1_COLOR_F, 2> glowStartColors_{};
    std::array<D2D1_COLOR_F, 2> glowTargetColors_{};
    double glowColorStart_ = -1.0;
    bool glowColorAnimating_ = false;
    GlowMotionState glowMotion_{};
    float glowPhase_ = 0.0f;
    bool glowMotionEnabled_ = true;
    double lastGlowMotionCheck_ = -1.0;
    float auroraGlowBlend_ = 0.0f;
    double lastGlowTime_ = -1.0;
    float settingsOpacity_ = 0.96f;''')

    change('''        glassRimBrush_.Reset();
        glassRimStops_.Reset();''',
           '''        glassRimBrush_.Reset();
        glassRimStops_.Reset();
        auroraGlowBrush_.Reset();
        auroraGlowBitmap_.Reset();''')

    # 9. Add DrawAmbientAuroraGlow method right before DrawSoftShadow
    glow_method = '''    // Ambient Aurora Glow.
    // Update once per frame, independently of how many pills are painted.
    bool UpdateAmbientGlow(const Settings& settings, bool mediaShown, bool playing, float amplitude, double now) {
        const float target = settings.ambientAuroraGlow && mediaShown &&
            settings.ambientGlowIntensity > 0.005f && settings.backdropMaterial == BackdropMaterial::None
            ? (playing ? 1.0f : 0.25f) : 0.0f;
        if (lastGlowMotionCheck_ < 0 || now - lastGlowMotionCheck_ >= 1.0) {
            BOOL enabled = TRUE;
            if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
                glowMotionEnabled_ = enabled != FALSE;
            lastGlowMotionCheck_ = now;
        }
        const double dt = lastGlowTime_ < 0.0 ? 0.0 : (std::max)(0.0, (std::min)(now - lastGlowTime_, 0.1));
        const bool motionAnimating = UpdateGlowMotion(glowMotion_, amplitude,
            target > 0 && playing, glowMotionEnabled_, dt);
        if (target > 0.0f && glowMotionEnabled_ && glowMotion_.playback > 0.0f)
            glowPhase_ += static_cast<float>(dt) * 0.785398f * glowMotion_.playback;
        const float previous = auroraGlowBlend_;
        if (!glowMotionEnabled_) {
            auroraGlowBlend_ = target;
        } else if (lastGlowTime_ < 0.0) {
            auroraGlowBlend_ = 0.0f;
        } else {
            const float k = 1.0f - static_cast<float>(std::exp(-dt / 0.10));
            auroraGlowBlend_ += (target - auroraGlowBlend_) * k;
            if (std::fabs(target - auroraGlowBlend_) <= 0.005f) auroraGlowBlend_ = target;
        }
        lastGlowTime_ = now;
        return motionAnimating || (target > 0 && glowColorAnimating_) || previous != auroraGlowBlend_ || auroraGlowBlend_ != target;
    }

    void DrawAmbientAuroraGlow(D2D1_RECT_F rect, float radius,
                               const SharedState& state, const Settings& settings,
                               IslandKind kind, double now) {
        if (kind != IslandKind::Media || settings.backdropMaterial != BackdropMaterial::None) {
            return;
        }

        if (auroraGlowBlend_ <= 0.005f || settings.ambientGlowIntensity <= 0.005f) {
            return;
        }

        D2D1_COLOR_F glowColor = currentAccent_;
        if (glowColor.a <= 0.001f) {
            return;
        }

        const float maxC = (std::max)({glowColor.r, glowColor.g, glowColor.b});
        // Neutral cover detection belongs to decoding, not the contrast-corrected UI color.
        if (settings.accentMode == AccentMode::Auto && state.media.art.accentIsNeutral) {
            glowColor = D2D1::ColorF(0.92f, 0.94f, 0.98f, 1.0f);
        } else if (maxC < 0.35f && maxC > 0.001f) {
            // Lift luminance slightly so dark but saturated cover accents bloom vibrantly
            const float boost = 0.35f / maxC;
            glowColor.r = (std::min)(1.0f, glowColor.r * boost);
            glowColor.g = (std::min)(1.0f, glowColor.g * boost);
            glowColor.b = (std::min)(1.0f, glowColor.b * boost);
        }

        const float totalIntensity = settings.ambientGlowIntensity * auroraGlowBlend_ * settingsOpacity_;
        if (totalIntensity <= 0.005f) {
            return;
        }

        const bool flushTop = settings.notchStyle || settings.borderMergedMode;
        const float offsetRatio = flushTop ? 0.16f : 0.08f;
        const float availBottom = (std::max)(0.0f, bitmapHeight_ - rect.bottom - 3.5f);
        const float availTop = (std::max)(0.0f, rect.top - 3.5f);
        float spreadY = (std::min)(Clamp(4.5f * settings.sizeScale, 2.0f, 5.5f),
                                   availBottom / (1.0f + offsetRatio));
        if (!flushTop) spreadY = (std::min)(spreadY, availTop / (0.70f - offsetRatio));
        const float yOffset = spreadY * offsetRatio;
        const float availX = (std::max)(0.0f, (std::min)(rect.left, bitmapWidth_ - rect.right) - 4.0f);
        const float spreadX = (std::min)({spreadY, availX, kRenderPadX - 4.0f});

        const auto& art = state.media.art;
        const bool autoColor = settings.accentMode == AccentMode::Auto;
        const bool paletteChanged = lastGlowArt_ != art.generation || lastGlowAuto_ != autoColor ||
            (!autoColor && (lastGlowAccent_.r != glowColor.r || lastGlowAccent_.g != glowColor.g ||
                           lastGlowAccent_.b != glowColor.b));
        if (paletteChanged || glowColorStart_ < 0) {
            glowStartColors_ = glowColors_;
            const int colorCount = autoColor ? (std::min)(2, art.glowColorCount) : 0;
            for (int j = 0; j < 2; ++j) {
                auto color = colorCount ? art.glowColors[(std::min)(j, colorCount - 1)] : glowColor;
                const float peak = (std::max)({color.r, color.g, color.b});
                if (peak > 0.001f && peak < 0.35f) {
                    color.r *= 0.35f / peak; color.g *= 0.35f / peak; color.b *= 0.35f / peak;
                }
                const float gray = 0.2126f * color.r + 0.7152f * color.g + 0.0722f * color.b;
                color.r = gray + (color.r - gray) * 0.90f;
                color.g = gray + (color.g - gray) * 0.90f;
                color.b = gray + (color.b - gray) * 0.90f;
                glowTargetColors_[j] = color;
            }
            if (glowColorStart_ < 0) glowStartColors_ = glowTargetColors_;
            glowColorStart_ = now;
            lastGlowArt_ = art.generation; lastGlowAccent_ = glowColor; lastGlowAuto_ = autoColor;
        }
        const float colorMix = glowMotionEnabled_ ?
            Clamp(static_cast<float>((now - glowColorStart_) / 0.5), 0.0f, 1.0f) : 1.0f;
        if (paletteChanged) glowColorAnimating_ = true;
        const bool fieldDue = lastGlowFieldTime_ < 0 || now - lastGlowFieldTime_ >= 1.0 / 30.0;
        const float fieldWidth = rect.right - rect.left, fieldHeight = rect.bottom - rect.top;
        const bool shapeChanged = fieldWidth != lastGlowWidth_ || fieldHeight != lastGlowHeight_ || radius != lastGlowRadius_;
        if (!auroraGlowBitmap_ || shapeChanged || (fieldDue && (paletteChanged || glowColorAnimating_ ||
                                                lastGlowFieldPhase_ != glowPhase_ || lastGlowEnergy_ != glowMotion_.energy))) {
            for (int j = 0; j < 2; ++j) {
                glowColors_[j].r = glowStartColors_[j].r + (glowTargetColors_[j].r - glowStartColors_[j].r) * colorMix;
                glowColors_[j].g = glowStartColors_[j].g + (glowTargetColors_[j].g - glowStartColors_[j].g) * colorMix;
                glowColors_[j].b = glowStartColors_[j].b + (glowTargetColors_[j].b - glowStartColors_[j].b) * colorMix;
            }
            glowColorAnimating_ = colorMix < 1.0f;
            const float weights[] = {0.8f, 0.2f};
            const auto pixels = BuildAlbumGlowField(glowColors_.data(), weights, 2, glowPhase_, fieldWidth, fieldHeight, radius, settings.sizeScale, glowMotion_.energy);
            const UINT pitch = kGlowFieldWidth * 4;
            if (!auroraGlowBitmap_) {
                if (FAILED(target_->CreateBitmap(D2D1::SizeU(kGlowFieldWidth, kGlowFieldHeight),
                    pixels.data(), pitch, D2D1::BitmapProperties(D2D1::PixelFormat(
                        DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &auroraGlowBitmap_))) return;
            } else if (FAILED(auroraGlowBitmap_->CopyFromMemory(nullptr, pixels.data(), pitch))) return;
            lastGlowFieldTime_ = now; lastGlowFieldPhase_ = glowPhase_;
            lastGlowEnergy_ = glowMotion_.energy;
            lastGlowWidth_ = fieldWidth; lastGlowHeight_ = fieldHeight; lastGlowRadius_ = radius;
        }
        if (!auroraGlowBrush_ && FAILED(target_->CreateBitmapBrush(auroraGlowBitmap_.Get(), &auroraGlowBrush_))) return;
        auroraGlowBrush_->SetTransform(D2D1::Matrix3x2F::Scale(
            fieldWidth / kGlowFieldWidth, fieldHeight / kGlowFieldHeight) *
            D2D1::Matrix3x2F::Translation(rect.left, rect.top));

        constexpr int kSteps = 10;
        const float compactFade = 1.0f - g_idleCompactBlend.load();
        for (int i = kSteps; i >= 1; --i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSteps);
            const float growX = spreadX * t;
            const float growY = spreadY * t;

            // Concentric smooth falloff
            const float falloff = (1.0f - t) * (1.0f - t);
            const float alpha = Clamp(0.18f * falloff * totalIntensity * compactFade, 0.0f, 1.0f);
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
            DrawIslandShape(glowRect, radius + growY, settings.w11Style,
                            settings.notchStyle, auroraGlowBrush_.Get(), settings.sizeScale);
        }
    }

'''
    glow_method += '''    void DrawAmbientAuroraRim(D2D1_RECT_F rect, float radius, IslandKind kind, const Settings& settings) {
        if (kind != IslandKind::Media || settings.backdropMaterial != BackdropMaterial::None ||
            !auroraGlowBrush_ || auroraGlowBlend_ <= 0.005f) return;
        const float band = Clamp((std::min)(rect.right - rect.left, rect.bottom - rect.top) / 8.0f,
                                 3.0f * settings.sizeScale, 10.0f * settings.sizeScale);
        const float stroke = band / 10.0f;
        const float normalization = 0.24f * (0.7f + 0.3f * glowMotion_.energy) /
                                    (0.275f + 0.15f * glowMotion_.energy);
        // Paint only the narrow rim; no cover-colored layer reaches the flat center.
        for (int i = 0; i < 10; ++i) {
            const float t = (i + 0.5f) / 10.0f;
            const float inset = band * t;
            const auto rim = D2D1::RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset);
            auroraGlowBrush_->SetOpacity(Clamp(normalization * (1-t) * (1-t) *
                settings.ambientGlowIntensity * auroraGlowBlend_ * settingsOpacity_, 0.0f, 1.0f));
            DrawIslandShape(rim, (std::max)(0.0f, radius - inset), settings.w11Style, settings.notchStyle,
                            auroraGlowBrush_.Get(), stroke);
        }
    }

'''
    change('''    // A real soft shadow, approximated by stacking concentric rounded shapes''',
           glow_method[glow_method.index('    void DrawAmbientAuroraGlow('):] + '''    // A real soft shadow, approximated by stacking concentric rounded shapes''')
    change('''    bool Render(const SharedState& state,''',
           glow_method[:glow_method.index('    void DrawAmbientAuroraGlow(')] + '''    bool Render(const SharedState& state,''')

    change('''        const bool continuousAnimation =
            mediaAnimating ||''', '''        const bool glowAnimating = renderer.UpdateAmbientGlow(
            g_settings, mediaShown, snapshot.media.playing,
            snapshot.waveformWrite ? snapshot.waveform[(snapshot.waveformWrite - 1) % snapshot.waveform.size()] : 0.0f, now);
        const bool continuousAnimation =
            glowAnimating || mediaAnimating ||''')

    change('''    AppendMenuW(menu, MF_STRING, 11, activeExpandOnHover ? L"Expand on Click" : L"Expand on Hover");''',
           '''    AppendMenuW(menu, MF_STRING, 11, activeExpandOnHover ? L"Expand on Click" : L"Expand on Hover");
    AppendMenuW(menu, MF_STRING | (GetSettingsCopy().ambientAuroraGlow ? MF_CHECKED : 0),
                2030, L"专辑氛围背光");''')
    change('''        case 30:''', '''        case 2030: {
            Wh_SetIntValue(L"AmbientAuroraGlowOverride", !GetSettingsCopy().ambientAuroraGlow);
            Wh_SetIntValue(L"AmbientAuroraGlowOverrideBase", Wh_GetIntSetting(L"Themes.AmbientAuroraGlow") != 0);
            LoadSettings();
            g_layoutDirty = true;
            break;
        }
        case 30:''')

    src = src.replace('the existing sampledAccent default (#4cc9f0)', 'the neutral sampledAccent default')
    src = src.replace('leave sampledAccent at the BitmapPixels default (#4cc9f0)', 'leave sampledAccent at the neutral BitmapPixels default')

    # 10. Call DrawAmbientAuroraGlow in DrawPill
    change('''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawSoftShadow(rect, radius);''',
           '''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawSoftShadow(rect, radius);
        DrawAmbientAuroraGlow(rect, radius, state, settings, activity.kind, now);''')

    change('''        DrawPillSurface(rect, radius, activity.kind, settings);''',
           '''        DrawPillSurface(rect, radius, activity.kind, settings);
        DrawAmbientAuroraRim(rect, radius, activity.kind, settings);''')

    return src
