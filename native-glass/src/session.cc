#include "session.h"

#include <algorithm>

#include "stats.h"

namespace {

// Release the DDA session after this long without any visible panel
// (don't hold system capture resources for nothing)
constexpr ULONGLONG kCaptureIdleReleaseMs = 3000;
// Luma sampling is repaint-driven (pushed only when desktop content changes);
// this merely throttles rapid changes to ~60Hz max (GetTickCount64 resolution
// is ~15.6ms, so a threshold of 15 keeps every frame without skipping)
constexpr ULONGLONG kLumaMinGapMs = 15;
constexpr ULONGLONG kAnchorReassertMs = 500;

bool RectIntersects(const RECT& a, const RECT& b) {
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

}  // namespace

GlassSession& GlassSession::Instance() {
    static GlassSession session;
    return session;
}

void GlassSession::EnsureThread() {
    if (running_.load()) return;
    if (thread_.joinable()) thread_.join();
    running_ = true;
    thread_ = std::thread([this] { ThreadMain(); });
}

void GlassSession::Post(std::function<void()> cmd) {
    EnsureThread();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        commands_.push_back(std::move(cmd));
    }
    postedSeq_.fetch_add(1, std::memory_order_release);
    cv_.notify_one();
}

void GlassSession::FlushPending(uint32_t timeoutMs) {
    const uint64_t target = postedSeq_.load(std::memory_order_acquire);
    if (completedSeq_.load(std::memory_order_acquire) >= target) return;
    std::unique_lock<std::mutex> lock(flushMutex_);
    flushCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, target] {
        return completedSeq_.load(std::memory_order_acquire) >= target;
    });
}

int GlassSession::CreatePanel(const PanelConfig& config) {
    const int id = nextId_.fetch_add(1);
    Post([this, id, config] {
        PanelEntry entry;
        entry.panel = std::make_unique<GlassPanel>();
        entry.renderer = std::make_unique<GlassRenderer>();
        entry.config = config;
        if (FAILED(entry.panel->Create(device_.Get(), config.bounds, config.params,
                                       config.excludeFromCapture, config.dpr))) {
            if (config.onReady) config.onReady(false);
            return;
        }
        if (FAILED(entry.renderer->Initialize(device_.Get()))) {
            if (config.onReady) config.onReady(false);
            return;
        }
        if (config.anchor) entry.panel->AnchorBelow(config.anchor);
        panels_[id] = std::move(entry);
    });
    return id;
}

void GlassSession::DestroyPanel(int id) {
    Post([this, id] { panels_.erase(id); });
}

void GlassSession::ShowPanel(int id, UINT fadeMs) {
    Post([this, id, fadeMs] {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        it->second.panel->Show();
        it->second.panel->BeginFade(1.0f, fadeMs);
        it->second.fading = true;
        it->second.needsInitialPaint = true;
    });
}

void GlassSession::HidePanel(int id, UINT fadeMs) {
    Post([this, id, fadeMs] {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        it->second.anchorHidden = false;
        if (fadeMs == 0) {
            it->second.panel->Hide();
            it->second.fading = false;
        } else {
            it->second.panel->BeginFade(0.0f, fadeMs);
            it->second.fading = true;
        }
    });
}

void GlassSession::SetPanelBounds(int id, const RECT& bounds) {
    Post([this, id, bounds] {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        // Do not move the glass window here. The move and the repaint that
        // matches it must happen in the same worker iteration, otherwise the
        // old background is briefly shown at the new position and looks like a
        // vertical jump during expansion.
        it->second.config.bounds = bounds;
        it->second.needsInitialPaint = true;
        pendingBounds_ = true;
    });
}

void GlassSession::SetPanelParams(int id, const GlassParams& params) {
    Post([this, id, params] {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        it->second.config.params = params;
        it->second.panel->SetParams(params);
        it->second.needsInitialPaint = true;
    });
}

void GlassSession::AnchorPanel(int id, HWND anchor) {
    Post([this, id, anchor] {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        it->second.config.anchor = anchor;
        it->second.panel->AnchorBelow(anchor);
    });
}

void GlassSession::SetLumaBands(int id, std::vector<LumaBand> bands) {
    Post([this, id, bands = std::move(bands)]() mutable {
        auto it = panels_.find(id);
        if (it == panels_.end()) return;
        it->second.lumaBands = std::move(bands);
        it->second.renderer->SetLumaWanted(!it->second.lumaBands.empty());
        it->second.lastLumaTick = 0;
    });
}

void GlassSession::SetLumaCallback(LumaCallback cb) {
    std::lock_guard<std::mutex> lock(lumaMutex_);
    lumaCallback_ = std::move(cb);
}

void GlassSession::Shutdown() {
    running_ = false;
    cv_.notify_one();
    if (thread_.joinable()) thread_.join();
}

void GlassSession::ThreadMain() {
    if (FAILED(CreateD3DDevice(&device_, &context_))) {
        running_ = false;
        return;
    }

    while (running_.load()) {
        PumpCommandsAndMessages();
        if (!running_.load()) break;
        RenderTick();
    }

    panels_.clear();
    capturer_.Shutdown();
    desktopCache_.Reset();
    context_.Reset();
    device_.Reset();
}

void GlassSession::PumpCommandsAndMessages() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(commands_);
    }
    for (auto& cmd : pending) cmd();

    // Pump window messages (panel windows belong to this thread)
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// Keep the latest desktop frame in a persistent mirror so a new panel on a
// static desktop still gets pixels for its first paint immediately.
// Incremental maintenance: only this frame's dirty bounding box (output-local
// coords) is copied; the mirror remains an exact replica of the desktop while
// copy volume is proportional to the changed area instead of the full screen
// (a small corner animation no longer triggers full-screen copies).
bool GlassSession::UpdateDesktopCache(ID3D11Texture2D* frameTex, const RECT& dirtyLocal) {
    D3D11_TEXTURE2D_DESC desc{};
    frameTex->GetDesc(&desc);
    bool fullCopy = false;
    if (!desktopCache_ || cacheW_ != desc.Width || cacheH_ != desc.Height || cacheFormat_ != desc.Format) {
        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        if (FAILED(device_->CreateTexture2D(&desc, nullptr, desktopCache_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        cacheW_ = desc.Width;
        cacheH_ = desc.Height;
        cacheFormat_ = desc.Format;
        fullCopy = true;  // fresh texture contents are undefined; one full copy is required
    }

    uint64_t bytes = 0;
    if (fullCopy) {
        context_->CopyResource(desktopCache_.Get(), frameTex);
        bytes = static_cast<uint64_t>(cacheW_) * cacheH_ * 4;
    } else {
        const RECT clamped{ std::max<LONG>(dirtyLocal.left, 0), std::max<LONG>(dirtyLocal.top, 0),
                            std::min<LONG>(dirtyLocal.right, cacheW_),
                            std::min<LONG>(dirtyLocal.bottom, cacheH_) };
        if (clamped.right <= clamped.left || clamped.bottom <= clamped.top) return true;
        D3D11_BOX box{ static_cast<UINT>(clamped.left), static_cast<UINT>(clamped.top), 0,
                       static_cast<UINT>(clamped.right), static_cast<UINT>(clamped.bottom), 1 };
        context_->CopySubresourceRegion(desktopCache_.Get(), 0, box.left, box.top, 0,
                                        frameTex, 0, &box);
        bytes = static_cast<uint64_t>(box.right - box.left) * (box.bottom - box.top) * 4;
    }
    GlassStats::Instance().cacheCopies.fetch_add(1, std::memory_order_relaxed);
    GlassStats::Instance().cacheCopyBytes.fetch_add(bytes, std::memory_order_relaxed);
    cacheValid_ = true;
    return true;
}

void GlassSession::RenderTick() {
    const ULONGLONG now = GetTickCount64();
    // Commands posted after this point are picked up by the next tick, so the
    // completion mark must not claim they were rendered.
    const uint64_t tickSeq = postedSeq_.load(std::memory_order_acquire);
    const bool fastRepaint = pendingBounds_;
    pendingBounds_ = false;
    GlassStats::Instance().loopIterations.fetch_add(1, std::memory_order_relaxed);

    bool anyVisible = false;
    bool anyFading = false;
    const PanelEntry* firstVisible = nullptr;
    std::vector<RECT> selfRects;
    for (auto& [id, entry] : panels_) {
        if (entry.config.anchor) {
            const bool hostVisible = IsWindowVisible(entry.config.anchor) != FALSE;
            if (!hostVisible && entry.panel->visible()) {
                entry.panel->Hide();
                entry.fading = false;
                entry.anchorHidden = true;
            } else if (hostVisible && entry.anchorHidden) {
                entry.anchorHidden = false;
                entry.panel->Show();
                entry.panel->BeginFade(1.0f, 0);
                entry.fading = true;
                entry.needsInitialPaint = true;
            }
        }
        if (entry.panel->visible()) {
            anyVisible = true;
            if (!firstVisible) firstVisible = &entry;
            selfRects.push_back(entry.config.bounds);
        }
        if (entry.fading) anyFading = true;
    }

    if (!anyVisible && !anyFading) {
        if (capturer_.initialized() && now - lastActiveTick_ > kCaptureIdleReleaseMs) {
            capturer_.Shutdown();
            cacheValid_ = false;
        }
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(80),
                     [this] { return !commands_.empty() || !running_.load(); });
        return;
    }
    lastActiveTick_ = now;

    // The capture session follows the display of the first visible panel
    if (firstVisible) {
        const RECT& b = firstVisible->config.bounds;
        if (!capturer_.initialized() || !RectIntersects(capturer_.desktopRect(), b)) {
            capturer_.Shutdown();
            cacheValid_ = false;
            if (FAILED(capturer_.Initialize(device_.Get(), b))) {
                // Init failed (e.g. secure desktop active): retry later while
                // panels keep their last frame
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait_for(lock, std::chrono::milliseconds(300));
                return;
            }
        }
    }

    CaptureFrameInfo frameInfo;
    bool newFrame = false;
    RECT dirtyVirtual{};
    // A bounds change is already waiting for a repaint. Do not spend up to 8ms
    // waiting for a fresh desktop frame first: the cached desktop is enough to
    // paint the new size, and delaying here is what let the host window move
    // ahead of its background.
    if (!fastRepaint) {
        ComPtr<ID3D11Texture2D> desktopTex;
        const HRESULT hr = capturer_.AcquireFrame(8, selfRects, &frameInfo, &desktopTex);
        if (hr == S_OK) {
            if (frameInfo.desktopUpdated &&
                UpdateDesktopCache(desktopTex.Get(), frameInfo.dirtyBounds)) {
                GlassStats::Instance().framesAcquired.fetch_add(1, std::memory_order_relaxed);
                newFrame = true;
                const RECT& dr = capturer_.desktopRect();
                dirtyVirtual = RECT{ frameInfo.dirtyBounds.left + dr.left,
                                     frameInfo.dirtyBounds.top + dr.top,
                                     frameInfo.dirtyBounds.right + dr.left,
                                     frameInfo.dirtyBounds.bottom + dr.top };
            }
            capturer_.ReleaseFrame();
        } else if (hr != S_FALSE) {
            // ACCESS_LOST etc.: rebuild the session (resolution change,
            // fullscreen exclusive mode, secure desktop transitions)
            capturer_.Shutdown();
            return;
        }
    }

    const bool fadeTickDue = now - lastFadeTick_ >= 16;
    for (auto& [id, entry] : panels_) {
        if (!entry.panel->visible()) continue;

        // Apply the pending window move here, immediately before the repaint
        // that samples the background for the same bounds. Doing the move in
        // the bounds command instead left the previous frame visible at the
        // new position until the next render tick.
        if (cacheValid_ && !EqualRect(&entry.panel->bounds(), &entry.config.bounds)) {
            entry.panel->SetBounds(entry.config.bounds);
            entry.needsInitialPaint = true;
        }

        // Luminance band sampling (adaptive text contrast): reads the staging
        // copy queued during the previous tick's render. Deferring one tick
        // (≤16ms, imperceptible) buys a zero-wait Map — mapping in the same
        // tick as the render forces a flush and synchronously waits for the
        // whole GPU pipeline (~400µs of pure stall per sample, measured).
        // lastLumaTick == 0 means the band config just changed: sample once
        // from the existing texture even without a render (synchronous fallback)
        if (!entry.lumaBands.empty() && entry.renderer->hasFrame() &&
            (entry.lumaStagingDirty || entry.lastLumaTick == 0) &&
            now - entry.lastLumaTick >= kLumaMinGapMs) {
            entry.lastLumaTick = now;
            entry.lumaStagingDirty = false;
            SampleLuma(id, entry);
        }

        const RECT region = GlassRenderer::RegionForPanel(
            entry.config.bounds, entry.config.params, entry.config.dpr);
        const bool dirtyHit = newFrame && RectIntersects(dirtyVirtual, region);
        bool rendered = false;

        if (cacheValid_ && (dirtyHit || entry.needsInitialPaint)) {
            auto params = entry.panel->params();
            params.sdrWhiteScale = capturer_.sdrWhiteScale();
            entry.panel->SetParams(params);
            if (SUCCEEDED(entry.renderer->RenderFromDesktop(
                    context_.Get(), desktopCache_.Get(), capturer_.desktopRect(),
                    *entry.panel, entry.config.dpr))) {
                entry.needsInitialPaint = false;
                if (!entry.readyReported) {
                    entry.readyReported = true;
                    if (entry.config.onReady) entry.config.onReady(true);
                }
                rendered = true;
                if (!entry.lumaBands.empty()) entry.lumaStagingDirty = true;
                GlassStats::Instance().renders.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Advance fades (redraw from the last region texture when no new frame)
        if (entry.fading && fadeTickDue) {
            const bool continuing = entry.panel->FadeStep();
            if (!rendered) entry.renderer->Redraw(context_.Get(), *entry.panel, entry.config.dpr);
            if (!continuing) {
                entry.fading = false;
                if (entry.panel->opacity() <= 0.0f) entry.panel->Hide();
            }
        }

        // Periodically re-assert z-order anchoring (defends against other
        // topmost windows cutting in)
        if (entry.config.anchor && now - entry.lastAnchorTick >= kAnchorReassertMs) {
            entry.lastAnchorTick = now;
            entry.panel->AnchorBelow(entry.config.anchor);
        }
    }
    if (fadeTickDue) lastFadeTick_ = now;
    completedSeq_.store(tickSeq, std::memory_order_release);
    flushCv_.notify_all();
}

void GlassSession::SampleLuma(int panelId, PanelEntry& entry) {
    std::vector<LumaBandStats> result;
    if (FAILED(entry.renderer->SampleBands(context_.Get(), entry.lumaBands,
                                           entry.config.dpr, &result)) ||
        result.empty()) {
        return;
    }
    LumaCallback cb;
    {
        std::lock_guard<std::mutex> lock(lumaMutex_);
        cb = lumaCallback_;
    }
    if (cb) cb(panelId, std::move(result));
}
