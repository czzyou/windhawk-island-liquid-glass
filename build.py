from pathlib import Path
import subprocess
import sys
import yaml
import json
import re

root = Path(__file__).resolve().parent
src = (root/'original.wh.cpp').read_text(encoding='utf-8-sig')
def replace(old, new):
    global src
    assert src.count(old) == 1, (old[:90], src.count(old))
    src = src.replace(old, new, 1)

replace('// @id              dynamic-island-for-windows', '// @id              dynamic-island-for-windows-yasb')
replace('// @name            Dynamic Island for Windows', '// @name            Dynamic Island for Windows - YASB integration')
replace('// @version         1.3.1', '// @version         1.3.1-local.60')
replace('(g_settings.autoHideIdleSeconds == -1 ? 0.0f : 36.0f)', '(g_settings.autoHideIdleSeconds == -1 ? 0.0f : IdleStripLayout::kHeight)')
notification_route = '''std::wstring NotificationLaunchTarget(const std::wstring& appId) {
    if (appId.empty() || appId.find_first_of(L"\\\\/:\\\"\\r\\n") != std::wstring::npos ||
        std::any_of(appId.begin(), appId.end(), [](wchar_t c) { return c < 32; }))
        return L"ms-actioncenter:";
    return L"shell:AppsFolder\\\\" + appId;
}

'''
replace('''    std::wstring app;
    std::wstring title;
    std::wstring body;''', '''    std::wstring app;
    std::wstring sourceAppId;
    std::wstring title;
    std::wstring body;''')
replace('''void HandleStatusClickAtPoint(HWND hwnd, LPARAM lParam) {''', notification_route + '''bool OpenNotificationSource(const std::wstring& appId) {
    const auto target = NotificationLaunchTarget(appId);
    SHELLEXECUTEINFOW launch{sizeof(launch)};
    launch.fMask = SEE_MASK_FLAG_NO_UI;
    launch.lpFile = target.c_str();
    launch.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&launch)) return true;
    if (target == L"ms-actioncenter:") return false;
    launch.lpFile = L"ms-actioncenter:";
    return ShellExecuteExW(&launch) != FALSE;
}

void HandleStatusClickAtPoint(HWND hwnd, LPARAM lParam) {''')
replace('''    static ULONGLONG s_touchStartTime = 0;''', '''    static ULONGLONG s_touchStartTime = 0;
    static bool notificationPressed = false;
    static std::wstring notificationAppId;
    static double notificationExpiry = 0.0;''')
replace('''                s_touchStartTime = GetTickCount64();''', '''                s_touchStartTime = GetTickCount64();
                notificationPressed = false;
                {
                    std::lock_guard lock(g_stateMutex);
                    const auto visible = ChooseActivities(g_state, g_settings, NowSeconds());
                    if (!visible.empty() && visible[0] == IslandKind::Notification) {
                        notificationPressed = true;
                        notificationAppId = g_state.notification.sourceAppId;
                        notificationExpiry = g_state.notification.expiresAt;
                    }
                }
                // A notification must not fall through to media transport hit tests.
                if (notificationPressed) return 0;''')
replace('''                ULONGLONG now = GetTickCount64();''', '''                if (notificationPressed) {
                    notificationPressed = false;
                    const bool tap = s_touchStartTime > 0 && GetTickCount64() - s_touchStartTime < 500 &&
                        std::abs(xPos - s_touchStart.x) <= 8 && std::abs(yPos - s_touchStart.y) <= 8;
                    s_touchStartTime = 0;
                    if (tap && OpenNotificationSource(notificationAppId)) {
                        std::lock_guard lock(g_stateMutex);
                        if (g_state.notification.expiresAt == notificationExpiry)
                            g_state.notification.active = false;
                        g_layoutDirty = true;
                    }
                    return 0;
                }
                ULONGLONG now = GetTickCount64();''')
replace('''    void Step(float totalDt, float stiffness, float damping) {''', '''    void StepExponential(float dt) {
        if (dt <= 0.0f) return;
        const float previous = value;
        // Match the media-button release easing, with no spring overshoot.
        value += (target - value) * (1.0f - std::exp(-dt / 0.075f));
        velocity = (value - previous) / dt;
        if (std::fabs(value - target) < 0.05f) {
            value = target;
            velocity = 0.0f;
        }
    }

    void Step(float totalDt, float stiffness, float damping) {''')
replace('''        widthSpring.Step(dt * speed, widthStiffness, widthDamping);''', '''        const bool compactResize = compactEligible && !isHoverExpanded && !pinned &&
            targetHeight > 0.0f && heightSpring.value <= 36.0f * g_settings.sizeScale + 0.5f;
        if (compactResize) widthSpring.StepExponential(dt * speed);
        else widthSpring.Step(dt * speed, widthStiffness, widthDamping);''')
replace('''        heightSpring.Step(dt * speed, heightStiffness, heightDamping);''', '''        if (compactResize) heightSpring.StepExponential(dt * speed);
        else heightSpring.Step(dt * speed, heightStiffness, heightDamping);''')
clock_helper = '''std::wstring FormatIslandStripClock(const SYSTEMTIME& local, const Settings& settings) {
    const auto time = FormatIslandTime(local, settings.clockFollowSystem,
                                      settings.use24HourClock, settings.showSeconds);
    wchar_t dateBuffer[64]{};
    const std::wstring date = GetDateFormatEx(L"en-US", 0, &local, L"ddd, MMM d",
        dateBuffer, ARRAYSIZE(dateBuffer), nullptr) > 0 ? dateBuffer :
        FormatIslandDate(local, L"MM-dd", L"MM-dd");
    return settings.dateFirst ? date + L" · " + time : time + L" · " + date;
}

'''
replace('const wchar_t* Loc(const wchar_t* english) {', clock_helper+'const wchar_t* Loc(const wchar_t* english) {')
replace('''        const std::wstring clock = FormatIslandTime(local, settings.clockFollowSystem,
                                                    settings.use24HourClock, settings.showSeconds);''',
        '''        const std::wstring clock = FormatIslandStripClock(local, settings);''')
replace('''        const std::wstring collapsedTime = FormatIslandTime(local, settings.clockFollowSystem,
                                                            settings.use24HourClock,
                                                            settings.showSeconds);''',
        '''        const std::wstring collapsedTime = FormatIslandStripClock(local, settings);''')
helper = '''std::atomic<bool> g_maximizedCompact{false};
std::atomic<float> g_idleCompactBlend{0.0f};
std::atomic<float> g_compactInsetY{0.0f};

float CompactIdleScale(float height, float normalHeight) {
    return (std::max)(29.0f / 36.0f, (std::min)(1.0f, height / normalHeight));
}

int YasbDockY(int screenTop) {
    // Compact mode keeps the normal pill's center; dashboard expansion stays top-anchored.
    return screenTop + 8 + static_cast<int>(std::lround(g_compactInsetY.load())) - static_cast<int>(kRenderPadY);
}

'''
replace('void PositionOverlayWindow(HWND hwnd, int width, int height) {', helper+'void PositionOverlayWindow(HWND hwnd, int width, int height) {')
replace('''    y += ResolveOffsetY(static_cast<float>(height));
    SetWindowPos''', '''    y += ResolveOffsetY(static_cast<float>(height));
    if (g_settings.position == Position::TopCenter && !flushTop) {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromRect(&work, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
            y = YasbDockY(monitor.rcMonitor.top);
        }
    }
    SetWindowPos''')
replace('''        const float scale = hoverScale;''', '''        const float scale = settings.position == Position::TopCenter ? 1.0f : hoverScale;''')
replace('''        const float top = (settings.notchStyle || settings.borderMergedMode) ? std::max(0.0f, nudge) : (kRenderPadY + nudge);
        const float left = kRenderPadX;''', '''        float top = (settings.notchStyle || settings.borderMergedMode) ? std::max(0.0f, nudge) : (kRenderPadY + nudge);
        float left = kRenderPadX;
        if (settings.position == Position::TopCenter && !settings.notchStyle && !settings.borderMergedMode) {
            RECT window{};
            RECT work = GetAnchorWorkRect();
            MONITORINFO monitor{sizeof(MONITORINFO)};
            if (GetWindowRect(hwnd_, &window) &&
                GetMonitorInfoW(MonitorFromRect(&work, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
                // Keep fractional screen coordinates when the native window moves by whole pixels.
                const float shift = CaptionDockShift(g_captionDockAnchor.load(),
                    g_captionDockBlend.load());
                const float screenLeft = (std::max)(static_cast<float>(work.left + 12),
                    (work.left + work.right - width) * 0.5f + settings.offsetX + shift);
                left = screenLeft - window.left;
                top = monitor.rcMonitor.top + 8 + g_compactInsetY.load() - window.top + nudge;
            }
        }''')
replace('''                               timeRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);''',
        '''                               timeRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);''')
replace('''                                   wRect, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);''',
        '''                                   wRect, textBrush_.Get(), static_cast<D2D1_DRAW_TEXT_OPTIONS>(
                                       D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT | D2D1_DRAW_TEXT_OPTIONS_NO_SNAP |
                                       D2D1_DRAW_TEXT_OPTIONS_DISABLE_COLOR_BITMAP_SNAPPING));''')
# A long collapsed date must not activate the expanded dashboard by width.
original_source=(root/'original.wh.cpp').read_text(encoding='utf-8-sig')
start=original_source.index('        // Cross-fade the collapsed status-bar view')
end=original_source.index('\n', original_source.index('        const float expandedAlpha = SmoothFade', start))
fade_helper = """float IdleDashboardExpandedAlpha(float height, float fullHeight = 144.0f) {
    // Keep large dashboard text out of the compact strip, regardless of width.
    float t = (height - 64.0f) / (fullHeight - 64.0f);
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

"""
replace(original_source[start:end], """        const float expandedAlpha = IdleDashboardExpandedAlpha(dashHeight);
        const float collapsedAlpha = 1.0f - expandedAlpha;""")
replace('const wchar_t* Loc(const wchar_t* english) {', fade_helper+'const wchar_t* Loc(const wchar_t* english) {')
replace('    std::set<uint32_t> seenIds;', '    Wh_SetStringValue(L"NotificationListenerStatus", L"Waiting for startup delay");\n    std::set<uint32_t> seenIds;')
replace('            auto listener = UserNotificationListener::Current();\n            auto access = listener.RequestAccessAsync().get();', '            Wh_SetStringValue(L"NotificationListenerStatus", L"Activating listener");\n            auto listener = UserNotificationListener::Current();\n            Wh_SetStringValue(L"NotificationListenerStatus", L"Requesting access");\n            auto access = listener.RequestAccessAsync().get();\n            Wh_SetIntValue(L"NotificationAccessStatus", static_cast<int>(access));\n            Wh_SetStringValue(L"NotificationListenerStatus", access == UserNotificationListenerAccessStatus::Allowed ? L"Listening" : L"Permission not allowed");')
replace('                    auto notifications = listener.GetNotificationsAsync(NotificationKinds::Toast).get();', '                    auto notifications = listener.GetNotificationsAsync(NotificationKinds::Toast).get();\n                    Wh_SetIntValue(L"NotificationCount", notifications.Size());')
replace('                Wh_Log(L"NotificationThreadProc connection error: %s (0x%08X). Retrying in 3s...", ex.message().c_str(), ex.to_abi());', '                Wh_SetStringValue(L"NotificationListenerStatus", L"Connection error");\n                Wh_SetIntValue(L"NotificationListenerHRESULT", static_cast<int>(ex.to_abi()));\n                Wh_Log(L"NotificationThreadProc connection error: %s (0x%08X). Retrying in 3s...", ex.message().c_str(), ex.to_abi());')
replace('                            if (g_settings.notificationRespectDnD && g_isDnDActive.load()) {', '                            Wh_SetIntValue(L"NotificationSuppressedDnD", g_settings.notificationRespectDnD && g_isDnDActive.load());\n                            if (g_settings.notificationRespectDnD && g_isDnDActive.load()) {')
replace('                            auto appInfo = userNotification.AppInfo();', '                            Wh_SetStringValue(L"NotificationParseStage", L"AppInfo");\n                            auto appInfo = userNotification.AppInfo();')
replace('''                            auto appInfo = userNotification.AppInfo();''', '''                            auto appInfo = userNotification.AppInfo();
                            try { snapshot.sourceAppId = appInfo.AppUserModelId().c_str(); } catch (...) {}''')
replace('                            auto displayInfo = appInfo.DisplayInfo();', '                            Wh_SetStringValue(L"NotificationParseStage", L"DisplayInfo");\n                            auto displayInfo = appInfo.DisplayInfo();')
replace('                            snapshot.app = displayInfo.DisplayName().c_str();', '                            Wh_SetStringValue(L"NotificationParseStage", L"DisplayName");\n                            snapshot.app = displayInfo.DisplayName().c_str();')
replace('                            TriggerNudge();\n                        } catch (const winrt::hresult_error& nex) {', '                            Wh_SetStringValue(L"NotificationParseStage", L"Published");\n                            TriggerNudge();\n                        } catch (const winrt::hresult_error& nex) {\n                            Wh_SetIntValue(L"NotificationParseHRESULT", static_cast<int>(nex.to_abi()));')
metadata_start=src.index('                            Wh_SetStringValue(L"NotificationParseStage", L"AppInfo");')
metadata_end=src.index('\n                            if (snapshot.icon.bgra.empty()',metadata_start)
metadata=src[metadata_start:metadata_end]
replace(metadata, '                            try {\n'+''.join('    '+line+'\n' for line in metadata.splitlines())+'                            } catch (const winrt::hresult_error&) {\n                                // Classic desktop toasts can omit app metadata; keep their text.\n                            }\n')
replace('                            auto notification = userNotification.Notification();', '                            if (snapshot.app.empty()) snapshot.app = L"Windows notification";\n                            Wh_SetStringValue(L"NotificationParseStage", L"Notification text");\n                            auto notification = userNotification.Notification();')
replace('                                g_state.notification = std::move(snapshot);', '                                snapshot.expiresAt = NowSeconds() + 4.0;\n                                g_state.notification = std::move(snapshot);')
replace('                    Wh_SetIntValue(L"NotificationCount", notifications.Size());', '                    static uint32_t lastNotificationCount = UINT32_MAX;\n                    if (lastNotificationCount != notifications.Size()) {\n                        lastNotificationCount = notifications.Size();\n                        Wh_SetIntValue(L"NotificationCount", lastNotificationCount);\n                    }')
replace('0.55f * settingsOpacity_;', '0.18f * settingsOpacity_;')
replace('0.18f * settingsOpacity_;', '0.18f * settingsOpacity_ * (1.0f - g_idleCompactBlend.load());')
replace('''        bg.a = Clamp(bg.a * settingsOpacity_, 0.0f, 1.0f);''', '''        bg.a = Clamp(bg.a * settingsOpacity_, 0.0f, 1.0f);
        if (kind == IslandKind::Idle)
            bg.a *= 1.0f - 0.1875f * g_idleCompactBlend.load();''')
replace('''        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen''', '''        static double nextCompactCheck = 0.0;
        static bool maximizedForeground = false;
        if (now >= nextCompactCheck) {
            nextCompactCheck = now + 0.25;
            HWND foreground = GetForegroundWindow();
            maximizedForeground = foreground && foreground != hwnd && IsZoomed(foreground) &&
                MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST) ==
                MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        }
        const bool compactEligible = g_settings.position == Position::TopCenter &&
            !g_settings.notchStyle && !g_settings.borderMergedMode && !gameMetricsPresent &&
            primary.kind == IslandKind::Idle && !secondary;
        const bool compactMaximized = compactEligible && maximizedForeground && !isFullscreen;
        if (g_maximizedCompact.exchange(compactMaximized) != compactMaximized)
            g_layoutDirty = true;
        const bool compactIdle = compactMaximized && !isHoverExpanded && !pinned;

        if (primary.kind == IslandKind::Idle) {
            if (!isFullscreen''')
replace('''        const bool fullscreenSuppressed =
            isFullscreen && !g_fullscreenOverrideVisible.load(std::memory_order_relaxed);''', '''        const float normalIdleWidth = primary.width;
        if (compactIdle && primary.width > 0.0f) {
            primary.width *= 29.0f / 36.0f;
            primary.height = 29.0f * g_settings.sizeScale;
        }
        static double fullscreenHoverUntil = 0.0;
        if (isFullscreen && hoverUnhides) fullscreenHoverUntil = now + 1.5;
        const bool fullscreenSuppressed =
            isFullscreen && now >= fullscreenHoverUntil &&
            !g_fullscreenOverrideVisible.load(std::memory_order_relaxed);''')
replace('''        nudgeSpring.Step(dt * speed, 280.0f * styleStiffnessMult, 24.0f * styleDampingMult);''', '''        nudgeSpring.Step(dt * speed, 280.0f * styleStiffnessMult, 24.0f * styleDampingMult);
        const float compactDrawScale = compactEligible && !isHoverExpanded && !pinned ?
            CompactIdleScale(heightSpring.value, 36.0f * g_settings.sizeScale) : 1.0f;
        const float compactBlend = (1.0f - compactDrawScale) / (1.0f - 29.0f / 36.0f);
        if (g_idleCompactBlend.exchange(compactBlend) != compactBlend) needsRender = true;
        const float compactInset = (1.0f - compactDrawScale) * 18.0f * g_settings.sizeScale;
        if (g_compactInsetY.exchange(compactInset) != compactInset) g_layoutDirty = true;''')
# Keep font formats at the configured scale; animate content with a draw transform.
replace('''        target_->SetTransform(D2D1::Matrix3x2F::Scale(settings.sizeScale, settings.sizeScale, pillCenter) * oldTransform);

        float invScale = 1.0f / settings.sizeScale;''', '''        const float contentScale = settings.sizeScale * (activity.kind == IslandKind::Idle ?
            1.0f - (7.0f / 36.0f) * g_idleCompactBlend.load() : 1.0f);
        target_->SetTransform(D2D1::Matrix3x2F::Scale(contentScale, contentScale, pillCenter) * oldTransform);

        float invScale = 1.0f / contentScale;''')
dock_helpers = (root/'caption-dock.h').read_text(encoding='utf-8')
replace('int YasbDockY(int screenTop) {', dock_helpers+'\nint YasbDockY(int screenTop) {')
replace('''            y = YasbDockY(monitor.rcMonitor.top);''', '''            y = YasbDockY(monitor.rcMonitor.top);
            x += static_cast<int>(std::lround(CaptionDockShift(
                g_captionDockAnchor.load(), g_captionDockBlend.load())));
            x = (std::max)(x, static_cast<int>(work.left + 12 - kRenderPadX));''')
replace('''    y += g_settings.offsetY;

    RECT r;''', '''    y += g_settings.offsetY;
    if (g_settings.position == Position::TopCenter && !flushTop) {
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (GetMonitorInfoW(MonitorFromRect(&work, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
            y = YasbDockY(monitor.rcMonitor.top) + static_cast<int>(kRenderPadY);
            x += static_cast<int>(std::lround(CaptionDockShift(
                g_captionDockAnchor.load(), g_captionDockBlend.load())));
        }
    }

    RECT r;''')
replace('''    IslandKind previousPrimary = IslandKind::Idle;''', '''    CaptionDockMotion dockAnchor, dockBlend;
    CaptionDockCandidate dockCandidate;
    double nextCaptionScan = 0.0;
    bool dockAnimationEnabled = true;
    IslandKind previousPrimary = IslandKind::Idle;''')
replace('''        // Check if animating structurally''', '''        GUITHREADINFO dockGui{sizeof(GUITHREADINFO)};
        // Custom title bars may bypass the system move loop, so also pause while pressed.
        const bool dockPaused = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
            (GetGUIThreadInfo(0, &dockGui) && (dockGui.flags & GUI_INMOVESIZE));
        if (dockPaused) {
            nextCaptionScan = now + 0.12;
            dockCandidate.since = -1.0;
        }
        if (!dockPaused && now >= nextCaptionScan) {
            nextCaptionScan = now + 0.12;
            float nextAnchor = 0.0f, nextBlend = 0.0f;
            if (g_settings.position == Position::TopCenter &&
                !g_settings.notchStyle && !g_settings.borderMergedMode) {
                RECT work = GetAnchorWorkRect();
                MONITORINFO monitor{sizeof(MONITORINFO)};
                int edge = work.right;
                if (GetMonitorInfoW(MonitorFromRect(&work, MONITOR_DEFAULTTOPRIMARY), &monitor)) {
                    // Always test the resting footprint; hover expansion cannot toggle docking.
                    float compactWidth = renderer.MeasureIdleStrip(snapshot, g_settings, now).totalWidth * g_settings.sizeScale;
                    if (GetCaptionDockTarget(hwnd, work, compactWidth,
                            monitor.rcMonitor.top + 8,
                            static_cast<int>(std::ceil(IdleStripLayout::kHeight * g_settings.sizeScale)), edge)) {
                        // Reserve the expanded width once; resizing then keeps this centre fixed.
                        const float reservedWidth = (std::max)(compactWidth,
                            MediaLayout::kExpandedWidth * g_settings.sizeScale);
                        nextAnchor = edge - (work.left + work.right) * 0.5f - reservedWidth * 0.5f;
                        nextBlend = 1.0f;
                    }
                }
            }
            if (dockCandidate.Observe(nextAnchor, nextBlend, now)) {
                dockAnchor.target = dockCandidate.anchor;
                dockBlend.target = dockCandidate.blend;
            }
            BOOL enabled = TRUE;
            if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0))
                dockAnimationEnabled = enabled != FALSE;
        }
        float previousDockShift = CaptionDockShift(dockAnchor.value, dockBlend.value);
        if (!dockPaused) {
            dockAnchor.Step(dt, dockAnimationEnabled);
            dockBlend.Step(dt, dockAnimationEnabled);
        }
        g_captionDockAnchor.store(dockAnchor.value);
        g_captionDockBlend.store(dockBlend.value);
        bool dockingAnimating = !dockPaused && (dockAnchor.Moving() || dockBlend.Moving());
        if (std::lround(previousDockShift) != std::lround(
                CaptionDockShift(dockAnchor.value, dockBlend.value)))
            g_layoutDirty = true;
        if (dockingAnimating) needsRender = true;

        // Check if animating structurally''')
replace('''        const bool springsAnimating =
            std::fabs(widthSpring.value''', '''        const bool springsAnimating = dockingAnimating ||
            std::fabs(widthSpring.value''')
from notification_patch import apply_notifications
src = apply_notifications(src)
from weather_patch import apply_weather
src = apply_weather(src)
from quick_home_patch import apply_quick_home
src = apply_quick_home(src)
from glass_patch import apply_glass
src = apply_glass(src)
from native_glass_patch import apply_native_glass
src = apply_native_glass(src)
from timer_patch import apply_timer
src = apply_timer(src)
from timer_ui_patch import apply_timer_ui
src = apply_timer_ui(src)
from privacy_patch import apply_privacy
src = apply_privacy(src)
from nba_patch import apply_nba
src = apply_nba(src)
from hourly_clock_patch import apply_hourly_clock
src = apply_hourly_clock(src)
from expanded_layout_patch import apply_expanded_layout
src = apply_expanded_layout(src)
from volume_control_patch import apply_volume_control
src = apply_volume_control(src)
from weather_icon_patch import apply_weather_icons
src = apply_weather_icons(src)
from neutral_palette_patch import apply_neutral_palette
src = apply_neutral_palette(src)
from tomorrow_brief_patch import apply_tomorrow_brief
src = apply_tomorrow_brief(src)
from calendar_markers_patch import apply_calendar_markers
src = apply_calendar_markers(src)

replace('#include <dwrite.h>', '#include <dwrite_2.h>')
replace('class Renderer {', (root/'chinese-font.h').read_text(encoding='utf-8') + '\nclass Renderer {')
replace('    ComPtr<IDWriteFactory> dwriteFactory_;', '''    ComPtr<IDWriteFactory> dwriteFactory_;
    ComPtr<IDWriteFontFallback> chineseFontFallback_;''')
replace('''        const Settings initialSettings = GetSettingsCopy();''', '''        chineseFontFallback_ = CreateIslandChineseFontFallback(dwriteFactory_.Get());
        const Settings initialSettings = GetSettingsCopy();''')
replace('        dwriteFactory_.Reset();', '        chineseFontFallback_.Reset();\n        dwriteFactory_.Reset();')
replace('''        };

        createFormat(mainFamily, defaultDisplay,''', '''            ComPtr<IDWriteTextFormat1> format1;
            if (out && chineseFontFallback_ && SUCCEEDED(out.As(&format1)))
                format1->SetFontFallback(chineseFontFallback_.Get());
        };

        createFormat(mainFamily, defaultDisplay,''')
replace('''            if (weatherDescFormat_) {
                weatherDescFormat_->SetWordWrapping''', '''            if (weatherDescFormat_) {
                ComPtr<IDWriteTextFormat1> format1;
                if (chineseFontFallback_ && SUCCEEDED(weatherDescFormat_.As(&format1)))
                    format1->SetFontFallback(chineseFontFallback_.Get());
                weatherDescFormat_->SetWordWrapping''')

# Trim inherited menu commands after feature patches have used their insertion anchors.
menu_start = src.index('void ShowContextMenu(HWND hwnd, POINT screenPoint) {')
menu_end = src.index('\n}\n', menu_start) + 3
old_menu = src[menu_start:menu_end]
menu = old_menu
for line in (
    '    AppendMenuW(menu, MF_STRING, 40, g_manuallyHidden.load() ? L"Show Island" : L"Hide Island");\n',
    '    AppendMenuW(menu, MF_STRING, 45, L"Edit notification app mappings");\n',
):
    assert menu.count(line) == 1
    menu = menu.replace(line, '', 1)
for begin, end in (
    ('    std::wstring shapeStr = GetStringSettingCopy(L"Appearance.ShapeStyle");',
     '    const int activeExpandOnHover ='),
    ('    AppendMenuW(menu, MF_STRING, 4, L"Transparency 100%");',
     '    AppendMenuW(menu, MF_STRING, 9, L"Open Windhawk settings");'),
    ('    // Theme ids are a contiguous range', '    switch (cmd) {'),
    ('        case 45: {', '        case 1:'),
    ('        case 4:', '        case 9: {'),
    ('        case 10: {', '        case 11: {'),
    ('        case 40: {', '        case 41: {'),
):
    begin_at = menu.index(begin)
    end_at = menu.index(end, begin_at)
    menu = menu[:begin_at] + menu[end_at:]
assert 'L"Show game overlay"' in menu and 'case 3:' in menu
assert 'L"Expand on Click"' in menu and 'case 11:' in menu
assert not re.search(r'case (?:4|5|6|7|8|10|12|40|45):', menu)
replace(old_menu, menu)

build_id = 'dynamic-island-for-windows-yasb'
build_version = '1.3.1-local.60'
if '--release' in sys.argv:
    from release_patch import apply_release
    from updater_patch import apply_updater
    from island_drag_patch import apply_island_drag
    release = json.loads((root/'release.json').read_text(encoding='utf-8-sig'))
    assert re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', release['version'])
    assert re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]{0,38}', release['author'])
    src = apply_updater(apply_release(src, release['version'], release['author']))
    src = apply_island_drag(src)
    build_id = 'dynamic-island-liquid-glass'
    build_version = release['version']
settings_yaml = src.split('// ==WindhawkModSettings==', 1)[1].split('/*', 1)[1].split('*/', 1)[0]
assert isinstance(yaml.safe_load(settings_yaml), list)
print('Windhawk settings YAML parsed successfully', flush=True)
out = root/(build_id + '.wh.cpp')
assert 'renderSettings.sizeScale *= compactDrawScale' not in src
assert 'Scale(contentScale, contentScale, pillCenter)' in src
out.write_text(src, encoding='utf-8')
if '--source-only' in sys.argv:
    sys.exit(0)
compiler=Path(r'C:\Program Files\Windhawk\Compiler')
clang=compiler/'bin/clang++.exe'

# Test the real tab arithmetic for every optional-module combination.
tab_test = '#include <windows.h>\n#include <d2d1.h>\n#include <algorithm>\n#include <atomic>\n#include <cassert>\n'
tab_test += (root/'update-badge.h').read_text(encoding='utf-8')
tab_test += 'struct Settings { bool weather=false, hardwareMonitorModule=false, fileTrayModule=false; };\nstd::atomic<int> g_idleTab{0};\n'
tabs_start = src.index('int ActiveTabCount(')
tab_test += src[tabs_start:src.index('// ── Localization', tabs_start)]
tab_test += r'''
int main() {
    for (float scale : {0.7f, 1.0f, 1.6f, 3.0f}) {
        for (float width : {96.0f, 320.0f, 540.0f}) {
            const auto badge = UpdateBadgeBounds(28.0f, 22.0f, width, 36.0f, scale);
            assert(badge.point.x - badge.radiusX > 28.0f + width);
            assert(badge.point.x + badge.radiusX < 28.0f + width + 28.0f);
            assert(badge.point.y - badge.radiusY >= 0.0f);
            assert(badge.point.y + badge.radiusY < 22.0f + 36.0f + 22.0f);
        }
    }
    for (int mask=0; mask<8; ++mask) {
        Settings s{bool(mask&1),bool(mask&2),bool(mask&4)};
        const int expected=3+s.weather+s.hardwareMonitorModule+s.fileTrayModule;
        assert(ActiveTabCount(s)==expected);
        assert(ClockTabIndex(s)==2+s.weather+s.hardwareMonitorModule);
        assert(FileTrayTabIndex(s)==(s.fileTrayModule ? ClockTabIndex(s)+1 : -1));
        assert(HardwareMonitorTabIndex(s)==(s.hardwareMonitorModule ? 2+s.weather : -1));
        g_idleTab=expected; assert(NormalizedTabIndex(s)==0);
        g_idleTab=-1; assert(NormalizedTabIndex(s)==expected-1);
    }
    Settings normal{true,true,false}; assert(ActiveTabCount(normal)==5 && ClockTabIndex(normal)==4);
}
'''
(root/'card-tabs-checks.cpp').write_text(tab_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'card-tabs-checks.cpp'),'-o',str(root/'card-tabs-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'card-tabs-checks.exe')],check=True)
print('Card tab and update-badge checks passed: module layout, badge outside content and inside render padding',flush=True)

# Exercise the real selection function with only platform/state plumbing stubbed.
timer_test = r"""#include <string>
#include <vector>
#include <atomic>
#include <cassert>
enum class IslandKind {Clipboard,CapsLock,Device,Bluetooth,DoNotDisturb,Volume,Notification,BatteryLow,Progress,Timer,NBAScore,HourlyClock,TomorrowBrief,Media,Idle};
struct Alert { bool active=false, justFinished=false, available=false; double expiresAt=0, finishedExpiresAt=0; };
struct SharedState { Alert clipboard,capsLock,device,bluetoothDevice,doNotDisturb,volume,notification,battery,progress,timer,nba,media,hourlyClock,tomorrowBrief; };
struct Settings { bool capsLock=true,bluetoothIndicator=true,doNotDisturbIndicator=true,volume=true,battery=true,progress=true,timerEnabled=true,nbaScores=true,media=true; };
std::atomic<bool> g_timerDashboardExpanded=false, g_isFullscreen=false, g_manuallyHidden=false;
int pin=0, nbaClock=0;
int Wh_GetIntValue(const wchar_t* key,int) { return std::wstring(key)==L"NBAShowClock" ? nbaClock : pin; }
"""
timer_test += src[src.index('std::vector<IslandKind> ChooseActivities('):src.index('bool VolumeIsPrimary(')]
timer_test += r"""int main() {
 SharedState state; Settings settings; state.timer.active=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Timer);
 g_timerDashboardExpanded=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Idle);
 g_timerDashboardExpanded=false; pin=1;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Idle);
 state.notification.active=true; state.notification.expiresAt=20;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Notification);
 state.notification.active=false; pin=0; state.media.available=true;
 auto kinds=ChooseActivities(state,settings,10);
 assert(kinds[0]==IslandKind::Timer && kinds[1]==IslandKind::Media);
 state.timer.active=false; state.timer.justFinished=true; state.timer.finishedExpiresAt=16;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Timer);
 assert(ChooseActivities(state,settings,16)[0]==IslandKind::Media);
 state.timer.active=true; settings.timerEnabled=false;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Media);
 settings.timerEnabled=true; state.timer.active=false; state.timer.justFinished=false;
 state.media.available=false; state.nba.active=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::NBAScore);
 state.timer.active=true;
 auto nbaKinds=ChooseActivities(state,settings,10);
 assert(nbaKinds[0]==IslandKind::Timer && nbaKinds[1]==IslandKind::NBAScore);
 state.timer.active=false; state.media.available=true;
 nbaKinds=ChooseActivities(state,settings,10);
 assert(nbaKinds[0]==IslandKind::NBAScore && nbaKinds[1]==IslandKind::Media);
 nbaClock=1;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Idle && state.nba.active);
 state.notification.active=true; state.notification.expiresAt=20;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Notification);
 state.notification.active=false; state.timer.active=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Timer);
 state.timer.active=false; nbaClock=0;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::NBAScore);
 settings.nbaScores=false;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Media);
 state.hourlyClock.active=true; state.hourlyClock.expiresAt=15;
 assert(ChooseActivities(state,settings,10)==std::vector<IslandKind>{IslandKind::HourlyClock});
 assert(ChooseActivities(state,settings,15)[0]==IslandKind::Media);
 state.notification.active=true; state.notification.expiresAt=20;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Notification);
 state.tomorrowBrief.active=true; state.hourlyClock.active=false;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Notification);
 state.notification.active=false;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::TomorrowBrief);
 g_isFullscreen=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Media);
 g_isFullscreen=false; pin=1;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Media);
 pin=0; g_manuallyHidden=true;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Media);
 g_manuallyHidden=false; state.timer.justFinished=true; state.timer.finishedExpiresAt=20;
 assert(ChooseActivities(state,settings,10)[0]==IslandKind::Timer);

}"""
(root/'timer-checks.cpp').write_text(timer_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'timer-checks.cpp'),'-o',str(root/'timer-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'timer-checks.exe')],check=True)
print('Timer checks passed: collapsed, expanded, pinned, alerts, media, completion and disabled module',flush=True)
hourly_test = '#include <windows.h>\n#include <cstdint>\n#include <cassert>\n'
hourly_test += (root/'hourly-clock.h').read_text(encoding='utf-8')
hourly_test += r'''
int main() {
    HourlyClockTracker t;
    SYSTEMTIME s{}; s.wYear=2026; s.wMonth=10; s.wDay=19; s.wHour=18; s.wMinute=59; s.wSecond=59;
    assert(!t.Tick(s, false));
    s.wHour=19; s.wMinute=0; s.wSecond=0;
    assert(t.Tick(s, false));
    assert(!t.Tick(s, false));
    s.wSecond=3; assert(!t.Tick(s, false));
    s.wHour=20; s.wMinute=5; assert(!t.Tick(s, false));
    s.wHour=21; s.wMinute=0; s.wSecond=1; assert(t.Tick(s, false));
    s.wHour=23; s.wMinute=59; s.wSecond=59; assert(!t.Tick(s, false));
    s.wDay=20; s.wHour=0; s.wMinute=0; s.wSecond=0; assert(t.Tick(s, false));
    HourlyClockTracker startup; assert(!startup.Tick(s, false));
    assert(startup.Tick(s, true));
    assert(!startup.Tick(s, false));
}
'''
(root/'hourly-clock-checks.cpp').write_text(hourly_test, encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'hourly-clock-checks.cpp'),'-o',str(root/'hourly-clock-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'hourly-clock-checks.exe')],check=True)
print('Hourly clock checks passed: boundary, duplicate, midnight, resume, startup and preview',flush=True)

test='#include <atomic>\n#include <algorithm>\n#include <cmath>\n#include <cassert>\nconstexpr float kRenderPadY=22.0f;\n'+helper+fade_helper
test+='''int main() {
 assert(IdleDashboardExpandedAlpha(28)==0);
 assert(IdleDashboardExpandedAlpha(36)==0);
 assert(IdleDashboardExpandedAlpha(64)==0);
 assert(IdleDashboardExpandedAlpha(104)==0.5f);
 assert(IdleDashboardExpandedAlpha(144)==1);
 assert(IdleDashboardExpandedAlpha(300)==1);
 assert(IdleDashboardExpandedAlpha(124,124)==1);
 assert(IdleDashboardExpandedAlpha(36,124)==0);
 assert(CompactIdleScale(29,36)==29.0f/36.0f);
 assert(CompactIdleScale(36,36)==1.0f);
 assert(CompactIdleScale(300,36)==1.0f);
 assert(CompactIdleScale(0,36)==29.0f/36.0f);
 g_maximizedCompact=true;
 assert(YasbDockY(0)+kRenderPadY==8);
 for(float dpi : {1.0f,1.25f,1.5f,2.0f}) {
   for(float h : {29.0f,31.0f,33.0f,36.0f}) {
     g_compactInsetY=(36.0f-h)*dpi/2;
     float center=YasbDockY(0)+kRenderPadY+h*dpi/2;
     assert(std::abs(center-(8+36*dpi/2))<=0.501f);
     const float screenTop=8+g_compactInsetY.load();
     const float renderTop=screenTop-YasbDockY(0);
     const float correctedCenter=YasbDockY(0)+renderTop+h*dpi/2;
     assert(std::abs(correctedCenter-(8+36*dpi/2))<0.0001f);
     for(float w : {240.25f,251.5f,298.875f}) {
       const float screenLeft=(2560-w)/2;
       const int windowLeft=static_cast<int>(screenLeft-kRenderPadY);
       const float renderLeft=screenLeft-windowLeft;
       assert(std::abs(windowLeft+renderLeft+w/2-1280)<0.0001f);
     }
   }
 }
 g_compactInsetY=0;
 g_maximizedCompact=false;
 for (int screenTop : {0,-1080,1080}) {
   assert(YasbDockY(screenTop)+kRenderPadY==screenTop+8);
   for (float dpi : {1.0f,1.25f,1.5f,2.0f}) {
     for (float height : {28.0f,36.0f,44.0f,300.0f}) {
       float top=YasbDockY(screenTop)+kRenderPadY;
       float cursor=screenTop+8+14*dpi;
       assert(cursor>=top && cursor<top+height*dpi);
     }
   }
 }
}'''
(root/'checks.cpp').write_text(test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'checks.cpp'),'-o',str(root/'checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'checks.exe')],check=True)
print('57 anchor/hover and collapsed-dashboard assertions passed',flush=True)
motion_test = '#include <algorithm>\n#include <atomic>\n#include <cmath>\n#include <cassert>\n' + dock_helpers[:dock_helpers.index('bool GetCaptionDockTarget')]
motion_test += src[src.index('struct SpringValue {'):src.index('\nSettings g_settings;')]
motion_test = '#include <string>\n' + motion_test + notification_route
motion_test += '''int main() {
 assert(NotificationLaunchTarget(L"")==L"ms-actioncenter:");
 assert(NotificationLaunchTarget(L"Package_family!App")==L"shell:AppsFolder\\\\Package_family!App");
 assert(NotificationLaunchTarget(L"https://example.com")==L"ms-actioncenter:");
 assert(NotificationLaunchTarget(L"C:\\\\app.exe")==L"ms-actioncenter:");
 assert(NotificationLaunchTarget(L"app\\nargs")==L"ms-actioncenter:");
 for(float fps : {30.0f,60.0f,144.0f,240.0f}) {
   SpringValue w,h; w.Reset(300); h.Reset(36); w.target=240; h.target=29;
   float last=36;
   for(int i=0;i<fps;i++) {
     w.StepExponential(1/fps); h.StepExponential(1/fps);
     assert(h.value<=last && h.value>=29); last=h.value;
     if(h.value>29.1f) assert(std::abs((300-w.value)/60-(36-h.value)/7)<0.001f);
   }
   assert(h.value==29 && w.value==240 && h.velocity==0 && w.velocity==0);
   h.target=36; h.StepExponential(1/fps); assert(h.value>29 && h.value<36);
   h.target=29; h.StepExponential(1/fps); assert(h.value>=29 && h.value<36);
 }
 for (float fps : {30.0f,60.0f,144.0f,240.0f}) {
   CaptionDockMotion m; m.target=-300; float last=0;
   for (int i=0;i<fps;i++) { m.Step(1/fps,true); assert(m.value<=last+0.001f && m.value>=-300.001f); last=m.value; }
   assert(std::abs(m.value+300)<0.02f);
   m.target=0; last=m.value;
   for (int i=0;i<fps;i++) { m.Step(1/fps,true); assert(m.value>=last-0.001f && m.value<=0.001f); last=m.value; }
   assert(std::abs(m.value)<0.02f);
 }
 CaptionDockMotion reduced; reduced.target=-250; reduced.Step(0.016f,false);
 assert(reduced.value==-250 && !reduced.Moving());
 for (float width : {150.0f,300.0f,600.0f}) {
   const float centre = 1280 + CaptionDockShift(-180,1);
   assert(std::abs(((centre-width/2)+(centre+width/2))/2-1100)<0.001f);
   assert(CaptionDockShift(-180,1)==-180);
   assert(CaptionDockShift(0,0)==0);
 }
 CaptionDockMotion reverse; reverse.target=-300;
 for(int i=0;i<6;i++) reverse.Step(1.0f/60,true);
 float before=reverse.value; reverse.target=0; reverse.Step(1.0f/60,true);
 assert(std::abs(reverse.value-before)<25);
 CaptionDockCandidate candidate;
 assert(!candidate.Observe(-300,1,0));
 assert(!candidate.Observe(-300,1,0.12));
 assert(candidate.Observe(-300,1,0.25));
 assert(!candidate.Observe(-280,1,0.3));
 assert(!candidate.Observe(-300,1,0.4));
 assert(candidate.Observe(-300,1,0.65));
 candidate.since=-1; // A drag must require a fresh settling interval.
 assert(!candidate.Observe(-300,1,1));
 assert(!candidate.Observe(0,0,1.1));
 assert(candidate.Observe(0,0,1.35));
}'''
motion_test = motion_test.replace('#include <atomic>', '#include <initializer_list>\n#include <atomic>')
(root/'motion-checks.cpp').write_text(motion_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'motion-checks.cpp'),'-o',str(root/'motion-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'motion-checks.exe')],check=True)
print('Motion checks passed at 30/60/144/240fps, including reduced motion and retargeting',flush=True)
original=(root/'original.wh.cpp').read_text(encoding='utf-8-sig')
formats=original[original.index('std::wstring FormatIslandTime('):original.index('const wchar_t* Loc(')]
clock_test = '''#include <windows.h>
#include <string>
#include <cassert>
struct Settings { bool clockFollowSystem=false, use24HourClock=true, showSeconds=false, dateFirst=true; std::wstring dateFormat=L"MM-dd"; };
''' + formats + clock_helper + '''int main() {
 SYSTEMTIME t{2024,2,4,29,0,0,0,0}; Settings s;
 assert(FormatIslandStripClock(t,s)==L"Thu, Feb 29 · 00:00");
 t.wHour=23; t.wMinute=59; t.wSecond=58; s.showSeconds=true;
 assert(FormatIslandStripClock(t,s)==L"Thu, Feb 29 · 23:59:58");
 s.dateFirst=false;
 assert(FormatIslandStripClock(t,s)==L"23:59:58 · Thu, Feb 29");
 s.dateFormat=L"";
 assert(FormatIslandStripClock(t,s).find(L"Thu, Feb 29")!=std::wstring::npos);
}'''
(root/'clock-checks.cpp').write_text(clock_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'clock-checks.cpp'),'-o',str(root/'clock-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'clock-checks.exe')],check=True)
print('4 date/time formatting assertions passed',flush=True)
mapping_test = r'''#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
std::wstring iniPath;
std::vector<std::wstring> launches;
bool failFirst=false;
std::wstring NotificationMappingsPath() { return iniPath; }
bool EqualsNoCase(const std::wstring& a,const std::wstring& b) { return _wcsicmp(a.c_str(),b.c_str())==0; }
bool ProcessImageNameForWindow(HWND,std::wstring*) { return false; }
BOOL TestEnumWindows(WNDENUMPROC,LPARAM) { return TRUE; }
BOOL TestShellExecute(SHELLEXECUTEINFOW* launch) {
 launches.push_back(launch->lpFile);
 return !(failFirst && launches.size()==1);
}
#define EnumWindows TestEnumWindows
#define ShellExecuteExW TestShellExecute
''' + notification_route + src[src.index('bool OpenNotificationSource('):src.index('void HandleStatusClickAtPoint(')]
mapping_test += r'''int main() {
 wchar_t temp[MAX_PATH]{}, file[MAX_PATH]{}, exe[MAX_PATH]{};
 GetTempPathW(MAX_PATH,temp); GetTempFileNameW(temp,L"din",0,file); iniPath=file;
 FILE* stream=_wfopen(file,L"wb"); assert(stream); const wchar_t bom=0xfeff;
 fwrite(&bom,sizeof(bom),1,stream); fclose(stream);
 GetModuleFileNameW(nullptr,exe,MAX_PATH);
 assert(OpenNotificationSource(L"Pkg!App",L"Unknown"));
 assert(launches.back()==L"shell:AppsFolder\\Pkg!App"); launches.clear();
 assert(OpenNotificationSource(L"",L"Unknown")); assert(launches.back()==L"ms-actioncenter:"); launches.clear();
 WritePrivateProfileStringW(L"Apps",L"微信",exe,file);
 assert(OpenNotificationSource(L"",L"微信")); assert(launches.back()==exe); launches.clear();
 WritePrivateProfileStringW(L"Apps",L"微信",L"C:\\nonexistent\\app.exe",file);
 assert(OpenNotificationSource(L"",L"微信")); assert(launches.back()==L"ms-actioncenter:"); launches.clear();
 WritePrivateProfileStringW(L"Apps",L"Mapped",L"app:Other!App",file);
 assert(OpenNotificationSource(L"Mapped",L"Unknown")); assert(launches.back()==L"shell:AppsFolder\\Other!App"); launches.clear();
 WritePrivateProfileStringW(L"Apps",L"微信",exe,file); failFirst=true;
 assert(OpenNotificationSource(L"",L"微信")); assert(launches.size()==2 && launches.back()==L"ms-actioncenter:");
 DeleteFileW(file);
}'''
(root/'mapping-checks.cpp').write_text(mapping_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'mapping-checks.cpp'),'-o',str(root/'mapping-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'mapping-checks.exe')],check=True)
print('Mapping checks passed: Unicode names, app IDs, executable paths and failure fallback; launches mocked',flush=True)
weather_test = r'''#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
''' + src[src.index('struct WeatherSnapshot {'):src.index('struct DoNotDisturbSnapshot {')]
weather_test += (root/'china-weather.h').read_text(encoding='utf-8')
weather_test += r'''int main(int argc,char** argv) {
 winrt::init_apartment(); assert(argc==3);
 std::ifstream a(argv[1]),b(argv[2]);
 std::string current((std::istreambuf_iterator<char>(a)),{}),forecast((std::istreambuf_iterator<char>(b)),{});
 SYSTEMTIME now{2026,10,5,16,12,0,0,0}; WeatherSnapshot result;
 assert(ParseChinaWeather(current,forecast,now,false,result));
 assert(std::abs(result.temperature-16.1f)<0.01f && result.lowTemperature==6 && result.highTemperature==18);
 assert(result.city==L"沈阳" && result.weatherDesc==L"晴" && result.hasDailyRange);
 for (const auto* rainCode : {L"d301", L"n301", L"301"}) assert(ChinaWeatherVisualCode(rainCode)==296);
 assert(ChinaWeatherVisualCode(L"d07")==296 && ChinaWeatherVisualCode(L"d00")==113);
 assert(ChinaWeatherVisualCode(L"d999")==0 && ChinaWeatherVisualCode(L"d301x")==0);
 assert(ParseChinaWeather(current,forecast,now,true,result)); assert(std::abs(result.temperature-60.98f)<0.01f);
 now.wDay=17; assert(!ParseChinaWeather(current,forecast,now,false,result)); now.wDay=16;
 now.wHour=16; assert(!ParseChinaWeather(current,forecast,now,false,result)); now.wHour=12;
 assert(!ParseChinaWeather("<html>rate limited</html>",forecast,now,false,result));
 assert(!ParseChinaWeather(current,"var fc={};",now,false,result));
 assert(!ParseChinaWeather(current,"var fc={\"f\":[{\"fi\":\"10/16\",\"fc\":\"1\",\"fd\":\"20\"}]};",now,false,result));
 float value=17; assert(!ChinaWeatherTemperature(L"nan",value) && value==17);
 assert(!ChinaWeatherTemperature(L"99",value)); assert(!ChinaWeatherTemperature(L"",value));
 assert(ChinaWeatherTemperature(L"-22.5",value) && value==-22.5f);
 assert(ChinaWeatherObject("var x={\"s\":\"};var fake={}\",\"a\":{}};var y={};","x")=="{\"s\":\"};var fake={}\",\"a\":{}}");
 assert(ChinaWeatherCityCode(L"沈阳")==L"101070101");
 assert(ChinaWeatherCityCode(L"101010100")==L"101010100"); assert(ChinaWeatherCityCode(L"unknown").empty());
}'''
(root/'weather-checks.cpp').write_text(weather_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'weather-checks.cpp'),'-lruntimeobject','-lole32','-loleaut32','-o',str(root/'weather-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'weather-checks.exe'),str(root/'cn-weather-now.txt'),str(root/'cn-weather-index.txt')],check=True)
print('China weather checks passed: live fixtures, daily range, stale dates, malformed data and Celsius/Fahrenheit',flush=True)
nba_test = r'''#include <windows.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>
'''
nba_test += (root/'nba-parser.h').read_text(encoding='utf-8')
nba_test += '\n#include <algorithm>\n#include <cstdint>\n#include <d2d1.h>\n'
bitmap_start = src.index('struct BitmapPixels {')
bitmap_end = src.index('\n};', bitmap_start) + 3
nba_test += src[bitmap_start:bitmap_end] + '\n'
nba_test += (root/'nba-logo.h').read_text(encoding='utf-8')

nba_test += r'''
int main(int argc, char** argv) {
    winrt::init_apartment();
    assert(argc == 3);
    auto read = [](const char* path) {
        std::ifstream f(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), {});
    };
    const std::string live = read(argv[1]);
    const std::string finalGame = read(argv[2]);
    NbaSnapshot s;
    assert(ParseNbaScoreboard(live, L"", L"", L"", &s));
    assert(s.live && !s.finished);
    assert(s.awayAbbr == L"BOS" && s.homeAbbr == L"DET");
    assert(s.awayScore == 98 && s.homeScore == 102 && s.period == 4);
    assert(s.status == L"第4节 5:12");
    assert(s.matchup == L"BOS @ DET" && !s.gameUrl.empty());
    assert(s.awayLogoUrl == L"https://a.espncdn.com/i/teamlogos/nba/500-dark/scoreboard/bos.png");
    assert(s.homeLogoUrl == L"https://a.espncdn.com/i/teamlogos/nba/500/det.png");
    BitmapPixels pixels;
    assert(ResizeNbaLogo(pixels).bgra.empty());
    pixels.width = 128; pixels.height = 64;
    pixels.bgra.resize(128 * 64 * 4, 128);
    auto logo = ResizeNbaLogo(pixels);
    assert(logo.width == 64 && logo.height == 64 && logo.bgra.size() == 16384);
    assert(logo.bgra[0] == 0 && logo.bgra[(16 * 64) * 4] == 128);
    assert(logo.bgra[(47 * 64 + 63) * 4] == 128 && logo.bgra[(48 * 64) * 4] == 0);
    pixels.bgra.pop_back();
    assert(ResizeNbaLogo(pixels).bgra.empty());
    pixels.width = pixels.height = 1; pixels.bgra = {0, 32, 64, 128};
    logo = ResizeNbaLogo(pixels);
    assert(logo.bgra[0] == 0 && logo.bgra[1] == 32 && logo.bgra.back() == 128);

    assert(ParseNbaScoreboard(live, L"bos", L"", L"", &s));
    assert(!ParseNbaScoreboard(live, L"LAL", L"", L"", &s));
    assert(!ParseNbaScoreboard(finalGame, L"", L"", L"", &s));
    assert(ParseNbaScoreboard(finalGame, L"", L"BOS", L"DET", &s));
    assert(s.finished && s.awayScore == 108 && s.homeScore == 111);
    assert(!ParseNbaScoreboard("<html>error</html>", L"", L"", L"", &s));
    assert(NormalizeNbaTeam(L"  lal ") == L"LAL");
    assert(NbaPeriodText(5, L"1:20") == L"加时 1:20");
    assert(NormalizeNbaTeams(L"骑士，76人") == L"CLE,PHI");
    assert(NormalizeNbaTeams(L" cle , phi ") == L"CLE,PHI");
    std::vector<NbaSnapshot> all;
    assert(ParseNbaGames(live, &all) && all.size() == 2 && all[1].scheduled);
    assert(ParseNbaGames("{\"events\":[]}", &all) && all.empty());
    assert(!ParseNbaGames("{}", &all));
    auto missingDate = live;
    const auto dateField = missingDate.find("2026-10-20T19:00Z");
    missingDate.replace(dateField, 17, "bad date");
    assert(!ParseNbaGames(missingDate, &all));
    NbaSnapshot cle, phi, other, selected;
    cle.gameId = L"cle"; cle.awayAbbr = L"CLE"; cle.homeAbbr = L"BOS"; cle.live = true;
    phi.gameId = L"phi"; phi.awayAbbr = L"PHI"; phi.homeAbbr = L"NYK"; phi.live = true;
    other.gameId = L"other"; other.awayAbbr = L"GSW"; other.homeAbbr = L"LAL"; other.live = true;
    assert(SelectNbaGame({other, phi, cle}, L"CLE,PHI", L"", L"", &selected) && selected.gameId == L"cle");
    assert(SelectNbaGame({other, cle, phi}, L"PHI,CLE", L"", L"", &selected) && selected.gameId == L"phi");
    assert(SelectNbaGame({other, phi}, L"CLE,PHI", L"", L"", &selected) && selected.gameId == L"phi");
    assert(!SelectNbaGame({other}, L"CLE,PHI", L"", L"", &selected));
    auto previous = AdvanceNbaSnapshot({}, {cle}, L"CLE,PHI", true, 100);
    assert(previous.active && previous.live && previous.lastUpdated == 100);
    auto missing = AdvanceNbaSnapshot(previous, {}, L"CLE,PHI", false, 110);
    assert(missing.active && missing.stale && !missing.finished);
    missing = AdvanceNbaSnapshot(previous, {}, L"CLE,PHI", true, 221);
    assert(!missing.active && !missing.finished);
    cle.live = false; cle.finished = true;
    auto ended = AdvanceNbaSnapshot(previous, {cle}, L"CLE,PHI", true, 120);
    assert(ended.finished && ended.active && ended.finishedAt == 120);
    ended = AdvanceNbaSnapshot(ended, {cle}, L"CLE,PHI", true, 719);
    assert(ended.active && ended.finishedAt == 120);
    ended = AdvanceNbaSnapshot(ended, {cle}, L"CLE,PHI", true, 720);
    assert(!ended.active && ended.finishedAt == 120);
    auto switched = AdvanceNbaSnapshot(previous, {phi}, L"CLE,PHI", true, 130);
    assert(switched.active && switched.gameId == L"phi" && !switched.finished);
    const double midnight = NbaParseUtc(L"2026-10-18T16:00Z");
    assert(midnight > 0 && NbaParseUtc(L"bad") == 0);
    cle.startTime = midnight; phi.startTime = midnight + 86400; other.startTime = midnight - 1;
    auto summary = SummarizeNbaDay({cle,phi,other}, midnight, midnight + 86400, L"CLE,PHI");
    assert(summary.total == 1 && summary.finished == 1 && summary.favorites == 1);
    SYSTEMTIME local{}; local.wYear = 2026; local.wMonth = 10; local.wDay = 19;
    auto day = GetNbaLocalDay(local);
    assert(day.dates[0] == L"20261018" && day.dates[1] == L"20261019" && day.dates[2] == L"20261020");
    assert(day.label == L"2026-10-19" && day.end > day.start);

}'''
(root/'nba-checks.cpp').write_text(nba_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'nba-checks.cpp'),'-lruntimeobject','-lole32','-loleaut32','-o',str(root/'nba-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'nba-checks.exe'),str(root/'nba-scoreboard-live.json'),str(root/'nba-scoreboard-final.json')],check=True)
print('NBA checks passed: multi-team priority, live/final/stale transitions, local-day boundaries, parsing and logos',flush=True)
point_start=src.index('struct MediaContentPoint {')
point_end=src.index('\n};',point_start)+3
quick_test = '#include <windows.h>\n#include <d2d1.h>\n#include <atomic>\n#include <cassert>\n#include <initializer_list>\n'
quick_test += src[point_start:point_end] + '\n' + (root/'quick-home.h').read_text(encoding='utf-8')
quick_test += '''int main() {
 for(float width : {300.0f,380.0f,420.0f}) {
   for(int index=0;index<4;index++) {
     auto box=QuickHomeButtonBounds(index,width);
     assert(box.left>=24 && box.right<=width-30 && box.top>=70 && box.bottom<=144);
     MediaContentPoint p{true,(box.left+box.right)/2,(box.top+box.bottom)/2,width,184};
     assert(QuickHomeHitTest(p)==index);
     p.valid=false; assert(QuickHomeHitTest(p)==-1);
     p.valid=true; p.height=80; assert(QuickHomeHitTest(p)==-1);
   }
   auto left=QuickHomeButtonBounds(1,width), right=QuickHomeButtonBounds(2,width);
   MediaContentPoint p{true,(left.right+right.left)/2,100,width,184}; assert(QuickHomeHitTest(p)==-1);
   p.y=20; assert(QuickHomeHitTest(p)==-1);
   p.x=width-10; p.y=90; assert(QuickHomeHitTest(p)==-1);
 }
}'''
(root/'quick-home-checks.cpp').write_text(quick_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'quick-home-checks.cpp'),'-o',str(root/'quick-home-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'quick-home-checks.exe')],check=True)
print('Quick-home checks passed: shared drawing/hit bounds, gaps and collapsed-state rejection',flush=True)
ui_start=src.index('namespace TimerDetailLayout {')
ui_end=src.index('// The conversion and hit-test helpers live further down')
ui_test = '#include <windows.h>\n#include <d2d1.h>\n#include <cmath>\n#include <algorithm>\n#include <cassert>\n#include <initializer_list>\n'
ui_test += src[point_start:point_end] + '\n'
ui_test += (root/'volume-controls.h').read_text(encoding='utf-8')

ui_test += src[ui_start:ui_end]
ui_test += '''int main() {
 MediaContentPoint volumePoint{true,66,36,300,54};
 assert(VolumePercentAt(volumePoint)==0 && VolumeControlHitTest(volumePoint)==0);
 volumePoint.x=286; assert(VolumePercentAt(volumePoint)==100);
 volumePoint.x=176; assert(VolumePercentAt(volumePoint)==50);
 volumePoint.x=-100; assert(VolumePercentAt(volumePoint)==0);
 volumePoint.x=1000; assert(VolumePercentAt(volumePoint)==100);
 volumePoint.x=28; volumePoint.y=27; assert(VolumeControlHitTest(volumePoint)==1);
 volumePoint.valid=false; assert(VolumePercentAt(volumePoint)==-1);

 for(float width : {320.0f,380.0f,420.0f}) {
   auto left=TimerDetailButtonBounds(0,width), right=TimerDetailButtonBounds(1,width);
   assert(std::abs((left.left+left.right)*0.5f-(width*0.5f-TimerDetailLayout::kButtonOffsetX))<0.001f);
   assert(std::abs((right.left+right.right)*0.5f-(width*0.5f+TimerDetailLayout::kButtonOffsetX))<0.001f);
   assert(std::abs((left.left+left.right)*0.5f+(right.left+right.right)*0.5f-width)<0.001f);
   assert(left.right<right.left);
   assert(left.top==TimerDetailLayout::kButtonCenterY-TimerDetailLayout::kButtonRadius);
   assert(left.bottom==TimerDetailLayout::kButtonCenterY+TimerDetailLayout::kButtonRadius);
   MediaContentPoint p{true,(left.left+left.right)*0.5f,(left.top+left.bottom)*0.5f,width,164};
   assert(TimerDetailHitTest(p)==0);
   p.x=(right.left+right.right)*0.5f; assert(TimerDetailHitTest(p)==1);
   p.x=width*0.5f; assert(TimerDetailHitTest(p)==-1);
   p.x=(left.left+left.right)*0.5f; p.y=TimerDetailLayout::kButtonCenterY+40; assert(TimerDetailHitTest(p)==-1);
   p.y=TimerDetailLayout::kButtonCenterY; p.height=120; assert(TimerDetailHitTest(p)==-1);
   p.height=150; assert(TimerDetailHitTest(p)==0);
   p.valid=false; assert(TimerDetailHitTest(p)==-1);
 }
 MediaContentPoint c{true,190,148,380,184};
 assert(MediaCompanionHitTest(c)==1);
 c.x=100; assert(MediaCompanionHitTest(c)==-1);
 c.x=190; c.height=80; assert(MediaCompanionHitTest(c)==-1);
 c.height=184; c.width=36; assert(MediaCompanionHitTest(c)==-1);
}'''
(root/'timer-ui-checks.cpp').write_text(ui_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'timer-ui-checks.cpp'),'-o',str(root/'timer-ui-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'timer-ui-checks.exe')],check=True)
print('Timer panel checks passed: centered two-button geometry, hit rejection and companion target',flush=True)
glass_test = '#include <windows.h>\n#include <d2d1.h>\n#include <d3dcompiler.h>\n#include <algorithm>\n#include <cstdio>\n#include <array>\n#include <cassert>\n'
glass_test += (root/'liquid-glass-rim.h').read_text(encoding='utf-8')
glass_renderer = (root/'native-glass/src/renderer.cc').read_text(encoding='utf-8')
glass_test += glass_renderer[glass_renderer.index('const char kShaderSource[]'):glass_renderer.index('\nstruct ShaderParams {')]
glass_profile = glass_renderer[glass_renderer.index('    float u = saturate('):glass_renderer.index('    disp = nrm')]
glass_test += 'float saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }\nfloat GlassRimProfile(float t) { float bump = 0;\n' + glass_profile + '\nreturn bump; }\n'
glass_test += '''int main() {
 for(const char* entry : {"VSMain", "PSBlur", "PSLens"}) {
   ID3DBlob *code=nullptr, *errors=nullptr;
   const HRESULT hr=D3DCompile(kShaderSource, sizeof(kShaderSource)-1, nullptr, nullptr, nullptr,
       entry, entry[0]=='V' ? "vs_5_0" : "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
   if(errors) { std::fputs(static_cast<const char*>(errors->GetBufferPointer()), stderr); errors->Release(); }
   assert(SUCCEEDED(hr));
   code->Release();
 }
 assert(GlassRimProfile(0)==0 && GlassRimProfile(1)==0);
 assert(GlassRimProfile(0.42f)>0.99999f && GlassRimProfile(0.42f)<1.00001f);
 for(int i=1;i<=1000;i++) {
   const float t=i/1000.0f, value=GlassRimProfile(t);
   assert(value>=-0.00001f && value<=1.00001f);
   const float slope=(value-GlassRimProfile(t-0.001f))/0.001f;
   assert(slope*0.13f<0.6f);
 }
 const auto stops=LiquidGlassRimStops();
 assert(stops.front().position==0 && stops.back().position==1);
 for(size_t i=0;i<stops.size();i++) {
   assert(stops[i].color.a>=0 && stops[i].color.a<=1);
   assert(stops[i].color.r==stops[i].color.g && stops[i].color.g==stops[i].color.b);
   if(i) assert(stops[i].position>stops[i-1].position);
 }
 assert(stops[0].color.r>stops[3].color.r && stops[6].color.r>stops[3].color.r);
 assert(stops[0].color.a==105/255.0f && stops[6].color.a==101/255.0f);
}'''
(root/'glass-checks.cpp').write_text(glass_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'glass-checks.cpp'),'-ld3dcompiler','-o',str(root/'glass-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'glass-checks.exe')],check=True)
print('Glass-rim checks passed: shader compilation, smooth refraction profile, RGB slope bound and gradient stops',flush=True)
brief_test = (root/'weather-checks.cpp').read_text(encoding='utf-8').split('bool ParseChinaWeather(')[0]
brief_test += (root/'tomorrow-brief-checks.inc').read_text(encoding='utf-8')
(root/'tomorrow-brief-checks.cpp').write_text(brief_test,encoding='utf-8')
subprocess.run([str(clang),'-std=c++23','-static',str(root/'tomorrow-brief-checks.cpp'),'-lole32','-loleaut32','-lruntimeobject','-o',str(root/'tomorrow-brief-checks.exe')],cwd=compiler,check=True)
subprocess.run([str(root/'tomorrow-brief-checks.exe')],check=True)
print('Tomorrow brief checks passed: rollover, stale/malformed forecasts, units, persistent dedup keys and local NBA date boundaries',flush=True)
args=[str(clang),'-std=c++23','-O2','-shared','-DUNICODE','-D_UNICODE',
 '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DNTDDI_VERSION=0x0A000008',
 '-D__USE_MINGW_ANSI_STDIO=0','-DWH_MOD',f'-DWH_MOD_ID=L"local@{build_id}"',
 f'-DWH_MOD_VERSION=L"{build_version}"',r'C:\Program Files\Windhawk\Engine\1.7.3\64\windhawk.lib',
 '-x','c++',str(out),'-include','windhawk_api.h','-target','x86_64-w64-mingw32',
 '-Wl,--export-all-symbols','-o',str(root/f'local@{build_id}_{build_version}.dll')]
args += ['-l'+x for x in 'ole32 oleaut32 shcore d2d1 dwrite dwmapi gdi32 user32 shell32 runtimeobject windowscodecs avrt setupapi winhttp pdh winmm d3d11 dxgi dcomp d3dcompiler dxguid comctl32'.split()]
subprocess.run(args,cwd=compiler,check=True)
args32 = [a.replace('\\64\\windhawk.lib', '\\32\\windhawk.lib')
          .replace('x86_64-w64-mingw32', 'i686-w64-mingw32') for a in args]
(root/'32').mkdir(exist_ok=True)
args32[args32.index('-o')+1] = str(root/'32'/f'local@{build_id}_{build_version}.dll')
subprocess.run(args32,cwd=compiler,check=True)
print('YASB integration compiled')
