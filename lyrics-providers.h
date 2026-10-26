// Direct line-synced lyric sources; API references:
// https://github.com/jacquesh/foo_openlyrics/tree/main/src/sources (MIT)
// Copyright (c) 2021 Jacques Heunis
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// Additional API reference:
// https://github.com/chenmozhijin/LDDC/tree/main/LDDC/core/api/lyrics
// No LDDC code or encrypted/word-level lyric decoders are included.
namespace LyricsSources {
using namespace winrt::Windows::Data::Json;
enum class Source { NetEase, QQ, Kugou };

struct Candidate {
    std::wstring title, artist, id, token;
    double duration = 0;
    int score = -1;
};

std::wstring Encode(const std::wstring& text) { return AsciiToWide(UrlEncodeComponent(text)); }

JsonObject Request(const wchar_t* host, const std::wstring& path, const wchar_t* headers) {
    const auto response = HttpGetEx(host, path.c_str(), true, kLyricsUserAgent, headers, 6000);
    JsonObject object{nullptr};
    if (!response.ok || response.status != 200 || response.body.empty() ||
        !JsonObject::TryParse(winrt::to_hstring(response.body), object))
        throw winrt::hresult_error(E_FAIL);
    return object;
}

std::wstring String(const JsonObject& object, const wchar_t* key) {
    return std::wstring(object.GetNamedString(key, L""));
}

std::wstring Id(const JsonObject& object, const wchar_t* key) {
    const auto value = object.GetNamedValue(key);
    if (value.ValueType() == JsonValueType::String) return std::wstring(value.GetString());
    const double number = value.GetNumber();
    if (!std::isfinite(number) || number <= 0 || number > 9007199254740991.0 || std::floor(number) != number)
        throw winrt::hresult_error(E_INVALIDARG);
    return std::to_wstring(static_cast<uint64_t>(number));
}

std::wstring Artists(const JsonArray& array) {
    std::wstring artists;
    for (const auto& value : array) {
        const auto name = String(value.GetObject(), L"name");
        if (!artists.empty()) artists += L" / ";
        artists += name;
    }
    return artists;
}

bool HasVersion(const std::wstring& title, const wchar_t* a, const wchar_t* b) {
    const auto lower = ToLowerCopy(title);
    return lower.find(a) != std::wstring::npos || lower.find(b) != std::wstring::npos;
}

int MatchScore(const Candidate& candidate, const std::wstring& title,
               const std::wstring& artist, int duration) {
    if (!std::isfinite(candidate.duration) || candidate.duration < 0 || candidate.duration > 86400) return -1;
    const int titleMatch = LyricsTextMatch(candidate.title, title);
    const int artistMatch = artist.empty() ? 0 : LyricsTextMatch(candidate.artist, PrimaryLyricsArtist(artist));
    if (!titleMatch || (!artist.empty() && !artistMatch)) return -1;
    if (HasVersion(title, L"live", L"现场") != HasVersion(candidate.title, L"live", L"现场") ||
        HasVersion(title, L"remix", L"混音") != HasVersion(candidate.title, L"remix", L"混音") ||
        HasVersion(title, L"instrumental", L"伴奏") != HasVersion(candidate.title, L"instrumental", L"伴奏")) return -1;
    const double delta = duration > 0 && candidate.duration > 0 ? std::abs(candidate.duration - duration) : -1;
    if (delta > 5 || (artist.empty() && (titleMatch != 2 || delta < 0 || delta > 2))) return -1;
    return titleMatch * 30 + artistMatch * 20 + (delta < 0 ? 0 : (delta <= 2 ? 40 : 25));
}

constexpr const wchar_t* kNetEaseHeaders = L"Referer: https://music.163.com/\r\nCookie: appver=2.0.2\r\n";
constexpr const wchar_t* kQQHeaders = L"Referer: https://y.qq.com/portal/player.html\r\n";

std::vector<Candidate> Search(Source source, const std::wstring& title,
                              const std::wstring& artist, int duration) {
    std::vector<Candidate> candidates;
    const auto searchArtist = PrimaryLyricsArtist(artist);
    const auto keyword = Encode(searchArtist.empty() ? title : searchArtist + L" " + title);
    JsonArray rows;
    if (source == Source::NetEase) {
        const auto object = Request(L"music.163.com", L"/api/search/get?s=" + keyword +
            L"&type=1&offset=0&limit=8", kNetEaseHeaders);
        if (object.GetNamedNumber(L"code") != 200) throw winrt::hresult_error(E_FAIL);
        const auto result = object.GetNamedObject(L"result");
        if (!result.HasKey(L"songs") && result.GetNamedNumber(L"songCount", -1) != 0)
            throw winrt::hresult_error(E_FAIL);
        rows = result.GetNamedArray(L"songs", JsonArray{});
        for (const auto& value : rows) {
            const auto row = value.GetObject();
            candidates.push_back({String(row, L"name"), Artists(row.GetNamedArray(L"artists")),
                Id(row, L"id"), L"", row.GetNamedNumber(L"duration", 0) / 1000.0});
        }
    } else if (source == Source::QQ) {
        const auto object = Request(L"c.y.qq.com", L"/splcloud/fcgi-bin/smartbox_new.fcg?format=json&inCharset=utf-8&outCharset=utf-8&key=" + keyword, kQQHeaders);
        if (object.GetNamedNumber(L"code") != 0) throw winrt::hresult_error(E_FAIL);
        rows = object.GetNamedObject(L"data").GetNamedObject(L"song").GetNamedArray(L"itemlist");
        for (const auto& value : rows) {
            const auto row = value.GetObject();
            candidates.push_back({String(row, L"name"), String(row, L"singer"), Id(row, L"mid")});
        }
    } else {
        // Resolve a full recording first: keyword-only lyric searches can return short clips.
        const auto object = Request(L"mobiles.kugou.com", L"/api/v3/search/song?keyword=" + keyword +
            L"&page=1&pagesize=8&showtype=14", nullptr);
        if (object.GetNamedNumber(L"status") != 1) throw winrt::hresult_error(E_FAIL);
        rows = object.GetNamedObject(L"data").GetNamedArray(L"info");
        for (const auto& value : rows) {
            const auto row = value.GetObject();
            candidates.push_back({String(row, L"songname"), String(row, L"singername"), String(row, L"hash"),
                L"", row.GetNamedNumber(L"duration", 0)});
        }
    }
    for (auto& candidate : candidates) candidate.score = MatchScore(candidate, title, artist, duration);
    std::erase_if(candidates, [source](const auto& candidate) {
        return candidate.score < 0 || candidate.id.empty() || (source == Source::Kugou && candidate.duration <= 0);
    });
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.score > b.score; });
    if (candidates.size() > 2) candidates.resize(2);
    return candidates;
}

void Fetch(Source source, const std::wstring& title, const std::wstring& artist,
           int duration, LyricsPick* pick, bool* definitive) {
    try {
        for (auto candidate : Search(source, title, artist, duration)) {
            if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) return;
            try {
                std::wstring text;
                if (source == Source::NetEase) {
                    const auto object = Request(L"music.163.com", L"/api/song/lyric?tv=-1&kv=-1&lv=-1&os=pc&id=" + Encode(candidate.id), kNetEaseHeaders);
                    if (object.GetNamedNumber(L"code") != 200) throw winrt::hresult_error(E_FAIL);
                    if (object.GetNamedBoolean(L"nolyric", false) || object.GetNamedBoolean(L"uncollected", false)) continue;
                    text = String(object.GetNamedObject(L"lrc"), L"lyric");
                } else if (source == Source::QQ) {
                    // Smartbox omits duration; verify the recording before downloading its lyrics.
                    const std::wstring data = L"{\"comm\":{\"ct\":24,\"cv\":0},\"songinfo\":{\"module\":\"music.pf_song_detail_svr\",\"method\":\"get_song_detail_yqq\",\"param\":{\"song_mid\":" +
                        std::wstring(JsonValue::CreateStringValue(candidate.id).Stringify()) + L"}}}";
                    const auto detail = Request(L"u.y.qq.com", L"/cgi-bin/musicu.fcg?data=" + Encode(data), kQQHeaders);
                    if (detail.GetNamedNumber(L"code") != 0) throw winrt::hresult_error(E_FAIL);
                    const auto response = detail.GetNamedObject(L"songinfo");
                    if (response.GetNamedNumber(L"code") != 0) throw winrt::hresult_error(E_FAIL);
                    const auto track = response.GetNamedObject(L"data").GetNamedObject(L"track_info");
                    candidate.title = String(track, L"name");
                    candidate.artist = Artists(track.GetNamedArray(L"singer"));
                    candidate.duration = track.GetNamedNumber(L"interval");
                    candidate.score = MatchScore(candidate, title, artist, duration);
                    if (candidate.score < 0) continue;
                    const auto object = Request(L"c.y.qq.com", L"/lyric/fcgi-bin/fcg_query_lyric_new.fcg?g_tk=5381&format=json&nobase64=1&songmid=" + Encode(candidate.id), kQQHeaders);
                    if (object.GetNamedNumber(L"retcode") != 0) throw winrt::hresult_error(E_FAIL);
                    text = String(object, L"lyric");
                } else {
                    const auto matches = Request(L"lyrics.kugou.com", L"/search?ver=1&man=no&client=pc&keyword=" +
                        Encode(candidate.artist + L" - " + candidate.title) + L"&hash=" + Encode(candidate.id) +
                        L"&duration=" + std::to_wstring(static_cast<int64_t>(candidate.duration * 1000)), nullptr);
                    if (matches.GetNamedNumber(L"status") == 404) continue;
                    if (matches.GetNamedNumber(L"status") != 200) throw winrt::hresult_error(E_FAIL);
                    Candidate lyric; int bestScore = -1;
                    for (const auto& value : matches.GetNamedArray(L"candidates")) {
                        const auto row = value.GetObject();
                        // Hash pins the recording; the lyric's singer field may name its uploader.
                        Candidate next{String(row, L"song"), candidate.artist, Id(row, L"id"),
                            String(row, L"accesskey"), row.GetNamedNumber(L"duration", 0) / 1000.0};
                        if (next.id.empty() || next.token.empty() || next.duration <= 0) continue;
                        const int score = MatchScore(next, candidate.title, candidate.artist,
                            static_cast<int>(candidate.duration + 0.5));
                        if (score > bestScore) { bestScore = score; lyric = std::move(next); }
                    }
                    if (bestScore < 0) continue;
                    const auto object = Request(L"lyrics.kugou.com", L"/download?ver=1&client=pc&fmt=lrc&charset=utf8&id=" + Encode(lyric.id) + L"&accesskey=" + Encode(lyric.token), nullptr);
                    if (object.GetNamedNumber(L"status") != 200) throw winrt::hresult_error(E_FAIL);
                    using namespace winrt::Windows::Security::Cryptography;
                    const auto content = object.GetNamedString(L"content");
                    if (content.empty()) continue;
                    text = std::wstring(CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8,
                        CryptographicBuffer::DecodeFromBase64String(content)));
                }
                auto lines = ParseSyncedLyrics(text);
                const bool synced = !lines.empty();
                if (!synced) lines = ParsePlainLyrics(text);
                if (lines.empty()) continue;
                // A plain-text result must not displace synchronized lyrics from another source.
                if (!pick->found || (synced && !pick->synced) || (synced == pick->synced && candidate.score > pick->score)) {
                    pick->found = true; pick->synced = synced;
                    pick->score = candidate.score; pick->lines = std::move(lines);
                }
                if (synced) return;
            } catch (const winrt::hresult_error&) { *definitive = false; }
        }
    } catch (const winrt::hresult_error&) { *definitive = false; }
}
} // namespace LyricsSources

bool FetchLyricsFromSources(const std::wstring& title, const std::wstring& artist,
                            const std::wstring& album, int duration, LyricsPick* pick, bool* definitive) {
    *definitive = true;
    const bool chinese = std::any_of(title.begin(), title.end(), [](wchar_t c) { return c >= 0x3400 && c <= 0x9fff; }) ||
        std::any_of(artist.begin(), artist.end(), [](wchar_t c) { return c >= 0x3400 && c <= 0x9fff; });
    auto lrclib = [&] {
        bool complete = true;
        FetchLyricsFromLrclib(title, artist, album, duration, pick, &complete);
        *definitive = *definitive && complete;
    };
    if (!chinese) lrclib();
    for (const auto source : {LyricsSources::Source::NetEase, LyricsSources::Source::QQ, LyricsSources::Source::Kugou}) {
        if (pick->found && pick->synced) return true;
        if (WaitForSingleObject(g_stopEvent, 0) != WAIT_TIMEOUT) return pick->found;
        LyricsSources::Fetch(source, title, artist, duration, pick, definitive);
    }
    if (chinese && !(pick->found && pick->synced) && WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT) lrclib();
    return pick->found;
}
