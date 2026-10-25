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
