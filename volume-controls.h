#pragma once
inline D2D1_RECT_F VolumeTrackBounds(float width, float height) {
    return {height + 12.0f, height * 0.5f + 7.0f, width - 14.0f, height * 0.5f + 12.0f};
}
inline int VolumePercentAt(const MediaContentPoint& point) {
    if (!point.valid || point.width < 140 || point.height < 24 || !std::isfinite(point.x)) return -1;
    const auto track = VolumeTrackBounds(point.width, point.height);
    return (std::clamp)(static_cast<int>(std::lround((point.x-track.left)*100.0f/(track.right-track.left))),0,100);
}
inline int VolumeControlHitTest(const MediaContentPoint& point) {
    if (VolumePercentAt(point) < 0) return -1;
    if (point.x >= 14 && point.x <= point.height-2 && point.y >= 8 && point.y <= point.height-8) return 1;
    const auto track=VolumeTrackBounds(point.width,point.height);
    return point.x>=track.left-8 && point.x<=track.right+8 && point.y>=track.top-10 && point.y<=track.bottom+10 ? 0 : -1;
}
