std::vector<MediaSourceInfo> AppendMediaSources(HMENU menu) {
    std::vector<MediaSourceInfo> sources;
    std::wstring active;
    { std::lock_guard lock(g_stateMutex); sources=g_state.mediaSources; active=g_state.media.sourceAppUserModelId; }
    HMENU submenu=CreatePopupMenu();
    AppendMenuW(submenu, MF_STRING | (GetPreferredMediaSource().empty() ? MF_CHECKED : 0), kMediaSourceMenuBase, L"自动跟随");
    for (size_t i=0;i<sources.size() && i+1<kMediaSourceMenuLimit-kMediaSourceMenuBase;++i)
        AppendMenuW(submenu, MF_STRING | (sources[i].aumid==active ? MF_CHECKED : 0),
                    kMediaSourceMenuBase+1+static_cast<UINT>(i), sources[i].name.c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), L"媒体来源（选择后锁定）");
    return sources;
}
void SelectMediaSourceCommand(UINT cmd, const std::vector<MediaSourceInfo>& sources) {
    if (cmd==kMediaSourceMenuBase) LockMediaSource(L"");
    else if (cmd>kMediaSourceMenuBase && cmd<kMediaSourceMenuLimit && cmd-kMediaSourceMenuBase-1<sources.size())
        LockMediaSource(sources[cmd-kMediaSourceMenuBase-1].aumid);
}
std::wstring g_mediaSourcePressed;
int MediaSourceAtPoint(int x, int y, std::vector<MediaSourceInfo>* sources) {
    {
        std::lock_guard lock(g_stateMutex);
        const auto kinds=ChooseActivities(g_state,g_settings,NowSeconds());
        if (kinds.empty() || kinds[0]!=IslandKind::Media || CurrentDashboardTab(g_settings)!=0) return -1;
        *sources=g_state.mediaSources;
    }
    return MediaSourceDockHit(MediaContentFromClient(x,y),static_cast<int>(sources->size()));
}
bool BeginMediaSourceClick(HWND hwnd, int x, int y) {
    std::vector<MediaSourceInfo> sources;
    const int index=MediaSourceAtPoint(x,y,&sources);
    if (index<0) return false;
    g_mediaSourcePressed=index==4 && sources.size()>5 ? L"*" : sources[index].aumid;
    SetCapture(hwnd);
    return true;
}
bool HandleMediaSourceClick(HWND hwnd, int x, int y) {
    if (g_mediaSourcePressed.empty()) return false;
    const auto pressed=std::exchange(g_mediaSourcePressed,L"");
    if (GetCapture()==hwnd) ReleaseCapture();
    std::vector<MediaSourceInfo> sources;
    const int index=MediaSourceAtPoint(x,y,&sources);
    if (index<0) return true;
    if (index==4 && sources.size()>5) {
        if (pressed!=L"*") return true;
        HMENU menu=CreatePopupMenu();
        AppendMediaSources(menu);
        POINT point{}; GetCursorPos(&point); SetForegroundWindow(hwnd);
        const UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,hwnd,nullptr);
        DestroyMenu(menu); PostMessageW(hwnd,WM_NULL,0,0);
        SelectMediaSourceCommand(cmd,sources);
    } else if (pressed==sources[index].aumid) RequestMediaSource(sources[index].aumid);
    return true;
}
void AppendLyricsMenu(HMENU menu) {
    const auto settings=GetSettingsCopy();
    AppendMenuW(menu,MF_STRING|(settings.collapsedLyrics ? MF_CHECKED:0),2001,L"收起时显示歌词");
    AppendMenuW(menu,MF_STRING|(settings.lyricsPinnedFirst ? MF_CHECKED:0),2002,L"歌词页置顶");
    AppendMenuW(menu,MF_STRING|(settings.mediaTabLyrics ? MF_CHECKED:0),2003,L"媒体页内显示歌词");
    AppendMenuW(menu,MF_STRING,2004,L"清除歌词缓存");
    HMENU sync=CreatePopupMenu();
    const bool bt=g_defaultOutputIsBluetooth.load();
    const auto label=std::wstring(bt ? L"蓝牙偏移：" : L"扬声器／有线偏移：")+
        std::to_wstring(bt ? g_lyricsEffectiveOffsetBtMs.load() : g_lyricsEffectiveOffsetMs.load())+L" ms";
    AppendMenuW(sync,MF_STRING|MF_DISABLED,0,label.c_str());
    const wchar_t* steps[]={L"提前 10 ms",L"提前 50 ms",L"提前 250 ms",L"延后 10 ms",L"延后 50 ms",L"延后 250 ms",L"重置当前设备微调"};
    for(int i=0;i<7;++i) AppendMenuW(sync,MF_STRING,2010+i,steps[i]);
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(sync),L"歌词同步");
    if(settings.quickLookup) AppendMenuW(menu,MF_STRING,2020,L"快捷查词…");
}
bool HandleLyricsMenu(HWND hwnd,UINT cmd) {
    const auto settings=GetSettingsCopy();
    if(cmd==2001) {
        const bool newVal = !settings.collapsedLyrics;
        Wh_SetIntValue(L"CollapsedLyrics", newVal);
        Wh_SetIntValue(L"CollapsedLyricsOverride", newVal);
        Wh_SetIntValue(L"CollapsedLyricsOverrideBase", Wh_GetIntSetting(L"Modules.CollapsedLyrics") != 0);
    }
    else if(cmd==2002) Wh_SetIntValue(L"LyricsPinnedFirst",!settings.lyricsPinnedFirst);
    else if(cmd==2003) {
        Wh_SetIntValue(L"MediaTabLyricsOverride",!settings.mediaTabLyrics);
        Wh_SetIntValue(L"MediaTabLyricsOverrideBase",Wh_GetIntSetting(L"Modules.MediaTabLyrics")!=0);
    }
    else if(cmd==2004) {g_lyricsCacheClearRequested=true; return true;}
    else if(cmd>=2010 && cmd<=2016) {
        const auto key=g_defaultOutputIsBluetooth.load() ? L"LyricsNudgeBtMs" : L"LyricsNudgeMs";
        constexpr int steps[]={10,50,250,-10,-50,-250};
        Wh_SetIntValue(key,cmd==2016 ? 0 : ClampInt(Wh_GetIntValue(key,0)+steps[cmd-2010],-3000,3000));
    } else if(cmd==2020) {StartQuickLookup(hwnd); return true;}
    else return false;
    LoadSettings(); g_layoutDirty=true;
    return true;
}
