struct GlowMotionState {
    float energy = 0.0f;
    float playback = 0.0f;
    bool flowing = false;
};

static bool UpdateGlowMotion(GlowMotionState& state, float amplitude, bool playing,
                             bool motionEnabled, double dt) {
    state.flowing = playing && motionEnabled;
    if (!motionEnabled) {
        const bool changed = state.energy != 0.0f || state.playback != (playing ? 1.0f : 0.0f);
        state.energy = 0.0f;
        state.playback = playing ? 1.0f : 0.0f;
        return changed;
    }
    const float target = playing ? std::sqrt((std::clamp)(amplitude, 0.0f, 1.0f)) : 0.0f;
    const float previousEnergy = state.energy;
    const float previousPlayback = state.playback;
    // Integrate several audio samples so individual drum hits cannot flash the rim.
    const float energyTime = target > state.energy ? 0.45f : 0.9f;
    state.energy += (target - state.energy) * (1.0f - static_cast<float>(std::exp(-dt / energyTime)));
    const float playbackTarget = playing ? 1.0f : 0.0f;
    state.playback += (playbackTarget - state.playback) *
        (1.0f - static_cast<float>(std::exp(-dt / 0.10)));
    if (std::fabs(state.energy - target) < 0.001f) state.energy = target;
    if (std::fabs(state.playback - playbackTarget) < 0.005f) state.playback = playbackTarget;
    return state.flowing || state.energy != previousEnergy || state.playback != previousPlayback;
}
// END_ALBUM_GLOW_MOTION
