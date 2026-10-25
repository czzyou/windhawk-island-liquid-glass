// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
struct LyricsLine {
    int64_t timeMs = -1;  // -1 for unsynced (plain) lines
    std::wstring text;
};

// Immutable, shared lyric lines. Same read API as the vector it replaces, so
// the drawing code (empty / size / operator[]) is unchanged.
class LyricsLines {
   public:
    LyricsLines() = default;
    explicit LyricsLines(std::vector<LyricsLine>&& v)
        : p_(std::make_shared<const std::vector<LyricsLine>>(std::move(v))) {}

    bool empty() const { return !p_ || p_->empty(); }
    size_t size() const { return p_ ? p_->size() : 0; }
    const LyricsLine& operator[](size_t i) const { return (*p_)[i]; }
    const std::vector<LyricsLine>& vec() const {
        static const std::vector<LyricsLine> none;
        return p_ ? *p_ : none;
    }

   private:
    std::shared_ptr<const std::vector<LyricsLine>> p_;
};

struct LyricsSnapshot {
    bool hasData = false;
    bool synced = false;
    bool notFound = false;
    bool fetching = false;
    bool failed = false;
    std::wstring trackIdentity;
    std::wstring matchedTitle;
    std::wstring matchedArtist;
    LyricsLines lines;
};

// ── Quick Lookup state ───────────────────────────────────────────────────────
enum class LookupStatus {
    Search,    // panel open: search box + recent list
    Loading,   // request running
    Found,     // title / subtitle / body (/ example) are filled in
    Failed,
    NoResult,  // nothing found, or the network failed
    NoText,    // legacy, no longer produced
};

struct LookupSnapshot {
    bool active = false;
    LookupStatus status = LookupStatus::Loading;
    std::wstring query;      // the normalised text being looked up
    std::wstring title;      // headword / article title
    std::wstring subtitle;   // "phonetic · part of speech", or the Wikipedia short description
    std::wstring body;       // definition / first sentences of the extract
    std::wstring example;    // dictionary example sentence, if any
    std::wstring source;     // "Dictionary" / "Wikipedia"
    double expiresAt = 0.0;
};

// Shared by Renderer::MeasureLookupCard / DrawLookup and ActivityForKind so the
// size the island animates to and the layout painted inside it cannot drift.
// One finished lookup, kept in memory only (never written to disk).
struct LookupRecent {
    std::wstring query;   // what was searched (normalised)
    std::wstring title;   // headword / article title that came back
    std::wstring detail;  // one-line definition preview
    std::wstring source;
};

namespace LookupLayout {
    constexpr float kWidth = 380.0f;
    constexpr float kPadX = 16.0f;
    constexpr float kPadTop = 12.0f;
    constexpr float kPadBottom = 12.0f;
    constexpr float kTitleH = 20.0f;
    constexpr float kSubtitleH = 15.0f;
    constexpr float kCardGap = 6.0f;
    constexpr float kCardPad = 9.0f;
    constexpr float kExampleGap = 5.0f;
    constexpr float kSimpleHeight = 54.0f;
    constexpr float kMaxHeight = 300.0f;
    constexpr int kMaxBodyLines = 7;
    constexpr int kMaxExampleLines = 2;

    // Search panel
    constexpr float kFieldTop = kPadTop;
    constexpr float kFieldH = 36.0f;
    constexpr float kFieldGap = 10.0f;
    constexpr float kBelowField = kFieldTop + kFieldH + kFieldGap;  // where content under the box starts
    constexpr float kSectionH = 18.0f;   // "Recent ... Clear" header band
    constexpr float kRowH = 32.0f;
    constexpr float kRowGap = 3.0f;
    constexpr float kStatusH = 30.0f;    // one-line states under the box
    constexpr int kMaxRows = 4;
    constexpr double kCaretPeriod = 1.0;
    constexpr double kCaretOn = 0.55;

    constexpr float RowsTop(bool header) { return kBelowField + (header ? kSectionH : 0.0f); }
    constexpr float SearchHeight(int rows, bool header) {
        return rows <= 0 ? kBelowField + kStatusH + kPadBottom
                         : RowsTop(header) + rows * kRowH + (rows - 1) * kRowGap + kPadBottom;
    }
    constexpr float StatusHeight() { return kBelowField + kStatusH + kPadBottom; }
}

// Input state of the search box. Render thread only (the window procedure and the
// render loop share that thread), so it needs no lock.
struct LookupUiState {
    bool open = false;
    std::wstring text;
    size_t caret = 0;
    bool selectAll = false;
    int selected = 0;               // highlighted row
    double caretResetAt = 0.0;      // blink restarts on every edit
    std::wstring clipboardQuery;    // clipboard suggestion captured when the panel opened
    HWND prevForeground = nullptr;  // window to hand focus back to
};
LookupUiState g_lookupUi;
