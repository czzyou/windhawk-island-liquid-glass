"""Port the selected 1.4.0 media, lyrics and lookup features onto the existing renderer."""
from pathlib import Path
import re


def apply_media_features(src):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    def header(name):
        return (root / name).read_text(encoding='utf-8')

    change('#include <winrt/base.h>', '#include <memory>\n#include <uiautomation.h>\n#include <appmodel.h>\n#include <winrt/base.h>\n#include <winrt/Windows.Data.Json.h>\n#include <winrt/Windows.Security.Cryptography.h>')
    change('    Idle,', '    Lookup,\n    Idle,')
    change('struct MediaSnapshot {', 'struct MediaSnapshot {\n    bool lyricsEligible = false;')
    change('struct Settings {', 'struct Settings {\n' + header('media-features-settings.h'))
    change('struct SharedState {', '''struct MediaSourceInfo {
    std::wstring aumid, name, badge;
    bool playing = false;
    BitmapPixels icon;
};
''' + header('media-features-types.h') + '\nstruct SharedState {\n    std::vector<MediaSourceInfo> mediaSources;\n    LyricsSnapshot lyrics;\n    LookupSnapshot lookup;\n    std::vector<LookupRecent> lookupRecent;')
    # Hit-test helpers need MediaContentPoint, which is declared after tab arithmetic.
    globals_ = header('media-features-globals.h')
    a = globals_.index('int MediaSourceDockHit(')
    b = globals_.index('void UpdateLyricsOutputDevice(', a)
    dock_hit = globals_[a:b]
    globals_ = globals_[:a] + globals_[b:]
    start = src.index('int ActiveTabCount(')
    end = src.index('// ── Localization', start)
    src = src[:start] + header('lyrics-policy.h') + '\n' + header('lyrics-layout.h') + '\n' + globals_ + '\n' + header('dashboard-tabs.h') + '\n' + src[end:]

    settings_yaml = '''  - Lyrics: true
    $name: 实时歌词
    $description: 仅网易云音乐、QQ 音乐、酷狗、酷我、Spotify、Apple Music 客户端支持歌词；浏览器不显示歌词。歌词通过网易云、QQ、酷狗和 LRCLIB 匹配。
  - CollapsedLyrics: true
    $name: 收起时显示歌词
    $description: 播放音乐且胶囊收起时，平滑自适应显示当前句歌词；右键菜单亦可随时切换。
  - MediaTabLyrics: false
    $name: 媒体页内显示歌词
  - LyricsCache: true
    $name: 缓存歌词到本机
  - LyricsCacheMaxMB: '25'
    $name: 歌词缓存容量
    $options:
      - '0': 关闭磁盘缓存
      - '10': 10 MB
      - '25': 25 MB
      - '100': 100 MB
  - LyricsOffsetMs: 0
    $name: 歌词偏移（扬声器／有线，毫秒）
    $description: -3000 到 3000；正数提前，负数延后。
  - LyricsOffsetBluetoothMs: 0
    $name: 歌词偏移（蓝牙，毫秒）
  - QuickLookup: true
    $name: 快捷查词
    $description: 打开搜索框，提交后查询英文词典或百科；最近记录仅存内存。
'''
    change('  - FileTrayModule: false', settings_yaml + '  - FileTrayModule: false')
    change('  - HideShowKey:', '''  - LookupModifiers: alt
    $name: 查词快捷键修饰键
    $options:
      - alt: Alt
      - ctrl_alt: Ctrl + Alt
      - ctrl_shift: Ctrl + Shift
      - alt_shift: Alt + Shift
      - win_alt: Win + Alt
  - LookupKey: Space
    $name: 查词快捷键（Space 或 A-Z、0-9）
  - HideShowKey:''')
    change('    Settings next;\n', '''    Settings next;
    next.lyrics = Wh_GetIntSetting(L"Modules.Lyrics") != 0;
    const int mediaLyricsSetting = Wh_GetIntSetting(L"Modules.MediaTabLyrics") != 0;
    int mediaLyricsOverride = Wh_GetIntValue(L"MediaTabLyricsOverride", -1);
    if (mediaLyricsOverride >= 0 && Wh_GetIntValue(L"MediaTabLyricsOverrideBase", -1) != mediaLyricsSetting) {
        Wh_SetIntValue(L"MediaTabLyricsOverride", -1); mediaLyricsOverride=-1;
    }
    next.mediaTabLyrics = mediaLyricsOverride >= 0 ? mediaLyricsOverride != 0 : mediaLyricsSetting != 0;
    const int collapsedLyricsSetting = Wh_GetIntSetting(L"Modules.CollapsedLyrics") != 0;
    int collapsedLyricsOverride = Wh_GetIntValue(L"CollapsedLyricsOverride", -1);
    if (collapsedLyricsOverride >= 0 && Wh_GetIntValue(L"CollapsedLyricsOverrideBase", -1) != collapsedLyricsSetting) {
        Wh_SetIntValue(L"CollapsedLyricsOverride", -1); collapsedLyricsOverride = -1;
    }
    int legacyCollapsedLyrics = Wh_GetIntValue(L"CollapsedLyrics", -1);
    if (collapsedLyricsOverride < 0 && legacyCollapsedLyrics >= 0) {
        collapsedLyricsOverride = legacyCollapsedLyrics;
    }
    next.collapsedLyrics = collapsedLyricsOverride >= 0 ? (collapsedLyricsOverride != 0) : (collapsedLyricsSetting != 0);
    next.lyricsPinnedFirst = Wh_GetIntValue(L"LyricsPinnedFirst", 0) != 0;
    next.lyricsCache = Wh_GetIntSetting(L"Modules.LyricsCache") != 0;
    next.lyricsCacheMaxMB = ClampInt(_wtoi(GetStringSettingCopy(L"Modules.LyricsCacheMaxMB").c_str()), 0, 100);
    next.lyricsOffsetMs = ClampInt(Wh_GetIntSetting(L"Modules.LyricsOffsetMs"), -3000, 3000);
    next.lyricsOffsetBtMs = ClampInt(Wh_GetIntSetting(L"Modules.LyricsOffsetBluetoothMs"), -3000, 3000);
    g_lyricsEffectiveOffsetMs = ClampInt(next.lyricsOffsetMs + Wh_GetIntValue(L"LyricsNudgeMs", 0), -3000, 3000);
    g_lyricsEffectiveOffsetBtMs = ClampInt(next.lyricsOffsetBtMs + Wh_GetIntValue(L"LyricsNudgeBtMs", 0), -3000, 3000);
    next.quickLookup = Wh_GetIntSetting(L"Modules.QuickLookup") != 0;
    const auto lookupModifiers = GetStringSettingCopy(L"Shortcuts.LookupModifiers");
    if (lookupModifiers == L"ctrl_alt") next.lookupModifiers = MOD_CONTROL | MOD_ALT;
    else if (lookupModifiers == L"ctrl_shift") next.lookupModifiers = MOD_CONTROL | MOD_SHIFT;
    else if (lookupModifiers == L"alt_shift") next.lookupModifiers = MOD_ALT | MOD_SHIFT;
    else if (lookupModifiers == L"win_alt") next.lookupModifiers = MOD_WIN | MOD_ALT;
    const auto lookupKey = GetStringSettingCopy(L"Shortcuts.LookupKey");
    if (lookupKey.size() == 1 && ((towupper(lookupKey[0]) >= L'A' && towupper(lookupKey[0]) <= L'Z') ||
        (lookupKey[0] >= L'0' && lookupKey[0] <= L'9'))) next.lookupVk = static_cast<UINT>(towupper(lookupKey[0]));
''')
    change('void ApplyHideShowHotkey();', 'void ApplyLookupHotkey();\nvoid ApplyHideShowHotkey();')
    change('void ApplyHideShowHotkey() {', 'void ApplyHideShowHotkey() {\n    ApplyLookupHotkey();')
    change('next.hideShowHotkeyEnabled != g_settings.hideShowHotkeyEnabled ||',
           'next.quickLookup != g_settings.quickLookup || next.lookupModifiers != g_settings.lookupModifiers || next.lookupVk != g_settings.lookupVk ||\n            next.hideShowHotkeyEnabled != g_settings.hideShowHotkeyEnabled ||')

    # Adapt upstream automatic/temporary/locked source selection before its caller.
    change('DWORD WINAPI MediaThreadProc(void*) {', header('media-sources.h') + '\n' + header('media-timeline.h') + '\nDWORD WINAPI MediaThreadProc(void*) {')
    change('    bool loggedUnavailable = false;\n', '    bool loggedUnavailable = false;\n    MediaSourceTracker sourceTracker;\n    MediaTimelineTracker timelineTracker;\n')
    change('        MediaSnapshot next;\n', '        MediaSnapshot next;\n        std::vector<MediaSourceInfo> sources;\n        bool gotSources = false;\n')
    change('                auto session = SelectActiveMediaSession(manager);', '''                gotSources = CollectMediaSources(manager, sourceTracker, &sources);
                const auto chosen = ChooseMediaSourceAumid(sourceTracker, GetPreferredMediaSource());
                auto session = SelectActiveMediaSession(manager);
                if (!chosen.empty()) {
                    session = nullptr;
                    for (const auto& candidate : manager.GetSessions())
                        if (candidate.SourceAppUserModelId().c_str() == chosen) { session = candidate; break; }
                }''')
    change('                    next.albumTitle = properties.AlbumTitle().c_str();', '                    next.albumTitle = properties.AlbumTitle().c_str();\n                    next.sourceAppUserModelId = session.SourceAppUserModelId().c_str();')
    change('                    next.sourceAppUserModelId = session.SourceAppUserModelId().c_str();\n\n                    if (timeline)', '''                    next.sourceAppUserModelId = session.SourceAppUserModelId().c_str();
                    const auto playbackType=properties.PlaybackType();
                    const bool music=playbackType && playbackType.Value()==winrt::Windows::Media::MediaPlaybackType::Music;
                    const bool video=playbackType && playbackType.Value()==winrt::Windows::Media::MediaPlaybackType::Video;
                    next.lyricsEligible=LyricsAllowed(next.sourceAppUserModelId,music,video,!next.artist.empty());

                    if (timeline)''')
    start = src.index('                    if (timeline) {', src.index('DWORD WINAPI MediaThreadProc('))
    end = src.index('\n                    next.sourceAppUserModelId', start)
    src = src[:start] + '''                    if (timeline) {
                        int64_t age = 0;
                        try { age = (winrt::clock::now() - timeline.LastUpdatedTime()).count(); } catch (...) {}
                        std::lock_guard lock(g_stateMutex);
                        timelineTracker.Update(next, g_state.media, timeline.Position().count(), timeline.EndTime().count(), age, GetTickCount64());
                    }
''' + src[end:]
    change('            g_state.media = std::move(next);', '''            if (gotSources) {
                bool changed = sources.size() != g_state.mediaSources.size();
                for (size_t i=0; !changed && i<sources.size(); ++i)
                    changed = sources[i].aumid != g_state.mediaSources[i].aumid || sources[i].playing != g_state.mediaSources[i].playing;
                if (changed) g_layoutDirty=true;
                g_state.mediaSources = std::move(sources);
            }
            if (MediaTrackIdentity(next) != MediaTrackIdentity(g_state.media)) {
                g_state.lyrics = LyricsSnapshot{};
                if (g_lyricsChangedEvent) SetEvent(g_lyricsChangedEvent);
            }
            g_state.media = std::move(next);''')
    change('        bool trackJustChanged = false;', '''        next.available = next.available && MediaSessionHasContent(next.playing, next.title);
        next.lyricsEligible = next.lyricsEligible && next.available;
        bool trackJustChanged = false;''')
    change('                g_idleTab = 0;\n', '')
    change('            if (!g_state.media.art.bgra.empty() &&\n                next.title', '            if (!g_state.media.art.bgra.empty() &&\n                next.sourceAppUserModelId == g_state.media.sourceAppUserModelId &&\n                next.title')
    change('        WaitForSingleObject(g_stopEvent, 1500);', '        HANDLE events[] = {g_stopEvent, g_mediaRefreshEvent};\n        WaitForMultipleObjects(2, events, FALSE, 750);')
    change('void SeekMediaToTicks(int64_t targetTicks) {', header('apple-music-seek.h') + '\nvoid SeekMediaToTicks(int64_t targetTicks) {')
    change('void SeekMediaToTicks(int64_t targetTicks) {\n    std::thread([targetTicks]() {', '''void SeekMediaToTicks(int64_t targetTicks) {
    std::wstring source;
    { std::lock_guard lock(g_stateMutex); source = g_state.media.sourceAppUserModelId; }
    if (IsAppleMusicSource(source) && g_scrubbing.load()) return;
    std::thread([targetTicks, source]() {''')
    change('''                std::wstring currentAumid;
                {
                    std::lock_guard lock(g_stateMutex);
                    currentAumid = g_state.media.sourceAppUserModelId;
                }''', '                const auto& currentAumid = source;')
    change('                if (!session) session = manager.GetCurrentSession();', '                if (!session && currentAumid.empty()) session = manager.GetCurrentSession();')
    change('                    session.TryChangePlaybackPositionAsync(targetTicks).get();', '''                    const bool seeked=IsAppleMusicSource(source) ? SeekAppleMusic(source,targetTicks) :
                        session.TryChangePlaybackPositionAsync(targetTicks).get();
                    if (seeked) {
                        std::lock_guard lock(g_stateMutex);
                        if (g_state.media.sourceAppUserModelId == source) {
                            g_state.media.positionTicks = targetTicks;
                            g_state.media.lastUpdatedTicks = GetTickCount64();
                            g_layoutDirty = true;
                        }
                        if (g_mediaRefreshEvent) SetEvent(g_mediaRefreshEvent);
                    }''')
    change('ResolveMediaSession() {', 'ResolveMediaSession(const std::wstring& currentAumid) {')
    change('''    std::wstring currentAumid;
    {
        std::lock_guard lock(g_stateMutex);
        currentAumid = g_state.media.sourceAppUserModelId;
    }
''', '')
    change('    return manager.GetCurrentSession();\n}', '    return currentAumid.empty() ? manager.GetCurrentSession() : nullptr;\n}')
    change('void SendMediaTransportCommand(int cmd) {', '''void SendMediaTransportCommand(int cmd) {
    std::wstring source;
    { std::lock_guard lock(g_stateMutex); source = g_state.media.sourceAppUserModelId; }''')
    change('std::thread([cmd]() {', 'std::thread([cmd, source]() {')
    change('auto session = ResolveMediaSession();', 'auto session = ResolveMediaSession(source);')
    # The operation already names a source. Global media keys could control another player.
    change('            if (!reportedSuccess) {', '            if (!reportedSuccess && source.empty()) {')

    runtime = '\n'.join(header(name) for name in ('media-http.h', 'media-web-data.h', 'lyrics.h', 'quick-lookup.h', 'lookup-worker.h'))
    runtime = runtime.replace('DWORD WINAPI LyricsThreadProc(', header('lyrics-providers.h') + '\nDWORD WINAPI LyricsThreadProc(')
    change('void ShowContextMenu(HWND hwnd, POINT screenPoint) {', dock_hit + runtime + '\nstd::vector<IslandKind> ChooseActivities(const SharedState&, const Settings&, double);\n' + header('media-features-menu.h') + '\nvoid ShowContextMenu(HWND hwnd, POINT screenPoint) {')
    change('    AppendMenuW(menu, MF_STRING, 40,',
           '    const auto sourceMenuItems = AppendMediaSources(menu);\n    AppendLyricsMenu(menu);\n    AppendMenuW(menu, MF_STRING, 40,')
    change('    // Theme ids are a contiguous range', '''    if (cmd >= kMediaSourceMenuBase && cmd < kMediaSourceMenuLimit) {
        SelectMediaSourceCommand(cmd, sourceMenuItems); return;
    }
    if (HandleLyricsMenu(hwnd, cmd)) return;
    // Theme ids are a contiguous range''')
    change('void SetClickThrough(HWND hwnd, bool clickThrough) {',
           'void SetClickThrough(HWND hwnd, bool clickThrough) {\n    clickThrough = clickThrough && !g_lookupUi.open;')
    change('    void DrawMedia(const SharedState& state,', header('lyrics-draw.h') + '\n' + header('quick-lookup-draw.h') + '\n    void DrawMedia(const SharedState& state,')
    change('            case IslandKind::Clipboard:\n                DrawClipboard', '            case IslandKind::Lookup:\n                DrawLookup(state, unscaledRect, settings, now);\n                break;\n            case IslandKind::Clipboard:\n                DrawClipboard')
    # Map idle renderer's old page IDs to the IDs already used by the media renderer.
    start = src.index('    void DrawIdleDashboard(')
    end = src.index('    int CurrentLyricLine(', start)
    idle = src[start:end]
    idle = re.sub(r'activeTabId == ([0123])', lambda m: 'activeTabId == ' + {'3':'0','0':'1','1':'2','2':'3'}[m[1]], idle)
    src = src[:start] + idle + src[end:]
    pattern = r'            std::vector<int> activeTabs;\n(?:            .*\n)+?(?=\n)'
    src, count = re.subn(pattern, '            const auto activeTabs = DashboardTabs(settings);\n', src)
    assert count == 2, count
    change('            else if (activeTabId == 5) DrawHourlyClock', '            else if (activeTabId == 6) DrawLyricsDashboard(state, rect, settings, now, scale);\n            else if (activeTabId == 5) DrawHourlyClock')
    change('            } else if (activeTabId == 5) {', '            } else if (activeTabId == 6) {\n                DrawLyricsDashboard(state, rect, settings, now, 1.0f);\n            } else if (activeTabId == 5) {')
    start = src.index('    void DrawMedia(')
    end = src.index('    void UpdateMediaButtonAnimations(', start)
    media = src[start:end]
    a = media.index('                // Expanded Apple DI media:')
    b = media.index('                // Timeline', a)
    media = media[:a] + '''                if (settings.mediaTabLyrics && state.media.lyricsEligible) {
                    DrawLyricsLines(state, D2D1::RectF(rect.left+26,rect.top+18,rect.right-26,rect.top+96), settings, now);
                } else {
''' + media[a:b] + '                }\n' + media[b:]
    media = media.replace('            // Pagination dots (Vertical on the right edge)', '            if (activeTabId == 0) DrawMediaSources(state, rect);\n\n            // Pagination dots (Vertical on the right edge)')
    media = media.replace('            g_idleTab = 0;\n', '')
    media = media.replace('            float shiftX = 0.0f;', '            DrawCollapsedLyric(state, rect, settings, now);\n            float shiftX = 0.0f;')
    src = src[:start] + media + src[end:]
    src = src.replace('        mediaSourceIconBitmap_.Reset();',
                      '        mediaSourceIconBitmap_.Reset();\n        sourceDockBitmaps_.clear(); sourceDockGenerations_.clear();\n        lyricInk_.Reset(); lyricShade_.Reset(); lyricBlocks_.clear(); compactLyricLayout_.Reset();\n        lyricBlockData_=nullptr; collapsedMeasureFormat_.Reset(); collapsedMeasureLines_={}; collapsedLyricCache_.layout.Reset();')
    change('(next.title != g_state.media.title || next.artist != g_state.media.artist || (!g_state.media.playing && next.playing))',
           '(next.sourceAppUserModelId != g_state.media.sourceAppUserModelId || next.title != g_state.media.title || next.artist != g_state.media.artist || (!g_state.media.playing && next.playing))')
    change('''            if (cache.layout) {
                cache.layout->GetMetrics(&cache.metrics);''', '''            if (cache.layout) {
                cache.layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                cache.layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                cache.layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                cache.layout->GetMetrics(&cache.metrics);''')
    change('float speed, MarqueeLayoutCache& cache) {', 'float speed, MarqueeLayoutCache& cache, double lyricDuration=0.0) {')
    change('''            target_->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), cache.layout.Get(), brush,''',
           '''            const float left=rect.left+(lyricDuration>0 ? (available-cache.metrics.widthIncludingTrailingWhitespace)*0.5f : 0.0f);
            target_->DrawTextLayout(D2D1::Point2F(left, rect.top), cache.layout.Get(), brush,''')
    change('const float offset = std::fmod(static_cast<float>(now) * speed, cycle);',
           'const float offset = lyricDuration>0 ? CollapsedLyricsOffset(cache.metrics.widthIncludingTrailingWhitespace,available,now,lyricDuration) : std::fmod(static_cast<float>(now) * speed, cycle);')
    # Every test of page position must follow the new optional first lyrics tab.
    src = src.replace('NormalizedTabIndex(g_settings)==1', 'CurrentDashboardTab(g_settings)==1')
    src = src.replace('NormalizedTabIndex(g_settings) == 0', 'CurrentDashboardTab(g_settings) == 0')
    src = src.replace('NormalizedTabIndex(settings) == 0', 'CurrentDashboardTab(settings) == 0')
    src = src.replace('currentTab == 0', 'currentTab == MediaTabIndex(g_settings)')

    change('    switch (kind) {\n        case IslandKind::Media:', '''    switch (kind) {
        case IslandKind::Lookup:
            activity.width = LookupLayout::kWidth;
            activity.height = LookupLayout::StatusHeight();
            break;
        case IslandKind::Media:''')
    marker = 'std::vector<IslandKind> ChooseActivities(const SharedState& state, const Settings& settings, double now) {'
    change(marker, marker + '''
    if (settings.quickLookup && state.lookup.active && now < state.lookup.expiresAt)
        return {IslandKind::Lookup};''')
    marker = '        const std::vector<IslandKind> kinds = ChooseActivities(snapshot, g_settings, now);'
    change(marker, '''        const bool lyricsUnavailable = snapshot.lyrics.trackIdentity==MediaTrackIdentity(snapshot.media) &&
            !snapshot.lyrics.hasData && (snapshot.lyrics.notFound || snapshot.lyrics.failed);
        RefreshDashboardTabs(g_settings, snapshot.media.available && snapshot.media.lyricsEligible, lyricsUnavailable);
        if (g_lookupUi.open && (!g_settings.quickLookup || !snapshot.lookup.active || now >= snapshot.lookup.expiresAt))
            CloseLookupPanel(hwnd);
''' + marker)
    change('        const float normalIdleWidth = primary.width;', '''        if (primary.kind == IslandKind::Lookup) {
            primary.width = LookupLayout::kWidth * g_settings.sizeScale;
            primary.height = renderer.MeasureLookupCard(snapshot, g_settings) * g_settings.sizeScale;
            secondary.reset();
            needsRender = true;
            isHidden = false;
        }
        if (primary.kind == IslandKind::Media && snapshot.media.lyricsEligible && g_settings.collapsedLyrics && !pinned) {
            const float lyricWidth = renderer.MeasureCollapsedLyricsWidth(snapshot,g_settings,now,isHoverExpanded) * g_settings.sizeScale;
            // Shrinking on hover would move a wide lyric strip out from under the pointer.
            primary.width = isHoverExpanded ? (std::max)(primary.width, lyricWidth) : lyricWidth;
            if (isHoverExpanded) primary.width=(std::max)(primary.width,widthSpring.value);
        }
        if (primary.kind == IslandKind::Media && (g_settings.collapsedLyrics || CurrentDashboardTab(g_settings)==6 || g_settings.mediaTabLyrics))
            needsRender = true;
        const float normalIdleWidth = primary.width;''')
    change('''        if (compactResize) widthSpring.StepExponential(dt * speed);''', '''        const bool lyricResize=primary.kind==IslandKind::Media && g_settings.collapsedLyrics && !secondary;
        static BOOL lyricMotionEnabled=TRUE;
        static double lyricMotionCheckedAt=-1;
        if (lyricResize && now-lyricMotionCheckedAt>=0.5) {
            SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&lyricMotionEnabled,0);
            lyricMotionCheckedAt=now;
        }
        if (lyricResize && !lyricMotionEnabled) widthSpring.Reset(widthSpring.target);
        else if (compactResize || lyricResize) widthSpring.StepExponential(dt * speed);''')
    change('bool isTransientAlert = (primary.kind == IslandKind::Clipboard ||', 'bool isTransientAlert = (primary.kind == IslandKind::Lookup || primary.kind == IslandKind::Clipboard ||')
    change('        case WM_HOTKEY: {', '        case WM_HOTKEY: {\n            if (wParam == ID_LOOKUP_HOTKEY) { StartQuickLookup(hwnd); return 0; }')
    change('    switch (msg) {\n        case WM_CREATE:', '''    switch (msg) {
        case WM_KEYDOWN:
            if (HandleLookupKeyDown(hwnd, wParam)) return 0;
            break;
        case WM_CHAR:
            if (HandleLookupChar(static_cast<wchar_t>(wParam))) return 0;
            break;
        case WM_KILLFOCUS:
            if (g_lookupUi.open) CloseLookupPanel(hwnd);
            break;
        case WM_CREATE:''')
    change('        case WM_DESTROY:\n', '        case WM_DESTROY:\n            if (g_lookupUi.open) CloseLookupPanel(hwnd);\n            if (g_lookupHotkeyRegistered) { UnregisterHotKey(hwnd, ID_LOOKUP_HOTKEY); g_lookupHotkeyRegistered=false; }\n')
    change('''        case WM_LBUTTONDOWN:
            {''', '''        case WM_LBUTTONDOWN:
            {
                if (g_lookupUi.open) return 0;''')
    change('                if (g_lookupUi.open) return 0;', '                if (g_lookupUi.open) return 0;\n                if (BeginMediaSourceClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) return 0;')
    change('''        case WM_LBUTTONUP:
            {''', '''        case WM_LBUTTONUP:
            {
                if (g_lookupUi.open) { HandleLookupClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0; }
                if (HandleMediaSourceClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) return 0;''')
    change('        case WM_MOUSEMOVE: {', '        case WM_MOUSEMOVE: {\n            if (g_lookupUi.open) { HandleLookupMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0; }')
    change('        case WM_SETCURSOR:\n', '''        case WM_SETCURSOR:
            if (g_lookupUi.open) {
                POINT pt{}; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
                SetCursor(LoadCursorW(nullptr, LookupCursorAt(pt.x, pt.y))); return TRUE;
            }
''')
    change('        if (g_manuallyHidden.load()) {\n            // Manual hide', '        if (g_manuallyHidden.load() && !g_lookupUi.open) {\n            // Manual hide')
    change('        case WM_MOUSEWHEEL: {', '''        case WM_MOUSEWHEEL: {
            if (g_lookupUi.open) return 0;''')
    # Lookup changes also need the hotkey re-registration when the old hide shortcut is unchanged.
    change('    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);', '''    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_mediaRefreshEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_lookupJobEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_lyricsChangedEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_mediaRefreshEvent || !g_lookupJobEvent || !g_lyricsChangedEvent) return false;''')
    change('    g_audioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);', '''    g_lyricsThread = CreateThread(nullptr, 0, LyricsThreadProc, nullptr, 0, nullptr);
    g_lookupThread = CreateThread(nullptr, 0, LookupThreadProc, nullptr, 0, nullptr);
    if (!g_lyricsThread || !g_lookupThread) return false;
    g_audioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);''')
    change('void StopThreads() {', 'void StopThreads() {\n    g_running = false;\n    ++g_lookupRequestSeq;')
    change('    HANDLE handles[] = {g_renderThread,', '''    for (HANDLE* worker : {&g_lyricsThread, &g_lookupThread, &g_mediaThread}) {
        if (*worker) { WaitForSingleObject(*worker, INFINITE); CloseHandle(*worker); *worker=nullptr; }
    }
    HANDLE handles[] = {g_renderThread,''')
    change('    if (g_stopEvent) {\n        CloseHandle(g_stopEvent);', '''    if (g_mediaRefreshEvent) { CloseHandle(g_mediaRefreshEvent); g_mediaRefreshEvent=nullptr; }
    if (g_lookupJobEvent) { CloseHandle(g_lookupJobEvent); g_lookupJobEvent=nullptr; }
    if (g_lyricsChangedEvent) { CloseHandle(g_lyricsChangedEvent); g_lyricsChangedEvent=nullptr; }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);''')
    return src
