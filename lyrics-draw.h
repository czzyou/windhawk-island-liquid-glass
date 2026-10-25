    int CurrentLyricLine(const SharedState& state, double now) {
        g_lastLyricsViewTime = now;
        if (!state.lyrics.hasData || state.lyrics.lines.empty()) return -1;
        if (!state.lyrics.synced) return 0;
        double position = state.media.positionTicks / 10000.0;
        if (g_scrubbing.load()) position = g_scrubDragFraction.load() * state.media.endTicks / 10000.0;
        else if (state.media.playing && state.media.lastUpdatedTicks > 0)
            position += static_cast<double>(GetTickCount64() - state.media.lastUpdatedTicks) + LyricsLookaheadSec() * 1000;
        const auto& lines = state.lyrics.lines.vec();
        const auto it = std::upper_bound(lines.begin(), lines.end(), position,
            [](double value, const LyricsLine& line) { return value < line.timeMs; });
        return static_cast<int>(it - lines.begin()) - 1;
    }

    void EnsureLyricBlocks(const SharedState& state, const Settings& settings, float width) {
        const float fontSize=18.0f*Clamp(settings.textScale,0.7f,1.6f);
        const auto* data=&state.lyrics.lines.vec();
        if (lyricBlockData_==data && lyricBlockWidth_==width && lyricBlockFont_==fontSize && lyricBlockFamily_==settings.fontFamily) return;
        const auto format=CreateLyricFormat(dwriteFactory_.Get(),settings.fontFamily,fontSize,chineseFontFallback_.Get());
        lyricBlocks_=BuildLyricBlocks(dwriteFactory_.Get(),format.Get(),*data,width,fontSize*1.05f);
        lyricBlockLines_=state.lyrics.lines;
        lyricBlockData_=data; lyricBlockWidth_=width; lyricBlockFont_=fontSize; lyricBlockFamily_=settings.fontFamily;
        lyricDrawAt_=-1;
    }

   public:
    float MeasureCollapsedLyricsWidth(const SharedState& state, const Settings& settings) {
        float widest=-1;
        if (state.lyrics.hasData && !state.lyrics.lines.empty()) {
            if (&collapsedMeasureLines_.vec()!=&state.lyrics.lines.vec() || collapsedMeasureFormat_.Get()!=textFormat_.Get()) {
                collapsedLongestWidth_=MeasureLongestLyricWidth(dwriteFactory_.Get(),textFormat_.Get(),state.lyrics.lines.vec());
                collapsedMeasureLines_=state.lyrics.lines;
                collapsedMeasureFormat_=textFormat_;
            }
            widest=collapsedLongestWidth_;
        }
        const auto work=GetAnchorWorkRect();
        return CollapsedLyricsWidth(widest,(work.right-work.left)/settings.sizeScale-32.0f);
    }

    void DrawCompactLyric(const std::wstring& text, D2D1_RECT_F area, const Settings& settings) {
        const float width=area.right-area.left-16, height=area.bottom-area.top-12;
        const float requested=18.0f*Clamp(settings.textScale,0.7f,1.6f);
        const auto key=text+L"\x1f"+settings.fontFamily;
        if (compactLyricKey_!=key || compactLyricWidth_!=width || compactLyricHeight_!=height || compactLyricFont_!=requested || !compactLyricLayout_) {
            compactLyricLayout_.Reset();
            for (float size=requested;size>=11.0f;size-=1.0f) {
                auto format=CreateLyricFormat(dwriteFactory_.Get(),settings.fontFamily,size,chineseFontFallback_.Get());
                if (!format) return;
                const std::vector<LyricsLine> line{{0,text}};
                auto blocks=BuildLyricBlocks(dwriteFactory_.Get(),format.Get(),line,width,0);
                if (blocks.empty()) return;
                compactLyricLayout_=blocks[0].layout;
                if (blocks[0].height<=height) break;
            }
            compactLyricKey_=key; compactLyricWidth_=width; compactLyricHeight_=height; compactLyricFont_=requested;
        }
        lyricInk_->SetOpacity(1.0f);
        if (compactLyricLayout_) target_->DrawTextLayout(D2D1::Point2F(area.left+8,area.top+6),compactLyricLayout_.Get(),lyricInk_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    void DrawLyricsLines(const SharedState& state, D2D1_RECT_F area, const Settings& settings, double now, bool compact=true) {
        if (!state.media.lyricsEligible) return;
        if (!lyricInk_) target_->CreateSolidColorBrush(D2D1::ColorF(0xF5F5F7),&lyricInk_);
        if (!lyricShade_) target_->CreateSolidColorBrush(D2D1::ColorF(0x252527,0.97f),&lyricShade_);
        if (!lyricInk_ || !lyricShade_) return;
        if (compact) target_->FillRoundedRectangle(D2D1::RoundedRect(area,9,9),lyricShade_.Get());
        const int active=CurrentLyricLine(state,now);
        target_->PushAxisAlignedClip(area,D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto& lyrics=state.lyrics;
        if (!lyrics.hasData || lyrics.lines.empty()) {
            const auto text=lyrics.failed ? L"歌词查询失败，稍后重试" : lyrics.fetching ? L"正在查找歌词…" : lyrics.notFound ? L"未找到匹配歌词" : L"等待歌词…";
            DrawCompactLyric(text,area,settings);
            target_->PopAxisAlignedClip();
            return;
        }
        const int current=ClampInt(active,0,static_cast<int>(lyrics.lines.size())-1);
        if (compact) {
            DrawCompactLyric(lyrics.lines[current].text,area,settings);
            target_->PopAxisAlignedClip();
            return;
        }
        EnsureLyricBlocks(state,settings,area.right-area.left);
        if (lyricBlocks_.empty()) {target_->PopAxisAlignedClip();return;}
        const float desired=lyricBlocks_[current].top-(std::max)(0.0f,(area.bottom-area.top-lyricBlocks_[current].height)*0.5f);
        if (lyricDrawAt_<0 || now-lyricDrawAt_>0.25 || std::fabs(desired-lyricScroll_)>area.bottom-area.top)
            lyricScroll_=desired;
        else lyricScroll_+=(desired-lyricScroll_)*(1-std::exp(-static_cast<float>(now-lyricDrawAt_)/0.18f));
        lyricDrawAt_=now;
        if (std::fabs(desired-lyricScroll_)>0.1f) g_layoutDirty=true;
        for (int i=(std::max)(0,current-1);i<static_cast<int>(lyricBlocks_.size()) && i<=current+1;++i) {
            const auto& block=lyricBlocks_[i];
            const float top=area.top+block.top-lyricScroll_;
            if (top>=area.bottom) break;
            if (top+block.height<=area.top) continue;
            const float alpha=i==active ? 1.0f : i<current ? 0.22f : (std::max)(0.20f,0.62f-0.13f*(i-current-1));
            lyricInk_->SetOpacity(alpha);
            target_->DrawTextLayout(D2D1::Point2F(area.left,top),block.layout.Get(),lyricInk_.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        target_->PopAxisAlignedClip();
    }

    void DrawLyricsDashboard(const SharedState& state, D2D1_RECT_F rect, const Settings& settings, double now, float scale) {
        D2D1_MATRIX_3X2_F previous; target_->GetTransform(&previous);
        target_->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(rect.left,rect.top)*previous);
        const float width=(rect.right-rect.left)/scale, height=(rect.bottom-rect.top)/scale;
        if (!lyricShade_) target_->CreateSolidColorBrush(D2D1::ColorF(0x252527,0.97f),&lyricShade_);
        if (lyricShade_) target_->FillRectangle(D2D1::RectF(0,0,width,height),lyricShade_.Get());
        DrawLyricsLines(state,D2D1::RectF(26,18,width-34,height-18),settings,now,false);
        target_->SetTransform(previous);
    }

    bool DrawCollapsedLyric(const SharedState& state, D2D1_RECT_F rect, const Settings& settings, double now) {
        if (!settings.collapsedLyrics || !state.media.lyricsEligible) return false;
        const int index = CurrentLyricLine(state, now);
        std::wstring line = state.media.title;
        if (state.lyrics.synced && index >= 0) line = state.lyrics.lines[index].text;
        const auto identity=MediaTrackIdentity(state.media);
        if (collapsedLyricIdentity_!=identity || collapsedLyricIndex_!=index || collapsedLyricCache_.text!=line) {
            collapsedLyricIdentity_=identity; collapsedLyricIndex_=index; collapsedLyricStartedAt_=now;
        }
        double duration=4.0;
        if (state.lyrics.synced && index>=0 && index+1<static_cast<int>(state.lyrics.lines.size()))
            duration=(std::max)(0.5,(state.lyrics.lines[index+1].timeMs-state.lyrics.lines[index].timeMs)/1000.0);
        DrawMarqueeText(line, D2D1::RectF(rect.left + 44, rect.top + 5, rect.right - 48, rect.bottom - 5),
                        textFormat_.Get(), textBrush_.Get(), now-collapsedLyricStartedAt_, 38.0f, collapsedLyricCache_, duration);
        return true;
    }

    void DrawMediaSources(const SharedState& state, D2D1_RECT_F rect) {
        const int count = static_cast<int>(state.mediaSources.size());
        if (count < 2) return;
        const int slots = (std::min)(count, 5);
        const float left = (rect.left + rect.right) * 0.5f - slots * 21.0f;
        const auto align = smallTextFormat_->GetTextAlignment();
        const auto para = smallTextFormat_->GetParagraphAlignment();
        smallTextFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        smallTextFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        for (int i=0; i<slots; ++i) {
            const auto& source = state.mediaSources[i];
            const bool selected = source.aumid == state.media.sourceAppUserModelId;
            const bool overflow = i == 4 && count > 5;
            const auto bounds = D2D1::RectF(left+i*42,rect.top+170,left+(i+1)*42,rect.top+184);
            const auto label = overflow ? std::wstring(L"…") : source.badge;
            textBrush_->SetOpacity(selected ? 0.98f : 0.42f);
            auto textBounds=bounds;
            if (!overflow && !source.icon.bgra.empty()) {
                DrawBitmapPixels(source.icon,D2D1::RectF(bounds.left+2,bounds.top+1,bounds.left+14,bounds.bottom-1),
                    sourceDockBitmaps_[source.aumid],sourceDockGenerations_[source.aumid]);
                textBounds.left+=15;
            }
            target_->DrawTextW(label.c_str(), static_cast<UINT32>(label.size()), smallTextFormat_.Get(), textBounds, textBrush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        smallTextFormat_->SetTextAlignment(align); smallTextFormat_->SetParagraphAlignment(para);
        textBrush_->SetOpacity(0.96f);
    }

    ComPtr<ID2D1SolidColorBrush> lyricInk_,lyricShade_;
    std::vector<LyricTextBlock> lyricBlocks_;
    LyricsLines lyricBlockLines_;
    const std::vector<LyricsLine>* lyricBlockData_=nullptr;
    float lyricBlockWidth_=0,lyricBlockFont_=0;
    std::wstring lyricBlockFamily_;
    float lyricScroll_=0;
    double lyricDrawAt_=-1;
    ComPtr<IDWriteTextLayout> compactLyricLayout_;
    std::wstring compactLyricKey_;
    float compactLyricWidth_=0,compactLyricHeight_=0,compactLyricFont_=0;
    LyricsLines collapsedMeasureLines_;
    ComPtr<IDWriteTextFormat> collapsedMeasureFormat_;
    float collapsedLongestWidth_=-1;
    MarqueeLayoutCache collapsedLyricCache_;
    std::wstring collapsedLyricIdentity_;
    int collapsedLyricIndex_=-2;
    double collapsedLyricStartedAt_=0;
    std::unordered_map<std::wstring,ComPtr<ID2D1Bitmap>> sourceDockBitmaps_;
    std::unordered_map<std::wstring,uint64_t> sourceDockGenerations_;
