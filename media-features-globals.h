std::atomic<bool> g_lyricsTabAvailable{false}, g_defaultOutputIsBluetooth{false};
std::atomic<int> g_lyricsEffectiveOffsetMs{0}, g_lyricsEffectiveOffsetBtMs{0};
std::atomic<double> g_lastLyricsViewTime{0};
constexpr int kLyricsOffsetLimitMs = 3000;
double LyricsLookaheadSec() {
    return 0.49 + (g_defaultOutputIsBluetooth.load() ? g_lyricsEffectiveOffsetBtMs.load()
                                                    : g_lyricsEffectiveOffsetMs.load()) / 1000.0;
}
float SmoothStep01(float x) { x = std::clamp(x, 0.0f, 1.0f); return x*x*(3.0f-2.0f*x); }
std::wstring MediaTrackIdentity(const MediaSnapshot& m) {
    return m.sourceAppUserModelId + L"\x1f" + m.title + L"\x1f" + m.artist + L"\x1f" +
           m.albumTitle + L"\x1f" + std::to_wstring(m.endTicks);
}

std::mutex g_mediaSourceMutex;
std::wstring g_preferredMediaSource, g_dockOverrideAumid;
std::atomic<unsigned> g_dockOverrideRev{0};
HANDLE g_mediaRefreshEvent = nullptr, g_lyricsThread = nullptr, g_lyricsChangedEvent = nullptr;
HANDLE g_lookupThread = nullptr, g_lookupJobEvent = nullptr;
constexpr int ID_LOOKUP_HOTKEY = 2;
constexpr UINT kMediaSourceMenuBase = 1000, kMediaSourceMenuLimit = 2000;
std::mutex g_lookupJobMutex;
struct LookupJob { uint64_t id = 0; std::wstring query, key; };
LookupJob g_lookupJob;

std::wstring GetPreferredMediaSource() {
    std::lock_guard lock(g_mediaSourceMutex); return g_preferredMediaSource;
}
void RequestMediaSource(const std::wstring& source) {
    {
        std::lock_guard lock(g_mediaSourceMutex);
        g_dockOverrideAumid = source;
        ++g_dockOverrideRev;
    }
    if (g_mediaRefreshEvent) SetEvent(g_mediaRefreshEvent);
    g_layoutDirty = true;
}
void LockMediaSource(const std::wstring& source) {
    {
        std::lock_guard lock(g_mediaSourceMutex);
        g_preferredMediaSource = source;
        g_dockOverrideAumid.clear();
        ++g_dockOverrideRev;
    }
    if (g_mediaRefreshEvent) SetEvent(g_mediaRefreshEvent);
    g_layoutDirty = true;
}

int MediaSourceDockHit(const MediaContentPoint& point, int count) {
    if (!point.valid || point.height < 140 || point.y < 170 || point.y > 184 || count < 2) return -1;
    const int slots = (std::min)(count, 5);
    const float left = point.width * 0.5f - slots * 21.0f;
    if (point.x < left || point.x >= left + slots * 42.0f) return -1;
    return static_cast<int>((point.x - left) / 42.0f);
}

void UpdateLyricsOutputDevice(IMMDevice* device) {
    bool bluetooth = false;
    ComPtr<IPropertyStore> store;
    if (device && SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
        const PROPERTYKEY key = {{0xA45C254E,0xDF1C,0x4EFD,{0x80,0x20,0x67,0xD1,0x46,0xA8,0x50,0xE0}},24};
        PROPVARIANT value{};
        if (SUCCEEDED(store->GetValue(key, &value)))
            bluetooth = value.vt == VT_LPWSTR && value.pwszVal && _wcsnicmp(value.pwszVal,L"BTH",3)==0;
        PropVariantClear(&value);
    }
    g_defaultOutputIsBluetooth = bluetooth;
}
