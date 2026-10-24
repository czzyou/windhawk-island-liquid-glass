#pragma once

struct ImportantDay {
    std::wstring id, title;
    int date = 0, advance = 1;
    int kind = 0;  // 0 other, 1 exam, 2 birthday
    bool operator==(const ImportantDay&) const = default;
};

inline int InferImportantKind(const std::wstring& title) {
    if (title.find(L"生日")!=std::wstring::npos) return 2;
    if (title.find(L"考试")!=std::wstring::npos) return 1;
    return 0;
}

inline const wchar_t* ImportantKindName(int kind) {
    return kind==1 ? L"考试" : kind==2 ? L"生日" : L"其他";
}

inline SYSTEMTIME ImportantDate(int date) {
    SYSTEMTIME day{};
    day.wYear=date/10000; day.wMonth=(date/100)%100; day.wDay=date%100;
    return day;
}

inline bool ImportantDayValid(const ImportantDay& day) {
    if (day.kind<0 || day.kind>2 || day.id.empty() || day.id.size()>64 || day.title.empty() || day.title.size()>40 ||
        day.date<19000101 || day.date>99991231 ||
        (day.advance!=0 && day.advance!=1 && day.advance!=3 && day.advance!=7)) return false;
    if (day.id.find_first_not_of(L"0123456789abcdefABCDEF-{}") != std::wstring::npos) return false;
    if (std::any_of(day.title.begin(),day.title.end(),[](wchar_t c){return c<32;})) return false;
    if (std::all_of(day.title.begin(),day.title.end(),[](wchar_t c){return iswspace(c);})) return false;
    const auto date=ImportantDate(day.date);
    FILETIME file{};
    return SystemTimeToFileTime(&date,&file)!=FALSE;
}

inline bool ParseImportantDays(const std::wstring& text, std::vector<ImportantDay>& result) {
    using namespace winrt::Windows::Data::Json;
    if (text.empty()) {result.clear(); return true;}
    try {
        const auto array=JsonArray::Parse(text);
        if (array.Size()>32) return false;
        std::vector<ImportantDay> days;
        for (const auto& item:array) {
            const auto object=item.GetObject();
            ImportantDay day;
            day.id=object.GetNamedString(L"id").c_str();
            day.title=object.GetNamedString(L"title").c_str();
            const double kind=object.GetNamedNumber(L"kind",InferImportantKind(day.title));
            if (!std::isfinite(kind) || kind<0 || kind>2 || std::floor(kind)!=kind) return false;
            day.kind=static_cast<int>(kind);
            const double date=object.GetNamedNumber(L"date"), advance=object.GetNamedNumber(L"advance");
            if (!std::isfinite(date) || date<19000101 || date>99991231 || std::floor(date)!=date ||
                !std::isfinite(advance) || advance<0 || advance>7 || std::floor(advance)!=advance) return false;
            day.date=static_cast<int>(date); day.advance=static_cast<int>(advance);
            if (!ImportantDayValid(day) || std::any_of(days.begin(),days.end(),[&](const auto& other){return other.id==day.id;})) return false;
            days.push_back(std::move(day));
        }
        result=std::move(days);
        return true;
    } catch (const winrt::hresult_error&) {return false;}
}

inline std::wstring EncodeImportantDays(const std::vector<ImportantDay>& days) {
    using namespace winrt::Windows::Data::Json;
    JsonArray array;
    for (const auto& day:days) {
        JsonObject object;
        object.SetNamedValue(L"id",JsonValue::CreateStringValue(day.id));
        object.SetNamedValue(L"title",JsonValue::CreateStringValue(day.title));
        object.SetNamedValue(L"date",JsonValue::CreateNumberValue(day.date));
        object.SetNamedValue(L"advance",JsonValue::CreateNumberValue(day.advance));
        object.SetNamedValue(L"kind",JsonValue::CreateNumberValue(day.kind));
        array.Append(object);
    }
    return std::wstring(array.Stringify());
}

inline int ImportantDaysAway(int date, const SYSTEMTIME& local) {
    const auto target=ImportantDate(date), today=ImportantDate(BriefDateKey(local));
    FILETIME a{},b{};
    if (!SystemTimeToFileTime(&target,&a) || !SystemTimeToFileTime(&today,&b)) return -1;
    ULARGE_INTEGER x{},y{};
    x.LowPart=a.dwLowDateTime; x.HighPart=a.dwHighDateTime;
    y.LowPart=b.dwLowDateTime; y.HighPart=b.dwHighDateTime;
    return static_cast<int>((static_cast<int64_t>(x.QuadPart)-static_cast<int64_t>(y.QuadPart))/864000000000LL);
}

inline TomorrowBrief MakeImportantReminder(const ImportantDay& day, const SYSTEMTIME& local, int shown) {
    TomorrowBrief result;
    if (!ImportantDayValid(day) || shown==BriefDateKey(local)) return result;
    const int remaining=ImportantDaysAway(day.date,local);
    if (remaining!=0 && remaining!=day.advance) return result;
    result.important=true;
    result.date=BriefDateKey(local);
    result.key=day.id;
    result.title=L"重要日子 · "+(remaining==0 ? std::wstring(L"今天") : remaining==1 ? std::wstring(L"明天") : L"还有 "+std::to_wstring(remaining)+L" 天");
    result.body=day.title;
    wchar_t date[32]{}; swprintf_s(date,L"%04d 年 %02d 月 %02d 日",day.date/10000,(day.date/100)%100,day.date%100);
    result.detail=date;
    return result;
}
