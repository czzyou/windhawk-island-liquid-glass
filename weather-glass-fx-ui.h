namespace WeatherFxTunerUi {

constexpr float kWinW = 468.0f;
constexpr float kWinH = 554.0f;
constexpr wchar_t kWndClassName[] = L"DynamicIslandWeatherFxTunerV2";

struct SliderSpec {
    const wchar_t* en;
    const wchar_t* zh;
    int minV;
    int maxV;
    int defV;
};

constexpr SliderSpec kSliders[5] = {
    { L"Intensity",  L"总强度",   0,   150, 100 },
    { L"Speed",      L"动画速度", 20,  250, 100 },
    { L"Drop Scale", L"颗粒大小", 50,  200, 100 },
    { L"Glass Fog",  L"玻璃雾气", 0,   100,  75 },
    { L"Refraction", L"折射高光", 0,   200, 100 },
};

constexpr int kChipModes[8] = { 0, 1, 2, 3, 4, 5, 6, -1 };

inline D2D1_RECT_F CloseBtnRect()  { return D2D1::RectF(422.0f, 17.0f, 450.0f, 45.0f); }
inline D2D1_RECT_F ResetBtnRect()  { return D2D1::RectF(18.0f,  502.0f, 126.0f, 536.0f); }
inline D2D1_RECT_F CancelBtnRect() { return D2D1::RectF(260.0f, 502.0f, 346.0f, 536.0f); }
inline D2D1_RECT_F SaveBtnRect()   { return D2D1::RectF(354.0f, 502.0f, 450.0f, 536.0f); }

inline D2D1_RECT_F ModeChipRect(int idx) {
    const int col = idx % 4;
    const int row = idx / 4;
    const float x = 30.0f + col * (96.0f + 8.0f);
    const float y = 104.0f + row * (32.0f + 8.0f);
    return D2D1::RectF(x, y, x + 96.0f, y + 32.0f);
}

inline D2D1_RECT_F ScopeSegRect(int idx) {
    const float x = 33.0f + idx * 134.0f;
    return D2D1::RectF(x, 228.0f, x + 134.0f, 250.0f);
}

inline D2D1_RECT_F SliderRowRect(int idx) {
    const float y = 307.0f + idx * 35.0f;
    return D2D1::RectF(26.0f, y, 442.0f, y + 32.0f);
}

inline D2D1_RECT_F SliderTrackRect(int idx) {
    const float cy = 307.0f + idx * 35.0f + 16.0f;
    return D2D1::RectF(124.0f, cy - 2.5f, 368.0f, cy + 2.5f);
}

inline bool PtInRectF(const D2D1_RECT_F& r, float x, float y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

struct ModeTheme {
    D2D1_COLOR_F accentStart;
    D2D1_COLOR_F accentEnd;
    D2D1_COLOR_F glow;
};

inline ModeTheme ThemeForMode(int mode) {
    switch (mode) {
        case 1:  return { D2D1::ColorF(0x2575F5), D2D1::ColorF(0x38BDF8), D2D1::ColorF(0x38BDF8, 0.16f) };
        case 2:  return { D2D1::ColorF(0x6366F1), D2D1::ColorF(0xA78BFA), D2D1::ColorF(0x818CF8, 0.17f) };
        case 3:  return { D2D1::ColorF(0x3B82F6), D2D1::ColorF(0x93C5FD), D2D1::ColorF(0x93C5FD, 0.15f) };
        case 4:  return { D2D1::ColorF(0x64748B), D2D1::ColorF(0xCBD5E1), D2D1::ColorF(0x94A3B8, 0.13f) };
        case 5:  return { D2D1::ColorF(0xEA8C1E), D2D1::ColorF(0xFCD34D), D2D1::ColorF(0xF59E0B, 0.16f) };
        case 6:  return { D2D1::ColorF(0x4F46E5), D2D1::ColorF(0x818CF8), D2D1::ColorF(0x6366F1, 0.17f) };
        case -1: return { D2D1::ColorF(0x475569), D2D1::ColorF(0x94A3B8), D2D1::ColorF(0x64748B, 0.10f) };
        default: return { D2D1::ColorF(0x2575F5), D2D1::ColorF(0x56CCF2), D2D1::ColorF(0x38BDF8, 0.15f) };
    }
}

struct TunerState {
    HWND hwnd = nullptr;
    float dpi = 96.0f;
    bool chinese = true;
    bool trackingMouse = false;
    bool saved = false;
    bool rolledBack = false;
    bool done = false;

    int mode = 0;
    int scope = 0;
    int values[5]{ 100, 100, 100, 75, 100 };

    int origMode = 0;
    int origScope = 0;
    int origValues[5]{ 100, 100, 100, 75, 100 };

    int hoverModeChip = -1;
    int hoverScopeSeg = -1;
    int hoverSlider = -1;
    int dragSlider = -1;
    int lastSlider = 0;
    int hoverBtn = -1; // 0=Close, 1=Reset, 2=Cancel, 3=Save
    int pressBtn = -1;

    Microsoft::WRL::ComPtr<ID2D1Factory> d2d;
    Microsoft::WRL::ComPtr<IDWriteFactory> dw;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> rt;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> roundStroke;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> fallback;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtTitle;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtSub;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtSec;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtSecR;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtHintR;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtChip;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtSeg;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtLabel;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtVal;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtBtn;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> fmtBtnLeft;

    void ReleaseTarget() {
        brush.Reset();
        rt.Reset();
    }

    void ReleaseAll() {
        ReleaseTarget();
        roundStroke.Reset();
        fmtTitle.Reset(); fmtSub.Reset(); fmtSec.Reset(); fmtSecR.Reset();
        fmtHintR.Reset(); fmtChip.Reset(); fmtSeg.Reset(); fmtLabel.Reset();
        fmtVal.Reset(); fmtBtn.Reset(); fmtBtnLeft.Reset();
        fallback.Reset();
        dw.Reset();
        d2d.Reset();
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> CreateFmt(float size, DWRITE_FONT_WEIGHT weight,
                                                        DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING) {
        Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
        if (!dw) return fmt;
        if (FAILED(dw->CreateTextFormat(L"SF Pro Text", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &fmt))) {
            dw->CreateTextFormat(L"Microsoft YaHei UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, size, L"zh-CN", &fmt);
        }
        if (fmt) {
            fmt->SetTextAlignment(align);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            if (fallback) {
                Microsoft::WRL::ComPtr<IDWriteTextFormat1> fmt1;
                if (SUCCEEDED(fmt.As(&fmt1))) fmt1->SetFontFallback(fallback.Get());
            }
        }
        return fmt;
    }

    bool InitDeviceIndependent() {
        if (!d2d) {
            if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2d.GetAddressOf()))) return false;
        }
        if (!dw) {
            if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                           reinterpret_cast<IUnknown**>(dw.GetAddressOf())))) return false;
        }
        if (!roundStroke) {
            D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
            d2d->CreateStrokeStyle(sp, nullptr, 0, &roundStroke);
        }
        if (!fallback && dw) {
            Microsoft::WRL::ComPtr<IDWriteFactory2> f2;
            Microsoft::WRL::ComPtr<IDWriteFontFallbackBuilder> builder;
            Microsoft::WRL::ComPtr<IDWriteFontFallback> sys;
            if (SUCCEEDED(dw.As(&f2)) && SUCCEEDED(f2->CreateFontFallbackBuilder(&builder))) {
                constexpr DWRITE_UNICODE_RANGE cjk[] = {
                    {0x2E80, 0x303F}, {0x3100, 0x312F}, {0x3400, 0x9FFF}, {0xF900, 0xFAFF}, {0xFF00, 0xFFEF}
                };
                const wchar_t* fams[] = { L"PingFang SC", L"Microsoft YaHei UI" };
                if (SUCCEEDED(builder->AddMapping(cjk, ARRAYSIZE(cjk), fams, ARRAYSIZE(fams), nullptr, nullptr, nullptr, 1.0f)) &&
                    SUCCEEDED(f2->GetSystemFontFallback(&sys)) &&
                    SUCCEEDED(builder->AddMappings(sys.Get()))) {
                    builder->CreateFontFallback(&fallback);
                }
            }
        }
        if (!fmtTitle) {
            fmtTitle   = CreateFmt(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            fmtSub     = CreateFmt(11.2f, DWRITE_FONT_WEIGHT_NORMAL);
            fmtSec     = CreateFmt(12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            fmtSecR    = CreateFmt(11.0f, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_TRAILING);
            fmtHintR   = CreateFmt(10.6f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_TRAILING);
            fmtChip    = CreateFmt(12.0f, DWRITE_FONT_WEIGHT_MEDIUM);
            fmtSeg     = CreateFmt(11.8f, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtLabel   = CreateFmt(12.5f, DWRITE_FONT_WEIGHT_NORMAL);
            fmtVal     = CreateFmt(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtBtn     = CreateFmt(12.6f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER);
            fmtBtnLeft = CreateFmt(12.6f, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_LEADING);
        }
        return true;
    }

    bool EnsureRenderTarget() {
        if (rt) return true;
        if (!hwnd || !InitDeviceIndependent()) return false;
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const UINT pxW = static_cast<UINT>((std::max)(1L, rc.right - rc.left));
        const UINT pxH = static_cast<UINT>((std::max)(1L, rc.bottom - rc.top));
        if (FAILED(d2d->CreateHwndRenderTarget(
                D2D1::RenderTargetProperties(),
                D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(pxW, pxH)),
                &rt))) {
            return false;
        }
        rt->SetDpi(dpi, dpi);
        rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &brush);
        return rt && brush;
    }

    void ApplyLive() const {
        g_weatherFxOverride.store(mode, std::memory_order_relaxed);
        g_weatherFxScope.store(ClampInt(scope, 0, 2), std::memory_order_relaxed);
        g_weatherFxIntensityPct.store(ClampInt(values[0], kSliders[0].minV, kSliders[0].maxV), std::memory_order_relaxed);
        g_weatherFxSpeedPct.store(ClampInt(values[1], kSliders[1].minV, kSliders[1].maxV), std::memory_order_relaxed);
        g_weatherFxScalePct.store(ClampInt(values[2], kSliders[2].minV, kSliders[2].maxV), std::memory_order_relaxed);
        g_weatherFxFogPct.store(ClampInt(values[3], kSliders[3].minV, kSliders[3].maxV), std::memory_order_relaxed);
        g_weatherFxRefractPct.store(ClampInt(values[4], kSliders[4].minV, kSliders[4].maxV), std::memory_order_relaxed);
        g_layoutDirty = true;
    }

    void SaveAndClose() {
        ApplyLive();
        Wh_SetIntValue(L"WeatherGlassFxMode", g_weatherFxOverride.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxScope", g_weatherFxScope.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxIntensity", g_weatherFxIntensityPct.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxSpeed", g_weatherFxSpeedPct.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxScale", g_weatherFxScalePct.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxFog", g_weatherFxFogPct.load(std::memory_order_relaxed));
        Wh_SetIntValue(L"WeatherGlassFxRefract", g_weatherFxRefractPct.load(std::memory_order_relaxed));
        saved = true;
        done = true;
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    }

    void Rollback() {
        if (saved || rolledBack) return;
        rolledBack = true;
        g_weatherFxOverride.store(origMode, std::memory_order_relaxed);
        g_weatherFxScope.store(origScope, std::memory_order_relaxed);
        g_weatherFxIntensityPct.store(origValues[0], std::memory_order_relaxed);
        g_weatherFxSpeedPct.store(origValues[1], std::memory_order_relaxed);
        g_weatherFxScalePct.store(origValues[2], std::memory_order_relaxed);
        g_weatherFxFogPct.store(origValues[3], std::memory_order_relaxed);
        g_weatherFxRefractPct.store(origValues[4], std::memory_order_relaxed);
        g_layoutDirty = true;
    }

    void CancelAndClose() {
        Rollback();
        done = true;
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    }

    void ResetDefaults() {
        scope = 0;
        for (int i = 0; i < 5; ++i) values[i] = kSliders[i].defV;
        ApplyLive();
        if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
    }

    bool SetSliderFromX(int idx, float dipX) {
        if (idx < 0 || idx >= 5) return false;
        const D2D1_RECT_F tr = SliderTrackRect(idx);
        const float frac = (std::max)(0.0f, (std::min)(1.0f, (dipX - tr.left) / (tr.right - tr.left)));
        const int v = ClampInt(static_cast<int>(std::lround(kSliders[idx].minV + frac * (kSliders[idx].maxV - kSliders[idx].minV))),
                               kSliders[idx].minV, kSliders[idx].maxV);
        if (v == values[idx]) return false;
        values[idx] = v;
        ApplyLive();
        return true;
    }

    int HitButton(float x, float y) const {
        if (PtInRectF(CloseBtnRect(), x, y))  return 0;
        if (PtInRectF(ResetBtnRect(), x, y))  return 1;
        if (PtInRectF(CancelBtnRect(), x, y)) return 2;
        if (PtInRectF(SaveBtnRect(), x, y))   return 3;
        return -1;
    }

    int HitModeChip(float x, float y) const {
        for (int i = 0; i < 8; ++i) if (PtInRectF(ModeChipRect(i), x, y)) return i;
        return -1;
    }

    int HitScopeSeg(float x, float y) const {
        for (int i = 0; i < 3; ++i) if (PtInRectF(ScopeSegRect(i), x, y)) return i;
        return -1;
    }

    int HitSliderRow(float x, float y) const {
        for (int i = 0; i < 5; ++i) if (PtInRectF(SliderRowRect(i), x, y)) return i;
        return -1;
    }

    int HitSliderTrackArea(float x, float y) const {
        for (int i = 0; i < 5; ++i) {
            const D2D1_RECT_F row = SliderRowRect(i);
            const D2D1_RECT_F tr = SliderTrackRect(i);
            if (x >= tr.left - 12.0f && x <= tr.right + 12.0f && y >= row.top && y <= row.bottom) return i;
        }
        return -1;
    }
};

inline void DrawModeGlyph(ID2D1RenderTarget* rt, ID2D1Factory* factory, int mode,
                          float cx, float cy, float sz, ID2D1SolidColorBrush* brush,
                          ID2D1StrokeStyle* roundStroke) {
    const float r = sz * 0.5f;
    if (mode == 0) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
        if (SUCCEEDED(factory->CreatePathGeometry(&geo))) {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(geo->Open(&sink))) {
                const float o = r * 0.92f;
                const float i = r * 0.28f;
                sink->BeginFigure(D2D1::Point2F(cx, cy - o), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx + i, cy - i));
                sink->AddLine(D2D1::Point2F(cx + o, cy));
                sink->AddLine(D2D1::Point2F(cx + i, cy + i));
                sink->AddLine(D2D1::Point2F(cx, cy + o));
                sink->AddLine(D2D1::Point2F(cx - i, cy + i));
                sink->AddLine(D2D1::Point2F(cx - o, cy));
                sink->AddLine(D2D1::Point2F(cx - i, cy - i));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                rt->FillGeometry(geo.Get(), brush);
            }
        }
    } else if (mode == 1) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
        if (SUCCEEDED(factory->CreatePathGeometry(&geo))) {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(geo->Open(&sink))) {
                sink->BeginFigure(D2D1::Point2F(cx, cy - r * 0.92f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(cx + r * 0.72f, cy - r * 0.05f),
                    D2D1::Point2F(cx + r * 0.76f, cy + r * 0.88f),
                    D2D1::Point2F(cx, cy + r * 0.88f)));
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(cx - r * 0.76f, cy + r * 0.88f),
                    D2D1::Point2F(cx - r * 0.72f, cy - r * 0.05f),
                    D2D1::Point2F(cx, cy - r * 0.92f)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                rt->FillGeometry(geo.Get(), brush);
            }
        }
    } else if (mode == 2) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
        if (SUCCEEDED(factory->CreatePathGeometry(&geo))) {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(geo->Open(&sink))) {
                sink->BeginFigure(D2D1::Point2F(cx + r * 0.14f, cy - r * 0.96f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(D2D1::Point2F(cx - r * 0.62f, cy + r * 0.08f));
                sink->AddLine(D2D1::Point2F(cx - r * 0.02f, cy + r * 0.08f));
                sink->AddLine(D2D1::Point2F(cx - r * 0.22f, cy + r * 0.96f));
                sink->AddLine(D2D1::Point2F(cx + r * 0.64f, cy - r * 0.14f));
                sink->AddLine(D2D1::Point2F(cx + r * 0.04f, cy - r * 0.14f));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                rt->FillGeometry(geo.Get(), brush);
            }
        }
    } else if (mode == 3) {
        for (int k = 0; k < 3; ++k) {
            const float ang = k * 3.14159265f / 3.0f;
            const float dx = std::cos(ang) * r * 0.88f;
            const float dy = std::sin(ang) * r * 0.88f;
            rt->DrawLine(D2D1::Point2F(cx - dx, cy - dy), D2D1::Point2F(cx + dx, cy + dy),
                         brush, 1.35f, roundStroke);
        }
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r * 0.24f, r * 0.24f), brush);
    } else if (mode == 4) {
        rt->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(cx - r * 0.86f, cy - r * 0.16f, cx + r * 0.86f, cy + r * 0.36f), r * 0.26f, r * 0.26f),
            brush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - r * 0.22f, cy - r * 0.34f), r * 0.42f, r * 0.38f), brush);
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + r * 0.30f, cy - r * 0.20f), r * 0.31f, r * 0.29f), brush);
        rt->DrawLine(D2D1::Point2F(cx - r * 0.56f, cy + r * 0.78f),
                     D2D1::Point2F(cx + r * 0.44f, cy + r * 0.78f), brush, 1.15f, roundStroke);
    } else if (mode == 5) {
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r * 0.44f, r * 0.44f), brush);
        for (int k = 0; k < 8; ++k) {
            const float ang = k * 3.14159265f / 4.0f;
            const float c = std::cos(ang), s = std::sin(ang);
            rt->DrawLine(D2D1::Point2F(cx + c * r * 0.68f, cy + s * r * 0.68f),
                         D2D1::Point2F(cx + c * r * 0.94f, cy + s * r * 0.94f),
                         brush, 1.25f, roundStroke);
        }
    } else if (mode == 6) {
        Microsoft::WRL::ComPtr<ID2D1PathGeometry> geo;
        if (SUCCEEDED(factory->CreatePathGeometry(&geo))) {
            Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
            if (SUCCEEDED(geo->Open(&sink))) {
                const float mr = r * 0.76f;
                sink->BeginFigure(D2D1::Point2F(cx + mr * 0.20f, cy - mr * 0.92f), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(cx + mr * 0.85f, cy + mr * 0.46f),
                    D2D1::SizeF(mr, mr), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(cx + mr * 0.20f, cy - mr * 0.92f),
                    D2D1::SizeF(mr * 0.78f, mr * 0.78f), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                rt->FillGeometry(geo.Get(), brush);
            }
        }
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + r * 0.56f, cy - r * 0.48f), r * 0.16f, r * 0.16f), brush);
    } else {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r * 0.74f, r * 0.74f), brush, 1.35f);
        rt->DrawLine(D2D1::Point2F(cx - r * 0.50f, cy - r * 0.50f),
                     D2D1::Point2F(cx + r * 0.50f, cy + r * 0.50f), brush, 1.35f, roundStroke);
    }
}

inline void PaintTuner(TunerState& st) {
    if (!st.EnsureRenderTarget()) return;
    ID2D1RenderTarget* rt = st.rt.Get();
    ID2D1Factory* d2d = st.d2d.Get();
    ID2D1SolidColorBrush* brush = st.brush.Get();
    ID2D1StrokeStyle* roundStroke = st.roundStroke.Get();
    const bool chinese = st.chinese;
    const ModeTheme theme = ThemeForMode(st.mode);

    rt->BeginDraw();

    // 1. Deep Obsidian Glass Background Gradient
    {
        D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, D2D1::ColorF(0x161922, 1.0f) },
            { 1.0f, D2D1::ColorF(0x0F1117, 1.0f) }
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> bgBrush;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
            SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, kWinH)),
                gsc.Get(), &bgBrush))) {
            rt->FillRectangle(D2D1::RectF(0, 0, kWinW, kWinH), bgBrush.Get());
        } else {
            rt->Clear(D2D1::ColorF(0x13161E, 1.0f));
        }
    }

    // 2. Subtle Mode-Tinted Top Ambient Radial Bloom
    {
        D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, theme.glow },
            { 1.0f, D2D1::ColorF(theme.glow.r, theme.glow.g, theme.glow.b, 0.0f) }
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
        Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> rad;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
            SUCCEEDED(rt->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(D2D1::Point2F(kWinW * 0.32f, -12.0f), D2D1::Point2F(0, 0), 340.0f, 165.0f),
                gsc.Get(), &rad))) {
            rt->FillRectangle(D2D1::RectF(0, 0, kWinW, 180.0f), rad.Get());
        }
    }

    // 3. Header Bar
    {
        const D2D1_RECT_F badgeR = D2D1::RectF(20.0f, 16.0f, 50.0f, 46.0f);
        brush->SetColor(D2D1::ColorF(theme.accentEnd.r, theme.accentEnd.g, theme.accentEnd.b, 0.16f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(badgeR, 9.0f, 9.0f), brush);
        brush->SetColor(D2D1::ColorF(theme.accentEnd.r, theme.accentEnd.g, theme.accentEnd.b, 0.42f));
        rt->DrawRoundedRectangle(D2D1::RoundedRect(badgeR, 9.0f, 9.0f), brush, 1.0f);
        brush->SetColor(theme.accentEnd);
        DrawModeGlyph(rt, d2d, st.mode, 35.0f, 31.0f, 15.0f, brush, roundStroke);

        const wchar_t* title = chinese ? L"天气液态玻璃动效 · 实时调节" : L"Weather Glass FX · Live Tuner";
        brush->SetColor(D2D1::ColorF(0xF5F7FC, 1.0f));
        rt->DrawTextW(title, static_cast<UINT32>(wcslen(title)), st.fmtTitle.Get(),
                      D2D1::RectF(60.0f, 12.0f, 410.0f, 33.0f), brush);

        brush->SetColor(D2D1::ColorF(0x34D399, 0.25f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(64.5f, 42.5f), 5.8f, 5.8f), brush);
        brush->SetColor(D2D1::ColorF(0x34D399, 1.0f));
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(64.5f, 42.5f), 2.9f, 2.9f), brush);

        const wchar_t* sub = chinese ? L"实时预览中 · 拖动滑块上方灵动岛即刻生效" : L"Live preview active · changes apply to island instantly";
        brush->SetColor(D2D1::ColorF(0x8D96AA, 1.0f));
        rt->DrawTextW(sub, static_cast<UINT32>(wcslen(sub)), st.fmtSub.Get(),
                      D2D1::RectF(74.0f, 34.0f, 414.0f, 51.0f), brush);

        const D2D1_RECT_F cr = CloseBtnRect();
        const float ccx = (cr.left + cr.right) * 0.5f;
        const float ccy = (cr.top + cr.bottom) * 0.5f;
        if (st.pressBtn == 0) {
            brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.16f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ccx, ccy), 14.0f, 14.0f), brush);
        } else if (st.hoverBtn == 0) {
            brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.09f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ccx, ccy), 14.0f, 14.0f), brush);
        }
        brush->SetColor(st.hoverBtn == 0 ? D2D1::ColorF(0xFFFFFF, 0.95f) : D2D1::ColorF(0x8E97AB, 0.90f));
        rt->DrawLine(D2D1::Point2F(ccx - 4.5f, ccy - 4.5f), D2D1::Point2F(ccx + 4.5f, ccy + 4.5f), brush, 1.45f, roundStroke);
        rt->DrawLine(D2D1::Point2F(ccx + 4.5f, ccy - 4.5f), D2D1::Point2F(ccx - 4.5f, ccy + 4.5f), brush, 1.45f, roundStroke);
    }

    auto DrawGlassCard = [&](const D2D1_RECT_F& r) {
        D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, D2D1::ColorF(0x1E222E, 0.94f) },
            { 1.0f, D2D1::ColorF(0x171A24, 0.94f) }
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> cardFill;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
            SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left, r.bottom)),
                gsc.Get(), &cardFill))) {
            rt->FillRoundedRectangle(D2D1::RoundedRect(r, 14.0f, 14.0f), cardFill.Get());
        }
        D2D1_GRADIENT_STOP bStops[2] = {
            { 0.0f, D2D1::ColorF(0xFFFFFF, 0.14f) },
            { 1.0f, D2D1::ColorF(0xFFFFFF, 0.05f) }
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> bgsc;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> borderBrush;
        if (SUCCEEDED(rt->CreateGradientStopCollection(bStops, 2, &bgsc)) &&
            SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left, r.bottom)),
                bgsc.Get(), &borderBrush))) {
            rt->DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f), 13.5f, 13.5f),
                borderBrush.Get(), 1.0f);
        }
    };

    // 4. Card 1: Weather Mode Selector (2x4 Interactive Chips)
    {
        DrawGlassCard(D2D1::RectF(18.0f, 66.0f, 450.0f, 186.0f));
        const wchar_t* sec = chinese ? L"动效模式" : L"Effect Mode";
        brush->SetColor(D2D1::ColorF(0xA4ADC2, 1.0f));
        rt->DrawTextW(sec, static_cast<UINT32>(wcslen(sec)), st.fmtSec.Get(),
                      D2D1::RectF(31.0f, 75.0f, 160.0f, 94.0f), brush);

        const wchar_t* activeBadgeZh[8] = {
            L"自动匹配当前天气", L"下雨 · 真实水珠折射", L"雷暴 · 水珠与闪电",
            L"下雪 · 柔焦飘雪", L"多云 · 流动雾霭", L"晴天 · 暖金阳光",
            L"晴夜 · 深邃星空", L"已关闭天气动效"
        };
        const wchar_t* activeBadgeEn[8] = {
            L"Auto-matches live weather", L"Rain · Glass droplets", L"Storm · Rain & lightning",
            L"Snow · Soft bokeh flakes", L"Cloudy · Drifting mist", L"Clear Day · Warm sunbeams",
            L"Clear Night · Starfield", L"Weather FX disabled"
        };
        int activeChipIdx = 0;
        for (int i = 0; i < 8; ++i) if (kChipModes[i] == st.mode) activeChipIdx = i;
        const wchar_t* badge = chinese ? activeBadgeZh[activeChipIdx] : activeBadgeEn[activeChipIdx];
        brush->SetColor(theme.accentEnd);
        rt->DrawTextW(badge, static_cast<UINT32>(wcslen(badge)), st.fmtSecR.Get(),
                      D2D1::RectF(170.0f, 75.0f, 437.0f, 94.0f), brush);

        const wchar_t* chipZh[8] = { L"跟随实时", L"下雨", L"雷暴", L"下雪", L"多云薄雾", L"晴天", L"晴夜星空", L"关闭动效" };
        const wchar_t* chipEn[8] = { L"Live Auto", L"Rain", L"Storm", L"Snow", L"Cloud/Fog", L"Clear Day", L"Night Sky", L"Off" };

        for (int i = 0; i < 8; ++i) {
            const D2D1_RECT_F r = ModeChipRect(i);
            const bool selected = (kChipModes[i] == st.mode);
            const bool hovered = (st.hoverModeChip == i);
            const ModeTheme chipTheme = ThemeForMode(kChipModes[i]);

            if (selected) {
                D2D1_GRADIENT_STOP stops[2] = {
                    { 0.0f, D2D1::ColorF(chipTheme.accentEnd.r, chipTheme.accentEnd.g, chipTheme.accentEnd.b, 0.25f) },
                    { 1.0f, D2D1::ColorF(chipTheme.accentStart.r, chipTheme.accentStart.g, chipTheme.accentStart.b, 0.14f) }
                };
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
                Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> selBrush;
                if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
                    SUCCEEDED(rt->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.left, r.top), D2D1::Point2F(r.left, r.bottom)),
                        gsc.Get(), &selBrush))) {
                    rt->FillRoundedRectangle(D2D1::RoundedRect(r, 9.0f, 9.0f), selBrush.Get());
                }
                brush->SetColor(D2D1::ColorF(chipTheme.accentEnd.r, chipTheme.accentEnd.g, chipTheme.accentEnd.b, 0.78f));
                rt->DrawRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f), 8.5f, 8.5f),
                    brush, 1.1f);
            } else {
                brush->SetColor(hovered ? D2D1::ColorF(0x252B3B, 1.0f) : D2D1::ColorF(0x131620, 0.96f));
                rt->FillRoundedRectangle(D2D1::RoundedRect(r, 9.0f, 9.0f), brush);
                brush->SetColor(hovered ? D2D1::ColorF(0xFFFFFF, 0.16f) : D2D1::ColorF(0xFFFFFF, 0.07f));
                rt->DrawRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f), 8.5f, 8.5f),
                    brush, 1.0f);
            }

            const float cy = (r.top + r.bottom) * 0.5f;
            brush->SetColor(selected ? chipTheme.accentEnd : (hovered ? D2D1::ColorF(0xDCE3F2, 0.92f) : D2D1::ColorF(0x8D97AC, 0.90f)));
            DrawModeGlyph(rt, d2d, kChipModes[i], r.left + 16.5f, cy, 12.5f, brush, roundStroke);

            const wchar_t* lbl = chinese ? chipZh[i] : chipEn[i];
            brush->SetColor(selected ? D2D1::ColorF(0xFFFFFF, 1.0f) : (hovered ? D2D1::ColorF(0xEAEFF8, 1.0f) : D2D1::ColorF(0xBAC3D6, 1.0f)));
            rt->DrawTextW(lbl, static_cast<UINT32>(wcslen(lbl)), st.fmtChip.Get(),
                          D2D1::RectF(r.left + 28.0f, r.top, r.right - 5.0f, r.bottom), brush);
        }
    }

    // 5. Card 2: Display Scope Segmented Control
    {
        DrawGlassCard(D2D1::RectF(18.0f, 196.0f, 450.0f, 265.0f));
        const wchar_t* sec = chinese ? L"显示范围" : L"Display Scope";
        brush->SetColor(D2D1::ColorF(0xA4ADC2, 1.0f));
        rt->DrawTextW(sec, static_cast<UINT32>(wcslen(sec)), st.fmtSec.Get(),
                      D2D1::RectF(31.0f, 203.0f, 150.0f, 221.0f), brush);

        const wchar_t* hint = chinese ? L"调节期间临时全局预览" : L"Temporarily previews live while open";
        brush->SetColor(D2D1::ColorF(0x6D768A, 1.0f));
        rt->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), st.fmtHintR.Get(),
                      D2D1::RectF(160.0f, 203.0f, 437.0f, 221.0f), brush);

        const D2D1_RECT_F well = D2D1::RectF(30.0f, 225.0f, 438.0f, 253.0f);
        brush->SetColor(D2D1::ColorF(0x11141C, 1.0f));
        rt->FillRoundedRectangle(D2D1::RoundedRect(well, 8.5f, 8.5f), brush);
        brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.08f));
        rt->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(well.left + 0.5f, well.top + 0.5f, well.right - 0.5f, well.bottom - 0.5f), 8.0f, 8.0f),
            brush, 1.0f);

        const wchar_t* segZh[3] = { L"全局持续显示", L"仅展开时显示", L"仅天气页显示" };
        const wchar_t* segEn[3] = { L"Always", L"Expanded Only", L"Weather Tab" };
        for (int i = 0; i < 3; ++i) {
            const D2D1_RECT_F sr = ScopeSegRect(i);
            const bool sel = (st.scope == i);
            const bool hov = (st.hoverScopeSeg == i);
            if (sel) {
                D2D1_GRADIENT_STOP stops[2] = {
                    { 0.0f, D2D1::ColorF(0x30374A, 1.0f) },
                    { 1.0f, D2D1::ColorF(0x252B3B, 1.0f) }
                };
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
                Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> segBrush;
                if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
                    SUCCEEDED(rt->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(sr.left, sr.top), D2D1::Point2F(sr.left, sr.bottom)),
                        gsc.Get(), &segBrush))) {
                    rt->FillRoundedRectangle(D2D1::RoundedRect(sr, 6.0f, 6.0f), segBrush.Get());
                }
                brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.16f));
                rt->DrawRoundedRectangle(
                    D2D1::RoundedRect(D2D1::RectF(sr.left + 0.5f, sr.top + 0.5f, sr.right - 0.5f, sr.bottom - 0.5f), 5.5f, 5.5f),
                    brush, 0.9f);
            } else if (hov) {
                brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.05f));
                rt->FillRoundedRectangle(D2D1::RoundedRect(sr, 6.0f, 6.0f), brush);
            }
            const wchar_t* lbl = chinese ? segZh[i] : segEn[i];
            brush->SetColor(sel ? D2D1::ColorF(0xFFFFFF, 1.0f) : (hov ? D2D1::ColorF(0xDCE2EE, 1.0f) : D2D1::ColorF(0x8E97AB, 1.0f)));
            rt->DrawTextW(lbl, static_cast<UINT32>(wcslen(lbl)), st.fmtSeg.Get(), sr, brush);
        }
    }

    // 6. Card 3: Fine Tuning Sliders
    {
        DrawGlassCard(D2D1::RectF(18.0f, 275.0f, 450.0f, 490.0f));
        const wchar_t* sec = chinese ? L"精细参数" : L"Parameters";
        brush->SetColor(D2D1::ColorF(0xA4ADC2, 1.0f));
        rt->DrawTextW(sec, static_cast<UINT32>(wcslen(sec)), st.fmtSec.Get(),
                      D2D1::RectF(31.0f, 283.0f, 150.0f, 302.0f), brush);

        const wchar_t* hint = chinese ? L"滚轮微调 · 双击恢复默认" : L"Scroll to nudge · Double-click to reset";
        brush->SetColor(D2D1::ColorF(0x6D768A, 1.0f));
        rt->DrawTextW(hint, static_cast<UINT32>(wcslen(hint)), st.fmtHintR.Get(),
                      D2D1::RectF(170.0f, 283.0f, 437.0f, 302.0f), brush);

        for (int i = 0; i < 5; ++i) {
            const D2D1_RECT_F row = SliderRowRect(i);
            const D2D1_RECT_F track = SliderTrackRect(i);
            const float cy = (track.top + track.bottom) * 0.5f;
            const bool active = (st.dragSlider == i);
            const bool hovered = (st.hoverSlider == i) || active;

            if (i > 0) {
                brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.045f));
                rt->DrawLine(D2D1::Point2F(31.0f, row.top - 1.5f), D2D1::Point2F(437.0f, row.top - 1.5f), brush, 1.0f);
            }

            const wchar_t* lbl = chinese ? kSliders[i].zh : kSliders[i].en;
            brush->SetColor(hovered ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xDCE2EE, 0.95f));
            rt->DrawTextW(lbl, static_cast<UINT32>(wcslen(lbl)), st.fmtLabel.Get(),
                          D2D1::RectF(31.0f, row.top, 118.0f, row.bottom), brush);

            const float span = static_cast<float>(kSliders[i].maxV - kSliders[i].minV);
            const float frac = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(st.values[i] - kSliders[i].minV) / span));
            const float defFrac = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(kSliders[i].defV - kSliders[i].minV) / span));
            const float trackW = track.right - track.left;
            const float thumbX = track.left + frac * trackW;
            const float defX = track.left + defFrac * trackW;

            // Subtle default reference pip cleanly tucked beneath the rail
            if (std::fabs(thumbX - defX) > 7.5f) {
                brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.26f));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(defX, cy + 6.6f), 1.35f, 1.35f), brush);
            }

            // Inactive track rail
            brush->SetColor(D2D1::ColorF(0x0E1118, 1.0f));
            rt->FillRoundedRectangle(D2D1::RoundedRect(track, 2.5f, 2.5f), brush);
            brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.09f));
            rt->DrawRoundedRectangle(D2D1::RoundedRect(track, 2.5f, 2.5f), brush, 0.8f);

            // Active gradient track fill
            if (thumbX > track.left + 1.0f) {
                D2D1_GRADIENT_STOP stops[2] = {
                    { 0.0f, theme.accentStart },
                    { 1.0f, theme.accentEnd }
                };
                Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
                Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> fillBrush;
                if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
                    SUCCEEDED(rt->CreateLinearGradientBrush(
                        D2D1::LinearGradientBrushProperties(D2D1::Point2F(track.left, cy), D2D1::Point2F(track.right, cy)),
                        gsc.Get(), &fillBrush))) {
                    rt->FillRoundedRectangle(
                        D2D1::RoundedRect(D2D1::RectF(track.left, track.top, thumbX, track.bottom), 2.5f, 2.5f),
                        fillBrush.Get());
                }
            }

            // Thumb glow + shadow + crisp white knob
            const float thumbR = active ? 8.2f : (hovered ? 7.6f : 6.8f);
            if (hovered) {
                brush->SetColor(D2D1::ColorF(theme.accentEnd.r, theme.accentEnd.g, theme.accentEnd.b, active ? 0.30f : 0.18f));
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, cy), thumbR + 4.2f, thumbR + 4.2f), brush);
            }
            brush->SetColor(D2D1::ColorF(0x000000, 0.42f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, cy + 1.0f), thumbR + 0.6f, thumbR + 0.6f), brush);
            brush->SetColor(D2D1::ColorF(0xF8FAFC, 1.0f));
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, cy), thumbR, thumbR), brush);
            if (active) {
                brush->SetColor(theme.accentStart);
                rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, cy), 2.6f, 2.6f), brush);
            }

            // Tabular percentage pill
            const D2D1_RECT_F badge = D2D1::RectF(384.0f, cy - 10.5f, 438.0f, cy + 10.5f);
            brush->SetColor(active ? D2D1::ColorF(theme.accentStart.r, theme.accentStart.g, theme.accentStart.b, 0.22f)
                                   : D2D1::ColorF(0x11141C, 0.96f));
            rt->FillRoundedRectangle(D2D1::RoundedRect(badge, 6.0f, 6.0f), brush);
            brush->SetColor(active ? D2D1::ColorF(theme.accentEnd.r, theme.accentEnd.g, theme.accentEnd.b, 0.72f)
                                   : (hovered ? D2D1::ColorF(0xFFFFFF, 0.16f) : D2D1::ColorF(0xFFFFFF, 0.08f)));
            rt->DrawRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(badge.left + 0.5f, badge.top + 0.5f, badge.right - 0.5f, badge.bottom - 0.5f), 5.5f, 5.5f),
                brush, 1.0f);

            const std::wstring valStr = std::to_wstring(st.values[i]) + L"%";
            brush->SetColor(hovered ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xCDD5E4, 1.0f));
            rt->DrawTextW(valStr.c_str(), static_cast<UINT32>(valStr.size()), st.fmtVal.Get(), badge, brush);
        }
    }

    // 7. Bottom Action Buttons
    auto DrawGhostBtn = [&](const D2D1_RECT_F& r, int btnId, const wchar_t* txt, bool withResetIcon) {
        const bool hov = (st.hoverBtn == btnId);
        const bool prs = (st.pressBtn == btnId);
        brush->SetColor(prs ? D2D1::ColorF(0x151822, 1.0f) : (hov ? D2D1::ColorF(0x252B3B, 1.0f) : D2D1::ColorF(0x1A1E29, 1.0f)));
        rt->FillRoundedRectangle(D2D1::RoundedRect(r, 9.0f, 9.0f), brush);
        brush->SetColor(hov ? D2D1::ColorF(0xFFFFFF, 0.20f) : D2D1::ColorF(0xFFFFFF, 0.11f));
        rt->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.left + 0.5f, r.top + 0.5f, r.right - 0.5f, r.bottom - 0.5f), 8.5f, 8.5f),
            brush, 1.0f);
        if (withResetIcon) {
            const float cx = r.left + (chinese ? 23.0f : 31.0f);
            const float cy = (r.top + r.bottom) * 0.5f;
            brush->SetColor(hov ? D2D1::ColorF(0xFFFFFF, 0.95f) : D2D1::ColorF(0xA6B0C4, 0.92f));
            Microsoft::WRL::ComPtr<ID2D1PathGeometry> arcGeo;
            if (SUCCEEDED(d2d->CreatePathGeometry(&arcGeo))) {
                Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
                if (SUCCEEDED(arcGeo->Open(&sink))) {
                    sink->BeginFigure(D2D1::Point2F(cx - 4.2f, cy - 2.2f), D2D1_FIGURE_BEGIN_HOLLOW);
                    sink->AddArc(D2D1::ArcSegment(
                        D2D1::Point2F(cx - 3.6f, cy + 3.4f),
                        D2D1::SizeF(4.8f, 4.8f), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    sink->Close();
                    rt->DrawGeometry(arcGeo.Get(), brush, 1.35f, roundStroke);
                }
            }
            rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy - 2.0f), D2D1::Point2F(cx - 4.6f, cy - 5.2f), brush, 1.35f, roundStroke);
            rt->DrawLine(D2D1::Point2F(cx - 4.5f, cy - 2.0f), D2D1::Point2F(cx - 1.4f, cy - 1.8f), brush, 1.35f, roundStroke);

            brush->SetColor(hov ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xCFD7E6, 1.0f));
            rt->DrawTextW(txt, static_cast<UINT32>(wcslen(txt)), st.fmtBtnLeft.Get(),
                          D2D1::RectF(cx + 10.5f, r.top, r.right - 6.0f, r.bottom), brush);
        } else {
            brush->SetColor(hov ? D2D1::ColorF(0xFFFFFF, 1.0f) : D2D1::ColorF(0xCFD7E6, 1.0f));
            rt->DrawTextW(txt, static_cast<UINT32>(wcslen(txt)), st.fmtBtn.Get(), r, brush);
        }
    };

    DrawGhostBtn(ResetBtnRect(), 1, chinese ? L"恢复默认" : L"Reset", true);
    DrawGhostBtn(CancelBtnRect(), 2, chinese ? L"取消" : L"Cancel", false);

    {
        const D2D1_RECT_F sr = SaveBtnRect();
        const bool hov = (st.hoverBtn == 3);
        const bool prs = (st.pressBtn == 3);
        D2D1_GRADIENT_STOP stops[2] = {
            { 0.0f, prs ? D2D1::ColorF(0x1D5FD6) : (hov ? D2D1::ColorF(0x3B8DFF) : D2D1::ColorF(0x2876F5)) },
            { 1.0f, prs ? D2D1::ColorF(0x2574E8) : (hov ? D2D1::ColorF(0x52B4FF) : D2D1::ColorF(0x399BF8)) }
        };
        Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> gsc;
        Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> ctaBrush;
        if (SUCCEEDED(rt->CreateGradientStopCollection(stops, 2, &gsc)) &&
            SUCCEEDED(rt->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(sr.left, sr.top), D2D1::Point2F(sr.right, sr.bottom)),
                gsc.Get(), &ctaBrush))) {
            rt->FillRoundedRectangle(D2D1::RoundedRect(sr, 9.0f, 9.0f), ctaBrush.Get());
        }
        brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.28f));
        rt->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(sr.left + 0.5f, sr.top + 0.5f, sr.right - 0.5f, sr.bottom - 0.5f), 8.5f, 8.5f),
            brush, 1.0f);
        const wchar_t* saveTxt = chinese ? L"保存设置" : L"Save";
        brush->SetColor(D2D1::ColorF(0xFFFFFF, 1.0f));
        rt->DrawTextW(saveTxt, static_cast<UINT32>(wcslen(saveTxt)), st.fmtBtn.Get(), sr, brush);
    }

    // 8. Subtle Glass Window Rim Hairline
    brush->SetColor(D2D1::ColorF(0xFFFFFF, 0.14f));
    rt->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0.5f, 0.5f, kWinW - 0.5f, kWinH - 0.5f), 8.0f, 8.0f),
        brush, 1.0f);

    if (rt->EndDraw() == D2DERR_RECREATE_TARGET) {
        st.ReleaseTarget();
        InvalidateRect(st.hwnd, nullptr, FALSE);
    }
}

inline HWND g_activeTunerHwnd = nullptr;

inline LRESULT CALLBACK TunerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<TunerState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<TunerState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        if (st) st->hwnd = hwnd;
        return TRUE;
    }
    if (!st) return DefWindowProcW(hwnd, message, wParam, lParam);

    const float scale = (st->dpi > 1.0f ? st->dpi : 96.0f) / 96.0f;

    switch (message) {
        case WM_CREATE: {
            const UINT curDpi = GetDpiForWindow(hwnd);
            st->dpi = curDpi > 0 ? static_cast<float>(curDpi) : 96.0f;
            const int targetW = static_cast<int>(std::lround(kWinW * st->dpi / 96.0f));
            const int targetH = static_cast<int>(std::lround(kWinH * st->dpi / 96.0f));
            RECT rc{};
            GetWindowRect(hwnd, &rc);
            if ((rc.right - rc.left) != targetW || (rc.bottom - rc.top) != targetH) {
                SetWindowPos(hwnd, nullptr, 0, 0, targetW, targetH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            BOOL useDark = TRUE;
            DwmSetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(20), &useDark, sizeof(useDark));
            const int roundPref = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(33), &roundPref, sizeof(roundPref));
            const COLORREF noBorder = 0xFFFFFFFE; // DWMWA_COLOR_NONE
            DwmSetWindowAttribute(hwnd, static_cast<DWMWINDOWATTRIBUTE>(34), &noBorder, sizeof(noBorder));
            const MARGINS margins{ 0, 0, 0, 1 };
            DwmExtendFrameIntoClientArea(hwnd, &margins);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            BeginPaint(hwnd, &ps);
            PaintTuner(*st);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DPICHANGED: {
            const float newDpi = static_cast<float>(HIWORD(wParam));
            st->dpi = newDpi > 1.0f ? newDpi : 96.0f;
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            const int w = static_cast<int>(std::lround(kWinW * st->dpi / 96.0f));
            const int h = static_cast<int>(std::lround(kWinH * st->dpi / 96.0f));
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            if (st->rt) {
                st->rt->SetDpi(st->dpi, st->dpi);
                st->rt->Resize(D2D1::SizeU(static_cast<UINT>((std::max)(1, w)), static_cast<UINT>((std::max)(1, h))));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            const float mx = static_cast<float>(pt.x) / scale;
            const float my = static_cast<float>(pt.y) / scale;
            if (my < 64.0f && !PtInRectF(CloseBtnRect(), mx, my)) return HTCAPTION;
            if (my > 492.0f && st->HitButton(mx, my) < 0) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                const bool interactive = (st->dragSlider >= 0 || st->hoverModeChip >= 0 ||
                                          st->hoverScopeSeg >= 0 || st->hoverSlider >= 0 || st->hoverBtn >= 0);
                SetCursor(LoadCursorW(nullptr, interactive ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            if (!st->trackingMouse) {
                TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                if (TrackMouseEvent(&tme)) st->trackingMouse = true;
            }
            const float mx = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
            const float my = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;
            if (st->dragSlider >= 0) {
                if (st->SetSliderFromX(st->dragSlider, mx)) {
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            const int nChip = st->HitModeChip(mx, my);
            const int nSeg  = st->HitScopeSeg(mx, my);
            const int nSld  = st->HitSliderRow(mx, my);
            const int nBtn  = st->HitButton(mx, my);
            if (nChip != st->hoverModeChip || nSeg != st->hoverScopeSeg ||
                nSld != st->hoverSlider || nBtn != st->hoverBtn) {
                st->hoverModeChip = nChip;
                st->hoverScopeSeg = nSeg;
                st->hoverSlider = nSld;
                st->hoverBtn = nBtn;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSELEAVE:
        case WM_NCMOUSEMOVE: {
            if (message == WM_MOUSELEAVE) st->trackingMouse = false;
            if (st->hoverModeChip != -1 || st->hoverScopeSeg != -1 || st->hoverSlider != -1 || st->hoverBtn != -1) {
                st->hoverModeChip = -1;
                st->hoverScopeSeg = -1;
                st->hoverSlider = -1;
                st->hoverBtn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (message == WM_NCMOUSEMOVE) break;
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            const float mx = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
            const float my = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;

            const int btn = st->HitButton(mx, my);
            if (btn >= 0) {
                st->pressBtn = btn;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const int chip = st->HitModeChip(mx, my);
            if (chip >= 0) {
                st->mode = kChipModes[chip];
                st->ApplyLive();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const int seg = st->HitScopeSeg(mx, my);
            if (seg >= 0) {
                st->scope = seg;
                st->ApplyLive();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            const int sld = st->HitSliderTrackArea(mx, my);
            if (sld >= 0) {
                st->dragSlider = sld;
                st->lastSlider = sld;
                st->hoverSlider = sld;
                SetCapture(hwnd);
                st->SetSliderFromX(sld, mx);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            const float mx = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
            const float my = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;
            const int sld = st->HitSliderRow(mx, my);
            if (sld >= 0) {
                st->lastSlider = sld;
                st->values[sld] = kSliders[sld].defV;
                st->ApplyLive();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            const float mx = static_cast<float>(GET_X_LPARAM(lParam)) / scale;
            const float my = static_cast<float>(GET_Y_LPARAM(lParam)) / scale;
            if (st->dragSlider >= 0) {
                st->dragSlider = -1;
                if (GetCapture() == hwnd) ReleaseCapture();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (st->pressBtn >= 0) {
                const int pressed = st->pressBtn;
                st->pressBtn = -1;
                if (GetCapture() == hwnd) ReleaseCapture();
                const int released = st->HitButton(mx, my);
                if (released == pressed) {
                    if (pressed == 0 || pressed == 2) { st->CancelAndClose(); return 0; }
                    if (pressed == 1) { st->ResetDefaults(); return 0; }
                    if (pressed == 3) { st->SaveAndClose(); return 0; }
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }
        case WM_CAPTURECHANGED: {
            if (st->dragSlider >= 0 || st->pressBtn >= 0) {
                st->dragSlider = -1;
                st->pressBtn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &pt);
            const float mx = static_cast<float>(pt.x) / scale;
            const float my = static_cast<float>(pt.y) / scale;
            int sld = st->HitSliderRow(mx, my);
            if (sld < 0 && my >= 275.0f && my <= 490.0f) sld = st->lastSlider;
            if (sld >= 0 && sld < 5) {
                st->lastSlider = sld;
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
                const int step = (wParam & (MK_SHIFT | MK_CONTROL)) ? 1 : 5;
                const int nv = ClampInt(st->values[sld] + delta * step, kSliders[sld].minV, kSliders[sld].maxV);
                if (nv != st->values[sld]) {
                    st->values[sld] = nv;
                    st->ApplyLive();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) { st->CancelAndClose(); return 0; }
            if (wParam == VK_RETURN) { st->SaveAndClose(); return 0; }
            if (wParam == VK_LEFT || wParam == VK_RIGHT) {
                const int sld = ClampInt(st->lastSlider, 0, 4);
                const int step = (GetKeyState(VK_SHIFT) < 0) ? 5 : 1;
                const int dir = (wParam == VK_RIGHT) ? 1 : -1;
                const int nv = ClampInt(st->values[sld] + dir * step, kSliders[sld].minV, kSliders[sld].maxV);
                if (nv != st->values[sld]) {
                    st->values[sld] = nv;
                    st->ApplyLive();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            return 0;
        }
        case WM_CLOSE:
            st->CancelAndClose();
            return 0;
        case WM_DESTROY:
            st->ReleaseAll();
            st->done = true;
            if (g_activeTunerHwnd == hwnd) g_activeTunerHwnd = nullptr;
            return 0;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace WeatherFxTunerUi

void ShowWeatherFxSetup(HWND owner) {
    if (Wh_GetIntValue(L"UpdateRequested", 0)) return;
    if (WeatherFxTunerUi::g_activeTunerHwnd && IsWindow(WeatherFxTunerUi::g_activeTunerHwnd)) {
        ShowWindow(WeatherFxTunerUi::g_activeTunerHwnd, SW_RESTORE);
        SetForegroundWindow(WeatherFxTunerUi::g_activeTunerHwnd);
        return;
    }

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    // Unregister any leftover class before registering so Windhawk mod reloads never invoke a stale WndProc
    UnregisterClassW(WeatherFxTunerUi::kWndClassName, hInst);
    HBRUSH bgBrush = CreateSolidBrush(RGB(19, 22, 30));
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WeatherFxTunerUi::TunerWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = bgBrush;
    wc.lpszClassName = WeatherFxTunerUi::kWndClassName;
    const bool classRegistered = (RegisterClassExW(&wc) != 0);
    if (!classRegistered && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        if (bgBrush) DeleteObject(bgBrush);
        return;
    }

    WeatherFxTunerUi::TunerState state;
    state.chinese = UiChinese();
    state.mode = state.origMode = g_weatherFxOverride.load(std::memory_order_relaxed);
    state.scope = state.origScope = ClampInt(g_weatherFxScope.load(std::memory_order_relaxed), 0, 2);
    state.values[0] = state.origValues[0] = ClampInt(g_weatherFxIntensityPct.load(std::memory_order_relaxed), 0, 150);
    state.values[1] = state.origValues[1] = ClampInt(g_weatherFxSpeedPct.load(std::memory_order_relaxed), 20, 250);
    state.values[2] = state.origValues[2] = ClampInt(g_weatherFxScalePct.load(std::memory_order_relaxed), 50, 200);
    state.values[3] = state.origValues[3] = ClampInt(g_weatherFxFogPct.load(std::memory_order_relaxed), 0, 100);
    state.values[4] = state.origValues[4] = ClampInt(g_weatherFxRefractPct.load(std::memory_order_relaxed), 0, 200);

    HMONITOR mon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    const UINT monDpi = owner ? GetDpiForWindow(owner) : 96;
    const float scale = (monDpi > 0 ? static_cast<float>(monDpi) : 96.0f) / 96.0f;
    const int winW = static_cast<int>(std::lround(WeatherFxTunerUi::kWinW * scale));
    const int winH = static_cast<int>(std::lround(WeatherFxTunerUi::kWinH * scale));
    const int workW = mi.rcWork.right - mi.rcWork.left;
    const int workH = mi.rcWork.bottom - mi.rcWork.top;
    // Place comfortably below top-center so the live Dynamic Island preview above is never occluded
    int posX = mi.rcWork.left + (workW - winW) / 2;
    int posY = mi.rcWork.top + (std::max)(static_cast<int>(210 * scale), (workH - winH) / 2);
    if (posY + winH > mi.rcWork.bottom - 12) posY = (std::max)(mi.rcWork.top + 12, mi.rcWork.bottom - winH - 12);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        WeatherFxTunerUi::kWndClassName,
        UiText(L"Weather Glass FX · Live Tuner", L"天气液态玻璃动效 · 实时调节"),
        WS_POPUP | WS_CLIPCHILDREN,
        posX, posY, winW, winH,
        owner, nullptr, hInst, &state);
    if (!hwnd) {
        if (classRegistered) UnregisterClassW(WeatherFxTunerUi::kWndClassName, hInst);
        if (bgBrush) DeleteObject(bgBrush);
        return;
    }

    WeatherFxTunerUi::g_activeTunerHwnd = hwnd;
    g_importantDaysOpen = true;
    g_weatherFxTuningOpen.store(true, std::memory_order_relaxed);
    g_layoutDirty = true;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (!state.done && IsWindow(hwnd)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                state.CancelAndClose();
                PostQuitMessage(static_cast<int>(msg.wParam));
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (state.done || !IsWindow(hwnd)) break;
        }
        if (state.done || !IsWindow(hwnd)) break;
        if (g_stopEvent) {
            const DWORD waitRes = MsgWaitForMultipleObjectsEx(1, &g_stopEvent, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (waitRes == WAIT_OBJECT_0) {
                state.CancelAndClose();
                break;
            }
        } else {
            WaitMessage();
        }
    }

    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    if (!state.saved) state.Rollback();
    state.ReleaseAll();
    if (classRegistered) UnregisterClassW(WeatherFxTunerUi::kWndClassName, hInst);
    if (bgBrush) DeleteObject(bgBrush);
    WeatherFxTunerUi::g_activeTunerHwnd = nullptr;
    g_weatherFxTuningOpen.store(false, std::memory_order_relaxed);
    g_importantDaysOpen = false;
    g_layoutDirty = true;
}
