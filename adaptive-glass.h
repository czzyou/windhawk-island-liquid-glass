struct AdaptiveGlassTone {
    bool light = false;
    bool initialized = false;
    int pending = 0;

    bool Update(float brightness) {
        if (!initialized) {
            initialized = true;
            light = brightness >= 0.56f;
            return true;
        }
        const bool change = light ? brightness < 0.48f : brightness > 0.64f;
        if (!change) { pending = 0; return false; }
        if (++pending < 2) return false;
        light = !light;
        pending = 0;
        return true;
    }
};

struct AdaptiveGlassPalette {
    D2D1_COLOR_F surface, primary, secondary;
};

AdaptiveGlassPalette RegularGlassPalette(bool light) {
    return light
        ? AdaptiveGlassPalette{D2D1::ColorF(0xF3F5F7, 0.72f), D2D1::ColorF(0x171A20), D2D1::ColorF(0x292D34)}
        : AdaptiveGlassPalette{D2D1::ColorF(0x15191F, 0.72f), D2D1::ColorF(0xF5F6F8), D2D1::ColorF(0xE4E7EB)};
}

std::optional<float> SampleGlassSurroundings(HWND hwnd) {
    RECT window{};
    MONITORINFO monitor{sizeof(monitor)};
    if (!GetWindowRect(hwnd, &window) || !GetMonitorInfoW(
            MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return std::nullopt;
    const int left = (std::max)(window.left + static_cast<int>(kRenderPadX) + 16, monitor.rcMonitor.left);
    const int right = (std::min)(window.right - static_cast<int>(kRenderPadX) - 16, monitor.rcMonitor.right);
    const int top = window.top + static_cast<int>(kRenderPadY) - 6;
    const int bottom = window.bottom - static_cast<int>(kRenderPadY) + 6;
    if (right <= left || top < monitor.rcMonitor.top || bottom >= monitor.rcMonitor.bottom) return std::nullopt;

    // shortcut: sample adjacent clear rows, not pixels behind the overlay; use a compositor backdrop for exact sampling.
    HDC screen = GetDC(nullptr);
    if (!screen) return std::nullopt;
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 16;
    info.bmiHeader.biHeight = -2;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = memory ? CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    std::optional<float> brightness;
    if (bitmap && bits) {
        HGDIOBJ previous = SelectObject(memory, bitmap);
        if (previous && previous != HGDI_ERROR) {
            SetStretchBltMode(memory, COLORONCOLOR);
            if (StretchBlt(memory, 0, 0, 16, 1, screen, left, top, right - left, 1, SRCCOPY | CAPTUREBLT) &&
                StretchBlt(memory, 0, 1, 16, 1, screen, left, bottom, right - left, 1, SRCCOPY | CAPTUREBLT) &&
                GdiFlush()) {
                float sum = 0;
                const auto pixels = static_cast<const uint32_t*>(bits);
                for (int i = 0; i < 32; ++i) {
                    const uint32_t p = pixels[i];
                    sum += 0.2126f * ((p >> 16) & 255) + 0.7152f * ((p >> 8) & 255) + 0.0722f * (p & 255);
                }
                brightness = sum / (32.0f * 255.0f);
            }
            SelectObject(memory, previous);
        }
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return brightness;
}
