    void DrawCalendarExam(D2D1_POINT_2F center,float radius) {
        if (!calendarExamRing_) {
            ComPtr<ID2D1GeometrySink> sink;
            if (FAILED(d2dFactory_->CreatePathGeometry(&calendarExamRing_)) ||
                FAILED(calendarExamRing_->Open(&sink))) return;
            for (int i=0;i<=48;++i) {
                const float a=0.22f+i*(5.76f/48);
                const float wobble=1.0f+0.035f*std::sin(a*3.0f+0.5f);
                const auto p=D2D1::Point2F(std::cos(a)*wobble,std::sin(a)*wobble*0.92f);
                if (!i) sink->BeginFigure(p,D2D1_FIGURE_BEGIN_HOLLOW); else sink->AddLine(p);
            }
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            if (FAILED(sink->Close())) {calendarExamRing_.Reset(); return;}
        }
        D2D1_MATRIX_3X2_F previous; target_->GetTransform(&previous);
        target_->SetTransform(D2D1::Matrix3x2F::Scale(radius,radius)*
            D2D1::Matrix3x2F::Translation(center.x,center.y)*previous);
        textBrush_->SetOpacity(0.86f);
        target_->DrawGeometry(calendarExamRing_.Get(),textBrush_.Get(),1.15f/radius);
        target_->SetTransform(previous);
    }

    void DrawCalendarCake(D2D1_POINT_2F c,float size,ID2D1Brush* outline) {
        if (!calendarCakeBrush_) {
            D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(0xFFFFFF)},{1,D2D1::ColorF(0xC6CFDF)}};
            ComPtr<ID2D1GradientStopCollection> colors;
            if (SUCCEEDED(target_->CreateGradientStopCollection(stops,2,&colors)))
                target_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(c,c),colors.Get(),&calendarCakeBrush_);
        }
        const float u=size/18;
        const auto body=D2D1::RoundedRect(D2D1::RectF(c.x-7.5f*u,c.y-2*u,c.x+7.5f*u,c.y+7*u),2*u,2*u);
        textBrush_->SetOpacity(0.98f);
        if (calendarCakeBrush_) {
            calendarCakeBrush_->SetStartPoint(D2D1::Point2F(c.x,c.y-2*u));
            calendarCakeBrush_->SetEndPoint(D2D1::Point2F(c.x,c.y+7*u));
            calendarCakeBrush_->SetOpacity(settingsOpacity_);
        }
        target_->FillRoundedRectangle(body,calendarCakeBrush_ ? static_cast<ID2D1Brush*>(calendarCakeBrush_.Get()) : textBrush_.Get());
        if (outline) target_->DrawRoundedRectangle(body,outline,0.65f*u);
        target_->DrawLine(D2D1::Point2F(c.x-8.5f*u,c.y+8*u),D2D1::Point2F(c.x+8.5f*u,c.y+8*u),textBrush_.Get(),1.2f*u);
        ComPtr<ID2D1SolidColorBrush> warm;
        target_->CreateSolidColorBrush(D2D1::ColorF(0xFFD16C,settingsOpacity_),&warm);
        for (int side:{-1,1}) {
            const float x=c.x+side*3.0f*u;
            target_->DrawLine(D2D1::Point2F(x,c.y-2*u),D2D1::Point2F(x,c.y-5*u),textBrush_.Get(),1.1f*u);
            if (warm) target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x,c.y-7*u),1.2f*u,1.7f*u),warm.Get());
        }
    }

    void DrawCalendarHint(const SharedState& state,D2D1_RECT_F rect,const Settings& settings,float scale,SYSTEMTIME local) {
        const float opacity=g_calendarHintOpacity.load();
        const int hovered=g_hoveredCalendarDate.load();
        if (hovered) calendarHintDate_=hovered;
        if (opacity<0.005f || calendarHintDate_/100!=BriefDateKey(local)/100 || !smallTextFormat_) return;
        const auto body=CalendarDayText(state.importantDays,calendarHintDate_);
        if (body.empty()) return;
        const float width=(rect.right-rect.left)/scale-68;
        const float font=smallTextFormat_->GetFontSize()+settings.textScale;
        if (!calendarHintLayout_ || calendarHintText_!=body || calendarHintWidth_!=width || calendarHintFont_!=font) {
            calendarHintLayout_.Reset();
            if (FAILED(dwriteFactory_->CreateTextLayout(body.c_str(),static_cast<UINT32>(body.size()),
                    smallTextFormat_.Get(),width,2000,&calendarHintLayout_))) return;
            calendarHintLayout_->SetFontFamilyName(L"Microsoft YaHei UI",DWRITE_TEXT_RANGE{0,static_cast<UINT32>(body.size())});
            calendarHintLayout_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            calendarHintLayout_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            calendarHintLayout_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            calendarHintLayout_->SetFontSize(font,DWRITE_TEXT_RANGE{0,static_cast<UINT32>(body.size())});
            calendarHintText_=body; calendarHintWidth_=width; calendarHintFont_=font;
            DWRITE_TEXT_METRICS metrics{}; calendarHintLayout_->GetMetrics(&metrics);
            const float height=std::max(66.0f,metrics.height+47.0f);
            if (g_calendarHintHeight.exchange(height)!=height) g_layoutDirty=true;
        }
        const float y=rect.top+(194+(1-opacity)*5)*scale;
        const auto card=D2D1::RectF(rect.left+22*scale,y,rect.right-22*scale,
                                  rect.top+(184+g_calendarHintHeight.load()-10)*scale);
        ComPtr<ID2D1SolidColorBrush> fill,rim;
        target_->CreateSolidColorBrush(D2D1::ColorF(0x111116,0.48f*opacity*settingsOpacity_),&fill);
        target_->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF,0.11f*opacity*settingsOpacity_),&rim);
        const auto rounded=D2D1::RoundedRect(card,12*scale,12*scale);
        if (fill) target_->FillRoundedRectangle(rounded,fill.Get());
        if (rim) target_->DrawRoundedRectangle(rounded,rim.Get(),0.6f*scale);
        wchar_t date[32]{}; swprintf_s(date,L"%d 月 %d 日",(calendarHintDate_/100)%100,calendarHintDate_%100);
        const auto align=smallTextFormat_->GetTextAlignment();
        const auto para=smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        textBrush_->SetOpacity(0.68f*opacity);
        target_->DrawTextW(date,static_cast<UINT32>(wcslen(date)),smallTextFormat_.Get(),
            D2D1::RectF(card.left+12*scale,card.top+5*scale,card.right-12*scale,card.top+24*scale),textBrush_.Get());
        smallTextFormat_->SetTextAlignment(align); smallTextFormat_->SetParagraphAlignment(para);
        textBrush_->SetOpacity(0.96f*opacity);
        target_->DrawTextLayout(D2D1::Point2F(card.left+12*scale,card.top+25*scale),calendarHintLayout_.Get(),textBrush_.Get());
        textBrush_->SetOpacity(0.96f);
    }
