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

// Control this external Windows setting without changing the host or runner.
BOOL testSystemAnimations = TRUE;
BOOL TestSystemParametersInfoW(UINT action, UINT, PVOID value, UINT) {
    assert(action == SPI_GETCLIENTAREAANIMATION);
    *static_cast<BOOL*>(value) = testSystemAnimations;
    return TRUE;
}
#define SystemParametersInfoW TestSystemParametersInfoW

float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }
// COLOR_IMPLEMENTATION
// BITMAP_IMPLEMENTATION
// PALETTE_IMPLEMENTATION
// FLUID_IMPLEMENTATION
// MOTION_IMPLEMENTATION
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
    float opacity = 0;
    void SetOpacity(float value) { opacity = value; }
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
    float lastGlowEnergy_ = -1;
    float lastGlowWidth_ = 0, lastGlowHeight_ = 0, lastGlowRadius_ = 0;
    uint64_t lastGlowArt_ = 0;
    D2D1_COLOR_F lastGlowAccent_{};
    bool lastGlowAuto_ = true;
    std::array<D2D1_COLOR_F, 2> glowColors_{}, glowStartColors_{}, glowTargetColors_{};
    double glowColorStart_ = -1;
    bool glowColorAnimating_ = false;
    GlowMotionState glowMotion_{};
    float auroraGlowBlend_ = 0, settingsOpacity_ = 1;
    double lastGlowTime_ = -1, lastGlowMotionCheck_ = -1;
    float glowPhase_ = 0;
    bool glowMotionEnabled_ = true;
    int bitmapWidth_ = 256, bitmapHeight_ = 100;
    D2D1_COLOR_F currentAccent_ = D2D1::ColorF(0.7975f, 0.85f, 0.9025f);
    std::vector<D2D1_RECT_F> shapes;
    std::vector<D2D1_RECT_F> rims;
    float rimWidth = 0, rimOpacity = 0;
    void DrawIslandShape(D2D1_RECT_F rect, float, bool, bool, Brush* brush, float width) {
        rims.push_back(rect); rimWidth = width; rimOpacity = brush->opacity;
    }
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

    GlowMotionState motion;
    for (int i = 0; i < 18; ++i) UpdateGlowMotion(motion, 0.1f, true, true, 1.0/60);
    assert(motion.playback > 0.94f && motion.playback < 0.96f);
    for (int i = 0; i < 120; ++i) UpdateGlowMotion(motion, 0.1f, true, true, 1.0/60);
    const float quiet = motion.energy;
    UpdateGlowMotion(motion, 1, true, true, 1.0/60);
    assert(motion.energy > quiet && motion.energy - quiet < 0.04f);
    for (int i = 0; i < 120; ++i) UpdateGlowMotion(motion, 1, true, true, 1.0/60);
    const float strong = motion.energy;
    assert(strong > quiet + 0.6f);
    UpdateGlowMotion(motion, 0, true, true, 1.0/60);
    assert(motion.energy < strong && motion.energy > strong - 0.02f);
    for (int i = 0; i < 600; ++i) UpdateGlowMotion(motion, 1, false, true, 1.0/60);
    assert(motion.energy == 0 && motion.playback == 0 && !motion.flowing);
    assert(!UpdateGlowMotion(motion, 1, false, true, 1.0/60));
    UpdateGlowMotion(motion, 1, true, false, 1.0/60);
    assert(motion.energy == 0 && motion.playback == 1 && !motion.flowing);
    assert(!UpdateGlowMotion(motion, 0.2f, true, false, 1.0/60));

    Settings settings;
    SharedState state;
    state.media.art.accentIsNeutral = false;
    Renderer renderer;
    renderer.UpdateAmbientGlow(settings, true, true, 0.1f, 0);
    const auto rect = D2D1::RectF(28, 22, 228, 58);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Timer, 0);
    assert(renderer.shapes.empty() && renderer.rims.empty());
    for (int i = 1; i <= 18; ++i) renderer.UpdateAmbientGlow(settings, true, true, 0.1f, i/60.0);
    assert(renderer.auroraGlowBlend_ > 0.94f);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 0.3);
    assert(renderer.rimOpacity > 0.10f);
    renderer.DrawAmbientAuroraRim(rect, 18, IslandKind::Media, settings);
    assert(renderer.shapes.empty() && !renderer.rims.empty());
    assert(renderer.glowColors_[0].b > renderer.glowColors_[0].r);
    const float quietWidth = renderer.rimWidth, quietOpacity = renderer.rimOpacity;
    const float quietEnergy = renderer.glowMotion_.energy;
    for (size_t i=renderer.rims.size()-10; i<renderer.rims.size(); ++i) {
        const auto r=renderer.rims[i];
        assert(r.top-rect.top < 4.5f && rect.bottom-r.bottom < 4.5f);
        assert(r.bottom-r.top > 26.0f);
    }
    for (int i = 19; i <= 180; ++i) renderer.UpdateAmbientGlow(settings, true, true, 1, i/60.0);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 3);
    renderer.DrawAmbientAuroraRim(rect, 18, IslandKind::Media, settings);
    assert(renderer.rimWidth == quietWidth);
    assert(renderer.rimOpacity*(0.275f+0.15f*renderer.glowMotion_.energy) > quietOpacity*(0.275f+0.15f*quietEnergy));
    const float phaseBefore = renderer.glowPhase_;
    renderer.UpdateAmbientGlow(settings, true, true, 1, 3.1);
    assert(std::fabs(renderer.glowPhase_ - phaseBefore - 0.0785398f) < 0.0001f);
    const float pausePhase = renderer.glowPhase_;
    renderer.UpdateAmbientGlow(settings, true, false, 1, 3.1 + 1.0/60);
    assert(renderer.glowPhase_ >= pausePhase && renderer.glowPhase_ - pausePhase < 0.014f);
    bool animating = true;
    for (int i = 1; i <= 600; ++i)
        animating = renderer.UpdateAmbientGlow(settings, true, false, 1, 3.2 + i/60.0);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 13.2);
    assert(renderer.auroraGlowBlend_ == 0.25f && renderer.glowMotion_.energy == 0);
    const float stoppedPhase = renderer.glowPhase_;
    const int pausedCopies = renderer.target.bitmap.copies;
    assert(!renderer.UpdateAmbientGlow(settings, true, false, 1, 13.3));
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 13.3);
    assert(renderer.glowPhase_ == stoppedPhase && renderer.target.bitmap.copies == pausedCopies);

    testSystemAnimations = FALSE;
    Renderer reduced;
    reduced.UpdateAmbientGlow(settings, true, true, 1, 0);
    for (int i = 1; i <= 120; ++i) reduced.UpdateAmbientGlow(settings, true, true, 1, i/60.0);
    reduced.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 2);
    for (int i = 121; i <= 180; ++i) reduced.UpdateAmbientGlow(settings, true, true, 1, i/60.0);
    reduced.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 3);
    assert(reduced.glowPhase_ == 0 && reduced.glowMotion_.energy == 0);
    assert(!reduced.UpdateAmbientGlow(settings, true, true, 0.1f, 3.1));
    testSystemAnimations = TRUE;
    assert(reduced.UpdateAmbientGlow(settings, true, true, 1, 4.1));
    assert(reduced.glowPhase_ > 0);

    for (float scale : {0.7f, 1.0f, 2.5f, 3.0f}) {
        settings.sizeScale = scale;
        for (bool notch : {false, true}) {
            settings.notchStyle = notch;
            for (auto edgeRect : {rect, D2D1::RectF(5, notch ? 0.0f : 4.0f, 251, 95)}) {
                renderer.shapes.clear(); renderer.rims.clear();
                renderer.DrawAmbientAuroraGlow(edgeRect, 18, state, settings, IslandKind::Media, 13.4);
                renderer.DrawAmbientAuroraRim(edgeRect, 18, IslandKind::Media, settings);
                assert(renderer.shapes.empty() && !renderer.rims.empty());
                for (const auto& r : renderer.rims) {
                    assert(r.left >= 0 && r.top >= 0 && r.right <= 256 && r.bottom <= 100);
                    assert(r.left >= edgeRect.left-5.51f && r.right <= edgeRect.right+5.51f);
                    assert(r.top >= edgeRect.top-5.51f && r.bottom <= edgeRect.bottom+6.4f);
                }
                assert(renderer.rimWidth < (edgeRect.bottom-edgeRect.top) * 0.5f);
            }
        }
    }
    settings.sizeScale = 1; settings.notchStyle = false;
    const auto priorColors = renderer.glowColors_;
    state.media.art.glowColorCount = 2;
    state.media.art.glowColors[0] = D2D1::ColorF(1, 0, 0);
    state.media.art.glowColors[1] = D2D1::ColorF(0, 0, 1);
    ++state.media.art.generation;
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 14);
    assert(renderer.glowColors_[0].r == priorColors[0].r);
    assert(renderer.UpdateAmbientGlow(settings, true, false, 0, 14.25));
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 14.25);
    const float halfRed = renderer.glowColors_[0].r;
    assert(std::fabs(halfRed - (priorColors[0].r + renderer.glowTargetColors_[0].r) * 0.5f) < 0.0001f);
    renderer.UpdateAmbientGlow(settings, true, false, 0, 14.5);
    renderer.DrawAmbientAuroraGlow(rect, 18, state, settings, IslandKind::Media, 14.5);
    assert(renderer.glowColors_[0].r == renderer.glowTargetColors_[0].r && !renderer.glowColorAnimating_);
    assert(renderer.glowColors_[0].g > 0 && renderer.glowColors_[0].r < 1);
    const float weights[] = {0.8f, 0.2f};
    for (float height : {36.0f, 100.0f}) {
        const auto top = AlbumGlowPoint(0, 200, height, 18);
        const auto right = AlbumGlowPoint(0.25f, 200, height, 18);
        const auto bottom = AlbumGlowPoint(0.5f, 200, height, 18);
        const auto left = AlbumGlowPoint(0.75f, 200, height, 18);
        assert(std::fabs(top.x-100)<0.0001f && std::fabs(top.y)<0.0001f);
        assert(std::fabs(right.x-200)<0.0001f && std::fabs(right.y-height/2)<0.0001f);
        assert(std::fabs(bottom.x-100)<0.0001f && std::fabs(bottom.y-height)<0.0001f);
        assert(std::fabs(left.x)<0.0001f && std::fabs(left.y-height/2)<0.0001f);
        const float a=82, b=height/2-18, quarter=a+b+1.57079633f*18;
        const auto corner = AlbumGlowPoint((a+0.78539816f*18)/(4*quarter),200,height,18);
        assert(std::fabs(std::hypot(corner.x-182,corner.y-18)-18)<0.0001f);
        const auto before=AlbumGlowPoint(0.99999f,200,height,18);
        assert(std::hypot(before.x-top.x,before.y-top.y)<0.01f);
    }
    const auto field = BuildAlbumGlowField(renderer.glowColors_.data(), weights, 2, 0, 200, 36, 18);
    const auto later = BuildAlbumGlowField(renderer.glowColors_.data(), weights, 2, 0.785398f*2, 200, 36, 18);
    const size_t topCenter = (kGlowFieldWidth/2)*4+3;
    const size_t rightCenter = ((kGlowFieldHeight/2)*kGlowFieldWidth+kGlowFieldWidth-1)*4+3;
    assert(field[topCenter] >= 50 && field[topCenter] <= 72 && field[rightCenter] == 0);
    assert(later[topCenter] == 0 && later[rightCenter] >= 50 && later[rightCenter] <= 72);
    const auto alphaAt = [&](int x,int y) { return field[(y*kGlowFieldWidth+x)*4+3]; };
    const int cx=kGlowFieldWidth/2;
    assert(alphaAt(cx,0)>alphaAt(cx,20) && alphaAt(cx,20)>alphaAt(cx,45));
    assert(alphaAt(cx,20)>0 && alphaAt(0,kGlowFieldHeight/2)==0);

    const D2D1_COLOR_F contrastColors[] = {D2D1::ColorF(1,0,0),D2D1::ColorF(0,0,1)};
    const auto tinted = BuildAlbumGlowField(contrastColors,weights,2,0,200,36,18);
    const auto loud = BuildAlbumGlowField(contrastColors,weights,2,0,200,36,18,1,1);
    const size_t beside = (kGlowFieldWidth/2-64)*4;
    assert(tinted[topCenter-1] > tinted[topCenter-3]);
    assert(tinted[beside] > tinted[beside+2]);
    assert(loud[topCenter] > tinted[topCenter]);
    int quietArea=0,loudArea=0;
    for(size_t i=3;i<tinted.size();i+=4) {quietArea += tinted[i]>10; loudArea += loud[i]>10;}
    assert(loudArea > quietArea * 2);
    int minAlpha = 255, maxAlpha = 0, changed = 0;
    for (size_t i = 0; i < field.size(); i += 4) {
        minAlpha = std::min(minAlpha, int(field[i+3])); maxAlpha = std::max(maxAlpha, int(field[i+3]));
        for (int c = 0; c < 3; ++c) {
            assert(field[i+c] <= field[i+3]);
        }
        changed += std::abs(int(field[i+3])-int(later[i+3])) > 20;
    }
    assert(minAlpha == 0 && maxAlpha >= 50 && maxAlpha <= 72 && changed > 100);

    settings.ambientAuroraGlow = false;
    for (int i = 1; i <= 600; ++i) animating = renderer.UpdateAmbientGlow(settings, true, true, 1, 15 + i/60.0);
    assert(renderer.auroraGlowBlend_ == 0 && renderer.glowMotion_.energy == 0 && !animating);
    settings.ambientAuroraGlow = true;
    assert(!renderer.UpdateAmbientGlow(settings, false, true, 1, 26));
    assert(renderer.auroraGlowBlend_ == 0 && !renderer.glowMotion_.flowing);
    settings.backdropMaterial = BackdropMaterial::Acrylic;
    assert(!renderer.UpdateAmbientGlow(settings, true, true, 1, 27));
    assert(renderer.auroraGlowBlend_ == 0);
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
