from pathlib import Path


def apply_calendar_markers(src):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    start = src.index('    static int GetDaysInMonth(')
    end = src.index('    void DrawCalendarDashboard(', start)
    src = src[:start] + src[end:]
    change('DWORD WINAPI AudioThreadProc(', (root/'calendar-days.h').read_text(encoding='utf-8').replace('#pragma once', '') + '\nDWORD WINAPI AudioThreadProc(')
    change('std::atomic<int> g_hoveredQuickAction{-1};', '''std::atomic<int> g_hoveredQuickAction{-1};
std::atomic<int> g_hoveredCalendarDate{0};
std::atomic<float> g_calendarHintOpacity{0}, g_calendarHintHeight{66};''')
    change('    ComPtr<IDWriteTextFormat> hourlyTimeFormat_;', '''    ComPtr<IDWriteTextFormat> hourlyTimeFormat_;
    ComPtr<ID2D1PathGeometry> calendarExamRing_;
    ComPtr<ID2D1LinearGradientBrush> calendarCakeBrush_;
    ComPtr<IDWriteTextLayout> calendarHintLayout_;
    std::wstring calendarHintText_;
    float calendarHintWidth_=0,calendarHintFont_=0;
    int calendarHintDate_=0;''')
    change('        for (auto& bitmap : briefLogoBitmaps_) bitmap.Reset();', '''        calendarExamRing_.Reset(); calendarCakeBrush_.Reset(); calendarHintLayout_.Reset();
        for (auto& bitmap : briefLogoBitmaps_) bitmap.Reset();''')
    change('    void DrawCalendarDashboard(', (root/'calendar-markers-draw.h').read_text(encoding='utf-8') + '\n    void DrawCalendarDashboard(')
    start = src.index('    void DrawCalendarDashboard(')
    end = src.index('    // Fixes windhawk-mods#4352', start)
    body = src[start:end]
    body = body.replace('        (void)state;', '''        static int lastCount=-1;
        if (lastCount!=static_cast<int>(state.importantDays.size())) {
            lastCount=static_cast<int>(state.importantDays.size());
            Wh_SetIntValue(L"CalendarDaysRendered",lastCount);
        }
        const auto panel=rect;
        rect.bottom=rect.top+184.0f*scale;
        const CalendarGrid geometry(local.wYear,local.wMonth);''')
    a = body.index('            const D2D1_RECT_F cell =', body.index('for (int d = 1;'))
    b = body.index('            const std::wstring dayText', a)
    body = body[:a] + '''            const auto logical=geometry.Cell(d);
            const auto cell=D2D1::RectF(rect.left+logical.left*scale,rect.top+logical.top*scale,
                                      rect.left+logical.right*scale,rect.top+logical.bottom*scale);
            const int date=local.wYear*10000+local.wMonth*100+d;
            const unsigned kinds=CalendarDayKinds(state.importantDays,date);
            const auto center=D2D1::Point2F((cell.left+cell.right)*0.5f,(cell.top+cell.bottom)*0.5f);
            if (kinds&2) {
                if (d==local.wDay && accent) target_->FillEllipse(
                    D2D1::Ellipse(center,markerR,markerR),accent.Get());
                DrawCalendarCake(center,18.0f*scale,selectedText.Get());
                if (kinds&1) DrawCalendarExam(center,markerR+0.3f*scale);
                if (++col>6) {col=0;++row;}
                continue;
            }
''' + body[b:]
    body = body.replace('                                  markerR, markerR),', '''                                  markerR-((kinds&1) ? 1.8f*scale : 0),
                                  markerR-((kinds&1) ? 1.8f*scale : 0)),''')
    body = body.replace('            if (++col > 6)', '''            if (kinds&1) DrawCalendarExam(center,markerR+0.3f*scale);
            if (kinds&4) {
                textBrush_->SetOpacity(0.72f);
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x,center.y+9.5f*scale),1.3f*scale,1.3f*scale),
                    d==local.wDay && selectedText ? selectedText.Get() : textBrush_.Get());
            }
            if (++col > 6)''')
    body = body.replace('        mutedBrush_->SetOpacity(0.75f);\n    }', '        mutedBrush_->SetOpacity(0.75f);\n        DrawCalendarHint(state,panel,settings,scale,local);\n    }')
    src = src[:start]+body+src[end:]

    change('            int quickHovered = -1;', '            int calendarHovered=0;\n            int quickHovered = -1;')
    change('                if (g_mediaCompanionExpanded.load() && g_settings.media && g_state.media.available) {', '''                if (!kinds.empty() && kinds[0]==IslandKind::Idle && !timerSession &&
                    NormalizedTabIndex(g_settings)==1 && !g_settings.gameOverlay && Wh_GetIntValue(L"GameOverlayPinned",0)==0) {
                    const auto p=MediaContentFromClient(xPos,yPos,true);
                    if (p.valid && p.height>=175 && p.width>=365) {
                        SYSTEMTIME local{}; GetLocalTime(&local);
                        const int day=CalendarGrid(local.wYear,local.wMonth).Hit(p.x,p.y);
                        const int date=local.wYear*10000+local.wMonth*100+day;
                        if (day && CalendarDayKinds(g_state.importantDays,date)) calendarHovered=date;
                    }
                }
                if (g_mediaCompanionExpanded.load() && g_settings.media && g_state.media.available) {''')
    change('            if (g_hoveredQuickAction.exchange(quickHovered) != quickHovered) g_layoutDirty = true;', '''            if (g_hoveredCalendarDate.exchange(calendarHovered)!=calendarHovered) {g_layoutDirty=true; Wh_SetIntValue(L"CalendarHoveredDate",calendarHovered);}
            if (g_hoveredQuickAction.exchange(quickHovered) != quickHovered) g_layoutDirty = true;''')
    change('''        case WM_MOUSELEAVE:
            if (g_hoveredQuickAction.exchange(-1)''', '''        case WM_MOUSELEAVE:
            if (g_hoveredCalendarDate.exchange(0)) g_layoutDirty=true;
            if (g_hoveredQuickAction.exchange(-1)''')
    change('        const float normalIdleWidth = primary.width;', '''        static double calendarHintTime=now;
        const bool calendarVisible=primary.kind==IslandKind::Idle && NormalizedTabIndex(g_settings)==1 &&
            !timerSessionActive && !gameMetricsPresent && !g_settings.gameOverlay &&
            !Wh_GetIntValue(L"GameOverlayPinned",0) && !isFullscreen && (pinned || isHoverExpanded);
        const bool hintWanted=calendarVisible && CalendarDayKinds(snapshot.importantDays,g_hoveredCalendarDate.load());
        const float hintTarget=hintWanted ? 1.0f : 0.0f;
        float hint=g_calendarHintOpacity.load();
        hint+=(hintTarget-hint)*(1.0f-std::exp(-static_cast<float>(now-calendarHintTime)/0.065f));
        calendarHintTime=now;
        if (std::fabs(hint-hintTarget)<0.002f) hint=hintTarget;
        if (g_calendarHintOpacity.exchange(hint)!=hint) needsRender=true;
        if (calendarVisible) primary.height+=g_calendarHintHeight.load()*hint*g_settings.sizeScale;
        const float normalIdleWidth = primary.width;''')
    return src
