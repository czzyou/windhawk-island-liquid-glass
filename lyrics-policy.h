bool LyricsAllowed(bool browser, bool music, bool video, bool hasArtist) {
    if (video) return false;
    return music || (!browser && hasArtist);
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
    const auto first=lower.find_first_not_of(L" \t");
    if (first==std::wstring::npos) return false;
    for (const auto* prefix : {L"作词",L"作曲",L"词曲",L"编曲",L"制作人",L"制作",L"录音",L"混音",L"母带",L"监制",L"出品",L"发行",L"词",L"曲",
                              L"lyrics by",L"composed by",L"written by",L"produced by",L"arranged by",L"composer",L"lyricist",L"producer",L"mixing",L"mastering"}) {
        const size_t end=first+wcslen(prefix);
        if (lower.compare(first,wcslen(prefix),prefix)==0 && end<lower.size() &&
            std::wstring(L" :：/\t").find(lower[end])!=std::wstring::npos) return true;
    }
    return false;
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
