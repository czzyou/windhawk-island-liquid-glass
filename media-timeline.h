// Adapted from upstream 1.4.0: distinguish a real backward seek from stale SMTC metadata.
struct MediaTimelineTracker {
    int64_t pendingPosition = -1, pendingAt = 0;
    void Update(MediaSnapshot& next, const MediaSnapshot& previous, int64_t rawPosition,
                int64_t end, int64_t age, int64_t now) {
        int64_t position = rawPosition;
        if (next.playing && age > 0 && age < 100000000LL) position += age;
        const bool same = next.playing && previous.playing && end == previous.endTicks &&
            next.sourceAppUserModelId == previous.sourceAppUserModelId && next.title == previous.title &&
            next.artist == previous.artist && next.albumTitle == previous.albumTitle;
        if (same) {
            const int64_t predicted = previous.positionTicks + (now - previous.lastUpdatedTicks) * 10000;
            if (position < predicted - 15000000LL) {
                const int64_t difference = position - (pendingPosition + (now - pendingAt) * 10000);
                if (pendingPosition >= 0 && std::abs(difference) < 5000000LL) pendingPosition = -1;
                else { pendingPosition = position; pendingAt = now; position = predicted; }
            } else {
                pendingPosition = -1;
                // Small clock corrections must not replay the previous lyric line.
                position = (std::max)(position, predicted);
            }
        } else pendingPosition = -1;
        next.positionTicks = (std::max)(int64_t{0}, position);
        next.endTicks = end;
        next.lastUpdatedTicks = now;
    }
};
