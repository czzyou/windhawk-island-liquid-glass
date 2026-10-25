from pathlib import Path


def apply_volume_control(src):
    root=Path(__file__).parent
    def change(old,new):
        nonlocal src
        assert src.count(old)==1,(old[:100],src.count(old))
        src=src.replace(old,new,1)
    helper=(root/'volume-controls.h').read_text(encoding='utf-8').replace('#pragma once','')
    change('struct Settings {',helper+'\nstruct Settings {')
    change('bool g_volumeInitialized = false;\n','')
    start=src.index('    static ComPtr<IAudioEndpointVolume> s_volume;')
    end=src.index('// ---- Privacy indicator helpers ----',start)
    src=src[:start]+'''    std::lock_guard lock(g_stateMutex);
    // The independent volume watcher owns these fields, including while parked.
    next.volumePercent=g_state.system.volumePercent;
    next.volumeMuted=g_state.system.volumeMuted;
    g_state.system=next;
}

'''+src[end:]
    runtime=(root/'volume-control-runtime.h').read_text(encoding='utf-8')
    change('void UpdateSystemSnapshot(bool includeGpuStats, bool includeNetStats) {',runtime+'\nvoid UpdateSystemSnapshot(bool includeGpuStats, bool includeNetStats) {')
    start=src.index('void ToggleEndpointMute() {')
    end=src.index('// Shared by the scrubber',start)
    src=src[:start]+'''void ToggleEndpointMute() {
    std::lock_guard lock(g_endpointVolumeMutex);
    const auto endpoint=OpenCurrentVolumeEndpoint();
    BOOL muted=FALSE;
    if (endpoint && SUCCEEDED(endpoint->GetMute(&muted)) && SUCCEEDED(endpoint->SetMute(!muted,nullptr)))
        PublishCurrentVolume(endpoint.Get(),true);
}

'''+src[end:]
    change('HANDLE g_hourlyClockThread = nullptr;','HANDLE g_hourlyClockThread = nullptr;\nHANDLE g_volumeControlThread = nullptr;\nHANDLE g_volumeChangedEvent = nullptr;')
    change('    g_hourlyClockThread = CreateThread(nullptr, 0, HourlyClockThreadProc, nullptr, 0, nullptr);','''    g_hourlyClockThread = CreateThread(nullptr, 0, HourlyClockThreadProc, nullptr, 0, nullptr);
    g_volumeChangedEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!g_volumeChangedEvent) return false;
    g_volumeControlThread = CreateThread(nullptr, 0, VolumeControlThreadProc, nullptr, 0, nullptr);
    if (!g_volumeControlThread) return false;''')
    change('g_nbaThread, g_hourlyClockThread, g_notificationThread','g_nbaThread, g_hourlyClockThread, g_volumeControlThread, g_notificationThread')
    change('    g_hourlyClockThread = nullptr;\n    g_notificationThread = nullptr;','    g_hourlyClockThread = nullptr;\n    g_volumeControlThread = nullptr;\n    if (g_volumeChangedEvent) { CloseHandle(g_volumeChangedEvent); g_volumeChangedEvent=nullptr; }\n    g_notificationThread = nullptr;')
    change('''    void DrawVolume(const SharedState& state, D2D1_RECT_F rect) {
''','''    void DrawVolume(const SharedState& state, D2D1_RECT_F rect) {
        D2D1_MATRIX_3X2_F transform;
        target_->GetTransform(&transform);
        PublishVolumeGeometry(rect,transform);
''')
    change('''        D2D1_RECT_F track = D2D1::RectF(tx, cy + 7.0f, rect.right - 14, cy + 12.0f);''','''        auto track=VolumeTrackBounds(rect.right-rect.left,rect.bottom-rect.top);
        track.left+=rect.left; track.right+=rect.left;
        track.top+=rect.top; track.bottom+=rect.top;''')
    change('''        accentBrush_->SetOpacity(1.0f);
        mutedBrush_->SetOpacity(0.58f);
    }

    void DrawHourlyClock''','''        textBrush_->SetOpacity(0.92f);
        const float thumbX=track.left+(track.right-track.left)*pct;
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX,(track.top+track.bottom)*0.5f),4.0f,4.0f),textBrush_.Get());
        accentBrush_->SetOpacity(1.0f);
        mutedBrush_->SetOpacity(0.58f);
    }

    void DrawHourlyClock''')
    change('''state.system.volumeMuted ? L"声音" : L"静音", L"专注 25′"''','''L"音量", L"专注 25′"''')
    change('else if (action == 1) ToggleEndpointMute();','else if (action == 1) ShowIslandVolume();')
    change('constexpr UINT WM_APP_CAPSLOCK =', '''bool VolumeIsPrimary() {
    const auto settings=GetSettingsCopy();
    std::lock_guard lock(g_stateMutex);
    const auto kinds=ChooseActivities(g_state,settings,NowSeconds());
    return !kinds.empty() && kinds[0]==IslandKind::Volume;
}

constexpr UINT WM_APP_CAPSLOCK =''')
    change('''                s_touchStart.x = xPos;''','''                if (VolumeIsPrimary()) {
                    const auto point=VolumePointFromClient(xPos,yPos);
                    const int hit=VolumeControlHitTest(point);
                    if (hit==1) { ToggleEndpointMute(); return 0; }
                    if (hit==0) {
                        g_volumeDragging=true;
                        SetCapture(hwnd);
                        SetIslandVolume(VolumePercentAt(point));
                        return 0;
                    }
                }
                s_touchStart.x = xPos;''')
    change('''            int quickHovered = -1;''','''            if (g_volumeDragging.load()) {
                SetIslandVolume(VolumePercentAt(VolumePointFromClient(xPos,yPos)));
                return 0;
            }
            int quickHovered = -1;''')
    change('''        case WM_LBUTTONUP:
            {''','''        case WM_LBUTTONUP:
            {
                if (g_volumeDragging.exchange(false)) {
                    SetIslandVolume(VolumePercentAt(VolumePointFromClient(GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam))));
                    ReleaseCapture();
                    return 0;
                }
                if (VolumeIsPrimary()) return 0;''')
    change('''        case WM_CAPTURECHANGED:
''','''        case WM_CAPTURECHANGED:
            g_volumeDragging=false;
''')
    change('''        case WM_SETCURSOR:
''','''        case WM_SETCURSOR:
            if (VolumeIsPrimary()) {
                POINT point{}; GetCursorPos(&point); ScreenToClient(hwnd,&point);
                if (VolumeControlHitTest(VolumePointFromClient(point.x,point.y))>=0) {
                    SetCursor(LoadCursorW(nullptr,IDC_HAND)); return TRUE;
                }
            }
''')
    change('settings.volume && state.volume.active && now < state.volume.expiresAt',
           'settings.volume && state.volume.active && (g_volumeDragging.load() || now < state.volume.expiresAt)')
    change('g_state.volume.active && now >= g_state.volume.expiresAt',
           'g_state.volume.active && !g_volumeDragging.load() && now >= g_state.volume.expiresAt')
    change('    constexpr float kControlSpacing = 64.0f;', '''    constexpr float kControlSpacing = 64.0f;
    constexpr float kVolumeInsetRight = 44.0f;
    constexpr float kVolumeButtonRadius = 14.0f;''')
    change('// Returns 0 = previous, 1 = play/pause, 2 = next, or -1 for no button.',
           '// Returns 0 = previous, 1 = play/pause, 2 = next, 3 = volume, or -1 for no button.')
    change('        {MediaLayout::kControlSpacing, MediaLayout::kNavButtonRadius, 2},',
           '''        {MediaLayout::kControlSpacing, MediaLayout::kNavButtonRadius, 2},
        {centerX - MediaLayout::kVolumeInsetRight, MediaLayout::kVolumeButtonRadius, 3},''')
    change('''                        SendMediaTransportCommand(cmd);''', '''                        if (cmd == 3) ShowIslandVolume();
                        else SendMediaTransportCommand(cmd);''')
    change('''                                  now);
            } else if (activeTabId == 1) {''', '''                                  now);
                DrawMediaVolumeButton(state.system.volumeMuted,
                    D2D1::Point2F(rect.right - MediaLayout::kVolumeInsetRight, cy));
            } else if (activeTabId == 1) {''')
    change('    void DrawMediaControls(bool playing,', '''    void DrawMediaVolumeButton(bool muted, D2D1_POINT_2F center) {
        const bool hovered = g_hoveredMediaButton.load() == 3;
        const float radius = MediaLayout::kVolumeButtonRadius;
        accentBrush_->SetOpacity(hovered ? 0.18f : 0.08f);
        target_->FillEllipse(D2D1::Ellipse(center, radius, radius), accentBrush_.Get());
        if (iconFormat_) {
            const auto alignment = iconFormat_->GetTextAlignment();
            const auto paragraph = iconFormat_->GetParagraphAlignment();
            iconFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            iconFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            const wchar_t* glyph = muted ? L"\\uE74F" : (usingFluentIcons_ ? L"\\uE767" : L"\\uE993");
            accentBrush_->SetOpacity(hovered ? 1.0f : 0.85f);
            target_->DrawTextW(glyph, 1, iconFormat_.Get(),
                D2D1::RectF(center.x - radius, center.y - radius, center.x + radius, center.y + radius),
                accentBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            iconFormat_->SetTextAlignment(alignment);
            iconFormat_->SetParagraphAlignment(paragraph);
        }
        accentBrush_->SetOpacity(1.0f);
    }

    void DrawMediaControls(bool playing,''')
    return src
