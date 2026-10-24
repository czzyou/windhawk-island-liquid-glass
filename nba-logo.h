#pragma once
// Fit into a transparent 64px square, averaging premultiplied pixels.
inline BitmapPixels ResizeNbaLogo(const BitmapPixels& source) {
    BitmapPixels out;
    if (!source.width || !source.height || source.width > 2048 || source.height > 2048 ||
        source.bgra.size() != static_cast<size_t>(source.width) * source.height * 4) return out;
    out.width = out.height = 64;
    out.bgra.resize(64 * 64 * 4);
    const UINT longest = (std::max)(source.width, source.height);
    const UINT width = (std::max)(1u, source.width * 64 / longest);
    const UINT height = (std::max)(1u, source.height * 64 / longest);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const UINT x0 = x * source.width / width, y0 = y * source.height / height;
            const UINT x1 = (std::max)(x0 + 1, (x + 1) * source.width / width);
            const UINT y1 = (std::max)(y0 + 1, (y + 1) * source.height / height);
            unsigned sums[4]{};
            for (UINT sy = y0; sy < y1; ++sy)
                for (UINT sx = x0; sx < x1; ++sx)
                    for (int c = 0; c < 4; ++c)
                        sums[c] += source.bgra[(sy * source.width + sx) * 4 + c];
            const UINT count = (x1 - x0) * (y1 - y0);
            const size_t offset = ((y + (64 - height) / 2) * 64 + x + (64 - width) / 2) * 4;
            for (int c = 0; c < 4; ++c) out.bgra[offset + c] = static_cast<uint8_t>(sums[c] / count);
        }
    }
    return out;
}
