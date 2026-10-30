from pathlib import Path


def apply_timer_ui(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:120], src.count(old))
        src = src.replace(old, new, 1)

    # ---- Shared state -----------------------------------------------------
    change('std::atomic<bool> g_timerDashboardExpanded = false;',
           '''std::atomic<bool> g_timerDashboardExpanded = false;
std::atomic<bool> g_mediaCompanionExpanded = false;
std::atomic<bool> g_timerDetailVisible = false;
std::atomic<int> g_hoveredTimerAction{-1};
std::atomic<int> g_hoveredCompanionButton{-1};''')

    change('''struct Activity {
    IslandKind kind = IslandKind::Idle;
    float width = 120.0f;
    float height = 36.0f;
};''', '''struct Activity {
    IslandKind kind = IslandKind::Idle;
    float width = 120.0f;
    float height = 36.0f;
    // True for the small round media island shown beside a running timer, so
    // DrawPill never mistakes it for an ordinary collapsed media pill.
    bool companion = false;
};''')

    # ---- Geometry shared by painter and hit tests -------------------------
    change('// The conversion and hit-test helpers live further down, just after Clamp().',
           '''// The timer's dedicated expanded panel. The painter (DrawTimerDetail) and the
// pointer hit test (TimerDetailHitTest) read the same numbers, so a button can
// never be painted somewhere it cannot be clicked.
namespace TimerDetailLayout {
    constexpr float kPanelWidth = 380.0f;
    constexpr float kHeight = 164.0f;
    constexpr float kPrivacyExtra = 16.0f;
    constexpr float kHeaderTop = 6.0f;
    constexpr float kHeaderBottom = 26.0f;
    constexpr float kHeroTop = 28.0f;
    constexpr float kHeroBottom = 80.0f;
    constexpr float kProgressY = 86.0f;
    constexpr float kProgressWidth = 180.0f;
    constexpr float kProgressThickness = 3.5f;
    constexpr float kButtonRadius = 21.0f;
    constexpr float kButtonOffsetX = 52.0f;
    constexpr float kButtonCenterY = 113.0f;
    constexpr float kLabelTop = 136.0f;
    constexpr float kLabelBottom = 156.0f;
    constexpr float kPrivacyRowCenterY = 168.0f;
}

inline D2D1_RECT_F TimerDetailButtonBounds(int index, float width) {
    const float offset = index == 0 ? -TimerDetailLayout::kButtonOffsetX
                                    : TimerDetailLayout::kButtonOffsetX;
    const float cx = width * 0.5f + offset;
    const float r = TimerDetailLayout::kButtonRadius;
    return D2D1::RectF(cx - r, TimerDetailLayout::kButtonCenterY - r,
                       cx + r, TimerDetailLayout::kButtonCenterY + r);
}

inline int TimerDetailHitTest(const MediaContentPoint& point) {
    if (!point.valid || point.height < 150.0f || point.width < 300.0f) return -1;
    for (int i = 0; i < 2; ++i) {
        const auto bounds = TimerDetailButtonBounds(i, point.width);
        if (point.x >= bounds.left && point.x < bounds.right &&
            point.y >= bounds.top && point.y < bounds.bottom) return i;
    }
    return -1;
}

// The media companion beside a running timer: a 36px round island that unfolds
// into a compact 200x44 pill while the pointer is on it.
namespace MediaCompanionLayout {
    constexpr float kOrbSize = 36.0f;
    constexpr float kExpandedWidth = 200.0f;
    constexpr float kExpandedHeight = 44.0f;
    constexpr float kButtonInset = 22.0f;
    constexpr float kButtonRadius = 13.0f;
    constexpr float kHitPad = 5.0f;
}

inline int MediaCompanionHitTest(const MediaContentPoint& point) {
    if (!point.valid || point.height > 60.0f || point.width < 140.0f) return -1;
    const float dx = point.x - (point.width - MediaCompanionLayout::kButtonInset);
    const float dy = point.y - point.height * 0.5f;
    const float reach = MediaCompanionLayout::kButtonRadius + MediaCompanionLayout::kHitPad;
    return (std::fabs(dx) <= reach && std::fabs(dy) <= reach) ? 1 : -1;
}

// The conversion and hit-test helpers live further down, just after Clamp().''')

    # ---- Panel sizing depends on whether privacy info needs a footer line --
    change('''    std::vector<FileTrayItem> fileTrayItems;
};''', '''    std::vector<FileTrayItem> fileTrayItems;
};

bool PrivacyIndicatorsVisible(const SharedState& state, const Settings& settings) {
    return (state.system.micActive && settings.privacyDots && settings.privacyDotsMic) ||
           (state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam);
}

float TimerDetailPanelHeight(const SharedState& state, const Settings& settings) {
    return TimerDetailLayout::kHeight +
        (PrivacyIndicatorsVisible(state, settings) ? TimerDetailLayout::kPrivacyExtra : 0.0f);
}

// Application names come from the ConsentStore key and can be long registry
// identifiers. Keep the two privacy rows inside the 380px panel.
std::wstring ShortAppName(const std::wstring& name, size_t maxChars = 12) {
    if (name.size() <= maxChars) return name;
    return name.substr(0, maxChars) + L"\\u2026";
}''')

    # ---- A dedicated content geometry for the timer panel -----------------
    change('''std::atomic<unsigned long long> g_mediaHitStamp{0};''',
           '''std::atomic<unsigned long long> g_mediaHitStamp{0};

std::atomic<unsigned> g_timerHitSeq{0};
std::atomic<float> g_timerHitLeft{0.0f};
std::atomic<float> g_timerHitTop{0.0f};
std::atomic<float> g_timerHitRight{0.0f};
std::atomic<float> g_timerHitBottom{0.0f};
std::atomic<float> g_timerHitScale{1.0f};
std::atomic<unsigned long long> g_timerHitStamp{0};''')

    change('''// Returns 0 = previous, 1 = play/pause, 2 = next, or -1 for no button.
int MediaTransportHitTest(const MediaContentPoint& pt) {''',
           '''// The timer panel is the primary pill and the media companion is drawn after
// it, so the shared media geometry cannot describe both. Keep a second channel
// for the timer panel itself.
void PublishTimerDetailGeometry(D2D1_RECT_F rect) {
    const unsigned seq = g_timerHitSeq.load(std::memory_order_relaxed);
    g_timerHitSeq.store(seq + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    g_timerHitLeft.store(rect.left, std::memory_order_relaxed);
    g_timerHitTop.store(rect.top, std::memory_order_relaxed);
    g_timerHitRight.store(rect.right, std::memory_order_relaxed);
    g_timerHitBottom.store(rect.bottom, std::memory_order_relaxed);
    g_timerHitScale.store(g_settings.sizeScale, std::memory_order_relaxed);
    g_timerHitStamp.store(GetTickCount64(), std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    g_timerHitSeq.store(seq + 2, std::memory_order_relaxed);
}

MediaContentPoint TimerDetailFromClient(int clientX, int clientY, bool allowCached = false) {
    MediaContentPoint out;
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f, scale = 1.0f;
    unsigned long long stamp = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const unsigned before = g_timerHitSeq.load(std::memory_order_relaxed);
        if (before & 1u) continue;
        std::atomic_thread_fence(std::memory_order_acquire);
        left = g_timerHitLeft.load(std::memory_order_relaxed);
        top = g_timerHitTop.load(std::memory_order_relaxed);
        right = g_timerHitRight.load(std::memory_order_relaxed);
        bottom = g_timerHitBottom.load(std::memory_order_relaxed);
        scale = g_timerHitScale.load(std::memory_order_relaxed);
        stamp = g_timerHitStamp.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (g_timerHitSeq.load(std::memory_order_relaxed) == before) break;
        stamp = 0;
    }
    if (stamp == 0 || (!allowCached && GetTickCount64() - stamp > 500)) return out;
    if (scale < 0.05f) scale = 1.0f;
    const float width = right - left;
    const float height = bottom - top;
    if (width <= 1.0f || height <= 1.0f) return out;
    const float centerX = (left + right) * 0.5f;
    const float centerY = (top + bottom) * 0.5f;
    const float contentX = (static_cast<float>(clientX) - centerX) / scale + centerX;
    const float contentY = (static_cast<float>(clientY) - centerY) / scale + centerY;
    out.valid = true;
    out.x = contentX - left;
    out.y = contentY - top;
    out.width = width;
    out.height = height;
    return out;
}

// Returns 0 = previous, 1 = play/pause, 2 = next, or -1 for no button.
int MediaTransportHitTest(const MediaContentPoint& pt) {''')

    # ---- Painter / hit-test selection -------------------------------------
    change('''            case IslandKind::Media:
                DrawMedia(state, unscaledRect, settings, now);
                break;''', '''            case IslandKind::Media:
                if (activity.companion) {
                    if (activity.width < 60.0f) DrawMediaOrb(state, unscaledRect, settings, now);
                    else DrawMediaCompanion(state, unscaledRect, settings, now);
                } else {
                    DrawMedia(state, unscaledRect, settings, now);
                }
                break;''')

    change('''            case IslandKind::Timer:
                DrawTimer(state, unscaledRect);
                break;''', '''            case IslandKind::Timer:
                if (activity.width < 60.0f) DrawTimerOrb(state, unscaledRect);
                else DrawTimer(state, unscaledRect);
                break;''')

    # The companion is a secondary pill; drawing the privacy dot on it would sit
    # on top of the album art. The primary pill already carries the indicator.
    change('''        DrawPrivacyDots(state, settings, unscaledRect, now);''',
           '''        if (!activity.companion) DrawPrivacyDots(state, settings, unscaledRect, now);''')

    # ---- Timer detail panel ----------------------------------------------
    change('''    void DrawIdleDashboard(const SharedState& state, D2D1_RECT_F rect, const Settings& settings,
                           double now) {''', '''    void DrawTimerPrivacyRow(const SharedState& state, const Settings& settings,
                             D2D1_RECT_F rect) {
        const bool mic = state.system.micActive && settings.privacyDots && settings.privacyDotsMic;
        const bool cam = state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam;
        if (!mic && !cam) return;

        std::wstring camText, micText;
        float camWidth = 0.0f, micWidth = 0.0f;
        if (cam) {
            camText = state.system.cameraApp.empty()
                ? std::wstring(L"摄像头")
                : ShortAppName(state.system.cameraApp) + L" · 摄像头";
            camWidth = MeasureTextWidthCached(camText, smallTextFormat_.Get(), privacyCamWidthCache_);
        }
        if (mic) {
            micText = state.system.micApp.empty()
                ? std::wstring(L"麦克风")
                : ShortAppName(state.system.micApp) + L" · 麦克风";
            micWidth = MeasureTextWidthCached(micText, smallTextFormat_.Get(), privacyMicWidthCache_);
        }

        const float dotR = 3.5f;
        const float afterDot = 7.0f;
        const float segmentGap = 14.0f;
        const float segmentWidth = dotR * 2.0f + afterDot;
        float total = 0.0f;
        if (cam) total += segmentWidth + camWidth;
        if (cam && mic) total += segmentGap;
        if (mic) total += segmentWidth + micWidth;

        const float cx = (rect.left + rect.right) * 0.5f;
        const float cy = rect.top + TimerDetailLayout::kPrivacyRowCenterY;
        float x = cx - total * 0.5f;

        const auto oldAlign = smallTextFormat_->GetTextAlignment();
        const auto oldParagraph = smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        auto drawSegment = [&](const std::wstring& text, float textWidth, D2D1_COLOR_F color) {
            D2D1_COLOR_F dotColor = color;
            dotColor.a = settingsOpacity_;
            ComPtr<ID2D1SolidColorBrush> dotBrush;
            target_->CreateSolidColorBrush(dotColor, &dotBrush);
            if (dotBrush) {
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + dotR, cy), dotR, dotR),
                                     dotBrush.Get());
            }
            const float textX = x + segmentWidth;
            mutedBrush_->SetOpacity(0.86f);
            target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()),
                smallTextFormat_.Get(),
                D2D1::RectF(textX, cy - 9.0f, textX + textWidth + 2.0f, cy + 9.0f),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            x += segmentWidth + textWidth;
        };

        if (cam) {
            drawSegment(camText, camWidth, settings.privacyDotsCamHex);
            if (mic) x += segmentGap;
        }
        if (mic) drawSegment(micText, micWidth, settings.privacyDotsMicHex);

        smallTextFormat_->SetTextAlignment(oldAlign);
        smallTextFormat_->SetParagraphAlignment(oldParagraph);
        mutedBrush_->SetOpacity(0.58f);
    }

    // The timer activity's own expanded surface. Deliberately does not reuse the
    // generic home: a running countdown should show its time, state and its own
    // controls instead of screenshot and mute actions that have nothing to do
    // with the session.
    void DrawTimerDetail(const SharedState& state, D2D1_RECT_F rect, const Settings& settings,
                         double now) {
        const float cx = (rect.left + rect.right) * 0.5f;
        const bool finished = state.timer.justFinished && !state.timer.active;
        const bool running = state.timer.running;
        const double remaining = finished
            ? 0.0
            : (running ? std::max(0.0, state.timer.endsAt - now) : state.timer.remainingAtPause);
        const int totalSec = state.timer.totalSeconds > 0 ? state.timer.totalSeconds : 1;
        const float progress = finished
            ? 1.0f
            : Clamp(1.0f - static_cast<float>(remaining / totalSec), 0.0f, 1.0f);

        g_timerDetailVisible.store(true, std::memory_order_relaxed);
        PublishTimerDetailGeometry(rect);

        const auto oldAlign = smallTextFormat_->GetTextAlignment();
        const auto oldParagraph = smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        std::wstring header;
        if (finished) {
            const int minutes = std::max(1, totalSec / 60);
            header = std::to_wstring(minutes) + L" 分钟 · " +
                (state.timer.isBreak ? L"休息" : L"专注");
        } else {
            header = state.timer.isBreak ? L"休息" : L"专注";
            header += running ? L" · 进行中" : L" · 已暂停";
        }
        mutedBrush_->SetOpacity(0.66f);
        target_->DrawTextW(header.c_str(), static_cast<UINT32>(header.size()),
            smallTextFormat_.Get(),
            D2D1::RectF(rect.left + 24, rect.top + TimerDetailLayout::kHeaderTop,
                        rect.right - 24, rect.top + TimerDetailLayout::kHeaderBottom),
            mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

        wchar_t hero[32]{};
        if (finished) {
            wcscpy_s(hero, state.timer.isBreak ? L"休息结束" : L"专注完成");
        } else {
            const int seconds = static_cast<int>(std::ceil(remaining));
            swprintf_s(hero, L"%d:%02d", seconds / 60, seconds % 60);
        }
        IDWriteTextFormat* heroFormat = timerHeroFormat_
            ? timerHeroFormat_.Get()
            : (hugeTextFormat_ ? hugeTextFormat_.Get() : textFormat_.Get());
        textBrush_->SetOpacity(0.97f);
        target_->DrawTextW(hero, static_cast<UINT32>(wcslen(hero)), heroFormat,
            D2D1::RectF(rect.left + 20, rect.top + TimerDetailLayout::kHeroTop,
                        rect.right - 20, rect.top + TimerDetailLayout::kHeroBottom),
            textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);

        const float barWidth = TimerDetailLayout::kProgressWidth;
        const float barHeight = TimerDetailLayout::kProgressThickness;
        const float barY = rect.top + TimerDetailLayout::kProgressY;
        ComPtr<ID2D1SolidColorBrush> trackBrush;
        ComPtr<ID2D1SolidColorBrush> fillBrush;
        target_->CreateSolidColorBrush(WithAlpha(material_.textPrimary, 0.16f * settingsOpacity_),
                                       &trackBrush);
        target_->CreateSolidColorBrush(WithAlpha(material_.textPrimary, 0.88f * settingsOpacity_),
                                       &fillBrush);
        if (trackBrush) {
            target_->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(cx - barWidth * 0.5f, barY - barHeight * 0.5f,
                                              cx + barWidth * 0.5f, barY + barHeight * 0.5f),
                                  barHeight * 0.5f, barHeight * 0.5f), trackBrush.Get());
        }
        if (fillBrush && progress > 0.002f) {
            target_->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(cx - barWidth * 0.5f, barY - barHeight * 0.5f,
                                              cx - barWidth * 0.5f + barWidth * progress,
                                              barY + barHeight * 0.5f),
                                  barHeight * 0.5f, barHeight * 0.5f), fillBrush.Get());
        }

        const wchar_t* labels[2];
        wchar_t glyphs[2];
        if (finished) {
            labels[0] = L"休息 5 分钟";
            labels[1] = L"再来一轮";
            glyphs[0] = wchar_t(0xE916);
            glyphs[1] = wchar_t(0xE72C);
        } else {
            labels[0] = running ? L"暂停" : L"继续";
            labels[1] = L"结束";
            glyphs[0] = running ? wchar_t(0xE769) : wchar_t(0xE768);
            glyphs[1] = wchar_t(0xE71A);
        }

        for (int i = 0; i < 2; ++i) {
            const auto box = TimerDetailButtonBounds(i, rect.right - rect.left);
            const D2D1_POINT_2F center{rect.left + (box.left + box.right) * 0.5f,
                                       rect.top + (box.top + box.bottom) * 0.5f};
            const bool hovered = g_hoveredTimerAction.load() == i;
            ComPtr<ID2D1SolidColorBrush> disc;
            target_->CreateSolidColorBrush(
                WithAlpha(hovered ? material_.accent : material_.textPrimary,
                          (hovered ? 0.16f : 0.06f) * settingsOpacity_), &disc);
            if (disc) {
                target_->FillEllipse(
                    D2D1::Ellipse(center, TimerDetailLayout::kButtonRadius,
                                  TimerDetailLayout::kButtonRadius), disc.Get());
            }
            if (iconFormat_) {
                iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                textBrush_->SetOpacity(hovered ? 1.0f : 0.84f);
                target_->DrawTextW(&glyphs[i], 1, iconFormat_.Get(),
                    D2D1::RectF(center.x - 20, center.y - 20, center.x + 20, center.y + 20),
                    textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            }
            mutedBrush_->SetOpacity(hovered ? 0.95f : 0.68f);
            target_->DrawTextW(labels[i], static_cast<UINT32>(wcslen(labels[i])),
                smallTextFormat_.Get(),
                D2D1::RectF(center.x - 70, rect.top + TimerDetailLayout::kLabelTop,
                            center.x + 70, rect.top + TimerDetailLayout::kLabelBottom),
                mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        }

        DrawTimerPrivacyRow(state, settings, rect);

        smallTextFormat_->SetTextAlignment(oldAlign);
        smallTextFormat_->SetParagraphAlignment(oldParagraph);
        textBrush_->SetOpacity(0.90f);
        mutedBrush_->SetOpacity(0.58f);
    }

    // Minimal timer: a progress ring with the whole minutes left, used when the
    // media activity owns the expanded panel.
    void DrawTimerOrb(const SharedState& state, D2D1_RECT_F rect) {
        const float cx = (rect.left + rect.right) * 0.5f;
        const float cy = (rect.top + rect.bottom) * 0.5f;
        const float ringR = std::max(8.0f, std::min(rect.right - rect.left,
                                                     rect.bottom - rect.top) * 0.5f - 3.0f);
        const bool finished = state.timer.justFinished && !state.timer.active;
        const double remaining = finished
            ? 0.0
            : (state.timer.running ? std::max(0.0, state.timer.endsAt - NowSeconds())
                                   : state.timer.remainingAtPause);
        const int totalSec = state.timer.totalSeconds > 0 ? state.timer.totalSeconds : 1;
        const float progress = finished
            ? 1.0f
            : Clamp(1.0f - static_cast<float>(remaining / totalSec), 0.0f, 1.0f);

        ComPtr<ID2D1SolidColorBrush> track;
        target_->CreateSolidColorBrush(WithAlpha(material_.textPrimary, 0.16f * settingsOpacity_),
                                       &track);
        if (track) {
            target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), ringR, ringR),
                                 track.Get(), 2.0f);
        }

        if (progress > 0.002f) {
            ComPtr<ID2D1PathGeometry> geometry;
            d2dFactory_->CreatePathGeometry(&geometry);
            ComPtr<ID2D1GeometrySink> sink;
            geometry->Open(&sink);
            const float start = -3.14159265f * 0.5f;
            const float sweep = 2.0f * 3.14159265f * progress;
            const int segments = std::max(2, static_cast<int>(32 * progress));
            auto pointAt = [&](float a) {
                return D2D1::Point2F(cx + std::cos(a) * ringR, cy + std::sin(a) * ringR);
            };
            sink->BeginFigure(pointAt(start), D2D1_FIGURE_BEGIN_HOLLOW);
            for (int i = 1; i <= segments; ++i) {
                sink->AddLine(pointAt(start + sweep * i / segments));
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();
            ComPtr<ID2D1SolidColorBrush> foreground;
            target_->CreateSolidColorBrush(
                WithAlpha(material_.textPrimary, 0.88f * settingsOpacity_), &foreground);
            if (foreground) target_->DrawGeometry(geometry.Get(), foreground.Get(), 2.2f);
        }

        wchar_t minutes[16]{};
        const int mins = finished ? 0 : std::max(1, static_cast<int>(std::ceil(remaining / 60.0)));
        if (finished) wcscpy_s(minutes, L"\\u2713");
        else swprintf_s(minutes, L"%d", mins);
        IDWriteTextFormat* format = boldTextFormat_ ? boldTextFormat_.Get() : textFormat_.Get();
        if (format) {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            textBrush_->SetOpacity(0.92f);
            target_->DrawTextW(minutes, static_cast<UINT32>(wcslen(minutes)), format,
                D2D1::RectF(cx - ringR, cy - ringR, cx + ringR, cy + ringR),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        }
    }

    void DrawMediaOrb(const SharedState& state, D2D1_RECT_F rect, const Settings& settings,
                      double now) {
        (void)settings;
        const D2D1_RECT_F art = D2D1::RectF(rect.left + 1.5f, rect.top + 1.5f,
                                            rect.right - 1.5f, rect.bottom - 1.5f);
        DrawAlbumArt(state.media, art, now, (art.right - art.left) * 0.5f, false);
        ComPtr<ID2D1SolidColorBrush> rim;
        target_->CreateSolidColorBrush(WithAlpha(material_.textPrimary, 0.10f * settingsOpacity_),
                                       &rim);
        if (rim) {
            const float cx = (rect.left + rect.right) * 0.5f;
            const float cy = (rect.top + rect.bottom) * 0.5f;
            const float r = (rect.right - rect.left) * 0.5f - 1.0f;
            target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), rim.Get(), 1.0f);
        }
    }

    void DrawMediaCompanion(const SharedState& state, D2D1_RECT_F rect, const Settings& settings,
                            double now) {
        PublishContentGeometry(rect);
        const float height = rect.bottom - rect.top;
        const float artSize = height - 12.0f;
        DrawAlbumArt(state.media,
                     D2D1::RectF(rect.left + 6.0f, rect.top + 6.0f,
                                 rect.left + 6.0f + artSize, rect.bottom - 6.0f),
                     now, artSize * 0.5f, false);

        const float buttonCx = rect.right - MediaCompanionLayout::kButtonInset;
        const float centerY = (rect.top + rect.bottom) * 0.5f;
        const float textLeft = rect.left + 6.0f + artSize + 9.0f;
        const float textRight = buttonCx - 20.0f;

        const auto oldAlign = smallTextFormat_->GetTextAlignment();
        const auto oldParagraph = smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        textBrush_->SetOpacity(0.94f);
        const std::wstring& title = state.media.title;
        target_->DrawTextW(title.empty() ? L"正在播放" : title.c_str(),
            static_cast<UINT32>(title.empty() ? 4 : title.size()), smallTextFormat_.Get(),
            D2D1::RectF(textLeft, rect.top + 5.0f, textRight, rect.top + 23.0f),
            textBrush_.Get(), static_cast<D2D1_DRAW_TEXT_OPTIONS>(
                D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_NO_SNAP));
        mutedBrush_->SetOpacity(0.64f);
        const std::wstring& artist = state.media.artist;
        if (!artist.empty()) {
            target_->DrawTextW(artist.c_str(), static_cast<UINT32>(artist.size()),
                smallTextFormat_.Get(),
                D2D1::RectF(textLeft, rect.top + 21.0f, textRight, rect.top + 39.0f),
                mutedBrush_.Get(), static_cast<D2D1_DRAW_TEXT_OPTIONS>(
                    D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_NO_SNAP));
        }
        smallTextFormat_->SetTextAlignment(oldAlign);
        smallTextFormat_->SetParagraphAlignment(oldParagraph);

        const bool hovered = g_hoveredCompanionButton.load() == 1;
        ComPtr<ID2D1SolidColorBrush> disc;
        target_->CreateSolidColorBrush(
            WithAlpha(hovered ? material_.accent : material_.textPrimary,
                      (hovered ? 0.16f : 0.08f) * settingsOpacity_), &disc);
        if (disc) {
            target_->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(buttonCx, centerY),
                              MediaCompanionLayout::kButtonRadius,
                              MediaCompanionLayout::kButtonRadius), disc.Get());
        }
        if (iconFormat_) {
            wchar_t glyph = state.media.playing ? wchar_t(0xE769) : wchar_t(0xE768);
            iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            textBrush_->SetOpacity(hovered ? 1.0f : 0.84f);
            target_->DrawTextW(&glyph, 1, iconFormat_.Get(),
                D2D1::RectF(buttonCx - 18, centerY - 18, buttonCx + 18, centerY + 18),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
            iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
        textBrush_->SetOpacity(0.96f);
        mutedBrush_->SetOpacity(0.75f);
    }

    void DrawIdleDashboard(const SharedState& state, D2D1_RECT_F rect, const Settings& settings,
                           double now) {
        g_timerDetailVisible.store(false, std::memory_order_relaxed);''')

    # ---- Generic home dispatches to the timer panel -----------------------
    change('''        const float cx = (rect.left + rect.right) * 0.5f;
        wchar_t date[64]{};
        GetDateFormatEx(L"en-US", 0, &local, L"ddd, MMM d", date, ARRAYSIZE(date), nullptr);''',
           '''        const float cx = (rect.left + rect.right) * 0.5f;
        if (settings.timerEnabled && (state.timer.active || state.timer.justFinished)) {
            DrawTimerDetail(state, rect, settings, now);
            return;
        }
        wchar_t date[64]{};
        GetDateFormatEx(L"en-US", 0, &local, L"ddd, MMM d", date, ARRAYSIZE(date), nullptr);''')

    # The timer countdown header on the generic home is now only a fallback for
    # a timer that was already expanded before this build; the dedicated panel
    # handles the normal path.
    change('''        if (settings.timerEnabled && state.timer.active) {
            const double remaining = state.timer.running ? std::max(0.0, state.timer.endsAt - now) : state.timer.remainingAtPause;
            const int seconds = static_cast<int>(std::ceil(remaining));
            swprintf_s(date, L"%s · %d:%02d%s", state.timer.isBreak ? L"休息" : L"专注",
                seconds / 60, seconds % 60, state.timer.running ? L"" : L" · 已暂停");
        }
''', '')

    # ---- Privacy chips on the generic home replace the English sentence ---
    change('''        const bool micActive = state.system.micActive && settings.privacyDots && settings.privacyDotsMic;
        const bool camActive = state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam;

        if (camActive || micActive) {
            std::wstring label;
            D2D1_COLOR_F dotColor;

            if (camActive && micActive) {
                dotColor = settings.privacyDotsCamHex;
                if (!state.system.cameraApp.empty() && !state.system.micApp.empty() && state.system.cameraApp == state.system.micApp) {
                    label = state.system.cameraApp + L" is using camera & microphone";
                } else if (!state.system.cameraApp.empty() && !state.system.micApp.empty()) {
                    label = state.system.cameraApp + L" & " + state.system.micApp + L" using camera & mic";
                } else if (!state.system.cameraApp.empty()) {
                    label = state.system.cameraApp + L" is using camera & microphone";
                } else if (!state.system.micApp.empty()) {
                    label = state.system.micApp + L" is using camera & microphone";
                } else {
                    label = L"Camera & microphone in use";
                }
            } else if (camActive) {
                dotColor = settings.privacyDotsCamHex;
                if (!state.system.cameraApp.empty()) {
                    label = state.system.cameraApp + L" is using your camera";
                } else {
                    label = L"Camera in use";
                }
            } else {
                dotColor = settings.privacyDotsMicHex;
                if (!state.system.micApp.empty()) {
                    label = state.system.micApp + L" is using your microphone";
                } else {
                    label = L"Microphone in use";
                }
            }

            IDWriteTextFormat* fmt = smallTextFormat_ ? smallTextFormat_.Get() : textFormat_.Get();
            if (fmt && dwriteFactory_) {
                ComPtr<IDWriteTextLayout> textLayout;
                HRESULT hr = dwriteFactory_->CreateTextLayout(
                    label.c_str(), static_cast<UINT32>(label.size()),
                    fmt, 500.0f, 30.0f, &textLayout);

                if (SUCCEEDED(hr) && textLayout) {
                    DWRITE_TEXT_METRICS tm = {};
                    textLayout->GetMetrics(&tm);

                    const float pillH = 22.0f * scale;
                    const float pillW = tm.width + 28.0f * scale;
                    const float pillX = cx - pillW * 0.5f;
                    const float pillY = rect.bottom - 18.0f * scale - pillH;

                    ComPtr<ID2D1SolidColorBrush> badgeBg;
                    target_->CreateSolidColorBrush(WithAlpha(material_.raised, material_.raised.a * settingsOpacity_), &badgeBg);
                    if (badgeBg) {
                        target_->FillRoundedRectangle(
                            D2D1::RoundedRect(D2D1::RectF(pillX, pillY, pillX + pillW, pillY + pillH), pillH * 0.5f, pillH * 0.5f),
                            badgeBg.Get());
                    }

                    ComPtr<ID2D1SolidColorBrush> dotBrush;
                    dotColor.a = settingsOpacity_;
                    target_->CreateSolidColorBrush(dotColor, &dotBrush);
                    if (dotBrush) {
                        const float badgeDotR = 3.5f * scale;
                        target_->FillEllipse(
                            D2D1::Ellipse(D2D1::Point2F(pillX + 10.0f * scale, pillY + pillH * 0.5f), badgeDotR, badgeDotR),
                            dotBrush.Get());
                    }

                    mutedBrush_->SetOpacity(0.90f);
                    target_->DrawTextLayout(
                        D2D1::Point2F(pillX + 18.0f * scale, pillY + (pillH - tm.height) * 0.5f),
                        textLayout.Get(), mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
                    mutedBrush_->SetOpacity(0.75f);
                }
            }
        }
''', '''        const bool micActive = state.system.micActive && settings.privacyDots && settings.privacyDotsMic;
        const bool camActive = state.system.cameraActive && settings.privacyDots && settings.privacyDotsCam;

        if (camActive || micActive) {
            struct Chip {
                std::wstring text;
                D2D1_COLOR_F color;
                float width = 0.0f;
            };
            std::vector<Chip> chips;
            if (camActive) {
                Chip chip;
                chip.text = state.system.cameraApp.empty()
                    ? std::wstring(L"摄像头")
                    : ShortAppName(state.system.cameraApp) + L" · 摄像头";
                chip.color = settings.privacyDotsCamHex;
                chip.width = MeasureTextWidthCached(chip.text, smallTextFormat_.Get(),
                                                    privacyCamWidthCache_);
                chips.push_back(chip);
            }
            if (micActive) {
                Chip chip;
                chip.text = state.system.micApp.empty()
                    ? std::wstring(L"麦克风")
                    : ShortAppName(state.system.micApp) + L" · 麦克风";
                chip.color = settings.privacyDotsMicHex;
                chip.width = MeasureTextWidthCached(chip.text, smallTextFormat_.Get(),
                                                    privacyMicWidthCache_);
                chips.push_back(chip);
            }

            const float chipH = 22.0f * scale;
            const float chipGap = 8.0f * scale;
            const float textPadLeft = 18.0f * scale;
            const float textPadRight = 10.0f * scale;
            float totalWidth = 0.0f;
            for (size_t i = 0; i < chips.size(); ++i) {
                totalWidth += chips[i].width + textPadLeft + textPadRight;
                if (i + 1 < chips.size()) totalWidth += chipGap;
            }
            float chipX = cx - totalWidth * 0.5f;
            const float chipY = rect.bottom - 8.0f * scale - chipH;

            const auto oldAlign = smallTextFormat_->GetTextAlignment();
            const auto oldParagraph = smallTextFormat_->GetParagraphAlignment();
            smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            for (const Chip& chip : chips) {
                const float chipW = chip.width + textPadLeft + textPadRight;
                ComPtr<ID2D1SolidColorBrush> badgeBg;
                target_->CreateSolidColorBrush(
                    WithAlpha(material_.raised, material_.raised.a * settingsOpacity_),
                    &badgeBg);
                if (badgeBg) {
                    target_->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(chipX, chipY, chipX + chipW, chipY + chipH),
                                          chipH * 0.5f, chipH * 0.5f), badgeBg.Get());
                }
                D2D1_COLOR_F dotColor = chip.color;
                dotColor.a = settingsOpacity_;
                ComPtr<ID2D1SolidColorBrush> dotBrush;
                target_->CreateSolidColorBrush(dotColor, &dotBrush);
                if (dotBrush) {
                    const float dotR = 3.5f * scale;
                    target_->FillEllipse(
                        D2D1::Ellipse(D2D1::Point2F(chipX + 9.0f * scale, chipY + chipH * 0.5f),
                                      dotR, dotR), dotBrush.Get());
                }
                mutedBrush_->SetOpacity(0.90f);
                target_->DrawTextW(chip.text.c_str(), static_cast<UINT32>(chip.text.size()),
                    smallTextFormat_.Get(),
                    D2D1::RectF(chipX + textPadLeft, chipY, chipX + chipW - textPadRight, chipY + chipH),
                    mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
                chipX += chipW + chipGap;
            }
            smallTextFormat_->SetTextAlignment(oldAlign);
            smallTextFormat_->SetParagraphAlignment(oldParagraph);
            mutedBrush_->SetOpacity(0.75f);
        }
''')

    # ---- Expanded panel height and cross-fade -----------------------------
    change('''        const float expandedAlpha = IdleDashboardExpandedAlpha(dashHeight, NormalizedTabIndex(settings) == 0 ? 124.0f : 144.0f);''',
           '''        const bool timerDetail = settings.timerEnabled &&
            (state.timer.active || state.timer.justFinished);
        const float expandedFullHeight = timerDetail
            ? TimerDetailPanelHeight(state, settings)
            : (NormalizedTabIndex(settings) == 0 ? 124.0f : 144.0f);
        const float expandedAlpha = IdleDashboardExpandedAlpha(dashHeight, expandedFullHeight);''')

    # ---- Finished state lives long enough to act on -----------------------
    change('''                g_state.timer.finishedExpiresAt = now + 6.0;''',
           '''                g_state.timer.finishedExpiresAt = now + 30.0;''')

    # ---- Finished collapsed pill says what happened ----------------------
    change('''        const wchar_t* label = state.timer.justFinished ? L"完成" :
            (!state.timer.running ? L"已暂停" : (state.timer.isBreak ? L"休息" : L"专注"));
        wchar_t value[32]{};
        const int seconds = static_cast<int>(std::ceil(remaining));
        swprintf_s(value, L"%d:%02d", seconds / 60, seconds % 60);
        const float labelWidth = MeasureTextWidthCached(label, smallTextFormat_.Get(), timerLabelWidthCache_);
        const float valueWidth = MeasureTextWidthCached(value, textFormat_.Get(), timerValueWidthCache_);
        const float groupWidth = 18 + 12 + labelWidth + 12 + valueWidth;''',
           '''        const wchar_t* label = state.timer.justFinished
            ? (state.timer.isBreak ? L"休息结束" : L"专注完成")
            : (!state.timer.running ? L"已暂停" : (state.timer.isBreak ? L"休息" : L"专注"));
        const bool showValue = !state.timer.justFinished;
        wchar_t value[32]{};
        const int seconds = static_cast<int>(std::ceil(remaining));
        swprintf_s(value, L"%d:%02d", seconds / 60, seconds % 60);
        const float labelWidth = MeasureTextWidthCached(label, smallTextFormat_.Get(), timerLabelWidthCache_);
        const float valueWidth = showValue
            ? MeasureTextWidthCached(value, textFormat_.Get(), timerValueWidthCache_)
            : 0.0f;
        const float groupWidth = 18 + 12 + labelWidth + (showValue ? 12 + valueWidth : 0.0f);''')

    change('''        target_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), smallTextFormat_.Get(),
            D2D1::RectF(tx, rect.top, tx + labelWidth + 1, rect.bottom), mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        target_->DrawTextW(value, static_cast<UINT32>(wcslen(value)), textFormat_.Get(),
            D2D1::RectF(tx + labelWidth + 12, rect.top, tx + labelWidth + 13 + valueWidth, rect.bottom),
            textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);''',
           '''        target_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), smallTextFormat_.Get(),
            D2D1::RectF(tx, rect.top, tx + labelWidth + 1, rect.bottom), mutedBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        if (showValue) {
            target_->DrawTextW(value, static_cast<UINT32>(wcslen(value)), textFormat_.Get(),
                D2D1::RectF(tx + labelWidth + 12, rect.top, tx + labelWidth + 13 + valueWidth, rect.bottom),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        }''')

    # ---- Text formats -----------------------------------------------------
    change('''        timerLabelWidthCache_.valid = false;
        timerValueWidthCache_.valid = false;''', '''        timerLabelWidthCache_.valid = false;
        timerValueWidthCache_.valid = false;
        privacyCamWidthCache_.valid = false;
        privacyMicWidthCache_.valid = false;''')

    change('''        hugeTextFormat_ = nullptr;''', '''        hugeTextFormat_ = nullptr;
        timerHeroFormat_ = nullptr;''')

    change('''        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 42.0f * ts, hugeTextFormat_);''',
           '''        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 42.0f * ts, hugeTextFormat_);
        // 32px keeps the countdown legible at 160% text size without spilling
        // out of the fixed-height timestamp row.
        createFormat(mainFamily, defaultDisplay,
                     DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                     DWRITE_FONT_STRETCH_NORMAL, 32.0f * ts, timerHeroFormat_);''')

    change('''        if (clockFormat_) {
            clockFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);''',
           '''        if (timerHeroFormat_) {
            timerHeroFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            timerHeroFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            timerHeroFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        if (clockFormat_) {
            clockFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);''')

    change('''    TextWidthCache timerLabelWidthCache_;
    TextWidthCache timerValueWidthCache_;''', '''    TextWidthCache timerLabelWidthCache_;
    TextWidthCache timerValueWidthCache_;
    TextWidthCache privacyCamWidthCache_;
    TextWidthCache privacyMicWidthCache_;''')

    change('''    ComPtr<IDWriteTextFormat> clockFormat_;''', '''    ComPtr<IDWriteTextFormat> clockFormat_;
    ComPtr<IDWriteTextFormat> timerHeroFormat_;''')

    # ---- Mouse: shared timer surface, actions and hover -------------------
    change('''    static int quickHomePressed = -1;''', '''    static int quickHomePressed = -1;
    static int timerDetailPressed = -1;''')

    change('''                quickHomePressed = -1;
                {
                    std::lock_guard lock(g_stateMutex);
                    const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                    if (!kinds.empty() && kinds[0] == IslandKind::Idle && NormalizedTabIndex(g_settings) == 0 &&
                        !g_settings.gameOverlay && Wh_GetIntValue(L"GameOverlayPinned", 0) == 0)
                        quickHomePressed = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
                }
                if (quickHomePressed >= 0) { g_layoutDirty = true; return 0; }

                bool mediaActive = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    mediaActive = g_settings.media && g_state.media.available;
                }''',
           '''                quickHomePressed = -1;
                timerDetailPressed = -1;
                bool timerSurface = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                    const bool timerSession = g_settings.timerEnabled &&
                        (g_state.timer.active || g_state.timer.justFinished);
                    if (!kinds.empty() && kinds[0] == IslandKind::Idle && timerSession &&
                        g_timerDetailVisible.load()) {
                        const MediaContentPoint timerPoint = TimerDetailFromClient(xPos, yPos, true);
                        if (timerPoint.valid && timerPoint.height >= 150.0f) {
                            timerSurface = true;
                            timerDetailPressed = TimerDetailHitTest(timerPoint);
                        }
                    } else if (!kinds.empty() && kinds[0] == IslandKind::Idle &&
                        NormalizedTabIndex(g_settings) == 0 && !g_settings.gameOverlay &&
                        Wh_GetIntValue(L"GameOverlayPinned", 0) == 0) {
                        quickHomePressed = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
                    }
                }
                if (timerSurface) {
                    if (timerDetailPressed >= 0) { g_layoutDirty = true; SetCapture(hwnd); }
                    return 0;
                }
                if (quickHomePressed >= 0) { g_layoutDirty = true; return 0; }

                bool mediaActive = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                    mediaActive = g_settings.media && g_state.media.available &&
                        !kinds.empty() && kinds[0] == IslandKind::Media;
                }''')

    change('''            int quickHovered = -1;
            {
                std::lock_guard lock(g_stateMutex);
                const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                if (!kinds.empty() && kinds[0] == IslandKind::Idle && NormalizedTabIndex(g_settings) == 0 &&
                    !g_settings.gameOverlay && Wh_GetIntValue(L"GameOverlayPinned", 0) == 0)
                    quickHovered = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
            }
            if (g_hoveredQuickAction.exchange(quickHovered) != quickHovered) g_layoutDirty = true;''',
           '''            int quickHovered = -1;
            int timerHovered = -1;
            int companionHovered = -1;
            {
                std::lock_guard lock(g_stateMutex);
                const auto kinds = ChooseActivities(g_state, g_settings, NowSeconds());
                const bool timerSession = g_settings.timerEnabled &&
                    (g_state.timer.active || g_state.timer.justFinished);
                if (!kinds.empty() && kinds[0] == IslandKind::Idle && timerSession &&
                    g_timerDetailVisible.load()) {
                    const MediaContentPoint timerPoint = TimerDetailFromClient(xPos, yPos, true);
                    if (timerPoint.valid && timerPoint.height >= 150.0f)
                        timerHovered = TimerDetailHitTest(timerPoint);
                } else if (!kinds.empty() && kinds[0] == IslandKind::Idle &&
                    NormalizedTabIndex(g_settings) == 0 && !g_settings.gameOverlay &&
                    Wh_GetIntValue(L"GameOverlayPinned", 0) == 0) {
                    quickHovered = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));
                }
                if (g_mediaCompanionExpanded.load() && g_settings.media && g_state.media.available) {
                    companionHovered = MediaCompanionHitTest(
                        MediaContentFromClient(xPos, yPos, true));
                }
            }
            if (g_hoveredQuickAction.exchange(quickHovered) != quickHovered) g_layoutDirty = true;
            if (g_hoveredTimerAction.exchange(timerHovered) != timerHovered) g_layoutDirty = true;
            if (g_hoveredCompanionButton.exchange(companionHovered) != companionHovered)
                g_layoutDirty = true;''')

    change('''        case WM_MOUSELEAVE:
            if (g_hoveredQuickAction.exchange(-1) != -1) g_layoutDirty = true;''',
           '''        case WM_MOUSELEAVE:
            if (g_hoveredQuickAction.exchange(-1) != -1) g_layoutDirty = true;
            if (g_hoveredTimerAction.exchange(-1) != -1) g_layoutDirty = true;
            if (g_hoveredCompanionButton.exchange(-1) != -1) g_layoutDirty = true;''')

    change('''                if (!kinds.empty() && kinds[0] == IslandKind::Idle && currentTab == 0 && !gameMetricsPresent) {
                    const int action = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));''',
           '''                bool timerSurface = false;
                bool timerFinished = false;
                int timerAction = -1;
                {
                    std::lock_guard lock(g_stateMutex);
                    const bool timerSession = g_settings.timerEnabled &&
                        (g_state.timer.active || g_state.timer.justFinished);
                    timerFinished = g_state.timer.justFinished && !g_state.timer.active;
                    if (!kinds.empty() && kinds[0] == IslandKind::Idle && timerSession &&
                        g_timerDetailVisible.load()) {
                        const MediaContentPoint timerPoint = TimerDetailFromClient(xPos, yPos, true);
                        if (timerPoint.valid && timerPoint.height >= 150.0f) {
                            timerSurface = true;
                            timerAction = TimerDetailHitTest(timerPoint);
                        }
                    }
                }
                if (timerSurface) {
                    const bool tap = timerAction >= 0 && timerAction == timerDetailPressed &&
                        std::abs(xPos - s_touchStart.x) <= 8 &&
                        std::abs(yPos - s_touchStart.y) <= 8;
                    timerDetailPressed = -1;
                    if (tap) {
                        if (timerFinished) {
                            if (timerAction == 0) {
                                StartFocusTimer(5, true);
                            } else {
                                int minutes = 25;
                                bool isBreak = false;
                                {
                                    std::lock_guard lock(g_stateMutex);
                                    minutes = std::max(1, g_state.timer.totalSeconds / 60);
                                    isBreak = g_state.timer.isBreak;
                                }
                                StartFocusTimer(minutes, isBreak);
                            }
                        } else if (timerAction == 0) {
                            ToggleTimerPause();
                        } else {
                            StopFocusTimer();
                        }
                        g_layoutDirty = true;
                    }
                    return 0;
                }

                if (g_mediaCompanionExpanded.load() &&
                    MediaCompanionHitTest(MediaContentFromClient(xPos, yPos, true)) == 1) {
                    SendMediaTransportCommand(1);
                    g_layoutDirty = true;
                    return 0;
                }

                if (!kinds.empty() && kinds[0] == IslandKind::Idle && currentTab == 0 && !gameMetricsPresent) {
                    const int action = QuickHomeHitTest(MediaContentFromClient(xPos, yPos, true));''')

    # ---- Cursor hand for the new targets ---------------------------------
    change('''                if (g_hoveredQuickAction.load() >= 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }''', '''                if (g_hoveredQuickAction.load() >= 0 || g_hoveredTimerAction.load() >= 0 ||
                    g_hoveredCompanionButton.load() >= 0) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }''')

    # ---- Expanded panel height for the timer activity ---------------------
    change('''        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen && (pinned || isHoverExpanded)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                const bool quickHome = NormalizedTabIndex(g_settings) == 0 && !privacyActive;
                primary.height = (quickHome ? 124.0f : MediaLayout::kExpandedHeight) * g_settings.sizeScale;''',
           '''        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen && (pinned || isHoverExpanded)) {
                primary.width = MediaLayout::kExpandedWidth * g_settings.sizeScale;
                const bool timerDetail = g_settings.timerEnabled &&
                    (snapshot.timer.active || snapshot.timer.justFinished);
                const bool quickHome = !timerDetail && NormalizedTabIndex(g_settings) == 0 && !privacyActive;
                primary.height = (timerDetail ? TimerDetailPanelHeight(snapshot, g_settings)
                    : (quickHome ? 124.0f : MediaLayout::kExpandedHeight)) * g_settings.sizeScale;''')

    # ---- Dual activity: timer pill + round media companion ----------------
    change('''        std::optional<Activity> secondary;
        if (kinds.size() >= 2) {
            secondary = ActivityForKind(kinds[1], g_settings, snapshot);
        }

        const bool pinned = Wh_GetIntValue(L"PinnedExpanded", 0) != 0;''',
           '''        std::optional<Activity> secondary;
        if (kinds.size() >= 2) {
            secondary = ActivityForKind(kinds[1], g_settings, snapshot);
        }

        const bool pinned = Wh_GetIntValue(L"PinnedExpanded", 0) != 0;

        // A running timer and active media share the island: the timer keeps the
        // main pill and media becomes a small round companion. It has its own
        // hover target, so each activity expands only when it is the one under
        // the pointer.
        const bool timerSessionActive = g_settings.timerEnabled &&
            (snapshot.timer.active || snapshot.timer.justFinished);
        const bool timerPrimary = primary.kind == IslandKind::Timer ||
            (primary.kind == IslandKind::Idle && timerSessionActive);
        const bool companionPossible = timerSessionActive && g_settings.media &&
            snapshot.media.available && secondary && secondary->kind == IslandKind::Media &&
            timerPrimary;''')

    change('''        bool isHoverExpanded = g_settings.expandOnHover ? hover : (hover && g_clickExpanded.load());''',
           '''        // The companion's published content rect is the source of truth for
        // whether the pointer is on it. Using the previous frame's geometry
        // avoids a feedback loop while the group re-centres as it grows.
        bool overCompanion = false;
        const bool timerWasExpanded = g_timerDashboardExpanded.load();
        if (companionPossible && hover) {
            const MediaContentPoint companionPoint = MediaContentFromClient(
                cursor.x - windowRect.left, cursor.y - windowRect.top, true);
            overCompanion = companionPoint.valid && companionPoint.height <= 60.0f &&
                companionPoint.width >= 30.0f &&
                companionPoint.x >= 0.0f && companionPoint.x <= companionPoint.width &&
                companionPoint.y >= 0.0f && companionPoint.y <= companionPoint.height;
            if (overCompanion) {
                const bool want = true;
                if (g_mediaCompanionExpanded.exchange(want) != want) {
                    g_layoutDirty = true;
                    needsRender = true;
                }
            }
        }
        if ((!companionPossible || !overCompanion) && g_mediaCompanionExpanded.exchange(false)) {
            g_layoutDirty = true;
            needsRender = true;
        }

        // Hovering the companion must not expand the timer from its collapsed
        // strip. If the timer panel is already open, leave it open beside the
        // unfolded media pill instead of collapsing and re-expanding it.
        const bool suppressTimerExpansion = overCompanion && !timerWasExpanded;
        bool isHoverExpanded = g_settings.expandOnHover
            ? (hover && !suppressTimerExpansion)
            : (hover && g_clickExpanded.load() && !suppressTimerExpansion);''')

    change('''        if (g_timerDashboardExpanded.exchange(!g_isFullscreen.load() && (pinned || isHoverExpanded)) !=
            (!g_isFullscreen.load() && (pinned || isHoverExpanded))) needsRender = true;''',
           '''        if (g_timerDashboardExpanded.exchange(!g_isFullscreen.load() && (pinned || isHoverExpanded)) !=
            (!g_isFullscreen.load() && (pinned || isHoverExpanded))) needsRender = true;

        if (companionPossible) {
            const bool expandedCompanion = g_mediaCompanionExpanded.load();
            secondary->companion = true;
            secondary->width = (expandedCompanion ? MediaCompanionLayout::kExpandedWidth
                                                  : MediaCompanionLayout::kOrbSize) *
                               g_settings.sizeScale;
            secondary->height = (expandedCompanion ? MediaCompanionLayout::kExpandedHeight
                                                   : MediaCompanionLayout::kOrbSize) *
                                g_settings.sizeScale;
        }''')

    # ---- Secondary pill reveal during the size animation -------------------
    change('''                DrawPill(state, settings, primary,
                         D2D1::RectF(left, pTop, left + primary.width, pTop + primary.height),
                         scale, now);
                DrawPill(state, settings, *secondary,
                         D2D1::RectF(left + primary.width + gap, sTop,
                                      left + primary.width + gap + secondary->width,
                                      sTop + secondary->height),
                         scale, now);''',
           '''                DrawPill(state, settings, primary,
                         D2D1::RectF(left, pTop, left + primary.width, pTop + primary.height),
                         scale, now);

                // While the window springs to the group's full width the
                // secondary pill sits outside the visible edge. Painting it at
                // its final position made it look clipped and dragged along by
                // the growing edge. Keep it hidden until the window can hold
                // it, then fade it in over the last 18 logical pixels of
                // travel. A fixed distance (not a fraction of the pill width)
                // matters: the media companion itself grows from 36 to 200px
                // while its left edge is already well inside the window, and
                // must stay fully visible during that growth.
                const float revealPx = 18.0f * settings.sizeScale;
                const float secondaryLeft = primary.width + gap;
                const float horizontalReveal = Clamp(
                    (width - secondaryLeft) / revealPx, 0.0f, 1.0f);
                const float secondaryTop = (maxH - secondary->height) * 0.5f;
                const float verticalReveal = Clamp(
                    (height - secondaryTop) / revealPx, 0.0f, 1.0f);
                const float secondaryAlpha = std::min(horizontalReveal, verticalReveal);
                if (secondaryAlpha > 0.01f) {
                    const float savedOpacity = settingsOpacity_;
                    settingsOpacity_ = Clamp(settings.pillOpacity * secondaryAlpha, 0.0f, 1.0f);
                    Settings secondarySettings = settings;
                    secondarySettings.pillOpacity = settingsOpacity_;
                    // Follow the window while the media companion unfolds,
                    // otherwise its 200px content would be laid out at full
                    // width and clipped by the still-growing window.
                    Activity secondaryDraw = *secondary;
                    if (secondary->companion && g_mediaCompanionExpanded.load()) {
                        const float available = std::max(0.0f, width - primary.width - gap);
                        secondaryDraw.width = Clamp(
                            available,
                            MediaCompanionLayout::kOrbSize * settings.sizeScale,
                            secondary->width);
                    }
                    DrawPill(state, secondarySettings, secondaryDraw,
                             D2D1::RectF(left + primary.width + gap, sTop,
                                          left + primary.width + gap + secondaryDraw.width,
                                          sTop + secondary->height),
                             scale, now);
                    settingsOpacity_ = savedOpacity;
                }''')

    return src
