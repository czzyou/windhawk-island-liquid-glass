std::atomic<bool> g_importantDaysOpen=false;

bool ReadImportantDays(std::vector<ImportantDay>& days) {
    wchar_t text[16384]{};
    Wh_GetStringValue(L"ImportantDays",text,ARRAYSIZE(text));
    return ParseImportantDays(text,days);
}

struct ImportantDaysDialog {
    std::vector<ImportantDay> days;
    HWND list=nullptr, title=nullptr, date=nullptr, advance=nullptr, kind=nullptr;
};

void RefreshImportantDaysList(ImportantDaysDialog& state) {
    SendMessageW(state.list,LB_RESETCONTENT,0,0);
    for (const auto& day:state.days) {
        const auto label=std::to_wstring(day.date/10000)+L"-"+std::to_wstring((day.date/100)%100)+L"-"+
            std::to_wstring(day.date%100)+L"    【"+ImportantKindName(day.kind)+L"】"+day.title+L"    "+
            (day.advance ? L"提前 "+std::to_wstring(day.advance)+L" 天及当天" : std::wstring(L"仅当天"));
        SendMessageW(state.list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    }
}

INT_PTR CALLBACK ImportantDaysDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state=reinterpret_cast<ImportantDaysDialog*>(GetWindowLongPtrW(hwnd,DWLP_USER));
    if (message==WM_INITDIALOG) {
        state=reinterpret_cast<ImportantDaysDialog*>(lParam);
        SetWindowLongPtrW(hwnd,DWLP_USER,reinterpret_cast<LONG_PTR>(state));
        SetWindowTextW(hwnd,L"重要日子");
        const float scale=GetDpiForWindow(hwnd)/96.0f;
        SetWindowPos(hwnd,nullptr,0,0,static_cast<int>(560*scale),static_cast<int>(470*scale),SWP_NOMOVE|SWP_NOZORDER);
        RECT client{}; GetClientRect(hwnd,&client);
        const int width=static_cast<int>(client.right/scale)-36;
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id) {
            HWND item=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,
                static_cast<int>(x*scale),static_cast<int>(y*scale),static_cast<int>(w*scale),static_cast<int>(h*scale),
                hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
            if (item) SendMessageW(item,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
            return item;
        };
        control(L"STATIC",L"选择日期和提前量；临近日期时主动展开，每个提醒阶段只展示一次。",0,18,14,width,24,0);
        state->list=control(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,18,44,width,126,100);
        control(L"STATIC",L"名称：",0,18,187,56,24,0);
        state->title=control(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,76,183,width-58,28,101);
        SendMessageW(state->title,EM_SETLIMITTEXT,40,0);
        control(L"STATIC",L"日期：",0,18,226,56,24,0);
        state->date=control(DATETIMEPICK_CLASSW,L"",WS_TABSTOP|DTS_SHORTDATEFORMAT,76,222,160,28,102);
        SendMessageW(state->date,DTM_SETFORMATW,0,reinterpret_cast<LPARAM>(L"yyyy-MM-dd"));
        control(L"STATIC",L"提醒：",0,248,226,50,24,0);
        state->advance=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,300,222,width-282,160,103);
        for (const auto* text:{L"仅当天",L"提前 1 天及当天",L"提前 3 天及当天",L"提前 7 天及当天"})
            SendMessageW(state->advance,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        SendMessageW(state->advance,CB_SETCURSEL,1,0);
        control(L"STATIC",L"类型：",0,18,266,56,24,0);
        state->kind=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,76,262,200,140,106);
        for (const auto* text:{L"其他 · 小圆点",L"考试 · 手绘圈",L"生日 · 蛋糕"})
            SendMessageW(state->kind,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
        SendMessageW(state->kind,CB_SETCURSEL,0,0);
        control(L"BUTTON",L"添加",WS_TABSTOP|BS_DEFPUSHBUTTON,18,310,80,32,104);
        control(L"BUTTON",L"保存选中",WS_TABSTOP,110,310,110,32,107);
        control(L"BUTTON",L"删除选中",WS_TABSTOP,232,310,110,32,105);
        control(L"BUTTON",L"完成",WS_TABSTOP,18+width-100,310,100,32,IDCANCEL);
        control(L"STATIC",L"每次添加或删除立即保存。饭点：11:00、17:00，提前 10 分钟提醒。",0,18,357,width,36,0);
        if (!state->list || !state->title || !state->date || !state->advance || !state->kind) {
            MessageBoxW(hwnd,L"无法创建日期管理控件，已有日程未修改。",L"重要日子",MB_OK|MB_ICONERROR);
            EndDialog(hwnd,0); return TRUE;
        }
        RefreshImportantDaysList(*state);
        SetFocus(state->title);
        return FALSE;
    }
    if (message==WM_COMMAND && state) {
        const int id=LOWORD(wParam);
        if (id==IDCANCEL) {EndDialog(hwnd,0); return TRUE;}
        if (id==100 && HIWORD(wParam)==LBN_SELCHANGE) {
            const LRESULT index=SendMessageW(state->list,LB_GETCURSEL,0,0);
            if (index>=0 && static_cast<size_t>(index)<state->days.size()) {
                const auto& day=state->days[index];
                SetWindowTextW(state->title,day.title.c_str());
                auto date=ImportantDate(day.date);
                SendMessageW(state->date,DTM_SETSYSTEMTIME,GDT_VALID,reinterpret_cast<LPARAM>(&date));
                SendMessageW(state->advance,CB_SETCURSEL,day.advance==0 ? 0 : day.advance==1 ? 1 : day.advance==3 ? 2 : 3,0);
                SendMessageW(state->kind,CB_SETCURSEL,day.kind,0);
            }
            return TRUE;
        }
        if (id==104 || id==107 || id==IDOK) {
            wchar_t title[41]{};
            GetWindowTextW(state->title,title,ARRAYSIZE(title));
            ImportantDay day; day.title=title;
            const auto first=day.title.find_first_not_of(L" \t");
            if (first!=std::wstring::npos) day.title=day.title.substr(first,day.title.find_last_not_of(L" \t")-first+1);
            else day.title.clear();
            SYSTEMTIME date{};
            if (SendMessageW(state->date,DTM_GETSYSTEMTIME,0,reinterpret_cast<LPARAM>(&date))!=GDT_VALID) return TRUE;
            day.date=BriefDateKey(date);
            const LRESULT selected=SendMessageW(state->advance,CB_GETCURSEL,0,0);
            const int advances[]={0,1,3,7};
            if (selected<0 || selected>3) return TRUE;
            day.advance=advances[selected];
            day.kind=static_cast<int>(SendMessageW(state->kind,CB_GETCURSEL,0,0));
            const LRESULT editing=id==107 ? SendMessageW(state->list,LB_GETCURSEL,0,0) : -1;
            if (id==107 && (editing<0 || static_cast<size_t>(editing)>=state->days.size())) return TRUE;
            GUID guid{}; wchar_t guidText[40]{};
            if (FAILED(CoCreateGuid(&guid)) || !StringFromGUID2(guid,guidText,ARRAYSIZE(guidText))) return TRUE;
            day.id=id==107 ? state->days[editing].id : guidText;
            if (!ImportantDayValid(day) || (id!=107 && state->days.size()>=32)) {
                MessageBoxW(hwnd,L"请填写有效名称和日期；最多保存 32 个重要日子。",L"重要日子",MB_OK|MB_ICONINFORMATION);
                return TRUE;
            }
            if (std::any_of(state->days.begin(),state->days.end(),[&](const auto& other){return other.id!=day.id && other.date==day.date && other.title==day.title;})) {
                MessageBoxW(hwnd,L"这个名称和日期已经添加。",L"重要日子",MB_OK|MB_ICONINFORMATION); return TRUE;
            }
            auto next=state->days;
            if (id==107) next[editing]=day; else next.push_back(day);
            if (!Wh_SetStringValue(L"ImportantDays",EncodeImportantDays(next).c_str())) {
                MessageBoxW(hwnd,L"保存失败，已有日程未修改。",L"重要日子",MB_OK|MB_ICONERROR); return TRUE;
            }
            state->days=std::move(next); RefreshImportantDaysList(*state);
            SetWindowTextW(state->title,L""); SetFocus(state->title);
            return TRUE;
        }
        if (id==105) {
            const LRESULT selected=SendMessageW(state->list,LB_GETCURSEL,0,0);
            if (selected<0 || static_cast<size_t>(selected)>=state->days.size()) return TRUE;
            auto next=state->days; next.erase(next.begin()+selected);
            if (!Wh_SetStringValue(L"ImportantDays",EncodeImportantDays(next).c_str())) {
                MessageBoxW(hwnd,L"保存失败，日程未删除。",L"重要日子",MB_OK|MB_ICONERROR); return TRUE;
            }
            state->days=std::move(next); RefreshImportantDaysList(*state);
            return TRUE;
        }
    }
    if (message==WM_CLOSE) {EndDialog(hwnd,0); return TRUE;}
    return FALSE;
}

void ShowImportantDays(HWND owner) {
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ImportantDaysDialog state;
    if (!ReadImportantDays(state.days)) {
        MessageBoxW(owner,L"已有日程数据无法读取，已保留原数据。",L"重要日子",MB_OK|MB_ICONERROR);
    } else {
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_DATE_CLASSES};
        if (InitCommonControlsEx(&controls)) {
            std::vector<DWORD> storage((sizeof(DLGTEMPLATE)+6+3)/4);
            auto* dialog=reinterpret_cast<DLGTEMPLATE*>(storage.data());
            dialog->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
            dialog->cx=340; dialog->cy=245;
            g_importantDaysOpen=true;
            const auto result=DialogBoxIndirectParamW(GetModuleHandleW(nullptr),dialog,owner,
                ImportantDaysDialogProc,reinterpret_cast<LPARAM>(&state));
            g_importantDaysOpen=false;
            if (result==-1) MessageBoxW(owner,L"无法打开日期管理窗口，已有日程未修改。",L"重要日子",MB_OK|MB_ICONERROR);
        }
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}
