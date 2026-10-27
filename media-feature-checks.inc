// MEDIA_LYRICS_WIDTH_IMPLEMENTATION
int main() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    CheckFileTrayDeletion();
    MediaContentPoint mediaPoint;
    mediaPoint.valid = true; mediaPoint.width = 380; mediaPoint.height = 184;
    mediaPoint.y = MediaLayout::kControlsY;
    mediaPoint.x = 190 - MediaLayout::kControlSpacing;
    assert(MediaTransportHitTest(mediaPoint) == 0);
    mediaPoint.x = 190;
    assert(MediaTransportHitTest(mediaPoint) == 1);
    mediaPoint.x = 190 + MediaLayout::kControlSpacing;
    assert(MediaTransportHitTest(mediaPoint) == 2);
    mediaPoint.x = 380 - MediaLayout::kVolumeInsetRight;
    assert(MediaTransportHitTest(mediaPoint) == 3);
    mediaPoint.y = MediaLayout::kScrubberY;
    assert(MediaTransportHitTest(mediaPoint) == -1);
    mediaPoint.y = MediaLayout::kControlsY; mediaPoint.height = 36;
    assert(MediaTransportHitTest(mediaPoint) == -1);
    assert(!FileTrayRemoveHit(271,300));
    assert(FileTrayRemoveHit(272,300) && FileTrayRemoveHit(300,300));
    assert(!FileTrayRemoveHit(301,300));
    auto* traySource = new FileTrayDropSource(nullptr);
    assert(traySource->QueryContinueDrag(FALSE, MK_LBUTTON) == S_OK);
    assert(traySource->QueryContinueDrag(FALSE, 0) == DRAGDROP_S_DROP);
    assert(traySource->QueryContinueDrag(TRUE, MK_LBUTTON) == DRAGDROP_S_CANCEL);
    traySource->Release();
    assert(!FileTrayDragThreshold({10,10},{13,13},4,4));
    assert(FileTrayDragThreshold({10,10},{14,10},4,4));
    assert(FileTrayDragThreshold({10,10},{10,6},4,4));
    assert(!FileTrayDragThreshold({10,10},{10,10},0,0));
    assert(CollapsedLyricsOffset(600,250,0,4)==0);
    assert(CollapsedLyricsOffset(600,250,0.6,4)==0);
    assert(std::abs(CollapsedLyricsOffset(600,250,2.1,4)-175)<0.01);
    assert(CollapsedLyricsOffset(600,250,10,4)==350);
    assert(CollapsedLyricsOffset(200,250,10,4)==0);
    assert(CollapsedLyricsOffset(600,250,0,0.5)==0);
    assert(CollapsedLyricsOffset(600,250,0.46,0.5)==350);
    assert(IsAppleMusicSource(L"AppleInc.AppleMusicWin_nzyj5cx40ttqa!App"));
    assert(!IsAppleMusicSource(L"browser") && !IsAppleMusicSource(L""));
    assert(!FindAppleMusicScrubber(L"browser") && !SeekAppleMusic(L"browser",10000000));
    assert(!MediaSessionHasContent(false,L""));
    assert(!MediaSessionHasContent(false,L" \t\n"));
    assert(MediaSessionHasContent(false,L"Paused song"));
    assert(MediaSessionHasContent(true,L""));
    g_stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr); assert(g_stopEvent);
    CheckLyricsProviders();
    for (const auto* browser : {L"chrome.exe", L"msedge.exe", L"firefox.exe", L"brave.exe", L"Opera.exe",
        L"Chrome._crx_blgdilankhlgpdndahbehalgkh", L"MSEdge._crx_spotify", L"browser"}) {
        assert(!LyricsAllowed(browser,true,false,true));
        assert(!LyricsAllowed(browser,false,true,true));
    }
    for (const auto* player : {L"cloudmusic.exe", L"QQMusic.exe", L"KuGou.exe", L"kwmusic.exe",
        L"Spotify.exe", L"AppleInc.AppleMusicWin_nzyj5cx40ttqa!App",
        L"SpotifyAB.SpotifyMusic_zpdnekdrzrea0!Spotify", L"C:\\Program Files\\QQMusic\\QQMusic.exe"}) {
        assert(LyricsAllowed(player,true,false,true));
        assert(LyricsAllowed(player,false,false,true));
        assert(!LyricsAllowed(player,true,true,true));
        assert(!LyricsAllowed(player,false,false,false));
    }
    for (const auto* other : {L"", L"vlc.exe", L"wmplayer.exe", L"unknown.exe", L"notspotify.exe", L"cloudmusic-helper.exe"})
        assert(!LyricsAllowed(other,true,false,true));
    assert(LyricOpacity(true,0,40)==1.0f && LyricOpacity(false,40,40)>=0.68f);
    const auto lyricBounds=MediaLyricsBounds(380);
    assert(lyricBounds.top>=14 && lyricBounds.bottom<=100 && lyricBounds.right<380);
    Microsoft::WRL::ComPtr<IDWriteFactory> textFactory;
    assert(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory.GetAddressOf()))));
    const auto lyricFormat=CreateLyricFormat(textFactory.Get(),L"Segoe UI",18,nullptr);
    assert(lyricFormat && lyricFormat->GetTextAlignment()==DWRITE_TEXT_ALIGNMENT_LEADING && lyricFormat->GetWordWrapping()==DWRITE_WORD_WRAPPING_WRAP);
    const std::vector<LyricsLine> paragraphLines{{0,L"A longer lyric sentence should wrap naturally across several lines, without hiding any words behind the progress bar."},{1000,L"The following paragraph starts after the full measured height of the first."}};
    const auto paragraphBlocks=BuildLyricBlocks(textFactory.Get(),lyricFormat.Get(),paragraphLines,320,27);
    assert(paragraphBlocks.size()==2 && paragraphBlocks[0].lineCount>1);
    assert(paragraphBlocks[0].height>18 && paragraphBlocks[1].top>=paragraphBlocks[0].height+27);
    DWRITE_TEXT_METRICS paragraphMetrics{}; paragraphBlocks[0].layout->GetMetrics(&paragraphMetrics);
    assert(paragraphMetrics.width<=320.1f);
    assert(CollapsedLyricsWidth(100,900)==192);
    assert(CollapsedLyricsWidth(100.5f,900)==193);
    assert(CollapsedLyricsWidth(270,900)==362);
    assert(CollapsedLyricsWidth(800,900)==892);
    assert(CollapsedLyricsWidth(800,350)==350);
    assert(CollapsedLyricsWidth(-1,900)==400);
    assert(CollapsedLyricsWidth(800,-1)==0);
    assert(IsLyricsCredit(L"作词 : a very long list of writers"));
    assert(IsLyricsCredit(L"作曲/编曲：名字"));
    assert(IsLyricsCredit(L"Written By: Writer One / Writer Two"));
    assert(!IsLyricsCredit(L"I ain't tryna tell you what to do, but try to play it cool"));
    LyricsWidthTarget changingWidth;
    assert(changingWidth.Update(300,0,false)==300);
    assert(changingWidth.Update(315,0.1,false)==300);
    assert(changingWidth.Update(600,0.2,false)==600);
    assert(changingWidth.Update(200,1,false)==600);
    assert(changingWidth.Update(200,1.49,false)==600);
    assert(changingWidth.Update(200,1.51,false)==200);
    assert(changingWidth.Update(900,2,true)==200);
    assert(changingWidth.Update(500,2.1,false)==500);
    assert(changingWidth.Update(200,3,false)==500);
    assert(changingWidth.Update(200,3.6,true)==500);
    assert(changingWidth.Update(200,4,false)==500);
    assert(changingWidth.Update(200,4.51,false)==200);
    for (float scale : {0.7f,1.0f,1.5f,2.5f}) {
        for (float collapsed : {192.0f,400.0f,592.0f,892.0f}) {
            const float width=collapsed*scale;
            assert(CheckedMediaLyricsWidth(380*scale,width,false)==width);
            const float expanded=CheckedMediaLyricsWidth(380*scale,width,true);
            assert(expanded>=width && expanded>=380*scale);
            for (float anchor : {0.0f,0.5f,1.0f}) {
                const float left=-width*anchor, nextLeft=-expanded*anchor;
                for (float cursor : {left+0.5f,left+width-0.5f})
                    assert(cursor>=nextLeft && cursor<nextLeft+expanded);
            }
        }
    }
    const auto single=MeasureLongestLyricWidth(textFactory.Get(),lyricFormat.Get(),{{0,L"Short"}});
    const auto longest=MeasureLongestLyricWidth(textFactory.Get(),lyricFormat.Get(),{{0,L"Short"},{1,L"A considerably longer lyric sentence"}});
    assert(single>0 && longest>single);
    const auto reversed=MeasureLongestLyricWidth(textFactory.Get(),lyricFormat.Get(),{{0,L"A considerably longer lyric sentence"},{1,L"Short"}});
    assert(std::abs(longest-reversed)<0.01f);
    LyricsSnapshot cleared, ready;
    ready.hasData=true; ready.trackIdentity=L"music";
    assert(!LyricsResultReady(cleared,L"music") && LyricsResultReady(ready,L"music"));
    assert(!LyricsResultReady(ready,L"browser"));
    assert(!LyricsShouldWait(true,cleared,L"music",0,100));
    assert(LyricsShouldWait(true,ready,L"music",0,100));
    assert(LyricsShouldWait(true,cleared,L"music",110,100));
    assert(!LyricsShouldWait(false,ready,L"browser",110,100));
    assert(!LyricsShouldWait(true,ready,L"music",90,100));
    auto lines=ParseSyncedLyrics(L"[00:03.123]third\r\n[00:01.2][00:02.34]repeat\n[ar:ignored]\n[00:99.2]invalid\n[00:04]fourth");
    assert(lines.size()==4 && lines[0].timeMs==1200 && lines[1].timeMs==2340 && lines[2].timeMs==3123 && lines[3].timeMs==4000);
    assert(lines[0].text==L"repeat" && lines[1].text==L"repeat" && lines[2].text==L"third");
    assert(ParseSyncedLyrics(L"[00:01.x]invalid\n[00:02.]invalid\n[-1:02]invalid").empty());
    assert(ParsePlainLyrics(L"one\ntwo").size()==2);
    const auto encoded=UrlEncodeComponent(L"a 中文&"); assert(encoded=="a%20%E4%B8%AD%E6%96%87%26");
    assert(JsonStringUnescape("a\\n\\u4e2d")==L"a\n中");
    assert(PrimaryLyricsArtist(L"Ariana Grande — yes, and? - Single")==L"Ariana Grande");
    replies={{true,200,R"({"syncedLyrics":"[00:01.00]test"})"}};
    replyIndex=0; requestPaths.clear();
    LyricsPick fastPick; bool definitive=true;
    assert(FetchLyricsFromLrclib(L"yes, and?",L"Ariana Grande — yes, and? - Single",L"",215,&fastPick,&definitive));
    assert(replyIndex==1 && fastPick.synced && requestPaths.size()==1);
    assert(requestPaths[0].find(L"artist_name=Ariana%20Grande&")!=std::wstring::npos);
    replies={{false,0,""}}; replyIndex=0; requestPaths.clear();
    LyricsPick failedPick;
    assert(!FetchLyricsFromLrclib(L"Song",L"Artist",L"",123,&failedPick,&definitive));
    assert(!definitive && requestPaths.size()==1);
    LyricsSnapshot snapshot; snapshot.hasData=true; snapshot.synced=true; snapshot.lines=LyricsLines(std::move(lines));
    const auto keys=LyricsCache::BuildKeys(L"Song",L"Artist",L"Album",1230000000);
    LyricsCache::Store(keys,LyricsCache::Status::Found,snapshot,false);
    LyricsSnapshot restored;
    assert(LyricsCache::Lookup(keys,false,L"Song",L"Artist",&restored) && restored.lines.size()==4);
    const auto alias=LyricsCache::BuildKeys(L"Song",L"Artist",L"Different album metadata",1231000000);
    assert(LyricsCache::Lookup(alias,false,L"Song",L"Artist",&restored) && restored.hasData);
    restored.trackIdentity=L"music";
    assert(LyricsResultReady(restored,L"music"));
    const auto missing=LyricsCache::BuildKeys(L"Missing",L"Artist",L"",1);
    LyricsCache::Store(missing,LyricsCache::Status::NotFound,{},false);
    assert(!LyricsCache::Lookup(missing,false,L"Missing",L"Artist",&restored));
    LyricsCache::Entry entry; entry.savedAt=LyricsCache::UnixNow(); entry.lines=snapshot.lines.vec(); entry.synced=true;
    entry.looseKey=keys.loose; entry.fullKey=keys.full; entry.looseHash=keys.looseHash; entry.fullHash=keys.fullHash; entry.durationSec=123;
    const auto bytes=LyricsCache::SerializeEntry(entry,L"Song",L"Artist");
    LyricsCache::Entry parsed; assert(LyricsCache::ParseEntry(bytes,&parsed) && parsed.lines.size()==4);
    assert(!LyricsCache::ParseEntry("DIWLC 99\n",&parsed));
    wchar_t directory[MAX_PATH]{},file[MAX_PATH]{}; assert(GetTempPathW(MAX_PATH,directory)); assert(GetTempFileNameW(directory,L"ilm",0,file));
    assert(LyricsCache::WriteFileAtomic(file,bytes));
    const auto temporary=std::wstring(file)+L".tmp";
    HANDLE locked=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr); assert(locked!=INVALID_HANDLE_VALUE);
    assert(!LyricsCache::WriteFileAtomic(file,"bad")); CloseHandle(locked); DeleteFileW(temporary.c_str());
    uint64_t size=0; assert(LyricsCache::LoadEntry(file,LyricsCache::UnixNow(),&parsed,&size)==LyricsCache::LoadResult::Ok);
    DeleteFileW(file);

    MediaTimelineTracker timeline;
    MediaSnapshot old; old.playing=true; old.title=L"song"; old.sourceAppUserModelId=L"music";
    old.positionTicks=100000000; old.endTicks=1000000000; old.lastUpdatedTicks=1000;
    MediaSnapshot next=old;
    timeline.Update(next,old,107000000,old.endTicks,0,2000);
    assert(next.positionTicks==110000000); // A 300 ms correction must not cross a lyric boundary backward.
    next=old;
    timeline.Update(next,old,10000000,old.endTicks,0,2000);
    assert(next.positionTicks==110000000); // A single stale report must not move lyrics backward.
    old=next; timeline.Update(next,old,17500000,old.endTicks,0,2750);
    assert(next.positionTicks==17500000); // A second coherent report confirms a backward seek.
    next.sourceAppUserModelId=L"browser";
    timeline.Update(next,old,1000000,old.endTicks,0,3000);
    assert(next.positionTicks==1000000); // A source change must reset the jitter guard.
    MediaSourceTracker tracker; tracker.order={L"music",L"browser"};
    tracker.wasPlaying={{L"music",true},{L"browser",true}}; tracker.startedSeq={{L"music",1},{L"browser",2}}; tracker.seqCounter=2;
    assert(ChooseMediaSourceAumid(tracker,L"")==L"browser");
    assert(ChooseMediaSourceAumid(tracker,L"music")==L"music");
    g_dockOverrideAumid=L"browser"; ++g_dockOverrideRev;
    assert(ChooseMediaSourceAumid(tracker,L"music")==L"browser");
    tracker.startedSeq[L"music"]=++tracker.seqCounter;
    assert(ChooseMediaSourceAumid(tracker,L"music")==L"music");
    tracker.order.erase(tracker.order.begin());
    assert(ChooseMediaSourceAumid(tracker,L"music")==L"browser");
    assert(MediaSourceDockHit({true,190,176,380,184},2)==1);
    assert(MediaSourceDockHit({true,190,148,380,184},2)==-1);
    assert(MediaSourceDockHit({true,190,176,380,44},2)==-1);

    assert(QuickLookup::NormalizeQuery(L"  “hello,” \n")==L"hello");
    assert(QuickLookup::NormalizeQuery(std::wstring(100,L'a')).size()>QuickLookup::kMaxQueryChars);
    QuickLookup::Result result;
    const std::string definition=R"({"en":[{"partOfSpeech":"noun","definitions":[{"definition":"A <b>greeting</b> &amp; welcome."}]}]})";
    assert(QuickLookup::ParseWiktionaryEnglish(definition,L"hello",&result));
    assert(QuickLookup::ParseWiktionaryEnglish("{\"en\" : "+definition.substr(6),L"hello",&result));
    assert(result.body.find(L"greeting")!=std::wstring::npos && result.body.find(L"<b>")==std::wstring::npos);
    replies={{true,404,""},{true,200,R"([{"word":"hello","meanings":[{"partOfSpeech":"interjection","definitions":[{"definition":"A greeting.","example":"Hello, friend!"}]}]}])"},{true,200,R"({"responseStatus":200,"responseData":{"translatedText":"一句问候。"}})"}};
    hosts.clear(); replyIndex=0;
    assert(QuickLookup::LookupText(L"hello",&result)==QuickLookup::Outcome::Found);
    assert(hosts.size()==3 && hosts[0]==L"en.wiktionary.org" && hosts[1]==L"api.dictionaryapi.dev" && hosts[2]==L"api.mymemory.translated.net");
    assert(result.body.find(L"一句问候")!=std::wstring::npos && result.body.find(L"A greeting.")!=std::wstring::npos);
    replies={{false,0,""},{false,0,""},{false,0,""}}; replyIndex=0;
    assert(QuickLookup::LookupText(L"hello",&result)==QuickLookup::Outcome::Failed);
    replies={{true,404,""},{true,404,""},{true,404,""},{true,404,""}}; replyIndex=0;
    assert(QuickLookup::LookupText(L"hello",&result)==QuickLookup::Outcome::NotFound);
    bool found=false;
    assert(!QuickLookup::CacheLookup(L"unsubmitted",&found,&result));
    QuickLookup::CacheStore(L"hello",true,result); assert(QuickLookup::CacheLookup(L"hello",&found,&result) && found);
    g_lookupRequestSeq=2; g_state.lookup.active=true; g_state.lookup.expiresAt=110;
    result.title=L"fresh";
    QuickLookup::ApplyResult(1,L"old",true,result); assert(g_state.lookup.title.empty());
    QuickLookup::ApplyResult(2,L"new",true,result); assert(g_state.lookup.title==L"fresh");
    g_state.lookup.active=false; result.title=L"stale";
    QuickLookup::ApplyResult(2,L"old",true,result); assert(!g_state.lookup.active && g_state.lookup.title==L"fresh");
    g_lookupUi.clipboardQuery=L"clipboard";
    const auto rows=BuildLookupRows(g_state.lookupRecent,g_lookupUi);
    assert(!rows.empty() && rows[0].kind==LookupRowKind::Clipboard && LookupHasHeader(g_lookupUi,rows));
    assert(LookupRowAtContentPoint({true,40,LookupLayout::RowsTop(true)+5,380,184},static_cast<int>(rows.size()),true)==0);
    assert(LookupRowAtContentPoint({true,40,10,380,184},static_cast<int>(rows.size()),true)==-1);
    g_lookupJobEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr); assert(g_lookupJobEvent);
    HANDLE worker=CreateThread(nullptr,0,LookupThreadProc,nullptr,0,nullptr); assert(worker);
    SetEvent(g_stopEvent); assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0);
    CloseHandle(worker); CloseHandle(g_lookupJobEvent); CloseHandle(g_stopEvent);
    winrt::uninit_apartment();
}
