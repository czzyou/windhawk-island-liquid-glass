bool LyricsAllowed(std::wstring source, bool music, bool video, bool hasArtist) {
    if (video || (!music && !hasArtist)) return false;
    for (auto& c : source) c = static_cast<wchar_t>(towlower(c));
    const auto slash = source.find_last_of(L"\\/");
    if (slash != std::wstring::npos) source.erase(0, slash + 1);
    // Match player identities, never song titles or browser-reported Music metadata.
    for (const auto* player : {L"cloudmusic", L"cloudmusic.exe", L"qqmusic", L"qqmusic.exe",
        L"kugou", L"kugou.exe", L"kwmusic", L"kwmusic.exe", L"spotify", L"spotify.exe",
        L"applemusic", L"applemusic.exe", L"spotifyab.spotifymusic_zpdnekdrzrea0!spotify",
        L"appleinc.applemusicwin_nzyj5cx40ttqa!app"}) {
        if (source == player) return true;
    }
    return false;
}
bool LyricsResultReady(const LyricsSnapshot& lyrics, const std::wstring& identity) {
    return lyrics.trackIdentity == identity && (lyrics.hasData || lyrics.notFound || lyrics.failed);
}
bool LyricsShouldWait(bool sameTrack, const LyricsSnapshot& published, const std::wstring& identity, double retryAt, double now) {
    return sameTrack && (retryAt>now || (retryAt<=0 && LyricsResultReady(published,identity)));
}
inline D2D1_RECT_F MediaLyricsBounds(float width) {
    return D2D1::RectF(26, 18, width - 26, 96);
}
float LyricOpacity(bool active, float distance, float halfHeight) {
    return active ? 1.0f : std::clamp(0.88f - distance / (std::max)(1.0f,halfHeight) * 0.18f,0.68f,0.88f);
}

float CollapsedLyricsOffset(float textWidth, float available, double elapsed, double duration) {
    const float overflow=(std::max)(0.0f,textWidth-available);
    const double hold=(std::min)(0.6,duration*0.15);
    const double travel=(std::max)(0.1,duration-hold-(std::min)(0.4,duration*0.1));
    return overflow*static_cast<float>(std::clamp((elapsed-hold)/travel,0.0,1.0));
}

bool IsLyricsCredit(const std::wstring& text) {
    auto lower=text;
    for (auto& c:lower) c=static_cast<wchar_t>(towlower(c));
    const auto first=lower.find_first_not_of(L" \t([（【");
    if (first==std::wstring::npos) return false;
    for (const auto* prefix : {L"作词",L"作曲",L"词曲",L"编曲",L"制作人",L"制作",L"录音",L"混音",L"母带",L"监制",L"出品",L"发行",L"词",L"曲",
                              L"lyrics by",L"composed by",L"written by",L"produced by",L"arranged by",L"composer",L"lyricist",L"producer",L"mixing",L"mastering"}) {
        const size_t end=first+wcslen(prefix);
        if (lower.compare(first,wcslen(prefix),prefix)==0 && end<lower.size() &&
            std::wstring(L" :：/\t").find(lower[end])!=std::wstring::npos) return true;
    }
    // Long slash-separated name lists may omit the usual credit label entirely.
    const auto slashes=std::count_if(lower.begin(),lower.end(),[](wchar_t c){return c==L'/' || c==L'／';});
    const bool bracketed=lower.find_first_not_of(L" \t")!=first;
    if (lower.size()<80 || slashes<4 || (!bracketed && (lower.size()<160 || slashes<8)) ||
        lower.find(L"://")!=std::wstring::npos || lower.find_first_of(L"!?！？。;；")!=std::wstring::npos) return false;
    std::vector<std::wstring> names;
    size_t start=0;
    while (start<lower.size()) {
        const auto end=lower.find_first_of(L"/／",start);
        auto name=lower.substr(start,end==std::wstring::npos ? end : end-start);
        const auto begin=name.find_first_not_of(L" \t([（【");
        const auto last=name.find_last_not_of(L" \t)]）】\r");
        if (begin==std::wstring::npos || last<begin) return false;
        name=name.substr(begin,last-begin+1);
        int words=0; bool inWord=false;
        for (auto c:name) {
            if (iswspace(c)) inWord=false;
            else if (!inWord) {++words; inWord=true;}
        }
        if (name.size()>64 || words>4) return false;
        if (std::find(names.begin(),names.end(),name)==names.end()) names.push_back(name);
        if (end==std::wstring::npos) break;
        start=end+1;
    }
    return names.size()>=5;
}

struct LyricsWidthTarget {
    float value=-1, pending=-1;
    double shrinkAt=0;
    float Update(float requested,double now,bool frozen) {
        if (value<0) value=requested;
        if (frozen) {pending=-1; return value;}
        if (requested>value+24) {value=requested; pending=-1;}
        else if (requested<value-24) {
            if (pending!=requested) {pending=requested; shrinkAt=now+0.5;}
            if (now>=shrinkAt) {value=requested; pending=-1;}
        } else pending=-1;
        return value;
    }
};
