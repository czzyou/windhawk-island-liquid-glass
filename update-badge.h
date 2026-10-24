inline D2D1_ELLIPSE UpdateBadgeBounds(float left, float top, float width, float height, float scale) {
    scale = (std::max)(0.7f, (std::min)(scale, 1.6f));
    const float radius = 6.0f * scale;
    return D2D1::Ellipse(D2D1::Point2F(left + width + radius + 1.0f,
        top + (std::min)(10.0f * scale, height * 0.5f)), radius, radius);
}
