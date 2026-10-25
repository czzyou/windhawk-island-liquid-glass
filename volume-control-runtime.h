std::mutex g_endpointVolumeMutex;
std::atomic<bool> g_volumeDragging{false};
std::mutex g_volumeHitMutex;
D2D1_RECT_F g_volumeHitBounds{};
float g_volumeLogicalWidth=0, g_volumeLogicalHeight=0;
bool g_endpointVolumeKnown=false;

ComPtr<IAudioEndpointVolume> OpenCurrentVolumeEndpoint() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioEndpointVolume> volume;
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
    if (SUCCEEDED(hr)) hr=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device);
    if (SUCCEEDED(hr)) hr=device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,
        reinterpret_cast<void**>(volume.GetAddressOf()));
    return volume;
}

bool PublishCurrentVolume(IAudioEndpointVolume* endpoint, bool show) {
    float level=0;
    BOOL muted=FALSE;
    if (!endpoint || FAILED(endpoint->GetMasterVolumeLevelScalar(&level)) || FAILED(endpoint->GetMute(&muted))) return false;
    const int percent=ClampInt(static_cast<int>(std::lround(level*100)),0,100);
    const auto settings=GetSettingsCopy();
    bool changed=false, wake=false;
    const double now=NowSeconds();
    {
        std::lock_guard lock(g_stateMutex);
        changed=g_endpointVolumeKnown && (percent!=g_state.system.volumePercent || (muted!=FALSE)!=g_state.system.volumeMuted);
        g_state.system.volumePercent=percent;
        g_state.system.volumeMuted=muted!=FALSE;
        g_state.muted=muted!=FALSE;
        g_state.volume.percent=percent;
        g_state.volume.muted=muted!=FALSE;
        g_state.volume.deviceName=L"系统音量";
        if (settings.volume && (show || changed || g_volumeDragging.load())) {
            wake=!g_state.volume.active || now>=g_state.volume.expiresAt || g_autoHiddenParked.load();
            g_state.volume.active=true;
            g_state.volume.expiresAt=now+1.0;
        }
    }
    if (changed || !g_endpointVolumeKnown || show) {
        Wh_SetIntValue(L"VolumePercent",percent);
        Wh_SetIntValue(L"VolumeMuted",muted!=FALSE);
    }
    g_endpointVolumeKnown=true;
    if (changed || show) {
        g_layoutDirty=true;
        if (settings.volume && wake) TriggerNudge();
    }
    return true;
}

void ShowIslandVolume() {
    std::lock_guard lock(g_endpointVolumeMutex);
    const auto endpoint=OpenCurrentVolumeEndpoint();
    PublishCurrentVolume(endpoint.Get(),true);
}

void SetIslandVolume(int percent) {
    if (percent<0 || percent>100) return;
    std::lock_guard lock(g_endpointVolumeMutex);
    const auto endpoint=OpenCurrentVolumeEndpoint();
    if (!endpoint) return;
    if (FAILED(endpoint->SetMasterVolumeLevelScalar(percent/100.0f,nullptr))) return;
    if (percent>0) endpoint->SetMute(FALSE,nullptr);
    PublishCurrentVolume(endpoint.Get(),true);
}

class VolumeChangeCallback final : public IAudioEndpointVolumeCallback {
    std::atomic<ULONG> refs_{1};
    HANDLE changed_;
public:
    explicit VolumeChangeCallback(HANDLE changed) : changed_(changed) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out=nullptr;
        if (iid==__uuidof(IUnknown) || iid==__uuidof(IAudioEndpointVolumeCallback)) {
            *out=static_cast<IAudioEndpointVolumeCallback*>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG refs=--refs_; if (!refs) delete this; return refs;
    }
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) override {
        if (data) SetEvent(changed_);
        return S_OK;
    }
};

DWORD WINAPI VolumeControlThreadProc(void*) {
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IAudioEndpointVolume> watched;
        ComPtr<VolumeChangeCallback> callback;
        callback.Attach(new VolumeChangeCallback(g_volumeChangedEvent));
        std::wstring deviceId;
        bool previousAvailable=false, subscribed=false;
        Wh_SetStringValue(L"VolumeEndpointStatus",L"starting");
        HANDLE events[]={g_stopEvent,g_volumeChangedEvent};
        while (WaitForSingleObject(g_stopEvent,0)==WAIT_TIMEOUT) {
            bool available=false;
            {
                std::lock_guard lock(g_endpointVolumeMutex);
                if (!enumerator) CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
                ComPtr<IMMDevice> device;
                std::wstring nextId;
                if (enumerator && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender,eConsole,&device))) {
                    LPWSTR id=nullptr;
                    if (SUCCEEDED(device->GetId(&id))) { nextId=id; CoTaskMemFree(id); }
                }
                if (nextId!=deviceId || !watched) {
                    UpdateLyricsOutputDevice(device.Get());
                    if (watched && subscribed) watched->UnregisterControlChangeNotify(callback.Get());
                    watched.Reset(); subscribed=false; deviceId=nextId;
                    if (device && !nextId.empty() && SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,
                            reinterpret_cast<void**>(watched.GetAddressOf())))) {
                        subscribed=SUCCEEDED(watched->RegisterControlChangeNotify(callback.Get()));
                    }
                }
                available=PublishCurrentVolume(watched.Get(),false);
                if (!available && watched) {
                    if (subscribed) watched->UnregisterControlChangeNotify(callback.Get());
                    watched.Reset(); subscribed=false;
                }
            }
            if (available!=previousAvailable) {
                Wh_SetStringValue(L"VolumeEndpointStatus",available ? L"event-driven default output" : L"output unavailable");
                previousAvailable=available;
            }
            // Device rebinding is checked periodically; volume changes wake immediately.
            if (WaitForMultipleObjects(2,events,FALSE,subscribed ? 1000 : 250)==WAIT_OBJECT_0) break;
        }
        if (watched && subscribed) watched->UnregisterControlChangeNotify(callback.Get());
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return 0;
}

void PublishVolumeGeometry(D2D1_RECT_F rect, const D2D1_MATRIX_3X2_F& m) {
    std::lock_guard lock(g_volumeHitMutex);
    g_volumeHitBounds={rect.left*m._11+rect.top*m._21+m._31,rect.left*m._12+rect.top*m._22+m._32,
                      rect.right*m._11+rect.bottom*m._21+m._31,rect.right*m._12+rect.bottom*m._22+m._32};
    g_volumeLogicalWidth=rect.right-rect.left;
    g_volumeLogicalHeight=rect.bottom-rect.top;
}
MediaContentPoint VolumePointFromClient(int x,int y) {
    std::lock_guard lock(g_volumeHitMutex);
    const float width=g_volumeHitBounds.right-g_volumeHitBounds.left;
    const float height=g_volumeHitBounds.bottom-g_volumeHitBounds.top;
    if (width<=0 || height<=0) return {};
    return {true,(x-g_volumeHitBounds.left)*g_volumeLogicalWidth/width,
                 (y-g_volumeHitBounds.top)*g_volumeLogicalHeight/height,g_volumeLogicalWidth,g_volumeLogicalHeight};
}
