std::atomic<int> g_hoveredQuickAction{-1};

D2D1_RECT_F QuickHomeButtonBounds(int index, float width) {
    const float buttonWidth = (width - 24.0f - 30.0f - 24.0f) * 0.25f;
    const float left = 24.0f + index * (buttonWidth + 8.0f);
    return {left, 70.0f, left + buttonWidth, 144.0f};
}

int QuickHomeHitTest(const MediaContentPoint& point) {
    if (!point.valid || point.height < 170.0f || point.width < 300.0f) return -1;
    for (int i = 0; i < 4; ++i) {
        const auto bounds = QuickHomeButtonBounds(i, point.width);
        if (point.x >= bounds.left && point.x < bounds.right &&
            point.y >= bounds.top && point.y < bounds.bottom) return i;
    }
    return -1;
}
