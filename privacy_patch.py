def apply_privacy(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:120], src.count(old))
        src = src.replace(old, new, 1)

    change('''struct IdleStripMetrics {
    float clockWidth = 0.0f;
    float weatherWidth = 0.0f;
    bool hasWeather = false;
    bool hasPrivacy = false;
    float totalWidth = IdleStripLayout::kMinWidth;
};''', '''struct IdleStripMetrics {
    float clockWidth = 0.0f;
    float weatherWidth = 0.0f;
    bool hasWeather = false;
    bool hasPrivacy = false;
    bool hasMic = false;
    bool hasCam = false;
    float totalWidth = IdleStripLayout::kMinWidth;
};''')

    change('''        metrics.hasPrivacy =
            (state.system.micActive && settings.privacyDots && settings.privacyDotsMic) ||
            (state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam);''',
           '''        metrics.hasMic =
            state.system.micActive && settings.privacyDots && settings.privacyDotsMic;
        metrics.hasCam =
            state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam;
        metrics.hasPrivacy = metrics.hasMic || metrics.hasCam;''')

    change('''        if (metrics.hasPrivacy) {
            total += IdleStripLayout::kPrivacyReserve;
        }''', '''        if (metrics.hasPrivacy) {
            // One dot needs 18px; two side-by-side need a little more so the
            // clock/weather slot never slides under the left-hand dot.
            total += (metrics.hasMic && metrics.hasCam)
                ? IdleStripLayout::kPrivacyReserve + 12.0f
                : IdleStripLayout::kPrivacyReserve;
        }''')

    change('''            const float privacyReserve =
                idleMetrics.hasPrivacy ? IdleStripLayout::kPrivacyReserve * scale : 0.0f;''',
           '''            const float privacyReserve = idleMetrics.hasPrivacy
                ? ((idleMetrics.hasMic && idleMetrics.hasCam
                        ? IdleStripLayout::kPrivacyReserve + 12.0f
                        : IdleStripLayout::kPrivacyReserve) * scale)
                : 0.0f;''')

    change('''        const float x = rect.right - margin - dotR;

        if (cam) {
            D2D1_COLOR_F camColor = settings.privacyDotsCamHex;
            camColor.a = settingsOpacity_;
            if (!camDotBrush_) target_->CreateSolidColorBrush(camColor, &camDotBrush_);
            else camDotBrush_->SetColor(camColor);

            if (camDotBrush_) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, dotY), dotR, dotR), camDotBrush_.Get());
            }
        } else if (mic) {
            D2D1_COLOR_F micColor = settings.privacyDotsMicHex;
            micColor.a = settingsOpacity_;
            if (!micDotBrush_) target_->CreateSolidColorBrush(micColor, &micDotBrush_);
            else micDotBrush_->SetColor(micColor);

            if (micDotBrush_) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, dotY), dotR, dotR), micDotBrush_.Get());
            }
        }''', '''        // Camera sits on the outer edge, microphone just inside it, so both
        // indicators stay visible at once instead of the camera dot hiding the
        // microphone one.
        const float camX = rect.right - margin - dotR;
        const float micX = camX - ((mic && cam) ? 12.0f : 0.0f);

        if (cam) {
            D2D1_COLOR_F camColor = settings.privacyDotsCamHex;
            camColor.a = settingsOpacity_;
            if (!camDotBrush_) target_->CreateSolidColorBrush(camColor, &camDotBrush_);
            else camDotBrush_->SetColor(camColor);

            if (camDotBrush_) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(camX, dotY), dotR, dotR),
                                     camDotBrush_.Get());
            }
        }
        if (mic) {
            D2D1_COLOR_F micColor = settings.privacyDotsMicHex;
            micColor.a = settingsOpacity_;
            if (!micDotBrush_) target_->CreateSolidColorBrush(micColor, &micDotBrush_);
            else micDotBrush_->SetColor(micColor);

            if (micDotBrush_) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(micX, dotY), dotR, dotR),
                                     micDotBrush_.Get());
            }
        }''')

    return src
