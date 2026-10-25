// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
    bool lyrics = true;
    bool collapsedLyrics = false;
    bool lyricsPinnedFirst = false;
    bool lyricsCache = true;
    int lyricsCacheMaxMB = 25;
    bool mediaTabLyrics = false;
    int lyricsOffsetMs = 0, lyricsOffsetBtMs = 0;
    bool quickLookup = true;
    UINT lookupModifiers = MOD_ALT;
    UINT lookupVk = VK_SPACE;
