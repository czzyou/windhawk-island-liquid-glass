#include <windows.h>
#include <d2d1.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <map>
#include <string>
#include <vector>

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
// COLOR_IMPLEMENTATION
// BITMAP_IMPLEMENTATION
// PALETTE_IMPLEMENTATION
// FLUID_IMPLEMENTATION
enum class IslandKind { Idle, Media, Timer };
enum class BackdropMaterial { None, Acrylic };
enum class AccentMode { Auto, Custom };
struct Settings {
    bool ambientAuroraGlow = true, notchStyle = false, borderMergedMode = false, w11Style = false;
    float ambientGlowIntensity = 1, sizeScale = 1;
    BackdropMaterial backdropMaterial = BackdropMaterial::None;
    AccentMode accentMode = AccentMode::Auto;
};
struct SharedState { struct { bool playing = true; BitmapPixels art; } media; };
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
struct Brush {
    D2D1_COLOR_F color{};
    void SetOpacity(float) {}
    void SetTransform(D2D1_MATRIX_3X2_F) {}
};
struct BrushPtr {
    Brush* value = nullptr;
    Brush* operator->() { return value; }
    Brush* Get() { return value; }
    explicit operator bool() { return value != nullptr; }
    Brush** operator&() { return &value; }
    void Reset() { value = nullptr; }
};
struct Bitmap {
    GlowFieldPixels pixels{};
    int copies = 0;
    HRESULT CopyFromMemory(const D2D1_RECT_U*, const void* source, UINT pitch) {
        assert(pitch == kGlowFieldWidth * 4);
        std::copy_n(static_cast<const uint8_t*>(source), pixels.size(), pixels.begin());
        ++copies; return S_OK;
    }
};
struct BitmapPtr {
    Bitmap* value = nullptr;
    Bitmap* operator->() { return value; }
    Bitmap* Get() { return value; }
    explicit operator bool() { return value != nullptr; }
    Bitmap** operator&() { return &value; }
};
struct Target {
    Brush brush;
    Bitmap bitmap;
    HRESULT CreateBitmap(D2D1_SIZE_U, const void* source, UINT pitch,
        D2D1_BITMAP_PROPERTIES, Bitmap** result) {
        bitmap.CopyFromMemory(nullptr, source, pitch); *result = &bitmap; return S_OK;
    }
    HRESULT CreateBitmapBrush(Bitmap* image, Brush** result) {
        brush.color = D2D1::ColorF(image->pixels[2]/255.0f, image->pixels[1]/255.0f, image->pixels[0]/255.0f);
        *result = &brush; return S_OK;
    }
};
struct Renderer {
    Target target;
    Target* target_ = &target;
    BrushPtr auroraGlowBrush_;
    BitmapPtr auroraGlowBitmap_;
    double lastGlowFieldTime_ = -1;
    float lastGlowFieldPhase_ = -1;
    uint64_t lastGlowArt_ = 0;
    D2D1_COLOR_F lastGlowAccent_{};
    bool lastGlowAuto_ = true;
    float auroraGlowBlend_ = 0, settingsOpacity_ = 1;
    double lastGlowTime_ = -1, lastGlowMotionCheck_ = -1;
    float glowPhase_ = 0;
    bool glowMotionEnabled_ = true;
    int bitmapWidth_ = 256, bitmapHeight_ = 100;
    D2D1_COLOR_F currentAccent_ = D2D1::ColorF(0.7975f, 0.85f, 0.9025f);
    std::vector<D2D1_RECT_F> shapes;
    void FillIslandShape(D2D1_RECT_F rect, float, bool, bool, Brush*) {
        shapes.push_back(rect);
        target.brush.color = D2D1::ColorF(target.bitmap.pixels[2]/255.0f,
            target.bitmap.pixels[1]/255.0f, target.bitmap.pixels[0]/255.0f);
    }
    // UPDATE_IMPLEMENTATION
    // DRAW_IMPLEMENTATION
};
int main() {
    BitmapPixels cover;
    cover.width = 10; cover.height = 10;
    cover.bgra.resize(400);
    for (int i = 0; i < 100; ++i) {
        cover.bgra[i*4 + (i < 70 ? 2 : 0)] = 255;
        cover.bgra[i*4+3] = 255;
    }
    SampleAlbumGlow(cover);
    assert(cover.glowColorCount == 2);
    assert(std::fabs(cover.glowWeights[0] - 0.7f) < 0.001f);
    assert(std::fabs(cover.glowWeights[1] - 0.3f) < 0.001f);
    assert(cover.glowColors[0].r == 1 && cover.glowColors[1].b == 1);
    for (int i = 0; i < 90; ++i)
        cover.bgra[i*4] = cover.bgra[i*4+1] = cover.bgra[i*4+2] = 200;
    SampleAlbumGlow(cover);
    assert(std::fabs(cover.glowWeights[0] - 0.9f) < 0.001f);
    std::fill(cover.bgra.begin(), cover.bgra.end(), 0);
    cover.glowColorCount = 0;
    SampleAlbumGlow(cover);
    assert(cover.glowColorCount == 0);

    Settings settings;
    SharedState state;
    state.media.art.accentIsNeutral = false;
    Renderer renderer;
    renderer.UpdateAmbientGlow(settings, true, true, 0);
    const auto rect = D2D1::RectF(28, 22, 228, 58);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Timer, 0);
    assert(renderer.shapes.empty());
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 0);
    assert(!renderer.shapes.empty() && renderer.auroraGlowBlend_ == 1);
    assert(renderer.target.brush.color.b > renderer.target.brush.color.r);
    assert(std::fabs(renderer.target.brush.color.r - 0.7975f) < 0.005f);

    const float phaseBefore = renderer.glowPhase_;
    renderer.UpdateAmbientGlow(settings, true, true, 0.1);
    assert(std::fabs(renderer.glowPhase_ - phaseBefore - 0.055f) < 0.0001f);
    const float pausedPhase = renderer.glowPhase_;
    bool animating = true;
    for (int i = 1; i <= 120; ++i) animating = renderer.UpdateAmbientGlow(settings, true, false, i / 60.0);
    assert(renderer.auroraGlowBlend_ == 0.65f && !animating);
    assert(renderer.glowPhase_ == pausedPhase);
    settings.ambientAuroraGlow = false;
    for (int i = 121; i <= 240; ++i) animating = renderer.UpdateAmbientGlow(settings, true, false, i / 60.0);
    assert(renderer.auroraGlowBlend_ == 0 && !animating);
    settings.ambientAuroraGlow = true;
    for (int i = 241; i <= 360; ++i) renderer.UpdateAmbientGlow(settings, true, true, i / 60.0);
    state.media.art.accentIsNeutral = true;
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 6);
    assert(std::fabs(renderer.target.brush.color.r - 0.92f) < 0.005f);
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
                for (const auto& r : renderer.shapes) {
                    assert(r.left >= 0 && r.top >= 0 && r.right <= 256 && r.bottom <= 100);
                    assert(r.left >= edgeRect.left-6.51f && r.right <= edgeRect.right+6.51f);
                    assert(r.top >= edgeRect.top-6.51f && r.bottom <= edgeRect.bottom+7.55f);
                }
            }
        }
    }
    state.media.art = cover;
    for (int i = 0; i < 100; ++i) {
        cover.bgra[i*4 + (i < 70 ? 2 : 0)] = 255;
        cover.bgra[i*4+3] = 255;
    }
    SampleAlbumGlow(cover);
    ++cover.generation;
    state.media.art = cover;
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 6);
    const auto oldField = renderer.target.bitmap.pixels;
    assert(oldField == BuildAlbumGlowField(cover.glowColors.data(), cover.glowWeights.data(),
        cover.glowColorCount, renderer.glowPhase_));
    for (int i = 1; i <= 60; ++i) renderer.UpdateAmbientGlow(settings, true, true, 6 + i/60.0);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 7);
    assert(renderer.target.bitmap.pixels != oldField);
    const auto& field = renderer.target.bitmap.pixels;
    int changed = 0, verticalVariation = 0;
    for (size_t i = 0; i < field.size(); i += 4) {
        changed += field[i] != oldField[i];
        assert(field[i+1] == 0 && field[i+3] == 255);
        assert(std::abs(int(field[i])*field[i]+int(field[i+2])*field[i+2]-255*255) <= 510);
        if (i < kGlowFieldWidth*4)
            verticalVariation += field[i] != field[i+(kGlowFieldHeight-1)*kGlowFieldWidth*4];
    }
    assert(changed > kGlowFieldWidth*kGlowFieldHeight/4 && verticalVariation > kGlowFieldWidth/2);
    // Motion is continuous per frame and visible over two seconds, without losing palette dominance.
    for (float phase : {0.0f, 1.5f, 4.0f, 8.0f}) {
        const auto a = BuildAlbumGlowField(cover.glowColors.data(), cover.glowWeights.data(), 2, phase);
        const auto b = BuildAlbumGlowField(cover.glowColors.data(), cover.glowWeights.data(), 2, phase+0.55f/30);
        const auto later = BuildAlbumGlowField(cover.glowColors.data(), cover.glowWeights.data(), 2, phase+0.55f*2);
        int visibleChanges = 0;
        double redEnergy = 0, blueEnergy = 0;
        for (size_t i = 0; i < a.size(); i += 4) {
            for (int c = 0; c < 3; ++c) assert(std::abs(int(a[i+c])-int(b[i+c])) <= 3);
            visibleChanges += std::abs(int(a[i])-int(later[i])) >= 4;
            redEnergy += int(a[i+2])*a[i+2]; blueEnergy += int(a[i])*a[i];
        }
        assert(redEnergy > blueEnergy && blueEnergy / (redEnergy+blueEnergy) > 0.1);
        assert(visibleChanges > kGlowFieldWidth*kGlowFieldHeight/4);
    }
    renderer.UpdateAmbientGlow(settings,true,false,7.1);
    renderer.DrawAmbientAuroraGlow(rect,18,state,settings,IslandKind::Media,7.1);
    const int copies=renderer.target.bitmap.copies;
    renderer.UpdateAmbientGlow(settings,true,false,8);
    renderer.DrawAmbientAuroraGlow(rect,18,state,settings,IslandKind::Media,8);
    assert(renderer.target.bitmap.copies == copies);
    renderer.glowMotionEnabled_ = false;
    renderer.lastGlowMotionCheck_ = 8;
    const float stoppedPhase = renderer.glowPhase_;
    renderer.UpdateAmbientGlow(settings,true,true,8.1);
    assert(renderer.glowPhase_ == stoppedPhase);
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
