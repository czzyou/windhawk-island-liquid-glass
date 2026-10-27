std::atomic<int> g_weatherPage{0};

int NextWeatherPage(int page) { return (page + 1) % 3; }

D2D1_RECT_F WeatherPageButtonBounds(float width) {
    return D2D1::RectF(width - 132.0f, 14.0f, width - 24.0f, 42.0f);
}

bool WeatherPageButtonHit(const MediaContentPoint& point) {
    if (!point.valid || point.width < 280.0f || point.height < 160.0f) return false;
    const auto box = WeatherPageButtonBounds(point.width);
    return point.x >= box.left && point.x <= box.right && point.y >= box.top && point.y <= box.bottom;
}
