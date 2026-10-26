#include <windows.h>
#include <d2d1.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <vector>

struct SharedState {
    std::array<float, 48> waveform{};
    size_t waveformWrite = 480;
};
float Clamp(float value, float low, float high) { return std::clamp(value, low, high); }
struct Brush {
    void SetOpacity(float) {}
    Brush* operator->() { return this; }
    Brush* Get() { return this; }
};
struct Target {
    std::vector<float> heights;
    void FillRoundedRectangle(D2D1_ROUNDED_RECT rect, Brush*) {
        heights.push_back(rect.rect.bottom - rect.rect.top);
    }
};
struct Renderer {
    Target* target_;
    Brush& accentBrush_;
    // WAVEFORM_IMPLEMENTATION
};
int main() {
    Target target;
    Brush brush;
    Renderer renderer{&target, brush};
    SharedState state;
    for (float width : {32.0f, 40.0f}) {
        // Audible low-volume samples must not all become the same minimum-height bar.
        for (size_t i = 0; i < state.waveform.size(); ++i)
            state.waveform[i] = 0.001f + 0.0001f * i;
        target.heights.clear();
        renderer.DrawWaveform(state, D2D1::RectF(0, 0, width, 20));
        const auto [low, high] = std::minmax_element(target.heights.begin(), target.heights.end());
        assert(*low > 3.0f && *high - *low > 1.0f && *high <= 17.201f);

        state.waveform.fill(0);
        target.heights.clear();
        renderer.DrawWaveform(state, D2D1::RectF(0, 0, width, 20));
        for (float height : target.heights) assert(height == 3.0f);

        state.waveform.fill(1);
        target.heights.clear();
        renderer.DrawWaveform(state, D2D1::RectF(0, 0, width, 20));
        for (float height : target.heights) assert(std::fabs(height - 17.2f) < 0.001f);
    }
}
