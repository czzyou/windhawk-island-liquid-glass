#pragma once

struct HourlyClockSnapshot {
    bool active = false;
    int hour = 0, minute = 0;
    double expiresAt = 0;
};
struct HourlyClockTracker {
    uint64_t lastHour = 0;
    bool Tick(const SYSTEMTIME& local, bool preview) {
        const uint64_t key = (((static_cast<uint64_t>(local.wYear) * 12 + local.wMonth) * 31 +
            local.wDay) * 24 + local.wHour) + 1;
        const bool due = lastHour != 0 && key != lastHour && local.wMinute == 0 && local.wSecond < 5;
        lastHour = key;
        return due || preview;
    }
};
