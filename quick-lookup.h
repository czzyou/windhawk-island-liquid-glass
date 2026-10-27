// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
// ── Quick Lookup panel: rows + hit testing ───────────────────────────────────
// Shared by the renderer (what to draw) and the window procedure (what a click or
// Enter means) so the two can never disagree about which row is which.
enum class LookupRowKind { Typed, Clipboard, Recent, Suggestion };

struct LookupRow {
    LookupRowKind kind = LookupRowKind::Recent;
    std::wstring query;   // what Enter / a click looks up
    std::wstring label;
    std::wstring detail;
};

std::wstring LookupTypedQuery(const LookupUiState& ui) {
    const size_t a = ui.text.find_first_not_of(L" \t");
    if (a == std::wstring::npos) {
        return std::wstring();
    }
    const size_t b = ui.text.find_last_not_of(L" \t");
    return ui.text.substr(a, b - a + 1);
}

std::vector<LookupRow> BuildLookupRows(const std::vector<LookupRecent>& recents,
                                       const LookupUiState& ui, const std::vector<LookupRecent>& suggestions = {}) {
    std::vector<LookupRow> rows;
    const size_t cap = static_cast<size_t>(LookupLayout::kMaxRows);
    const std::wstring typed = LookupTypedQuery(ui);

    if (typed.empty()) {
        const std::wstring clipKey = ToLowerCopy(ui.clipboardQuery);
        if (!ui.clipboardQuery.empty()) {
            rows.push_back({LookupRowKind::Clipboard, ui.clipboardQuery, ui.clipboardQuery,
                            Loc(L"Clipboard")});
        }
        for (const LookupRecent& r : recents) {
            if (rows.size() >= cap) break;
            if (!clipKey.empty() && ToLowerCopy(r.query) == clipKey) continue;
            rows.push_back({LookupRowKind::Recent, r.query,
                            r.title.empty() ? r.query : r.title, r.detail});
        }
        return rows;
    }

    for (const auto& suggestion : suggestions) {
        if (rows.size() >= cap) break;
        rows.push_back({LookupRowKind::Suggestion, suggestion.query, suggestion.title, suggestion.detail});
    }
    const auto typedKey = ToLowerCopy(typed);
    if (rows.size() < cap && std::none_of(rows.begin(), rows.end(), [&](const auto& row) { return ToLowerCopy(row.query) == typedKey; }))
        rows.push_back({LookupRowKind::Typed, typed, typed, std::wstring()});
    const std::wstring needle = ToLowerCopy(typed);
    for (const LookupRecent& r : recents) {
        if (rows.size() >= cap) break;
        const std::wstring q = ToLowerCopy(r.query);
        if (q == needle || std::any_of(rows.begin(), rows.end(), [&](const auto& row) { return ToLowerCopy(row.query) == q; })) continue;
        if (q.find(needle) == std::wstring::npos &&
            ToLowerCopy(r.title).find(needle) == std::wstring::npos) {
            continue;
        }
        rows.push_back({LookupRowKind::Recent, r.query,
                        r.title.empty() ? r.query : r.title, r.detail});
    }
    return rows;
}

// The "Recent / Clear" header only exists while the box is empty.
inline bool LookupHasHeader(const LookupUiState& ui, const std::vector<LookupRow>& rows) {
    return !rows.empty() && LookupTypedQuery(ui).empty();
}

inline bool LookupHasRecentRows(const std::vector<LookupRow>& rows) {
    return std::any_of(rows.begin(), rows.end(),
                       [](const LookupRow& r) { return r.kind == LookupRowKind::Recent; });
}

inline bool LookupCaretVisible(double now, double resetAt) {
    return std::fmod(std::max(0.0, now - resetAt), LookupLayout::kCaretPeriod) <
           LookupLayout::kCaretOn;
}

inline bool LookupFieldHitTest(const MediaContentPoint& pt) {
    return pt.valid && pt.x >= LookupLayout::kPadX && pt.x <= pt.width - LookupLayout::kPadX &&
           pt.y >= LookupLayout::kFieldTop &&
           pt.y <= LookupLayout::kFieldTop + LookupLayout::kFieldH;
}

inline bool LookupClearHitTest(const MediaContentPoint& pt) {
    return pt.valid && pt.x >= pt.width - LookupLayout::kPadX - 64.0f &&
           pt.x <= pt.width - LookupLayout::kPadX &&
           pt.y >= LookupLayout::kBelowField - 3.0f &&
           pt.y <= LookupLayout::kBelowField + LookupLayout::kSectionH + 3.0f;
}

int LookupRowAtContentPoint(const MediaContentPoint& pt, int rowCount, bool header) {
    if (!pt.valid || rowCount <= 0) return -1;
    if (pt.x < LookupLayout::kPadX || pt.x > pt.width - LookupLayout::kPadX) return -1;
    const float y = pt.y - LookupLayout::RowsTop(header);
    if (y < 0.0f) return -1;
    const float stride = LookupLayout::kRowH + LookupLayout::kRowGap;
    const int index = static_cast<int>(y / stride);
    if (index >= rowCount) return -1;
    if (y - static_cast<float>(index) * stride > LookupLayout::kRowH) return -1;
    return index;
}

// Quick Lookup: opening only reads clipboard suggestions locally. After an edit,
// a 250 ms pause sends the typed text to Youdao for dictionary/spelling candidates.
// Enter or a candidate click requests its bilingual definition. Queries stay off disk and logs.

// Identify this client without sending personal data.
constexpr wchar_t kLookupUserAgent[] =
    L"WindhawkIsland/" WH_MOD_VERSION L" (https://github.com/czzyou/windhawk-island-liquid-glass) QuickLookup";

// Bumped for every new request AND when the card is dismissed. A worker only
// publishes if its id is still current, so a slow reply can never resurrect a card
// the user already closed. Only changed or compared while holding g_stateMutex.
std::atomic<uint64_t> g_lookupRequestSeq{0};
bool g_lookupHotkeyRegistered = false;

namespace QuickLookup {

constexpr double kPanelIdleSeconds = 25.0;       // panel closes this long after the last key / mouse move
constexpr double kResultSeconds = kPanelIdleSeconds;
constexpr double kMessageSeconds = kPanelIdleSeconds;
constexpr size_t kRecentCapacity = 12;
constexpr double kLoadingTimeoutSeconds = 30.0;  // upper bound for "Looking up..."
constexpr size_t kMaxQueryChars = 80;            // longer text is never sent anywhere
constexpr size_t kMaxBodyChars = 360;            // pre-cap on a definition
constexpr size_t kCacheCapacity = 40;

struct Result {
    std::wstring title;
    std::wstring subtitle;
    std::wstring body;
    std::wstring example;
    std::wstring source;
};

// Failed means "could not get a real answer" (timeout, 429, 5xx, no network).
// Only Found and NotFound are cached, so a flaky connection is never remembered.
enum class Outcome { Found, NotFound, Failed };

// ---- text helpers -------------------------------------------------------------
// Collapses runs of whitespace (including newlines and NBSP) into single spaces.
// Stops once the result is longer than maxChars, so a huge clipboard stays cheap.
std::wstring CollapseWhitespace(const std::wstring& in, size_t maxChars = std::wstring::npos) {
    std::wstring out;
    bool pendingSpace = false;
    for (wchar_t ch : in) {
        if (iswspace(ch) || ch == 0x00A0 || ch == L'\0') {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(L' ');
            pendingSpace = false;
        }
        out.push_back(ch);
        if (out.size() > maxChars) {
            break;
        }
    }
    return out;
}

bool IsEdgePunct(wchar_t c) {
    return c != 0 && wcschr(L".,;:!?\"'()[]{}<>\u201c\u201d\u2018\u2019\u00ab\u00bb", c) != nullptr;
}

// Clipboard text -> lookup query. Too-long text is returned still too long (one
// char over the limit) so the caller can reject it instead of silently truncating.
std::wstring NormalizeQuery(const std::wstring& raw) {
    std::wstring text = CollapseWhitespace(raw, kMaxQueryChars);
    if (text.size() > kMaxQueryChars) {
        return text;
    }
    // "hello," / (hello) / “hello” -> hello
    while (!text.empty() && (IsEdgePunct(text.front()) || text.front() == L' ')) {
        text.erase(text.begin());
    }
    while (!text.empty() && (IsEdgePunct(text.back()) || text.back() == L' ')) {
        text.pop_back();
    }
    return text;
}

// Cuts to maxChars on a word boundary and appends an ellipsis.
std::wstring ClipToChars(std::wstring text, size_t maxChars) {
    if (text.size() <= maxChars) {
        return text;
    }
    text.resize(maxChars);
    const size_t space = text.find_last_of(L' ');
    if (space != std::wstring::npos && space > maxChars / 2) {
        text.resize(space);
    }
    while (!text.empty() && (iswspace(text.back()) || text.back() == L',' ||
                             text.back() == L';' || text.back() == L':' || text.back() == L'-')) {
        text.pop_back();
    }
    if (!text.empty() && IS_HIGH_SURROGATE(text.back())) {
        text.pop_back();
    }
    text += L"\u2026";
    return text;
}

// ---- in-memory cache (session only, MRU at the back) ----------------------------
struct CacheEntry {
    std::wstring key;
    bool found = false;
    Result result;
};

std::mutex g_cacheMutex;

std::vector<CacheEntry>& Cache() {
    static std::vector<CacheEntry> cache;
    return cache;
}

bool CacheLookup(const std::wstring& key, bool* found, Result* out) {
    std::lock_guard lock(g_cacheMutex);
    auto& cache = Cache();
    for (size_t i = cache.size(); i-- > 0;) {
        if (cache[i].key == key) {
            *found = cache[i].found;
            *out = cache[i].result;
            std::rotate(cache.begin() + static_cast<std::ptrdiff_t>(i),
                        cache.begin() + static_cast<std::ptrdiff_t>(i) + 1, cache.end());
            return true;
        }
    }
    return false;
}

void CacheStore(const std::wstring& key, bool found, const Result& result) {
    std::lock_guard lock(g_cacheMutex);
    auto& cache = Cache();
    cache.erase(std::remove_if(cache.begin(), cache.end(),
                               [&](const CacheEntry& e) { return e.key == key; }),
                cache.end());
    CacheEntry entry;
    entry.key = key;
    entry.found = found;
    entry.result = result;
    cache.push_back(std::move(entry));
    while (cache.size() > kCacheCapacity) {
        cache.erase(cache.begin());
    }
}

// LOOKUP_DICTIONARY

// ---- publishing ---------------------------------------------------------------------
void FillSnapshot(LookupSnapshot& lk, const std::wstring& query, bool found, const Result* r,
                  double now) {
    lk = LookupSnapshot{};
    lk.active = true;
    lk.query = query;
    if (found && r) {
        lk.status = LookupStatus::Found;
        lk.title = r->title;
        lk.subtitle = r->subtitle;
        lk.body = r->body;
        lk.example = r->example;
        lk.source = r->source;
        lk.expiresAt = now + kResultSeconds;
    } else {
        lk.status = LookupStatus::NoResult;
        lk.expiresAt = now + kMessageSeconds;
    }
}

// Moves `query` to the front of the recent list. Caller must hold g_stateMutex.
void PushRecentLocked(SharedState& st, const std::wstring& query, const Result& r) {
    LookupRecent entry;
    entry.query = query;
    entry.title = r.title;
    entry.source = r.source;

    std::wstring detail = r.body;
    std::replace(detail.begin(), detail.end(), L'\n', L' ');
    if (detail.rfind(L"1. ", 0) == 0) {
        detail.erase(0, 3);
    }
    entry.detail = ClipToChars(CollapseWhitespace(detail), 60);

    auto& list = st.lookupRecent;
    const std::wstring key = ToLowerCopy(query);
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const LookupRecent& e) { return ToLowerCopy(e.query) == key; }),
               list.end());
    list.insert(list.begin(), std::move(entry));
    if (list.size() > kRecentCapacity) {
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(kRecentCapacity), list.end());
    }
}

// Worker thread -> island. Dropped if the request was superseded or dismissed, or
// if its "Looking up..." card has already timed out.
void ApplyResult(uint64_t id, const std::wstring& query, bool found, const Result& r) {
    if (!g_running.load() || WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) {
        return;
    }
    const double now = NowSeconds();
    {
        std::lock_guard lock(g_stateMutex);
        LookupSnapshot& lk = g_state.lookup;
        if (g_lookupRequestSeq.load() != id || !lk.active || now >= lk.expiresAt) {
            return;
        }
        FillSnapshot(lk, query, found, &r, now);
        if (found) {
            PushRecentLocked(g_state, query, r);
        }
    }
    g_layoutDirty = true;  // repaint; the springs resize the island to the new card
}

}  // namespace QuickLookup

uint64_t PublishNewLookup(const LookupSnapshot& snap, bool nudge = true) {
    uint64_t id;
    {
        std::lock_guard lock(g_stateMutex);
        g_state.lookup = snap;
        g_state.lookupSuggestions.clear();
        id = ++g_lookupRequestSeq;
    }
    g_layoutDirty = true;
    if (nudge) {
        TriggerNudge();  // typing and searching stay quiet; only opening the panel bounces
    }
    return id;
}

// ── Quick Lookup panel: session + input ──────────────────────────────────────
// All of this runs on the render thread. g_state.lookup is the only part worker
// threads also touch, so it only changes under g_stateMutex.

// Keeps the panel alive while the user is interacting with it.
void TouchLookupPanel() {
    std::lock_guard lock(g_stateMutex);
    if (g_state.lookup.active) {
        g_state.lookup.expiresAt =
            std::max(g_state.lookup.expiresAt, NowSeconds() + QuickLookup::kPanelIdleSeconds);
    }
}

std::vector<LookupRow> CurrentLookupRows(LookupStatus* status) {
    std::lock_guard lock(g_stateMutex);
    if (status) *status = g_state.lookup.status;
    return BuildLookupRows(g_state.lookupRecent, g_lookupUi, g_state.lookupSuggestions);
}

void ForceForeground(HWND hwnd) {
    INPUT input{};
    SendInput(1, &input, sizeof(INPUT));
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    SetClickThrough(hwnd, false);

    SetFocus(hwnd);
}

// The island is WS_EX_NOACTIVATE so it never steals focus. While the search box is
// open it has to be able to receive keys, so the flag is dropped for the session.
void BeginLookupSession(HWND hwnd) {
    HWND previous = GetForegroundWindow();
    g_lookupUi.prevForeground = (previous && previous != hwnd) ? previous : nullptr;

    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_NOACTIVATE) {
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex & ~static_cast<LONG_PTR>(WS_EX_NOACTIVATE));
    }
    if (!IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
    ForceForeground(hwnd);
}

void EndLookupSession(HWND hwnd) {
    if (!g_lookupUi.open) return;
    HWND previous = g_lookupUi.prevForeground;
    const bool hadFocus = GetForegroundWindow() == hwnd;
    g_lookupUi = LookupUiState{};  // open = false first, so the focus change below can't re-enter

    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_NOACTIVATE);
    if (hadFocus && previous && IsWindow(previous) && IsWindowVisible(previous)) {
        SetForegroundWindow(previous);
    }
}

void CloseLookupPanel(HWND hwnd) {
    {
        std::lock_guard lock(g_stateMutex);
        g_state.lookup.active = false;
        ++g_lookupRequestSeq;  // a reply still in flight is now ignored
    }
    g_layoutDirty = true;
    EndLookupSession(hwnd);
}

// Runs a lookup and shows the outcome in the panel.
void SubmitLookup(const std::wstring& rawText) {
    const std::wstring query = QuickLookup::NormalizeQuery(rawText);
    if (query.empty() || query.size() > QuickLookup::kMaxQueryChars) {
        return;
    }

    if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) return;
    const double now = NowSeconds();
    LookupUiState& ui = g_lookupUi;
    ui.text = query;
    ui.caret = query.size();
    ui.selectAll = false;
    ui.selected = 0;
    ui.caretResetAt = now;

    LookupSnapshot snap;
    snap.active = true;
    snap.query = query;

    const std::wstring key = ToLowerCopy(query);
    bool cachedFound = false;
    QuickLookup::Result cached;
    if (QuickLookup::CacheLookup(key, &cachedFound, &cached)) {
        QuickLookup::FillSnapshot(snap, query, cachedFound, &cached, now);
        if (cachedFound) {
            std::lock_guard lock(g_stateMutex);
            QuickLookup::PushRecentLocked(g_state, query, cached);  // bump it to the top
        }
        PublishNewLookup(snap, false);
        return;
    }

    snap.status = LookupStatus::Loading;
    snap.expiresAt = now + QuickLookup::kLoadingTimeoutSeconds;
    const uint64_t id = PublishNewLookup(snap, false);

    // The query text is deliberately not logged.
    Wh_Log(L"Quick Lookup: request started.");
    {
        std::lock_guard lock(g_lookupJobMutex);
        g_lookupJob = {id, query, key};
    }
    SetEvent(g_lookupJobEvent);
}

void SubmitLookupSelection() {
    LookupStatus status = LookupStatus::Search;
    const std::vector<LookupRow> rows = CurrentLookupRows(&status);
    if (status == LookupStatus::Search && !rows.empty()) {
        const int idx = ClampInt(g_lookupUi.selected, 0, static_cast<int>(rows.size()) - 1);
        SubmitLookup(rows[static_cast<size_t>(idx)].query);
    } else {
        SubmitLookup(g_lookupUi.text);
    }
}

// Call after any change to the text. Editing leaves the result view and goes back
// to the list, and drops any reply still in flight.
void LookupUiEdited() {
    LookupUiState& ui = g_lookupUi;
    const double now = NowSeconds();
    ui.selected = 0;
    ui.selectAll = false;
    ui.caretResetAt = now;

    LookupSnapshot snap;
    snap.active = true;
    snap.status = LookupStatus::Search;
    snap.expiresAt = now + QuickLookup::kPanelIdleSeconds;
    const auto id = PublishNewLookup(snap, false);
    const auto query = QuickLookup::NormalizeQuery(ui.text);
    if (!query.empty() && query.size() <= QuickLookup::kMaxQueryChars) {
        {
            std::lock_guard lock(g_lookupJobMutex);
            g_lookupJob = {id, query, ToLowerCopy(query), true};
        }
        SetEvent(g_lookupJobEvent);
    }
    g_layoutDirty = true;
}

void InsertIntoLookupField(const std::wstring& raw) {
    LookupUiState& ui = g_lookupUi;
    std::wstring s;
    s.reserve(raw.size());
    for (wchar_t c : raw) {
        s.push_back((c < 0x20 || c == 0x7F) ? L' ' : c);  // single-line field
    }
    if (s.empty()) return;

    if (ui.selectAll) {
        ui.text.clear();
        ui.caret = 0;
        ui.selectAll = false;
    }
    ui.caret = std::min(ui.caret, ui.text.size());

    const size_t maxChars = QuickLookup::kMaxQueryChars;
    const size_t room = maxChars > ui.text.size() ? maxChars - ui.text.size() : 0;
    if (room == 0) return;
    if (s.size() > room) {
        s.resize(room);
        if (!s.empty() && IS_HIGH_SURROGATE(s.back())) s.pop_back();
    }
    ui.text.insert(ui.caret, s);
    ui.caret += s.size();
    LookupUiEdited();
}

size_t LookupStepBack(const std::wstring& t, size_t i) {
    if (i == 0) return 0;
    --i;
    if (i > 0 && IS_LOW_SURROGATE(t[i]) && IS_HIGH_SURROGATE(t[i - 1])) --i;
    return i;
}

size_t LookupStepForward(const std::wstring& t, size_t i) {
    if (i >= t.size()) return t.size();
    return (IS_HIGH_SURROGATE(t[i]) && i + 1 < t.size()) ? i + 2 : i + 1;
}

size_t LookupPrevWord(const std::wstring& t, size_t i) {
    while (i > 0 && iswspace(t[i - 1])) --i;
    while (i > 0 && !iswspace(t[i - 1])) --i;
    return i;
}

size_t LookupNextWord(const std::wstring& t, size_t i) {
    while (i < t.size() && !iswspace(t[i])) ++i;
    while (i < t.size() && iswspace(t[i])) ++i;
    return i;
}

// Called on the render thread when the hotkey fires.
void StartQuickLookup(HWND hwnd) {
    bool enabled = false;
    {
        std::lock_guard lock(g_settingsMutex);
        enabled = g_settings.quickLookup;
    }
    // Opening is explicit; wake a parked island without permanently changing its hide preference.
    if (!enabled || GetCapture()==hwnd || Wh_GetIntValue(L"UpdateRequested", 0)) {
        return;
    }

    // Hotkey again = close.
    if (g_lookupUi.open) {
        CloseLookupPanel(hwnd);
        return;
    }

    // Clipboard text is only offered as a suggestion row; nothing is sent anywhere yet.
    std::wstring clip = QuickLookup::NormalizeQuery(ReadClipboardText(hwnd));
    if (clip.size() > QuickLookup::kMaxQueryChars) {
        clip.clear();
    }

    g_autoHiddenParked = false;
    g_hotkeyUnhideUntil.store(NowSeconds() + QuickLookup::kPanelIdleSeconds);
    const double now = NowSeconds();
    g_lookupUi = LookupUiState{};
    g_lookupUi.open = true;
    g_lookupUi.clipboardQuery = clip;
    g_lookupUi.caretResetAt = now;

    LookupSnapshot snap;
    snap.active = true;
    snap.status = LookupStatus::Search;
    snap.expiresAt = now + QuickLookup::kPanelIdleSeconds;
    PublishNewLookup(snap);

    BeginLookupSession(hwnd);
}

// WM_KEYDOWN. Printable characters arrive separately through WM_CHAR.
bool HandleLookupKeyDown(HWND hwnd, WPARAM vk) {
    LookupUiState& ui = g_lookupUi;
    if (!ui.open) return false;
    const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool moved = false;

    switch (vk) {
        case VK_ESCAPE:
            CloseLookupPanel(hwnd);
            return true;
        case VK_RETURN:
            SubmitLookupSelection();
            return true;
        case VK_UP:
        case VK_DOWN: {
            LookupStatus status = LookupStatus::Search;
            const std::vector<LookupRow> rows = CurrentLookupRows(&status);
            if (status == LookupStatus::Search && !rows.empty()) {
                const int n = static_cast<int>(rows.size());
                ui.selected = (ClampInt(ui.selected, 0, n - 1) + (vk == VK_DOWN ? 1 : n - 1)) % n;
            }
            moved = true;
            break;
        }
        case VK_LEFT:
            ui.caret = ui.selectAll ? 0 : (ctrl ? LookupPrevWord(ui.text, ui.caret)
                                                : LookupStepBack(ui.text, ui.caret));
            ui.selectAll = false;
            moved = true;
            break;
        case VK_RIGHT:
            ui.caret = ui.selectAll ? ui.text.size() : (ctrl ? LookupNextWord(ui.text, ui.caret)
                                                             : LookupStepForward(ui.text, ui.caret));
            ui.selectAll = false;
            moved = true;
            break;
        case VK_HOME:
            ui.caret = 0;
            ui.selectAll = false;
            moved = true;
            break;
        case VK_END:
            ui.caret = ui.text.size();
            ui.selectAll = false;
            moved = true;
            break;
        case VK_BACK:
            if (ui.selectAll) {
                ui.text.clear();
                ui.caret = 0;
            } else if (ui.caret > 0) {
                const size_t from = ctrl ? LookupPrevWord(ui.text, ui.caret)
                                         : LookupStepBack(ui.text, ui.caret);
                ui.text.erase(from, ui.caret - from);
                ui.caret = from;
            } else {
                return true;
            }
            LookupUiEdited();
            return true;
        case VK_DELETE:
            if (ui.selectAll) {
                ui.text.clear();
                ui.caret = 0;
            } else if (ui.caret < ui.text.size()) {
                const size_t to = LookupStepForward(ui.text, ui.caret);
                ui.text.erase(ui.caret, to - ui.caret);
            } else {
                return true;
            }
            LookupUiEdited();
            return true;
        case 'A':
            if (!ctrl) return false;
            ui.selectAll = !ui.text.empty();
            moved = true;
            break;
        case 'V':
            if (!ctrl) return false;
            InsertIntoLookupField(QuickLookup::CollapseWhitespace(
                ReadClipboardText(hwnd), QuickLookup::kMaxQueryChars));
            return true;
        default:
            return false;
    }

    if (moved) {
        ui.caretResetAt = NowSeconds();
        TouchLookupPanel();
        g_layoutDirty = true;
    }
    return true;
}

bool HandleLookupChar(wchar_t ch) {
    if (!g_lookupUi.open) return false;
    if (ch < 0x20 || ch == 0x7F) return true;  // control keys are handled in WM_KEYDOWN
    InsertIntoLookupField(std::wstring(1, ch));
    return true;
}

// Returns true when the click belonged to the panel (so it must not fall through
// to "open the media app" and friends).
bool HandleLookupClick(int x, int y) {
    LookupUiState& ui = g_lookupUi;
    if (!ui.open) return false;

    LookupStatus status = LookupStatus::Search;
    const std::vector<LookupRow> rows = CurrentLookupRows(&status);
    const MediaContentPoint pt = MediaContentFromClient(x, y);
    TouchLookupPanel();
    if (!pt.valid) return true;

    if (status == LookupStatus::Search) {
        const bool header = LookupHasHeader(ui, rows);
        if (header && LookupHasRecentRows(rows) && LookupClearHitTest(pt)) {
            {
                std::lock_guard lock(g_stateMutex);
                g_state.lookupRecent.clear();
            }
            ui.selected = 0;
            g_layoutDirty = true;
            return true;
        }
        const int row = LookupRowAtContentPoint(pt, static_cast<int>(rows.size()), header);
        if (row >= 0) {
            SubmitLookup(rows[static_cast<size_t>(row)].query);
            return true;
        }
    }
    if (LookupFieldHitTest(pt)) {
        ui.caret = ui.text.size();
        ui.selectAll = false;
        ui.caretResetAt = NowSeconds();
        g_layoutDirty = true;
    }
    return true;
}

void HandleLookupMouseMove(int x, int y) {
    LookupUiState& ui = g_lookupUi;
    LookupStatus status = LookupStatus::Search;
    const std::vector<LookupRow> rows = CurrentLookupRows(&status);
    TouchLookupPanel();
    if (status != LookupStatus::Search) return;

    const int row = LookupRowAtContentPoint(MediaContentFromClient(x, y),
                                            static_cast<int>(rows.size()),
                                            LookupHasHeader(ui, rows));
    if (row >= 0 && row != ui.selected) {
        ui.selected = row;
        g_layoutDirty = true;
    }
}

LPCWSTR LookupCursorAt(int x, int y) {
    LookupStatus status = LookupStatus::Search;
    const std::vector<LookupRow> rows = CurrentLookupRows(&status);
    const MediaContentPoint pt = MediaContentFromClient(x, y);
    if (status == LookupStatus::Search) {
        const bool header = LookupHasHeader(g_lookupUi, rows);
        if ((header && LookupHasRecentRows(rows) && LookupClearHitTest(pt)) ||
            LookupRowAtContentPoint(pt, static_cast<int>(rows.size()), header) >= 0) {
            return IDC_HAND;
        }
    }
    return LookupFieldHitTest(pt) ? IDC_IBEAM : IDC_ARROW;
}

// Registers (or releases) the Quick Lookup hotkey. Render thread only, because
// RegisterHotKey must run on the thread that owns the window.
void ApplyLookupHotkey() {
    HWND hwnd = g_hwnd;
    if (!hwnd) {
        return;
    }
    if (g_lookupHotkeyRegistered) {
        UnregisterHotKey(hwnd, ID_LOOKUP_HOTKEY);
        g_lookupHotkeyRegistered = false;
    }

    const Settings settings = GetSettingsCopy();
    if (!settings.quickLookup) {
        return;
    }
    if (RegisterHotKey(hwnd, ID_LOOKUP_HOTKEY, settings.lookupModifiers | MOD_NOREPEAT, settings.lookupVk)) {
        g_lookupHotkeyRegistered = true;
    } else {
        Wh_Log(L"Failed to register Quick Lookup hotkey (error %lu).", GetLastError());
    }
}
