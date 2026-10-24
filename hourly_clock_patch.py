from pathlib import Path


def apply_hourly_clock(src):
    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    header = (Path(__file__).parent / 'hourly-clock.h').read_text(encoding='utf-8').replace('#pragma once', '')
    change('struct SharedState {', header + '\nstruct SharedState {\n    HourlyClockSnapshot hourlyClock;')
    change('    NBAScore,\n    DoNotDisturb,', '    NBAScore,\n    HourlyClock,\n    DoNotDisturb,')
    change('''    if (settings.battery && state.battery.active && now < state.battery.expiresAt) {''', '''    if (activities.empty() && state.hourlyClock.active && now < state.hourlyClock.expiresAt) {
        return {IslandKind::HourlyClock};
    }
    if (settings.battery && state.battery.active && now < state.battery.expiresAt) {''')
    change('''        case IslandKind::NBAScore:
            activity.width = 300.0f;''', '''        case IslandKind::HourlyClock:
            activity.width = 260.0f;
            activity.height = 84.0f;
            break;
        case IslandKind::NBAScore:
            activity.width = 300.0f;''')
    change('''            case IslandKind::NBAScore:
                DrawNba(state, unscaledRect, settings);''', '''            case IslandKind::HourlyClock:
                DrawHourlyClock(state.hourlyClock, unscaledRect);
                break;
            case IslandKind::NBAScore:
                DrawNba(state, unscaledRect, settings);''')
    change('''                                 primary.kind == IslandKind::Notification ||''', '''                                 primary.kind == IslandKind::Notification ||
                                 primary.kind == IslandKind::HourlyClock ||''')
    change('''    TextWidthCache idleClockWidthCache_;''', '''    TextWidthCache idleClockWidthCache_;
    TextWidthCache hourlyClockWidthCache_;''')
    change('''        idleClockWidthCache_.valid = false;''', '''        idleClockWidthCache_.valid = false;
        hourlyClockWidthCache_.valid = false;''')
    change('''    void DrawNba(const SharedState& state, D2D1_RECT_F rect, const Settings& settings) {''', '''    void DrawHourlyClock(const HourlyClockSnapshot& clock, D2D1_RECT_F rect) {
        IDWriteTextFormat* format = timerHeroFormat_ ? timerHeroFormat_.Get() : clockFormat_.Get();
        if (!format) return;
        wchar_t label[16]{};
        swprintf_s(label, L"%02d:%02d", clock.hour, clock.minute);
        const float textWidth = MeasureTextWidthCached(label, format, hourlyClockWidthCache_);
        const float left = (rect.left + rect.right - (44.0f + 20.0f + textWidth)) * 0.5f;
        const auto center = D2D1::Point2F(left + 22.0f, (rect.top + rect.bottom) * 0.5f);
        textBrush_->SetOpacity(0.92f);
        target_->DrawEllipse(D2D1::Ellipse(center, 22, 22), textBrush_.Get(), 1.8f);
        for (int tick = 0; tick < 12; ++tick) {
            const float angle = tick * 3.14159265f / 6.0f;
            target_->DrawLine(D2D1::Point2F(center.x + std::sin(angle) * 18.0f,
                                         center.y - std::cos(angle) * 18.0f),
                              D2D1::Point2F(center.x + std::sin(angle) * 20.0f,
                                         center.y - std::cos(angle) * 20.0f), textBrush_.Get(), 1.0f);
        }
        const float hourAngle = (clock.hour % 12 + clock.minute / 60.0f) * 3.14159265f / 6.0f;
        const float minuteAngle = clock.minute * 3.14159265f / 30.0f;
        target_->DrawLine(center, D2D1::Point2F(center.x + std::sin(hourAngle) * 11,
            center.y - std::cos(hourAngle) * 11), textBrush_.Get(), 2.4f);
        target_->DrawLine(center, D2D1::Point2F(center.x + std::sin(minuteAngle) * 16,
            center.y - std::cos(minuteAngle) * 16), textBrush_.Get(), 1.8f);
        target_->FillEllipse(D2D1::Ellipse(center, 2, 2), textBrush_.Get());
        const auto oldAlign = format->GetTextAlignment();
        const auto oldPara = format->GetParagraphAlignment();
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        target_->DrawTextW(label, static_cast<UINT32>(wcslen(label)), format,
            D2D1::RectF(left + 64, rect.top, left + 66 + textWidth, rect.bottom),
            textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
        format->SetTextAlignment(oldAlign);
        format->SetParagraphAlignment(oldPara);
        textBrush_->SetOpacity(0.90f);
    }

    void DrawNba(const SharedState& state, D2D1_RECT_F rect, const Settings& settings) {''')
    change('''HANDLE g_nbaThread = nullptr;''', '''HANDLE g_nbaThread = nullptr;
HANDLE g_hourlyClockThread = nullptr;''')
    change('''bool StartThreads() {''', '''DWORD WINAPI HourlyClockThreadProc(void*) {
    HourlyClockTracker tracker;
    int lastPreview = Wh_GetIntValue(L"HourlyClockPreview", 0);
    Wh_SetIntValue(L"HourlyClockActive", 0);
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) {
        SYSTEMTIME local{};
        GetLocalTime(&local);
        const int preview = Wh_GetIntValue(L"HourlyClockPreview", 0);
        const bool triggered = tracker.Tick(local, preview != lastPreview);
        lastPreview = preview;
        bool expired = false;
        const double now = NowSeconds();
        {
            std::lock_guard lock(g_stateMutex);
            if (triggered) {
                g_state.hourlyClock = {true, local.wHour, local.wMinute, now + 10.0};
            } else if (g_state.hourlyClock.active && now >= g_state.hourlyClock.expiresAt) {
                g_state.hourlyClock.active = false;
                expired = true;
            }
        }
        if (triggered || expired) {
            Wh_SetIntValue(L"HourlyClockActive", triggered ? 1 : 0);
            g_layoutDirty = true;
            TriggerNudge();
        }
        if (WaitForSingleObject(g_stopEvent, 1000) != WAIT_TIMEOUT) break;
    }
    return 0;
}

bool StartThreads() {''')
    change('''    g_nbaThread = CreateThread(nullptr, 0, NbaThreadProc, nullptr, 0, nullptr);''', '''    g_nbaThread = CreateThread(nullptr, 0, NbaThreadProc, nullptr, 0, nullptr);
    g_hourlyClockThread = CreateThread(nullptr, 0, HourlyClockThreadProc, nullptr, 0, nullptr);''')
    change('''g_weatherThread, g_nbaThread, g_notificationThread''', '''g_weatherThread, g_nbaThread, g_hourlyClockThread, g_notificationThread''')
    change('''    g_nbaThread = nullptr;
    g_notificationThread = nullptr;''', '''    g_nbaThread = nullptr;
    g_hourlyClockThread = nullptr;
    g_notificationThread = nullptr;''')
    return src
