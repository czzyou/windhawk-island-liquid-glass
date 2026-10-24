    void DrawTomorrowBrief(const TomorrowBrief& brief, D2D1_RECT_F rect) {
        auto draw = [&](const std::wstring& text, IDWriteTextFormat* fmt, float x, float y,
                        float width, float height, float opacity, bool centered = false, bool wrap = false) {
            if (!fmt) return;
            const auto align = fmt->GetTextAlignment();
            const auto para = fmt->GetParagraphAlignment();
            const auto wrapping = fmt->GetWordWrapping();
            if (wrap) fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            fmt->SetTextAlignment(centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            textBrush_->SetOpacity(opacity);
            target_->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), fmt,
                D2D1::RectF(rect.left+x, rect.top+y, rect.left+x+width, rect.top+y+height),
                textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            fmt->SetTextAlignment(align); fmt->SetParagraphAlignment(para);
            fmt->SetWordWrapping(wrapping);
        };
        draw(brief.title, smallTextFormat_.Get(), 26, 14, 288, 22, 0.70f);
        if (brief.important) {
            textBrush_->SetOpacity(0.86f);
            const auto box=D2D1::RectF(rect.left+30,rect.top+59,rect.left+80,rect.top+103);
            if (brief.body.size()<=12) {
                target_->DrawRoundedRectangle(D2D1::RoundedRect(box,5,5),textBrush_.Get(),1.8f);
                target_->DrawLine(D2D1::Point2F(box.left,box.top+12),D2D1::Point2F(box.right,box.top+12),textBrush_.Get(),1.5f);
            }
            draw(brief.body,brief.body.size()>6 ? textFormat_.Get() : timerHeroFormat_.Get(),
                 brief.body.size()>12 ? 26 : 101,47,brief.body.size()>12 ? 288 : 213,64,0.98f,false,true);
            draw(brief.detail,smallTextFormat_.Get(),26,117,288,20,0.62f);
        } else if (brief.meal) {
            textBrush_->SetOpacity(0.86f);
            const auto center=D2D1::Point2F(rect.left+55,rect.top+79);
            target_->DrawEllipse(D2D1::Ellipse(center,19,19),textBrush_.Get(),1.8f);
            target_->DrawEllipse(D2D1::Ellipse(center,13,13),textBrush_.Get(),1.2f);
            for (int i=0;i<3;++i) {
                const float x=rect.left+26+i*3.0f;
                target_->DrawLine(D2D1::Point2F(x,rect.top+61),D2D1::Point2F(x,rect.top+74),textBrush_.Get(),1.5f);
            }
            target_->DrawLine(D2D1::Point2F(rect.left+29,rect.top+73),D2D1::Point2F(rect.left+29,rect.top+99),textBrush_.Get(),2.0f);
            target_->DrawLine(D2D1::Point2F(rect.left+82,rect.top+61),D2D1::Point2F(rect.left+82,rect.top+99),textBrush_.Get(),2.5f);
            draw(brief.body,timerHeroFormat_.Get(),105,53,209,48,0.98f);
            draw(brief.detail,smallTextFormat_.Get(),26,117,288,20,0.62f);
        } else if (brief.weatherCode) {
            const auto split = brief.body.find(L"  ");
            const auto condition = brief.body.substr(0, split);
            const auto range = split == std::wstring::npos ? brief.body : brief.body.substr(split+2);
            DrawWeatherIcon(D2D1::Point2F(rect.left+53, rect.top+76), 54,
                WeatherVisualFromCode(brief.weatherCode), textBrush_.Get(), textBrush_.Get());
            draw(range, timerHeroFormat_.Get(), 101, 43, 213, 46, 0.98f);
            draw(condition, textFormat_.Get(), 103, 87, 210, 25, 0.82f);
            draw(brief.detail, smallTextFormat_.Get(), 26, 117, 288, 20, 0.62f);
        } else {
            const bool single = brief.games.size() == 1;
            for (size_t row = 0; row < brief.games.size(); ++row) {
                const auto& game = brief.games[row];
                const float cy = single ? 76.0f : 62.0f + row*49.0f;
                const float radius = single ? 24.0f : 17.0f;
                const std::wstring urls[] = {game.awayLogoUrl,game.homeLogoUrl};
                const std::wstring labels[] = {game.awayAbbr,game.homeAbbr};
                for (int side=0;side<2;++side) {
                    const size_t index = row*2+side;
                    BitmapPixels pixels;
                    {
                        std::lock_guard lock(g_nbaLogoMutex);
                        if (g_briefLogos[index].url == urls[side]) pixels=g_briefLogos[index].pixels;
                    }
                    const float cx = single ? (side == 0 ? 69.0f : 271.0f) : (side == 0 ? 43.0f : 297.0f);
                    DrawBitmapPixels(pixels, D2D1::RectF(rect.left+cx-radius,rect.top+cy-radius,
                        rect.left+cx+radius,rect.top+cy+radius), briefLogoBitmaps_[index],
                        briefLogoGenerations_[index], 0.98f);
                    if (single) draw(labels[side], boldTextFormat_.Get(), cx-30, cy+radius+2, 60, 19, 0.86f, true);
                    else draw(labels[side], boldTextFormat_.Get(), side == 0 ? 68 : 220, cy-12, 52, 24, 0.86f, true);
                }
                const time_t start = static_cast<time_t>(game.startTime);
                tm local{};
                wchar_t time[16]{};
                if (localtime_s(&local,&start)==0) swprintf_s(time,L"%02d:%02d",local.tm_hour,local.tm_min);
                draw(time, single ? timerHeroFormat_.Get() : textFormat_.Get(), 120, cy-22, 100, 38, 0.98f, true);
                if (single) draw(L"本地时间", smallTextFormat_.Get(), 120, cy+17, 100, 18, 0.55f, true);
            }
        }
        textBrush_->SetOpacity(0.96f);
    }
