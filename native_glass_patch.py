from pathlib import Path


def apply_native_glass(src):
    root = Path(__file__).parent

    def change(old, new):
        nonlocal src
        assert src.count(old) == 1, (old[:100], src.count(old))
        src = src.replace(old, new, 1)

    # Inline the MIT native backend so the installed Windhawk source stays rebuildable.
    files = ['d3d_utils.h', 'panel.h', 'capture.h', 'renderer.h', 'session.h', 'stats.h',
             'd3d_utils.cc', 'panel.cc', 'capture.cc', 'renderer.cc', 'session.cc']
    native = '\n/* electron-liquid-glass, https://github.com/hicccc77/electron-liquid-glass\n'
    native += (root/'native-glass/LICENSE').read_text(encoding='utf-8') + '\n*/\n'
    for name in files:
        text = (root/'native-glass/src'/name).read_text(encoding='utf-8-sig')
        native += '\n'.join(line for line in text.splitlines()
                            if not line.startswith('#include "') and line != '#pragma once') + '\n'
    helper = (root/'native-island-glass.h').read_text(encoding='utf-8')
    native += '\n'.join(line for line in helper.splitlines()
                        if not line.startswith('#include "') and line != '#pragma once')
    change('\nnamespace {\n\nconstexpr wchar_t kWindowClass[]',
           native + '\n\nnamespace {\n\nconstexpr wchar_t kWindowClass[]')
    change('-lpdh -lwinmm\n', '-lpdh -lwinmm -ld3d11 -ldxgi -ldcomp -ld3dcompiler -ldxguid\n')
    change('''  - TintIntensity: 72''', '''  - LiquidGlass: false
    $name: Refractive Liquid Glass
    $description: 'Custom theme, backdrop Off: render live desktop refraction on the GPU. The effect excludes itself only from its desktop-duplication source to prevent feedback; ordinary screenshots remain available.'
  - TintIntensity: 72''')
    change('''    bool materialDepth = true;''', '''    bool nativeGlass = false;
    bool materialDepth = true;''')
    change('''    next.backdropTint = Clamp(''', '''    next.nativeGlass = Wh_GetIntSetting(L"Themes.LiquidGlass") != 0;
    next.backdropTint = Clamp(''')
    change('''   public:
    bool Initialize(HWND hwnd) {''', '''   public:
    static bool UsesNativeGlass(const Settings& settings) {
        return settings.nativeGlass && settings.themePreset == ThemePreset::Custom &&
            settings.backdropMaterial == BackdropMaterial::None && !settings.notchStyle && !settings.borderMergedMode;
    }
    bool NativeGlassChanged() {
        if (!nativeGlass_.Changed()) return false;
        Wh_SetIntValue(L"NativeGlassReady", nativeGlass_.Ready());
        Wh_SetIntValue(L"NativeGlassDarkText", 0);
        return true;
    }
    void ApplyGlassTheme(Settings& settings) const {
        if (!UsesNativeGlass(settings) || !nativeGlass_.Ready()) return;
        const float alpha = settings.pillBgColor.a;
        settings.pillBgColor = D2D1::ColorF(0x101014, alpha);
        settings.textPrimaryColor = D2D1::ColorF(0xF2F2F7);
        settings.textSecondaryColor = D2D1::ColorF(0xD1D1D6);
        settings.tintOpacity = 0;
    }

    bool Initialize(HWND hwnd) {''')
    change('''    HWND hwnd_ = nullptr;
    HDC memDc_''', '''    NativeIslandGlass nativeGlass_;
    HWND hwnd_ = nullptr;
    HDC memDc_''')
    change('''        target_->BeginDraw();
        target_->Clear''', '''        nativeGlass_.BeginFrame(hwnd_, UsesNativeGlass(settings));
        target_->BeginDraw();
        target_->Clear''')
    change('''        hr = target_->EndDraw();''', '''        nativeGlass_.EndFrame();
        hr = target_->EndDraw();''')
    change('''    void Shutdown() {
        marqueeTitleCache_''', '''    void Shutdown() {
        nativeGlass_.Shutdown();
        marqueeTitleCache_''')
    change('''        DrawSoftShadow(rect, radius);

        DrawPillSurface''', '''        nativeGlass_.Sync(rect.left, rect.top, rect.right, rect.bottom, radius,
            settings.sizeScale, settings.pillOpacity);
        DrawSoftShadow(rect, radius);

        DrawPillSurface''')
    change('''        if (needsRender) {
            // When Ctrl+hover''', '''        if (renderer.NativeGlassChanged()) needsRender = true;
        if (needsRender) {
            // When Ctrl+hover''')
    change('''            renderer.Render(snapshot, renderSettings, primary, secondary,''', '''            renderer.ApplyGlassTheme(renderSettings);
            renderer.Render(snapshot, renderSettings, primary, secondary,''')
    start = src.index('    void DrawNotification(')
    end = src.index('    void DrawVolume(', start)
    display = src[start:end]
    assert display.count('mutedBrush_->SetOpacity(0.75f);') == 1
    src = src[:start] + display.replace('mutedBrush_->SetOpacity(0.75f);',
        'mutedBrush_->SetOpacity(nativeGlass_.Ready() ? 0.95f : 0.75f);') + src[end:]
    return src
