// Glass session: a dedicated worker thread running the capture → render →
// present loop.
// - Panel windows are created on the worker thread (their message queues
//   belong to it)
// - No capture while no panel is visible (sleeps waiting for commands); on a
//   static desktop AcquireNextFrame just times out — zero rendering
// - A panel's repaint is skipped when the dirty region doesn't intersect it
// - Every JS-facing call is posted through the command queue; no pixel data
//   ever crosses threads
#pragma once
#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "capture.h"
#include "d3d_utils.h"
#include "panel.h"
#include "renderer.h"

struct PanelConfig {
    RECT bounds{};            // screen physical pixels
    GlassParams params{};     // physical-pixel units
    float dpr = 1.0f;
    bool excludeFromCapture = true;
    HWND anchor = nullptr;    // panel is pinned right below this window (z-order)
    std::function<void(bool)> onReady;  // first successful frame, or creation failure
};

// Luminance band: rect in panel-local physical pixels
struct LumaBand {
    int id = 0;
    RECT rect{};
};

// Band statistics: mean color + luma percentiles (the complete input for an
// adaptive text-contrast algorithm)
struct LumaBandStats {
    int id = 0;
    float r = 0;
    float g = 0;
    float b = 0;
    float darkTail = 0;   // luma p15 (0-255)
    float lightTail = 0;  // luma p85
};

class GlassSession {
public:
    using LumaCallback = std::function<void(int panelId, std::vector<LumaBandStats>)>;

    static GlassSession& Instance();

    int CreatePanel(const PanelConfig& config);
    void DestroyPanel(int id);
    void ShowPanel(int id, UINT fadeMs);
    void HidePanel(int id, UINT fadeMs);
    void SetPanelBounds(int id, const RECT& bounds);
    void SetPanelParams(int id, const GlassParams& params);
    void AnchorPanel(int id, HWND anchor);
    void SetLumaBands(int id, std::vector<LumaBand> bands);
    void SetLumaCallback(LumaCallback cb);
    // Wait (bounded) until the worker has completed a render tick that started
    // after the most recent posted command. Used by the host render thread to
    // keep the laid-out position and the glass background in the same present.
    void FlushPending(uint32_t timeoutMs);  // invoked on the worker thread (caller handles thread-safe dispatch)

    void Shutdown();

private:
    GlassSession() = default;

    struct PanelEntry {
        std::unique_ptr<GlassPanel> panel;
        std::unique_ptr<GlassRenderer> renderer;
        PanelConfig config;
        std::vector<LumaBand> lumaBands;
        bool needsInitialPaint = true;
        bool readyReported = false;
        bool anchorHidden = false;
        bool fading = false;
        bool lumaStagingDirty = false;  // staging holds an unsampled blur result (set after each render)
        ULONGLONG lastLumaTick = 0;
        ULONGLONG lastAnchorTick = 0;
    };

    void EnsureThread();
    void Post(std::function<void()> cmd);
    void ThreadMain();
    void PumpCommandsAndMessages();
    void RenderTick();
    bool UpdateDesktopCache(ID3D11Texture2D* frameTex, const RECT& dirtyLocal);
    void SampleLuma(int panelId, PanelEntry& entry);

    std::thread thread_;
    std::atomic<bool> running_{ false };
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> commands_;

    // Members below are worker-thread only
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    DesktopCapturer capturer_;
    ComPtr<ID3D11Texture2D> desktopCache_;  // persistent desktop mirror (instant first paint for new panels on a static desktop)
    UINT cacheW_ = 0;
    DXGI_FORMAT cacheFormat_ = DXGI_FORMAT_UNKNOWN;
    UINT cacheH_ = 0;
    bool cacheValid_ = false;
    // A bounds command arrived. The next RenderTick applies the window move and
    // repaints in the same iteration instead of letting the window sit at the
    // new position with the previous frame's background.
    bool pendingBounds_ = false;
    std::map<int, PanelEntry> panels_;
    ULONGLONG lastFadeTick_ = 0;
    ULONGLONG lastActiveTick_ = 0;

    std::mutex lumaMutex_;
    LumaCallback lumaCallback_;
    std::atomic<uint64_t> postedSeq_{0};
    std::atomic<uint64_t> completedSeq_{0};
    std::mutex flushMutex_;
    std::condition_variable flushCv_;
    std::atomic<int> nextId_{ 1 };
};
