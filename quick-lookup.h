// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
// ── Quick Lookup panel: rows + hit testing ───────────────────────────────────
// Shared by the renderer (what to draw) and the window procedure (what a click or
// Enter means) so the two can never disagree about which row is which.
enum class LookupRowKind { Typed, Clipboard, Recent };

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
                                       const LookupUiState& ui) {
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

    rows.push_back({LookupRowKind::Typed, typed, typed, std::wstring()});
    const std::wstring needle = ToLowerCopy(typed);
    for (const LookupRecent& r : recents) {
        if (rows.size() >= cap) break;
        const std::wstring q = ToLowerCopy(r.query);
        if (q == needle) continue;  // the Typed row already covers it
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

// ── Quick Lookup ─────────────────────────────────────────────────────────────
// Press the Quick Lookup hotkey to look up whatever text is on the clipboard.

// ── Quick Lookup ─────────────────────────────────────────────────────────────
// Press the Quick Lookup hotkey to look up whatever text is on the clipboard.
//
//   hotkey (render thread) -> StartQuickLookup()
//        1. read + normalise the clipboard text
//        2. in-memory cache hit?  -> publish the result at once
//        3. otherwise publish a "Looking up..." card and spawn ONE worker thread
//   worker thread          -> LookupText() -> publish the result if still wanted
//
// Privacy: nothing is looked up when the clipboard changes. Text is only sent over
// the network when the hotkey is pressed, only if it is a short word or phrase, and
// it is never written to the log or to disk.
//
// Adding a source later = write one function with the signature
//     Outcome LookupXxx(const std::wstring& query, Result* out)
// and add one line to LookupText().

// Wikimedia asks API clients to send a descriptive User-Agent with contact info.
constexpr wchar_t kLookupUserAgent[] =
    L"DynamicIslandForWindows/" WH_MOD_VERSION L" (https://github.com/devcode90/Dynamic-Island-for-Windows) QuickLookup";

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
constexpr size_t kMaxExtractSentences = 3;
constexpr size_t kMaxExtractChars = 320;
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

std::wstring TitleCase(std::wstring text) {
    bool wordStart = true;
    for (wchar_t& c : text) {
        if (c == L' ') {
            wordStart = true;
        } else {
            if (wordStart) {
                c = static_cast<wchar_t>(towupper(c));
            }
            wordStart = false;
        }
    }
    return text;
}

bool IsSentenceCloser(wchar_t c) {
    return c == L'"' || c == L'\'' || c == L')' || c == L'\u201d' || c == L'\u2019';
}

// A '.', '!' or '?' ends a sentence only when whitespace and then a capital, digit
// or opening quote follow, and (for '.') the word before it isn't an initial or a
// common abbreviation. Good enough for encyclopedia text, not a full tokenizer.
bool IsSentenceEnd(const std::wstring& t, size_t i) {
    const wchar_t c = t[i];
    if (c != L'.' && c != L'!' && c != L'?') {
        return false;
    }
    size_t j = i + 1;
    while (j < t.size() && IsSentenceCloser(t[j])) {
        ++j;
    }
    if (j >= t.size()) {
        return true;
    }
    if (!iswspace(t[j])) {
        return false;
    }
    while (j < t.size() && iswspace(t[j])) {
        ++j;
    }
    if (j >= t.size()) {
        return true;
    }
    if (!(iswupper(t[j]) || iswdigit(t[j]) || t[j] == L'"' || t[j] == L'\u201c' || t[j] == L'(')) {
        return false;
    }
    if (c != L'.') {
        return true;
    }

    size_t s = i;
    while (s > 0 && iswalpha(t[s - 1])) {
        --s;
    }
    const std::wstring word = ToLowerCopy(t.substr(s, i - s));
    if (word.size() == 1) {
        return false;  // initials: "J. R. R. Tolkien"
    }
    static const wchar_t* const kAbbrev[] = {
        L"mr", L"mrs", L"ms", L"dr", L"prof", L"st", L"jr", L"sr", L"vs", L"inc", L"ltd",
        L"co", L"no", L"ca", L"approx", L"mt", L"gen", L"col", L"lt", L"sgt", L"rev", L"fig", L"vol"};
    for (const wchar_t* abbrev : kAbbrev) {
        if (word == abbrev) {
            return false;
        }
    }
    return true;
}

// First `maxSentences` sentences, but stops early rather than exceed maxChars
// (the first sentence is always kept, clipped if it alone is too long).
std::wstring FirstSentences(const std::wstring& text, size_t maxSentences, size_t maxChars) {
    size_t sentences = 0;
    size_t acceptedEnd = 0;
    for (size_t i = 0; i < text.size() && sentences < maxSentences; ++i) {
        if (!IsSentenceEnd(text, i)) {
            continue;
        }
        size_t end = i + 1;
        while (end < text.size() && IsSentenceCloser(text[end])) {
            ++end;
        }
        if (sentences > 0 && end > maxChars) {
            break;
        }
        acceptedEnd = end;
        ++sentences;
        i = end - 1;
    }
    if (acceptedEnd == 0) {
        acceptedEnd = text.size();  // no sentence boundary found: use it all, clipped
    }
    std::wstring out = text.substr(0, acceptedEnd);
    while (!out.empty() && iswspace(out.back())) {
        out.pop_back();
    }
    return ClipToChars(out, maxChars);
}

// ---- JSON helper (same string-find style as the lyrics code) --------------------
// Returns the balanced {...} object that starts at `open`, string- and escape-aware.
std::string BalancedObjectAt(const std::string& json, size_t open) {
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = open; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            if (--depth == 0) {
                return json.substr(open, i - open + 1);
            }
        }
    }
    return std::string();
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

// ---- source 1: dictionary (single words) ----------------------------------------
// https://api.dictionaryapi.dev/api/v2/entries/en/<word>
// Shape: [{"word":..,"phonetic":..,"phonetics":[{"text":..}],
//          "meanings":[{"partOfSpeech":..,"definitions":[{"definition":..,"example":..}]}]}]
Outcome LookupDictionary(const std::wstring& word, Result* out) {
    const std::wstring path =
        L"/api/v2/entries/en/" + AsciiToWide(UrlEncodeComponent(ToLowerCopy(word)));
    const HttpResult res = HttpGetEx(L"api.dictionaryapi.dev", path.c_str(), true, kLookupUserAgent);
    if (!res.IsDefinitive()) {
        return Outcome::Failed;
    }
    if (res.status != 200) {
        return Outcome::NotFound;
    }

    const std::vector<std::string> entries = SplitJsonObjectArray(res.body);
    if (entries.empty()) {
        return Outcome::NotFound;
    }
    const std::string& entry = entries.front();

    std::string wordRaw, phoneticRaw;
    ExtractJsonStringField(entry, "word", &wordRaw);
    if (!ExtractJsonStringField(entry, "phonetic", &phoneticRaw) || phoneticRaw.empty()) {
        phoneticRaw.clear();
        ExtractJsonStringField(entry, "text", &phoneticRaw);  // first phonetics[].text
    }

    // First meaning's part of speech, then the first definition object after it.
    const size_t meaningsPos = entry.find("\"meanings\":[");
    if (meaningsPos == std::string::npos) {
        return Outcome::NotFound;
    }
    std::string posRaw;
    const size_t posKey = entry.find("\"partOfSpeech\":\"", meaningsPos);
    if (posKey != std::string::npos) {
        ExtractJsonStringField(entry.substr(posKey), "partOfSpeech", &posRaw);
    }
    const size_t defKey =
        entry.find("\"definition\":\"", posKey != std::string::npos ? posKey : meaningsPos);
    if (defKey == std::string::npos) {
        return Outcome::NotFound;
    }

    // Bound the search to this one definition object so a later definition's
    // "example" is never attributed to the first one.
    const size_t defOpen = entry.rfind('{', defKey);
    const std::string defObject = defOpen != std::string::npos ? BalancedObjectAt(entry, defOpen)
                                                               : entry.substr(defKey);
    std::string defRaw, exampleRaw;
    ExtractJsonStringField(defObject, "definition", &defRaw);
    ExtractJsonStringField(defObject, "example", &exampleRaw);

    const std::wstring definition = CollapseWhitespace(JsonStringUnescape(defRaw));
    if (definition.empty()) {
        return Outcome::NotFound;
    }

    Result r;
    r.title = CollapseWhitespace(JsonStringUnescape(wordRaw));
    if (r.title.empty()) {
        r.title = word;
    }
    r.subtitle = CollapseWhitespace(JsonStringUnescape(phoneticRaw));
    const std::wstring pos = CollapseWhitespace(JsonStringUnescape(posRaw));
    if (!pos.empty()) {
        if (!r.subtitle.empty()) {
            r.subtitle += L"  \u00b7  ";
        }
        r.subtitle += pos;
    }
    r.body = ClipToChars(definition, kMaxBodyChars);
    const std::wstring example = CollapseWhitespace(JsonStringUnescape(exampleRaw));
    if (!example.empty()) {
        r.example = L"\u201c" + ClipToChars(example, 200) + L"\u201d";
    }
    r.source = L"Dictionary";
    *out = std::move(r);
    return Outcome::Found;
}

// ---- source 2: Wikipedia summary (phrases, or words the dictionary lacks) ---------
// https://en.wikipedia.org/api/rest_v1/page/summary/<title>
// Titles are case-sensitive after the first letter, so a miss is retried once in
// Title Case ("machine learning" -> "Machine Learning").
Outcome LookupWikipedia(const std::wstring& query, Result* out) {
    const std::wstring candidates[2] = {query, TitleCase(query)};
    Outcome outcome = Outcome::NotFound;

    for (int i = 0; i < 2; ++i) {
        if (i == 1 && candidates[1] == candidates[0]) {
            break;
        }
        std::wstring title = candidates[i];
        std::replace(title.begin(), title.end(), L' ', L'_');
        const std::wstring path =
            L"/api/rest_v1/page/summary/" + AsciiToWide(UrlEncodeComponent(title));

        const HttpResult res = HttpGetEx(L"en.wikipedia.org", path.c_str(), true, kLookupUserAgent);
        if (!res.IsDefinitive()) {
            return Outcome::Failed;
        }
        if (res.status != 200) {
            continue;  // 404: try the next spelling
        }

        std::string extractRaw, titleRaw, descRaw;
        if (!ExtractJsonStringField(res.body, "extract", &extractRaw) || extractRaw.empty()) {
            continue;
        }
        ExtractJsonStringField(res.body, "title", &titleRaw);
        ExtractJsonStringField(res.body, "description", &descRaw);

        const std::wstring extract = CollapseWhitespace(JsonStringUnescape(extractRaw));
        if (extract.empty()) {
            continue;
        }

        Result r;
        r.title = CollapseWhitespace(JsonStringUnescape(titleRaw));
        if (r.title.empty()) {
            r.title = query;
        }
        r.subtitle = CollapseWhitespace(JsonStringUnescape(descRaw));
        r.body = FirstSentences(extract, kMaxExtractSentences, kMaxExtractChars);
        r.source = L"Wikipedia";
        *out = std::move(r);
        return Outcome::Found;
    }
    return outcome;
}

// ---- source 3: Wiktionary (primary dictionary) --------------------------------------
// https://en.wiktionary.org/api/rest_v1/page/definition/<word>
// Shape: {"en":[{"partOfSpeech":"Noun","language":"English","definitions":[
//          {"definition":"<html>","parsedExamples":[{"example":"<html>"}]}, ...]}, ...],
//         "fr":[...], ...}
// Definitions come back as HTML, so tags are stripped and entities decoded.

// Removes <tags>, decodes common HTML entities, collapses whitespace.
std::wstring StripHtml(const std::wstring& in) {
    std::wstring noTags;
    noTags.reserve(in.size());
    bool inTag = false;
    for (wchar_t c : in) {
        if (c == L'<') {
            inTag = true;
            continue;
        }
        if (c == L'>' && inTag) {
            inTag = false;
            continue;
        }
        if (!inTag) {
            noTags.push_back(c);
        }
    }

    std::wstring decoded;
    decoded.reserve(noTags.size());
    for (size_t i = 0; i < noTags.size(); ++i) {
        if (noTags[i] == L'&') {
            const size_t semi = noTags.find(L';', i);
            if (semi != std::wstring::npos && semi - i <= 9) {
                const std::wstring ent = noTags.substr(i + 1, semi - i - 1);
                wchar_t rep = 0;
                if (ent == L"amp") rep = L'&';
                else if (ent == L"lt") rep = L'<';
                else if (ent == L"gt") rep = L'>';
                else if (ent == L"quot") rep = L'"';
                else if (ent == L"apos") rep = L'\'';
                else if (ent == L"nbsp") rep = L' ';
                else if (ent.size() > 1 && ent[0] == L'#') {
                    const unsigned long code =
                        (ent[1] == L'x' || ent[1] == L'X')
                            ? wcstoul(ent.c_str() + 2, nullptr, 16)
                            : wcstoul(ent.c_str() + 1, nullptr, 10);
                    if (code > 0 && code < 0x10000 && !(code >= 0xD800 && code <= 0xDFFF)) {
                        rep = static_cast<wchar_t>(code);
                    }
                }
                if (rep) {
                    decoded.push_back(rep);
                    i = semi;
                    continue;
                }
            }
        }
        decoded.push_back(noTags[i]);
    }
    return CollapseWhitespace(decoded);
}

// Reads the "en" section of a Wiktionary definition response.
bool ParseWiktionaryEnglish(const std::string& body, const std::wstring& title, Result* out) {
    size_t enKey = body.find("\"en\"");
    if (enKey == std::string::npos) return false;
    enKey = body.find(':', enKey + 4);
    if (enKey == std::string::npos) return false;
    enKey = body.find_first_not_of(" \t\r\n", enKey + 1);
    if (enKey == std::string::npos || body[enKey] != '[') return false;

    auto skipSeparators = [](const std::string& s, size_t p) {
        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '\n' ||
                                s[p] == '\r' || s[p] == '\t')) {
            ++p;
        }
        return p;
    };

    std::vector<std::wstring> partsOfSpeech;
    std::vector<std::wstring> definitions;
    std::wstring example;

    size_t p = enKey + 1;
    while (p < body.size()) {
        p = skipSeparators(body, p);
        if (p >= body.size() || body[p] != '{') {
            break;
        }
        const std::string block = BalancedObjectAt(body, p);
        if (block.empty()) {
            break;
        }
        p += block.size();

        std::string posRaw;
        ExtractJsonStringField(block, "partOfSpeech", &posRaw);
        const std::wstring pos = ToLowerCopy(CollapseWhitespace(JsonStringUnescape(posRaw)));
        if (!pos.empty() && partsOfSpeech.size() < 3 &&
            std::find(partsOfSpeech.begin(), partsOfSpeech.end(), pos) == partsOfSpeech.end()) {
            partsOfSpeech.push_back(pos);
        }

        // Definitions are taken from the first part of speech only.
        if (!definitions.empty()) {
            continue;
        }
        const size_t defsKey = block.find("\"definitions\":[");
        if (defsKey == std::string::npos) {
            continue;
        }
        size_t q = defsKey + 15;  // just past  "definitions":[
        while (q < block.size() && definitions.size() < 3) {
            q = skipSeparators(block, q);
            if (q >= block.size() || block[q] != '{') {
                break;
            }
            const std::string defObj = BalancedObjectAt(block, q);
            if (defObj.empty()) {
                break;
            }
            q += defObj.size();

            std::string defRaw;
            if (!ExtractJsonStringField(defObj, "definition", &defRaw)) {
                continue;
            }
            const std::wstring text = StripHtml(JsonStringUnescape(defRaw));
            if (text.empty()) {
                continue;
            }
            definitions.push_back(text);

            // Only the first definition's example is shown.
            if (definitions.size() == 1) {
                std::string exRaw;
                if (ExtractJsonStringField(defObj, "example", &exRaw)) {
                    example = StripHtml(JsonStringUnescape(exRaw));
                }
            }
        }
    }

    if (definitions.empty()) {
        return false;
    }

    Result r;
    r.title = title;
    for (size_t i = 0; i < partsOfSpeech.size(); ++i) {
        if (i > 0) {
            r.subtitle += L"  \u00b7  ";
        }
        r.subtitle += partsOfSpeech[i];
    }

    std::wstring bodyText;
    for (size_t i = 0; i < definitions.size(); ++i) {
        if (i > 0) {
            bodyText += L"\n";
        }
        if (definitions.size() > 1) {
            bodyText += std::to_wstring(i + 1) + L". ";
        }
        bodyText += ClipToChars(definitions[i], 170);
    }
    r.body = ClipToChars(bodyText, kMaxBodyChars);

    if (!example.empty()) {
        r.example = L"\u201c" + ClipToChars(example, 200) + L"\u201d";
    }
    r.source = L"Wiktionary";
    *out = std::move(r);
    return true;
}

// Titles are case-sensitive, so lowercase is tried first, then the text as typed.
Outcome LookupWiktionary(const std::wstring& word, Result* out) {
    const std::wstring lower = ToLowerCopy(word);
    const std::wstring candidates[2] = {lower, word};

    for (int i = 0; i < 2; ++i) {
        if (i == 1 && candidates[1] == candidates[0]) {
            break;
        }
        const std::wstring path = L"/api/rest_v1/page/definition/" +
                                  AsciiToWide(UrlEncodeComponent(candidates[i]));
        const HttpResult res = HttpGetEx(L"en.wiktionary.org", path.c_str(), true, kLookupUserAgent);
        if (!res.IsDefinitive()) {
            return Outcome::Failed;  // timeout / 429 / 5xx: never cached as "not found"
        }
        if (res.status != 200) {
            continue;  // 404: try the next spelling
        }
        if (ParseWiktionaryEnglish(res.body, candidates[i], out)) {
            return Outcome::Found;
        }
    }
    return Outcome::NotFound;
}

// ---- router -----------------------------------------------------------------------
// Single words:  Wiktionary -> dictionaryapi.dev (backup) -> Wikipedia
// Phrases:       Wikipedia  -> Wiktionary (idioms like "kick the bucket")
// To add a source: write LookupXxx() above and add one attempt() line below.
Outcome LookupEnglishText(const std::wstring& query, Result* out) {
    const bool singleWord = query.find(L' ') == std::wstring::npos;

    bool anyFailed = false;
    auto attempt = [&](Outcome outcome) -> bool {
        if (outcome == Outcome::Found) return true;
        if (outcome == Outcome::Failed) anyFailed = true;
        return false;
    };

    if (singleWord) {
        if (attempt(LookupWiktionary(query, out))) return Outcome::Found;
        if (attempt(LookupDictionary(query, out))) return Outcome::Found;
        if (attempt(LookupWikipedia(query, out))) return Outcome::Found;
    } else {
        if (attempt(LookupWikipedia(query, out))) return Outcome::Found;
        if (attempt(LookupWiktionary(query, out))) return Outcome::Found;
    }

    // "Nothing there" is only cacheable if every source gave a definite answer.
    return anyFailed ? Outcome::Failed : Outcome::NotFound;
}

// Keep the dictionary definition and add its Chinese translation, never replace it with a guessed meaning.
Outcome LookupText(const std::wstring& query, Result* out) {
    const auto outcome=LookupEnglishText(query,out);
    if (outcome!=Outcome::Found) return outcome;
    const std::wstring english=out->body;
    std::wstring segment=ClipToChars(CollapseWhitespace(english),240);
    if (LyricsCache::WideToUtf8(segment).size()>500) segment=query;
    const auto path=L"/get?q="+AsciiToWide(UrlEncodeComponent(segment))+L"&langpair=en%7Czh-CN";
    const auto response=HttpGetEx(L"api.mymemory.translated.net",path.c_str(),true,kLookupUserAgent);
    std::wstring chinese;
    double status=0;
    std::string raw;
    if (response.ok && response.status==200 && ExtractJsonNumberField(response.body,"responseStatus",&status) && status==200 &&
        ExtractJsonStringField(response.body,"translatedText",&raw))
        chinese=ClipToChars(CollapseWhitespace(JsonStringUnescape(raw)),180);
    out->body=L"中文（MyMemory 机译）："+(chinese.empty() ? std::wstring(L"暂不可用，可稍后重试") : chinese)+L"\n英文："+english;
    return outcome;
}

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
    return BuildLookupRows(g_state.lookupRecent, g_lookupUi);
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

    bool leaveResult = false;
    {
        std::lock_guard lock(g_stateMutex);
        leaveResult = g_state.lookup.active && g_state.lookup.status != LookupStatus::Search;
    }
    if (leaveResult) {
        LookupSnapshot snap;
        snap.active = true;
        snap.status = LookupStatus::Search;
        snap.expiresAt = now + QuickLookup::kPanelIdleSeconds;
        PublishNewLookup(snap, false);
    } else {
        TouchLookupPanel();
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
