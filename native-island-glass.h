#pragma once
#include <atomic>
#include <array>
#include <cmath>
#include <memory>
#include "native-glass/src/session.h"

class NativeIslandGlass {
    struct State {
        std::atomic<bool> dirty{false}, failed{false};
        std::array<std::atomic<bool>, 2> ready{};
    };
    struct Slot {
        int id = 0;
        RECT bounds{};
        float radius = 0, opacity = 0;
        bool shown = false;
    };
    HWND host_ = nullptr;
    bool active_ = false;
    bool boundsChanged_ = false;
    int used_ = 0;
    std::array<Slot, 2> slots_{};
    std::shared_ptr<State> state_ = std::make_shared<State>();

public:
    void BeginFrame(HWND host, bool enabled) {
        used_ = 0;
        if (!enabled) {
            Stop();
            if (state_->failed.load()) state_ = std::make_shared<State>();
            return;
        }
        if (state_->failed.load()) { Stop(); return; }
        if (active_) return;
        host_ = host;
        // Exclude only from our desktop-duplication source, not ordinary screenshots.
        if (!ExcludeGlassFromDesktopDuplication(host_, true)) {
            state_->failed = true;
            return;
        }
        active_ = true;
    }

    void Sync(float left, float top, float right, float bottom, float radius, float dpr, float opacity) {
        if (!active_ || used_ >= static_cast<int>(slots_.size())) return;
        const int index = used_++;
        auto& slot = slots_[index];
        RECT hostBounds{};
        if (!GetWindowRect(host_, &hostBounds)) return;
        const RECT bounds{
            hostBounds.left + static_cast<LONG>(std::lround(left)),
            hostBounds.top + static_cast<LONG>(std::lround(top)),
            hostBounds.left + static_cast<LONG>(std::lround(right)),
            hostBounds.top + static_cast<LONG>(std::lround(bottom))};
        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return;
        if (!EqualRect(&bounds, &slot.bounds)) boundsChanged_ = true;
        GlassParams params;
        params.cornerRadius = radius;
        params.blurSigma = 0.9f * dpr;
        params.displacementScale = 70;
        params.aberrationIntensity = 0.7f;
        params.saturation = 1.08f;
        params.opacity = opacity;
        auto& session = GlassSession::Instance();
        if (!slot.id) {
            PanelConfig config;
            config.bounds = bounds; config.params = params; config.dpr = dpr; config.anchor = host_;
            auto state = state_;
            config.onReady = [state, index](bool ready) {
                state->ready[index] = ready;
                if (!ready) state->failed = true;
                state->dirty = true;
            };
            slot.id = session.CreatePanel(config);
        } else {
            if (!EqualRect(&bounds, &slot.bounds)) session.SetPanelBounds(slot.id, bounds);
            if (slot.radius != radius || slot.opacity != opacity) session.SetPanelParams(slot.id, params);
        }
        slot.bounds = bounds; slot.radius = radius; slot.opacity = opacity;
        if (!slot.shown) { session.ShowPanel(slot.id, 0); slot.shown = true; }
    }

    void EndFrame() {
        for (int i=used_; i<static_cast<int>(slots_.size()); ++i) {
            auto& slot = slots_[i];
            if (slot.shown) { GlassSession::Instance().HidePanel(slot.id,0); slot.shown = false; }
        }
        // The host is about to present this frame. Give the worker a short,
        // bounded chance to move and repaint the glass for the new bounds so
        // the pill and its background land in the same present; on timeout the
        // old asynchronous behaviour is kept rather than stalling the UI.
        if (boundsChanged_) {
            boundsChanged_ = false;
            if (active_ && !state_->failed.load())
                GlassSession::Instance().FlushPending(3);
        }
    }
    bool Ready() const { return active_ && state_->ready[0].load() && !state_->failed.load(); }
    bool Changed() { return state_->dirty.exchange(false); }
    void Stop() {
        if (!active_) return;
        boundsChanged_ = false;
        for (auto& slot : slots_) {
            if (slot.id) GlassSession::Instance().DestroyPanel(slot.id);
            slot = {};
        }
        if (IsWindow(host_)) ExcludeGlassFromDesktopDuplication(host_, false);
        active_ = false;
        for (auto& ready : state_->ready) ready = false;
        state_->dirty = true;
    }
    void Shutdown() {
        Stop();
        GlassSession::Instance().Shutdown();
    }
};
