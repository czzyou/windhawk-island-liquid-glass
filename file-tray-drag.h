bool FileTrayDragThreshold(POINT start, POINT now, int width, int height) {
    return std::abs(now.x - start.x) >= std::max(1, width) ||
           std::abs(now.y - start.y) >= std::max(1, height);
}

constexpr float kFileTrayRemoveWidth = 28.0f;

bool FileTrayRemoveHit(float x, float rowRight) {
    return x >= rowRight - kFileTrayRemoveWidth && x <= rowRight;
}

class FileTrayDropSource final : public IDropSource {
public:
    explicit FileTrayDropSource(HANDLE stop) : stop_(stop) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(IDropSource)) return E_NOINTERFACE;
        *out = static_cast<IDropSource*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --refs_;
        if (!remaining) delete this;
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape || (stop_ && WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0)) return DRAGDROP_S_CANCEL;
        return (keys & MK_LBUTTON) ? S_OK : DRAGDROP_S_DROP;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD effect) override {
        // Avoid Shell's automatic drag image over the glass panel.
        SetCursor(LoadCursor(nullptr, effect == DROPEFFECT_NONE ? IDC_NO : IDC_ARROW));
        return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};
    HANDLE stop_;
};

void DragFileTrayItem(HWND hwnd, const std::wstring& path, HANDLE stop) {
    const HRESULT initialized = OleInitialize(nullptr);
    if (FAILED(initialized)) return;
    PIDLIST_ABSOLUTE item = ILCreateFromPathW(path.c_str());
    if (item) {
        IShellFolder* parent = nullptr;
        PCUITEMID_CHILD child = nullptr;
        if (SUCCEEDED(SHBindToParent(item, __uuidof(IShellFolder),
                                    reinterpret_cast<void**>(&parent), &child))) {
            IDataObject* data = nullptr;
            if (SUCCEEDED(parent->GetUIObjectOf(hwnd, 1, &child, __uuidof(IDataObject),
                                               nullptr, reinterpret_cast<void**>(&data)))) {
                DWORD effect = DROPEFFECT_NONE;
                // Copy only: dragging a shortcut from the shelf must not move the original.
                auto* source = new FileTrayDropSource(stop);
                DoDragDrop(data, source, DROPEFFECT_COPY, &effect);
                source->Release();
                data->Release();
            }
            parent->Release();
        }
        CoTaskMemFree(item);
    }
    OleUninitialize();
}
