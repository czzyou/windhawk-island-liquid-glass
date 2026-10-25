// Apple Music disables SMTC seeking; use its own accessible transport slider.
bool IsAppleMusicSource(const std::wstring& source) {
    return source == L"AppleInc.AppleMusicWin_nzyj5cx40ttqa!App";
}
struct AppleMusicWindowSearch { const std::wstring* source; HWND window=nullptr; };
BOOL CALLBACK FindAppleMusicWindow(HWND window, LPARAM data) {
    auto& search=*reinterpret_cast<AppleMusicWindowSearch*>(data);
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if (!process) return TRUE;
    wchar_t id[512]{}; UINT32 size=512;
    const LONG result=GetApplicationUserModelId(process,&size,id);
    CloseHandle(process);
    if (result==ERROR_SUCCESS && *search.source==id && IsWindowVisible(window) && !GetWindow(window,GW_OWNER)) {
        search.window=window; return FALSE;
    }
    return TRUE;
}
Microsoft::WRL::ComPtr<IUIAutomationRangeValuePattern> FindAppleMusicScrubber(const std::wstring& source) {
    Microsoft::WRL::ComPtr<IUIAutomationRangeValuePattern> range;
    if (!IsAppleMusicSource(source)) return range;
    AppleMusicWindowSearch search{&source};
    EnumWindows(FindAppleMusicWindow,reinterpret_cast<LPARAM>(&search));
    if (!search.window) return range;
    Microsoft::WRL::ComPtr<IUIAutomation2> automation;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation8),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))) return range;
    automation->put_AutoSetFocus(FALSE);
    automation->put_ConnectionTimeout(500);
    automation->put_TransactionTimeout(1000);
    Microsoft::WRL::ComPtr<IUIAutomationElement> window, slider;
    if (FAILED(automation->ElementFromHandle(search.window,&window))) return range;
    VARIANT id{}; id.vt=VT_BSTR; id.bstrVal=SysAllocString(L"LCDScrubber");
    if (!id.bstrVal) return range;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
    HRESULT result=automation->CreatePropertyCondition(UIA_AutomationIdPropertyId,id,&condition);
    VariantClear(&id);
    if (FAILED(result) || FAILED(window->FindFirst(TreeScope_Descendants,condition.Get(),&slider)) || !slider) return range;
    slider->GetCurrentPatternAs(UIA_RangeValuePatternId,IID_PPV_ARGS(&range));
    return range;
}
bool SeekAppleMusic(const std::wstring& source, int64_t ticks) {
    auto range=FindAppleMusicScrubber(source);
    BOOL readOnly=TRUE; double minimum=0,maximum=0;
    if (!range || FAILED(range->get_CurrentIsReadOnly(&readOnly)) || readOnly ||
        FAILED(range->get_CurrentMinimum(&minimum)) || FAILED(range->get_CurrentMaximum(&maximum)) ||
        !std::isfinite(minimum) || !std::isfinite(maximum) || maximum<=minimum) return false;
    return SUCCEEDED(range->SetValue(std::clamp(ticks/10000000.0,minimum,maximum)));
}
