#pragma once

inline int GetDaysInMonth(int year, int month) {
    if (month==2) return (year%4==0 && year%100!=0) || year%400==0 ? 29 : 28;
    return month==4 || month==6 || month==9 || month==11 ? 30 : 31;
}

inline int GetDayOfWeek(int year, int month, int day) {
    if (month<3) {month+=12; --year;}
    const int k=year%100,j=year/100;
    return ((day+13*(month+1)/5+k+k/4+j/4+5*j)%7+6)%7;
}

inline float CalendarContentOffset(float width) {
    // The date card and month grid occupy x=22..330 in the original layout.
    return (std::max)(0.0f,(width-352.0f)*0.5f);
}

struct CalendarGrid {
    int first, days, rows;
    float left=134, top=41, col=28, row;
    CalendarGrid(int year,int month):first(GetDayOfWeek(year,month,1)),days(GetDaysInMonth(year,month)),
        rows((first+days+6)/7),row(129.0f/rows) {}
    D2D1_RECT_F Cell(int day) const {
        const int slot=first+day-1;
        return D2D1::RectF(left+(slot%7)*col,top+(slot/7)*row,left+(slot%7+1)*col,top+(slot/7+1)*row);
    }
    int Hit(float x,float y) const {
        if (x<left || x>=left+7*col || y<top || y>=170) return 0;
        const int day=static_cast<int>((y-top)/row)*7+static_cast<int>((x-left)/col)-first+1;
        return day>=1 && day<=days ? day : 0;
    }
};

inline unsigned CalendarDayKinds(const std::vector<ImportantDay>& days,int date) {
    unsigned mask=0;
    for (const auto& day:days) if (day.date==date) mask |= day.kind==1 ? 1u : day.kind==2 ? 2u : 4u;
    return mask;
}

inline std::wstring CalendarDayText(const std::vector<ImportantDay>& days,int date) {
    std::wstring text;
    for (const auto& day:days) if (day.date==date) {
        if (!text.empty()) text+=L"\n";
        if (day.title!=ImportantKindName(day.kind)) text+=std::wstring(ImportantKindName(day.kind))+L" · ";
        text+=day.title;
    }
    return text;
}
