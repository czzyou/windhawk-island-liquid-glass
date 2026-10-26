#include <windows.h>
#include <d2d1.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
// COLOR_IMPLEMENTATION
enum class IslandKind { Idle, Media, Timer };
enum class BackdropMaterial { None, Acrylic };
enum class AccentMode { Auto, Custom };
struct Settings {
    bool ambientAuroraGlow = true, notchStyle = false, borderMergedMode = false, w11Style = false;
    float ambientGlowIntensity = 1, sizeScale = 1;
    BackdropMaterial backdropMaterial = BackdropMaterial::None;
    AccentMode accentMode = AccentMode::Auto;
};
struct SharedState { struct { bool playing = true; struct { bool accentIsNeutral = false; } art; } media; };
std::atomic<float> g_idleCompactBlend{0};
constexpr float kRenderPadX = 28;
std::map<std::wstring, int> stored;
int setting = 1;
int Wh_GetIntSetting(const wchar_t* key) {
    return std::wstring(key) == L"Themes.AmbientGlowIntensity" ? 100 : setting;
}
int Wh_GetIntValue(const wchar_t* key, int fallback) { return stored.count(key) ? stored[key] : fallback; }
void Wh_SetIntValue(const wchar_t* key, int value) { stored[key] = value; }
Settings LoadGlowSettings() {
    Settings next;
    // SETTINGS_IMPLEMENTATION
    return next;
}
struct Brush { D2D1_COLOR_F color{}; void SetColor(D2D1_COLOR_F c) { color = c; } void SetOpacity(float) {} };
struct BrushPtr {
    Brush* value = nullptr;
    Brush* operator->() { return value; }
    Brush* Get() { return value; }
    explicit operator bool() { return value != nullptr; }
    Brush** operator&() { return &value; }
};
struct Target {
    Brush brush;
    HRESULT CreateSolidColorBrush(D2D1_COLOR_F color, Brush** result) {
        brush.color = color; *result = &brush; return S_OK;
    }
};
struct Renderer {
    Target target;
    Target* target_ = &target;
    BrushPtr auroraGlowBrush_;
    float auroraGlowBlend_ = 0, settingsOpacity_ = 1;
    double lastGlowTime_ = -1;
    int bitmapWidth_ = 256, bitmapHeight_ = 100;
    D2D1_COLOR_F currentAccent_ = D2D1::ColorF(0.7975f, 0.85f, 0.9025f);
    std::vector<D2D1_RECT_F> shapes;
    void FillIslandShape(D2D1_RECT_F rect, float, bool, bool, Brush*) { shapes.push_back(rect); }
    // UPDATE_IMPLEMENTATION
    // DRAW_IMPLEMENTATION
};
int main() {
    Settings settings;
    SharedState state;
    Renderer renderer;
    renderer.UpdateAmbientGlow(settings, true, true, 0);
    const auto rect = D2D1::RectF(28, 22, 228, 58);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Timer, 0);
    assert(renderer.shapes.empty());
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 0);
    assert(!renderer.shapes.empty() && renderer.auroraGlowBlend_ == 1);
    assert(renderer.target.brush.color.b > renderer.target.brush.color.r);
    assert(std::fabs(renderer.target.brush.color.r - 0.7975f) < 0.001f);

    bool animating = true;
    for (int i = 1; i <= 120; ++i) animating = renderer.UpdateAmbientGlow(settings, true, false, i / 60.0);
    assert(renderer.auroraGlowBlend_ == 0.65f && !animating);
    settings.ambientAuroraGlow = false;
    for (int i = 121; i <= 240; ++i) animating = renderer.UpdateAmbientGlow(settings, true, false, i / 60.0);
    assert(renderer.auroraGlowBlend_ == 0 && !animating);
    settings.ambientAuroraGlow = true;
    for (int i = 241; i <= 360; ++i) renderer.UpdateAmbientGlow(settings, true, true, i / 60.0);
    state.media.art.accentIsNeutral = true;
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 6);
    assert(renderer.target.brush.color.r == 0.92f);
    const auto bg = D2D1::ColorF(0xF5F6F8);
    const auto uiAccent = EnsureContrastAgainstBackground(renderer.target.brush.color, bg);
    assert((RelativeLuminance(bg) + 0.05) / (RelativeLuminance(uiAccent) + 0.05) >= 3);

    for (float scale : {0.7f, 1.0f, 2.5f, 3.0f}) {
        settings.sizeScale = scale;
        for (bool notch : {false, true}) {
            settings.notchStyle = notch;
            for (auto edgeRect : {rect, D2D1::RectF(5, notch ? 0.0f : 4.0f, 251, 95)}) {
                renderer.shapes.clear();
                renderer.DrawAmbientAuroraGlow(edgeRect, 18, state, settings, IslandKind::Media, 6);
                assert(!renderer.shapes.empty());
                for (const auto& r : renderer.shapes)
                    assert(r.left >= 0 && r.top >= 0 && r.right <= 256 && r.bottom <= 100);
            }
        }
    }
    assert(LoadGlowSettings().ambientAuroraGlow);
    Wh_SetIntValue(L"AmbientAuroraGlowOverride", 0);
    Wh_SetIntValue(L"AmbientAuroraGlowOverrideBase", 1);
    assert(!LoadGlowSettings().ambientAuroraGlow);
    setting = 0;
    assert(!LoadGlowSettings().ambientAuroraGlow);
    assert(Wh_GetIntValue(L"AmbientAuroraGlowOverride", -1) == -1);
    setting = 1;
    assert(LoadGlowSettings().ambientAuroraGlow);
}
