// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
std::wstring MediaSourceDisplayName(const std::wstring& aumid) {
    std::wstring name = FriendlyMediaSourceName(aumid);
    if (name.find(L'\\') != std::wstring::npos || name.find(L'/') != std::wstring::npos ||
        name.find(L':') != std::wstring::npos) {
        name = StripExtension(BaseNameFromPath(aumid));
        if (!name.empty()) {
            name[0] = static_cast<wchar_t>(towupper(name[0]));
        }
    }
    return name.empty() ? std::wstring(L"Media") : name;
}

struct MediaSourceTracker {
    // Auto mode / dock pick bookkeeping (see ChooseMediaSourceAumid).
    std::unordered_map<std::wstring, bool> wasPlaying;      // playing state at the previous poll
    std::unordered_map<std::wstring, uint64_t> startedSeq;  // when each source last STARTED playing (higher = newer)
    uint64_t seqCounter = 0;
    std::wstring dockOverride;                              // temporary dock pick; empty = none
    uint64_t dockOverrideBaseSeq = 0;                       // any start newer than this ends the dock pick
    unsigned dockOverrideRevSeen = 0;
    std::vector<std::wstring> order;                     // first seen = first in the dock
    std::unordered_map<std::wstring, BitmapPixels> icons;
    std::unordered_map<std::wstring, int> iconRetry;     // polls before searching for an icon again
};

bool MediaSessionHasContent(bool playing, const std::wstring& title) {
    return playing || std::any_of(title.begin(), title.end(), [](wchar_t c) { return !iswspace(c); });
}

bool CollectMediaSources(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager const& manager,
    MediaSourceTracker& tracker, std::vector<MediaSourceInfo>* out) {
    using PlaybackStatus =
        winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

    std::vector<std::pair<std::wstring, bool>> live;  // AUMID, playing
    try {
        for (auto const& session : manager.GetSessions()) {
            std::wstring aumid = session.SourceAppUserModelId().c_str();
            if (aumid.empty()) continue;
            bool playing = false;
            try {
                playing = session.GetPlaybackInfo().PlaybackStatus() == PlaybackStatus::Playing;
            } catch (...) {
            }
            auto it = std::find_if(live.begin(), live.end(),
                                   [&](const auto& e) { return e.first == aumid; });
            if (it != live.end()) {
                it->second = it->second || playing;
            } else {
                live.emplace_back(std::move(aumid), playing);
            }
        }
    } catch (...) {
        return false;  // keep whatever the dock showed before
    }

    // Remember when each source last STARTED playing (not playing -> playing).
    // Auto mode follows the newest start.
    {
        std::wstring currentAumid;
        try {
            auto current = manager.GetCurrentSession();
            if (current) {
                currentAumid = current.SourceAppUserModelId().c_str();
            }
        } catch (...) {
        }

        // A source that disappeared forgets its history, so reopening it counts as a fresh start.
        for (auto it = tracker.wasPlaying.begin(); it != tracker.wasPlaying.end();) {
            const std::wstring& id = it->first;
            const bool stillLive = std::any_of(live.begin(), live.end(),
                                               [&](const auto& e) { return e.first == id; });
            if (stillLive) {
                ++it;
            } else {
                tracker.startedSeq.erase(id);
                it = tracker.wasPlaying.erase(it);
            }
        }

        std::vector<std::wstring> started;
        for (const auto& e : live) {
            bool& was = tracker.wasPlaying[e.first];
            if (e.second && !was) {
                started.push_back(e.first);
            }
            was = e.second;
        }
        // Several sources starting inside one poll: the one Windows calls "current" counts as newest.
        std::stable_sort(started.begin(), started.end(),
                         [&](const std::wstring& a, const std::wstring& b) {
                             return a != currentAumid && b == currentAumid;
                         });
        for (const std::wstring& id : started) {
            tracker.startedSeq[id] = ++tracker.seqCounter;
        }
    }

    // Stable order: new sources go on the end, vanished ones drop out.
    tracker.order.erase(
        std::remove_if(tracker.order.begin(), tracker.order.end(),
                       [&](const std::wstring& id) {
                           return std::none_of(live.begin(), live.end(),
                                               [&](const auto& e) { return e.first == id; });
                       }),
        tracker.order.end());
    for (const auto& e : live) {
        if (std::find(tracker.order.begin(), tracker.order.end(), e.first) == tracker.order.end()) {
            tracker.order.push_back(e.first);
        }
    }
    for (auto it = tracker.icons.begin(); it != tracker.icons.end();) {
        if (std::find(tracker.order.begin(), tracker.order.end(), it->first) == tracker.order.end()) {
            tracker.iconRetry.erase(it->first);
            it = tracker.icons.erase(it);
        } else {
            ++it;
        }
    }

    out->clear();
    for (const std::wstring& id : tracker.order) {
        MediaSourceInfo info;
        info.aumid = id;
        info.name = MediaSourceDisplayName(id);
        info.badge = MediaSourceBadge(info.name);
        for (const auto& e : live) {
            if (e.first == id) info.playing = e.second;
        }

        BitmapPixels& icon = tracker.icons[id];
        if (icon.bgra.empty()) {  // window enumeration is not free, so retry rarely
            int& wait = tracker.iconRetry[id];
            if (wait <= 0) {
                icon = FindMediaSourceIcon(id);
                wait = 8;
            } else {
                --wait;
            }
        }
        info.icon = icon;
        out->push_back(std::move(info));
    }
    return true;
}

// Which source should the island show?  Returns "" to fall back to the system's current session.
//   1. Dock pick: temporary. Ends when something NEW starts playing anywhere, or when
//      that app goes away. After that, the lock or Auto applies again.
//   2. Locked app (right-click menu): wins over Auto.
//   3. Auto: the playing app that started most recently. If it closes, the next most
//      recent playing app. With nothing playing: the app that played last.
std::wstring ChooseMediaSourceAumid(MediaSourceTracker& tracker, const std::wstring& lockedAumid) {
    auto exists = [&](const std::wstring& id) {
        return std::find(tracker.order.begin(), tracker.order.end(), id) != tracker.order.end();
    };
    auto seqOf = [&](const std::wstring& id) -> uint64_t {
        auto it = tracker.startedSeq.find(id);
        return it == tracker.startedSeq.end() ? 0 : it->second;
    };

    // Pick up a new dock request (or an Auto reset from the menu).
    const unsigned rev = g_dockOverrideRev.load();
    if (rev != tracker.dockOverrideRevSeen) {
        tracker.dockOverrideRevSeen = rev;
        {
            std::lock_guard lock(g_mediaSourceMutex);
            tracker.dockOverride = g_dockOverrideAumid;
        }
        tracker.dockOverrideBaseSeq = tracker.seqCounter;
    }

    if (!tracker.dockOverride.empty()) {
        bool somethingNewStarted = false;
        for (const std::wstring& id : tracker.order) {
            if (seqOf(id) > tracker.dockOverrideBaseSeq) {
                somethingNewStarted = true;
                break;
            }
        }
        if (!exists(tracker.dockOverride) || somethingNewStarted) {
            tracker.dockOverride.clear();  // back to the lock / Auto
        } else {
            return tracker.dockOverride;
        }
    }

    if (!lockedAumid.empty() && exists(lockedAumid)) {
        return lockedAumid;
    }

    // Auto. Playing sources beat paused ones; within each group the newest start wins.
    std::wstring best;
    bool bestPlaying = false;
    uint64_t bestSeq = 0;
    for (const std::wstring& id : tracker.order) {
        const auto pit = tracker.wasPlaying.find(id);
        const bool playing = pit != tracker.wasPlaying.end() && pit->second;
        const uint64_t seq = seqOf(id);
        if (best.empty() || (playing && !bestPlaying) || (playing == bestPlaying && seq > bestSeq)) {
            best = id;
            bestPlaying = playing;
            bestSeq = seq;
        }
    }
    if (!bestPlaying && bestSeq == 0) {
        return std::wstring();  // nothing has ever played: let Windows decide
    }
    return best;
}
