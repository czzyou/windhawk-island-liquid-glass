// Subtle ambient light, guided by https://developer.apple.com/videos/play/wwdc2025/219/.
// Original field implementation, not Apple's material renderer.
constexpr int kGlowFieldWidth = 160, kGlowFieldHeight = 48;
using GlowFieldPixels = std::array<uint8_t, kGlowFieldWidth * kGlowFieldHeight * 4>;

static GlowFieldPixels BuildAlbumGlowField(const D2D1_COLOR_F* colors,
    const float* weights, int count, float phase) {
    struct Lobe { float x, y, rx, ry, weight; D2D1_COLOR_F color; };
    std::array<Lobe, 10> lobes{};
    count = (std::max)(0, (std::min)(5, count));
    float weightSum = 0, accumulated = 0;
    for (int j = 0; j < count; ++j) weightSum += (std::max)(0.0f, weights[j]);
    for (int j = 0; j < count; ++j) {
        const float share = weightSum > 0 ? (std::max)(0.0f, weights[j]) / weightSum : 0;
        const float center = accumulated + share * 0.5f;
        accumulated += share;
        const auto color = colors[j];
        for (int k = 0; k < 2; ++k) {
            const int i = j * 2 + k;
            const float seed = i * 2.39996f;
            const float t = phase * 0.55f + seed;
            lobes[i] = {
                (k ? 1.0f - center : center) + 0.085f * std::sin(t),
                (k ? 0.85f : 0.15f) + 0.09f * std::sin(t * 0.79f),
                (0.18f + 0.28f * std::sqrt(share)) * (1.0f + 0.06f * std::sin(t)),
                0.65f + 0.04f * std::cos(t * 0.83f),
                share * 0.5f, D2D1::ColorF(color.r * color.r, color.g * color.g, color.b * color.b)};
        }
    }
    GlowFieldPixels pixels{};
    for (int y = 0; y < kGlowFieldHeight; ++y) {
        const float v = (y + 0.5f) / kGlowFieldHeight;
        const float warpX = 0.14f * std::sin(phase * 0.61f) +
            0.065f * std::sin(v * 3.0f + phase * 0.52f);
        for (int x = 0; x < kGlowFieldWidth; ++x) {
            const float u = (x + 0.5f) / kGlowFieldWidth + warpX;
            const float warpedV = v + 0.14f * std::sin(u * 4.2f - phase * 0.43f);
            float r = 0, g = 0, b = 0, total = 0;
            for (int i = 0; i < count * 2; ++i) {
                const auto& lobe = lobes[i];
                const float dx = (u - lobe.x) / lobe.rx, dy = (warpedV - lobe.y) / lobe.ry;
                const float q = 1.0f + dx * dx + dy * dy;
                const float influence = lobe.weight / (q * q);
                total += influence;
                r += lobe.color.r * influence; g += lobe.color.g * influence; b += lobe.color.b * influence;
            }
            const size_t index = (y * kGlowFieldWidth + x) * 4;
            if (total > 0) {
                // Gamma-2 light mixing avoids dark, muddy joins between saturated colours.
                pixels[index] = static_cast<uint8_t>(std::sqrt(Clamp(b / total, 0, 1)) * 255 + 0.5f);
                pixels[index + 1] = static_cast<uint8_t>(std::sqrt(Clamp(g / total, 0, 1)) * 255 + 0.5f);
                pixels[index + 2] = static_cast<uint8_t>(std::sqrt(Clamp(r / total, 0, 1)) * 255 + 0.5f);
                pixels[index + 3] = 255;
            }
        }
    }
    return pixels;
}

// END_ALBUM_GLOW_FLUID
