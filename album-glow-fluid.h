// Subtle ambient light, guided by https://developer.apple.com/videos/play/wwdc2025/219/.
// Original field implementation, not Apple's material renderer.
constexpr int kGlowFieldWidth = 320, kGlowFieldHeight = 96;
using GlowFieldPixels = std::array<uint8_t, kGlowFieldWidth * kGlowFieldHeight * 4>;

// Constant-speed clockwise travel along straight edges and circular corners.
static D2D1_POINT_2F AlbumGlowPoint(float position, float width, float height, float radius) {
    const float r = (std::min)(radius, (std::min)(width, height) * 0.5f);
    const float a = width * 0.5f - r, b = height * 0.5f - r;
    const float quarter = a + b + 1.57079633f * r;
    const float turn = (position - std::floor(position)) * 4.0f;
    const int quadrant = static_cast<int>(turn);
    const float fraction = turn - quadrant;
    const float distance = ((quadrant & 1) ? 1.0f - fraction : fraction) * quarter;
    float x, y;
    if (distance <= a) {
        x = distance; y = -height * 0.5f;
    } else if (distance < a + 1.57079633f * r) {
        const float angle = (distance - a) / r;
        x = a + r * std::sin(angle); y = -b - r * std::cos(angle);
    } else {
        x = width * 0.5f; y = -b + distance - a - 1.57079633f * r;
    }
    if (quadrant >= 2) x = -x;
    if (quadrant == 1 || quadrant == 2) y = -y;
    return D2D1::Point2F(width * 0.5f + x, height * 0.5f + y);
}

static GlowFieldPixels BuildAlbumGlowField(const D2D1_COLOR_F* colors,
    const float* weights, int count, float phase, float width, float height, float radius,
    float scale = 1.0f, float energy = 0.0f) {
    count = (std::max)(0, (std::min)(2, count));
    float total = 0;
    std::array<D2D1_POINT_2F, 2> lights{};
    for (int j = 0; j < count; ++j) {
        total += (std::max)(0.0f, weights[j]);
        // Keep the weak cover color beside the main reflection, not mixed into it.
        lights[j] = AlbumGlowPoint(phase / 6.28318531f - j * 0.05f, width, height, radius);
    }
    GlowFieldPixels pixels{};
    if (total <= 0) return pixels;
    const float spread = (12.0f + 12.0f * energy) * scale;
    for (int y = 0; y < kGlowFieldHeight; ++y) {
        for (int x = 0; x < kGlowFieldWidth; ++x) {
            float red = 0, green = 0, blue = 0, alpha = 0;
            for (int j = 0; j < count; ++j) {
                const float dx = (x + 0.5f) / kGlowFieldWidth * width - lights[j].x;
                const float dy = (y + 0.5f) / kGlowFieldHeight * height - lights[j].y;
                const float glow = (0.275f + 0.15f * energy) * weights[j] / total *
                    std::exp(-(dx*dx + dy*dy) / (2 * spread * spread));
                alpha += glow;
                red += colors[j].r * glow; green += colors[j].g * glow; blue += colors[j].b * glow;
            }
            const size_t i = (y * kGlowFieldWidth + x) * 4;
            pixels[i] = static_cast<uint8_t>(Clamp(blue, 0, 1) * 255 + 0.5f);
            pixels[i+1] = static_cast<uint8_t>(Clamp(green, 0, 1) * 255 + 0.5f);
            pixels[i+2] = static_cast<uint8_t>(Clamp(red, 0, 1) * 255 + 0.5f);
            pixels[i+3] = static_cast<uint8_t>(Clamp(alpha, 0, 1) * 255 + 0.5f);
        }
    }
    return pixels;
}
// END_ALBUM_GLOW_FLUID
