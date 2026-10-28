// Approximate visible area on a bounded grid; keep neutral regions and undo PBGRA.
static void SampleAlbumGlow(BitmapPixels& pixels) {
    struct Bin { float r = 0, g = 0, b = 0, count = 0; };
    std::array<Bin, 512> bins{};
    if (!pixels.width || !pixels.height ||
        pixels.bgra.size() != static_cast<size_t>(pixels.width) * pixels.height * 4) return;
    const UINT nx = (std::min)(pixels.width, 64u), ny = (std::min)(pixels.height, 64u);
    float total = 0;
    for (UINT y = 0; y < ny; ++y) {
        for (UINT x = 0; x < nx; ++x) {
            const size_t i = (static_cast<size_t>((2 * y + 1) * pixels.height / (2 * ny)) *
                pixels.width + (2 * x + 1) * pixels.width / (2 * nx)) * 4;
            const float a = pixels.bgra[i + 3] / 255.0f;
            if (a < 0.125f) continue;
            const float r = (std::min)(1.0f, pixels.bgra[i + 2] / (255.0f * a));
            const float g = (std::min)(1.0f, pixels.bgra[i + 1] / (255.0f * a));
            const float b = (std::min)(1.0f, pixels.bgra[i] / (255.0f * a));
            const int index = ((std::min)(7, int(r * 8)) << 6) |
                ((std::min)(7, int(g * 8)) << 3) | (std::min)(7, int(b * 8));
            auto& bin = bins[index];
            bin.r += r * a; bin.g += g * a; bin.b += b * a; bin.count += a;
            total += a;
        }
    }
    if (total == 0) return;
    for (auto& bin : bins) if (bin.count > 0) {
        bin.r /= bin.count; bin.g /= bin.count; bin.b /= bin.count;
    }
    auto distance = [](const Bin& a, const Bin& b) {
        return (a.r-b.r)*(a.r-b.r) + (a.g-b.g)*(a.g-b.g) + (a.b-b.b)*(a.b-b.b);
    };
    std::array<Bin, 5> centers{};
    int count = 0;
    for (; count < 5; ++count) {
        float best = 0;
        const Bin* seed = nullptr;
        for (const auto& bin : bins) {
            if (bin.count == 0) continue;
            float separation = 1;
            for (int j = 0; j < count; ++j) separation = (std::min)(separation, distance(bin, centers[j]));
            const float score = bin.count * separation;
            if (score > best) { best = score; seed = &bin; }
        }
        if (!seed || (count && best < total * 0.0001f)) break;
        centers[count] = *seed;
    }
    for (int pass = 0; pass < 6; ++pass) {
        std::array<Bin, 5> sums{};
        for (const auto& bin : bins) {
            if (bin.count == 0) continue;
            int nearest = 0;
            for (int j = 1; j < count; ++j)
                if (distance(bin, centers[j]) < distance(bin, centers[nearest])) nearest = j;
            auto& sum = sums[nearest];
            sum.r += bin.r * bin.count; sum.g += bin.g * bin.count;
            sum.b += bin.b * bin.count; sum.count += bin.count;
        }
        for (int j = 0; j < count; ++j) {
            if (sums[j].count > 0) {
                centers[j] = sums[j];
                centers[j].r /= sums[j].count; centers[j].g /= sums[j].count; centers[j].b /= sums[j].count;
            } else centers[j].count = 0;
        }
    }
    std::sort(centers.begin(), centers.begin() + count,
        [](const Bin& a, const Bin& b) { return a.count > b.count; });
    pixels.glowColorCount = 0;
    for (int j = 0; j < count; ++j) {
        if (centers[j].count <= 0) continue;
        const int k = pixels.glowColorCount++;
        pixels.glowColors[k] = D2D1::ColorF(centers[j].r, centers[j].g, centers[j].b, 1);
        pixels.glowWeights[k] = centers[j].count / total;
    }
}
