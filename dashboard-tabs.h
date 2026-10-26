std::vector<int> DashboardTabs(const Settings& settings) {
    std::vector<int> tabs{0, 1};
    if (settings.weather) tabs.push_back(2);
    if (settings.hardwareMonitorModule) tabs.push_back(3);
    tabs.push_back(5);
    if (settings.fileTrayModule) tabs.push_back(4);
    if (settings.lyrics && g_lyricsTabAvailable.load()) {
        if (settings.lyricsPinnedFirst && g_lyricsPinAllowed.load()) tabs.insert(tabs.begin(), 6);
        else tabs.push_back(6);
    }
    return tabs;
}
int ActiveTabCount(const Settings& settings) { return static_cast<int>(DashboardTabs(settings).size()); }
int NormalizedTabIndex(const Settings& settings) {
    const int total = ActiveTabCount(settings), raw = g_idleTab.load();
    return ((raw % total) + total) % total;
}
int DashboardTabIndex(const Settings& settings, int id) {
    const auto tabs = DashboardTabs(settings);
    const auto it = std::find(tabs.begin(), tabs.end(), id);
    return it == tabs.end() ? -1 : static_cast<int>(it - tabs.begin());
}
int CurrentDashboardTab(const Settings& settings) { return DashboardTabs(settings)[NormalizedTabIndex(settings)]; }
int MediaTabIndex(const Settings& settings) { return DashboardTabIndex(settings, 0); }
int CalendarTabIndex(const Settings& settings) { return DashboardTabIndex(settings, 1); }
int HardwareMonitorTabIndex(const Settings& settings) { return DashboardTabIndex(settings, 3); }
int ClockTabIndex(const Settings& settings) { return DashboardTabIndex(settings, 5); }
int FileTrayTabIndex(const Settings& settings) { return DashboardTabIndex(settings, 4); }

void RefreshDashboardTabs(const Settings& settings, bool mediaAvailable, bool lyricsUnavailable=false) {
    static std::vector<int> previous;
    const int raw = g_idleTab.load();
    const int count = static_cast<int>(previous.size());
    int selected = previous.empty() ? 0 : previous[((raw % count) + count) % count];
    if (lyricsUnavailable && selected==6 && previous.front()==6) selected=0;
    g_lyricsTabAvailable = settings.media && mediaAvailable;
    g_lyricsPinAllowed = !lyricsUnavailable;
    const auto next = DashboardTabs(settings);
    if (next != previous) {
        const auto it = std::find(next.begin(), next.end(), selected);
        g_idleTab = it == next.end() ? MediaTabIndex(settings) : static_cast<int>(it - next.begin());
        previous = next;
        g_layoutDirty = true;
    }
}
