std::atomic<float> g_captionDockAnchor{0.0f};
std::atomic<float> g_captionDockBlend{0.0f};

struct CaptionDockMotion {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    void Step(float dt, bool animate) {
        if (!animate) { value = target; velocity = 0.0f; return; }
        // Exact critically damped motion preserves velocity when the target changes.
        float delta = value - target;
        float c = velocity + 18.0f * delta;
        float decay = std::exp(-18.0f * dt);
        float next = target + (delta + c * dt) * decay;
        velocity = (velocity - 18.0f * c * dt) * decay;
        if ((value - target) * (next - target) < 0.0f ||
            (std::abs(next - target) < 0.01f && std::abs(velocity) < 0.1f)) {
            next = target;
            velocity = 0.0f;
        }
        value = next;
    }

    bool Moving() const {
        return std::abs(value - target) > 0.01f || std::abs(velocity) > 0.1f;
    }
};

float CaptionDockShift(float anchor, float blend) {
    return anchor * blend;
}

struct CaptionDockCandidate {
    float anchor = 0.0f, blend = 0.0f;
    double since = -1.0;

    bool Observe(float nextAnchor, float nextBlend, double now) {
        if (since < 0.0 || anchor != nextAnchor || blend != nextBlend) {
            anchor = nextAnchor;
            blend = nextBlend;
            since = now;
            return false;
        }
        return now - since >= 0.24;
    }
};

bool GetCaptionDockTarget(HWND island, const RECT& work, float compactWidth,
                          int top, int height, int& rightEdge) {
    struct Search {
        HWND island;
        RECT footprint;
        RECT work;
        int edge;
        bool found = false;
        std::vector<RECT> occluders;
        DWORD islandProcess = 0;
    } search{island,
             {static_cast<LONG>((work.left + work.right - compactWidth) * 0.5f), top,
              static_cast<LONG>((work.left + work.right + compactWidth) * 0.5f), top + height},
             work, work.right};
    GetWindowThreadProcessId(island, &search.islandProcess);

    EnumWindows([](HWND hwnd, LPARAM param) WINAPI -> BOOL {
        auto& s = *reinterpret_cast<Search*>(param);
        if (hwnd == s.island || !IsWindowVisible(hwnd) || IsIconic(hwnd) ||
            hwnd == GetShellWindow()) return TRUE;
        DWORD process = 0;
        GetWindowThreadProcessId(hwnd, &process);
        // Our glass panels transmit the buttons and must not occlude collision checks.
        if (s.islandProcess && process == s.islandProcess) return TRUE;
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
            return TRUE;
        RECT raw{}, bounds{};
        if (!GetWindowRect(hwnd, &raw)) return TRUE;
        bounds = raw;
        DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds));
        RECT overlap{};
        if (!IntersectRect(&overlap, &bounds, &s.footprint)) return TRUE;

        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        if ((style & WS_CAPTION) == WS_CAPTION &&
            (style & (WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX))) {
            RECT buttons{};
            bool valid = SUCCEEDED(DwmGetWindowAttribute(
                hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons))) &&
                !IsRectEmpty(&buttons) && buttons.left >= 0 &&
                buttons.right <= raw.right - raw.left + 1;
            if (valid) {
                OffsetRect(&buttons, raw.left, raw.top);
            } else {
                // Custom title bars can omit DWM bounds; use the standard button widths.
                float dpi = GetDpiForWindow(hwnd) / 96.0f;
                int count = 1 + !!(style & WS_MINIMIZEBOX) + !!(style & WS_MAXIMIZEBOX);
                buttons = {bounds.right - static_cast<LONG>(std::lround(46.0f * dpi * count)),
                           bounds.top, bounds.right,
                           bounds.top + static_cast<LONG>(std::lround(36.0f * dpi))};
            }
            RECT collision{};
            if (IntersectRect(&collision, &buttons, &s.footprint)) {
                POINT sample{(collision.left + collision.right) / 2,
                             (collision.top + collision.bottom) / 2};
                bool covered = false;
                for (const auto& previous : s.occluders) {
                    if (PtInRect(&previous, sample)) { covered = true; break; }
                }
                if (!covered) {
                    int gap = static_cast<int>(std::lround(12.0f * GetDpiForWindow(hwnd) / 96.0f));
                    s.edge = (std::min)(s.edge, static_cast<int>(buttons.left) - gap);
                    s.found = true;
                }
            }
        }
        s.occluders.push_back(bounds);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    rightEdge = search.edge;
    return search.found;
}
