#pragma once
// NBA scoreboard parsing, kept separate from the island so it can be tested
// against frozen JSON without the Windhawk runtime.
//
// Data source: ESPN's public NBA scoreboard endpoint. It is not an official
// NBA API; treat failures as normal and keep the last good score.

#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <ctime>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

struct NbaSnapshot {
    bool active = false;
    bool live = false;
    bool finished = false;
    bool scheduled = false;
    bool stale = false;
    std::wstring gameId;
    double startTime = 0;
    std::wstring awayAbbr;
    std::wstring homeAbbr;
    std::wstring awayName;
    std::wstring homeName;
    std::wstring awayRecord;
    std::wstring homeRecord;
    int awayScore = 0;
    int homeScore = 0;
    int period = 0;
    std::wstring clock;
    std::wstring status;       // localized short status, e.g. "第4节 5:12"
    std::wstring matchup;      // "BOS @ DET"
    std::wstring gameUrl;
    std::wstring awayLogoUrl;
    std::wstring homeLogoUrl;
    double lastUpdated = 0.0;
    double finishedAt = 0.0;
};

inline std::wstring NormalizeNbaTeam(std::wstring value) {
    size_t begin = 0;
    while (begin < value.size() && iswspace(value[begin])) ++begin;
    size_t end = value.size();
    while (end > begin && iswspace(value[end - 1])) --end;
    value = value.substr(begin, end - begin);
    if (value.size() > 4) value.resize(4);
    for (wchar_t& c : value) c = static_cast<wchar_t>(towupper(c));
    return value;
}

inline std::wstring NbaPeriodText(int period, const std::wstring& clock) {
    std::wstring label;
    if (period >= 1 && period <= 4) {
        label = L"第" + std::to_wstring(period) + L"节";
    } else if (period > 4) {
        label = period == 5 ? L"加时" : L"加时" + std::to_wstring(period - 4);
    }
    if (!clock.empty() && clock != L"0:00") {
        if (!label.empty()) label += L" ";
        label += clock;
    }
    return label;
}

inline double NbaSystemTimeSeconds(const SYSTEMTIME& time) {
    FILETIME ft{};
    if (!SystemTimeToFileTime(&time, &ft)) return 0;
    ULARGE_INTEGER ticks{};
    ticks.LowPart = ft.dwLowDateTime; ticks.HighPart = ft.dwHighDateTime;
    return static_cast<double>(ticks.QuadPart) / 10000000.0 - 11644473600.0;
}

inline double NbaParseUtc(const std::wstring& text) {
    SYSTEMTIME st{};
    if (text.empty() || text.back() != L'Z' ||
        swscanf_s(text.c_str(), L"%hu-%hu-%huT%hu:%hu", &st.wYear, &st.wMonth,
            &st.wDay, &st.wHour, &st.wMinute) != 5) return 0;
    return NbaSystemTimeSeconds(st);
}

inline std::wstring NormalizeNbaTeams(std::wstring value) {
    for (auto& c : value) if (c == L'，' || c == L';' || c == L'；') c = L',';
    std::wstring result;
    size_t begin = 0;
    while (begin <= value.size()) {
        const size_t end = value.find(L',', begin);
        std::wstring token = value.substr(begin, end == std::wstring::npos ? end : end - begin);
        if (token.find(L"骑士") != std::wstring::npos) token = L"CLE";
        if (token.find(L"76人") != std::wstring::npos) token = L"PHI";
        token = NormalizeNbaTeam(token);
        if (!token.empty()) {
            if (!result.empty()) result += L",";
            result += token;
        }
        if (end == std::wstring::npos) break;
        begin = end + 1;
    }
    return result;
}

inline int NbaFavoriteRank(const NbaSnapshot& game, const std::wstring& favorites) {
    if (favorites.empty()) return 0;
    size_t begin = 0;
    int rank = 0;
    while (begin <= favorites.size()) {
        const size_t end = favorites.find(L',', begin);
        const std::wstring team = favorites.substr(begin, end == std::wstring::npos ? end : end - begin);
        if (_wcsicmp(team.c_str(), game.awayAbbr.c_str()) == 0 ||
            _wcsicmp(team.c_str(), game.homeAbbr.c_str()) == 0) return rank;
        if (end == std::wstring::npos) break;
        begin = end + 1;
        ++rank;
    }
    return (std::numeric_limits<int>::max)();
}

inline std::wstring NbaFavoritesLabel(const std::wstring& favorites) {
    std::wstring label = favorites;
    size_t pos;
    while ((pos = label.find(L"CLE")) != std::wstring::npos) label.replace(pos, 3, L"骑士");
    while ((pos = label.find(L"PHI")) != std::wstring::npos) label.replace(pos, 3, L"76人");
    for (auto& c : label) if (c == L',') c = L'、';
    return label.empty() ? L"全部球队" : label;
}

inline bool ParseNbaGames(const std::string& body, std::vector<NbaSnapshot>* out) {
    if (!out || body.empty()) return false;
    try {
        using namespace winrt::Windows::Data::Json;
        const JsonObject root = JsonObject::Parse(winrt::to_hstring(body));
        if (!root.HasKey(L"events")) return false;
        const JsonArray events = root.GetNamedArray(L"events");

        std::vector<NbaSnapshot> games;

        for (uint32_t i = 0; i < events.Size(); ++i) {
            const JsonObject event = events.GetObjectAt(i);
            if (!event.HasKey(L"competitions")) continue;
            const JsonArray competitions = event.GetNamedArray(L"competitions");
            if (competitions.Size() == 0) continue;
            const JsonObject competition = competitions.GetObjectAt(0);

            JsonObject status;
            if (competition.HasKey(L"status")) status = competition.GetNamedObject(L"status");
            else if (event.HasKey(L"status")) status = event.GetNamedObject(L"status");
            else continue;
            if (!status.HasKey(L"type")) continue;
            const JsonObject type = status.GetNamedObject(L"type");
            std::wstring state;
            if (type.HasKey(L"state")) state = type.GetNamedString(L"state").c_str();
            if (state != L"in" && state != L"post" && state != L"pre") continue;

            int period = 0;
            std::wstring clock;
            try {
                if (status.HasKey(L"period")) period = static_cast<int>(status.GetNamedNumber(L"period"));
                if (status.HasKey(L"displayClock")) clock = status.GetNamedString(L"displayClock").c_str();
            } catch (...) {
            }

            std::wstring awayAbbr, homeAbbr, awayName, homeName, awayRecord, homeRecord;
            std::wstring awayLogo, homeLogo;
            int awayScore = 0, homeScore = 0;
            bool haveAway = false, haveHome = false;
            const JsonArray competitors = competition.GetNamedArray(L"competitors");
            for (uint32_t j = 0; j < competitors.Size(); ++j) {
                const JsonObject competitor = competitors.GetObjectAt(j);
                std::wstring homeAway;
                if (competitor.HasKey(L"homeAway")) homeAway = competitor.GetNamedString(L"homeAway").c_str();
                if (!competitor.HasKey(L"team")) continue;
                const JsonObject team = competitor.GetNamedObject(L"team");
                std::wstring abbr, name, logo;
                if (team.HasKey(L"abbreviation")) abbr = team.GetNamedString(L"abbreviation").c_str();
                if (team.HasKey(L"shortDisplayName")) name = team.GetNamedString(L"shortDisplayName").c_str();
                else if (team.HasKey(L"displayName")) name = team.GetNamedString(L"displayName").c_str();
                if (team.HasKey(L"logoDark")) logo = team.GetNamedString(L"logoDark").c_str();
                if (logo.empty() && team.HasKey(L"logo")) logo = team.GetNamedString(L"logo").c_str();

                int score = 0;
                if (competitor.HasKey(L"score")) {
                    const JsonValue value = competitor.GetNamedValue(L"score");
                    if (value.ValueType() == JsonValueType::String) score = _wtoi(value.GetString().c_str());
                    else if (value.ValueType() == JsonValueType::Number) score = static_cast<int>(value.GetNumber());
                }
                std::wstring record;
                if (competitor.HasKey(L"records")) {
                    const JsonArray records = competitor.GetNamedArray(L"records");
                    if (records.Size() > 0) {
                        const JsonObject first = records.GetObjectAt(0);
                        if (first.HasKey(L"summary")) record = first.GetNamedString(L"summary").c_str();
                    }
                }
                if (homeAway == L"away") {
                    awayAbbr = abbr; awayName = name; awayScore = score; awayRecord = record;
                    awayLogo = logo;
                    haveAway = true;
                } else if (homeAway == L"home") {
                    homeAbbr = abbr; homeName = name; homeScore = score; homeRecord = record;
                    homeLogo = logo;
                    haveHome = true;
                }
            }
            if (!haveAway || !haveHome) continue;

            NbaSnapshot candidate;
            if (event.HasKey(L"id")) candidate.gameId = event.GetNamedString(L"id").c_str();
            if (event.HasKey(L"date")) candidate.startTime = NbaParseUtc(event.GetNamedString(L"date").c_str());
            if (candidate.gameId.empty() || candidate.startTime == 0) return false;
            candidate.scheduled = state == L"pre";
            candidate.awayAbbr = awayAbbr;
            candidate.homeAbbr = homeAbbr;
            candidate.awayName = awayName;
            candidate.homeName = homeName;
            candidate.awayRecord = awayRecord;
            candidate.homeRecord = homeRecord;
            candidate.awayLogoUrl = awayLogo;
            candidate.homeLogoUrl = homeLogo;
            candidate.awayScore = awayScore;
            candidate.homeScore = homeScore;
            candidate.period = period;
            candidate.clock = clock;
            candidate.matchup = awayAbbr + L" @ " + homeAbbr;
            candidate.live = state == L"in";
            candidate.finished = state == L"post";
            candidate.status = candidate.live ? NbaPeriodText(period, clock) : L"已结束";
            if (event.HasKey(L"links")) {
                const JsonArray links = event.GetNamedArray(L"links");
                for (uint32_t k = 0; k < links.Size(); ++k) {
                    const JsonObject link = links.GetObjectAt(k);
                    if (link.HasKey(L"href")) {
                        candidate.gameUrl = link.GetNamedString(L"href").c_str();
                        break;
                    }
                }
            }

            games.push_back(std::move(candidate));
        }
        *out = std::move(games);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool SelectNbaGame(const std::vector<NbaSnapshot>& games, const std::wstring& favorites,
                          const std::wstring& preferAway, const std::wstring& preferHome,
                          NbaSnapshot* out, const std::wstring& previousId = L"") {
    if (!out) return false;
    int bestRank = (std::numeric_limits<int>::max)();
    const NbaSnapshot* live = nullptr;
    const NbaSnapshot* finalGame = nullptr;
    for (const auto& game : games) {
        const int rank = NbaFavoriteRank(game, favorites);
        if (rank == (std::numeric_limits<int>::max)()) continue;
        const bool previous = game.awayAbbr == preferAway && game.homeAbbr == preferHome &&
            (previousId.empty() || game.gameId == previousId);
        if (game.live && (rank < bestRank || (rank == bestRank && previous))) {
            bestRank = rank;
            live = &game;
        }
        if (game.finished && previous && !preferAway.empty()) finalGame = &game;
    }
    if (live) { *out = *live; return true; }
    if (finalGame) { *out = *finalGame; return true; }
    return false;
}

inline bool ParseNbaScoreboard(const std::string& body, const std::wstring& favorites,
                               const std::wstring& preferAway, const std::wstring& preferHome,
                               NbaSnapshot* out) {
    std::vector<NbaSnapshot> games;
    return ParseNbaGames(body, &games) &&
        SelectNbaGame(games, NormalizeNbaTeams(favorites), preferAway, preferHome, out);
}

inline NbaSnapshot AdvanceNbaSnapshot(const NbaSnapshot& previous,
                                      const std::vector<NbaSnapshot>& games,
                                      const std::wstring& favorites, bool success, double now) {
    NbaSnapshot next;
    if (success && SelectNbaGame(games, favorites, previous.awayAbbr, previous.homeAbbr, &next, previous.gameId)) {
        next.active = true;
        next.lastUpdated = now;
        if (next.finished) {
            next.finishedAt = previous.finishedAt > 0 ? previous.finishedAt : now;
            next.active = now - next.finishedAt < 600;
        }
        return next;
    }
    next = previous;
    // Missing/failed data cannot prove the game ended. Mark old data and expire it.
    if (next.active) {
        next.stale = true;
        if (now - next.lastUpdated >= 120 ||
            (next.finishedAt > 0 && now - next.finishedAt >= 600)) next.active = false;
    }
    return next;
}

struct NbaDaySummary {
    int total = 0, scheduled = 0, live = 0, finished = 0, favorites = 0;
};
inline NbaDaySummary SummarizeNbaDay(const std::vector<NbaSnapshot>& games,
                                    double dayStart, double dayEnd, const std::wstring& favorites) {
    NbaDaySummary summary;
    for (const auto& game : games) {
        if (game.startTime < dayStart || game.startTime >= dayEnd) continue;
        ++summary.total;
        summary.scheduled += game.scheduled;
        summary.live += game.live;
        summary.finished += game.finished;
        summary.favorites += NbaFavoriteRank(game, favorites) != (std::numeric_limits<int>::max)();
    }
    return summary;
}

struct NbaLocalDay {
    std::wstring dates[3];
    std::wstring label;
    double start = 0, end = 0;
};
inline NbaLocalDay GetNbaLocalDay(SYSTEMTIME local) {
    local.wHour = local.wMinute = local.wSecond = local.wMilliseconds = 0;
    NbaLocalDay day;
    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc)) return day;
    day.start = NbaSystemTimeSeconds(utc);
    FILETIME ft{};
    if (!SystemTimeToFileTime(&local, &ft)) return {};
    ULARGE_INTEGER ticks{};
    ticks.LowPart = ft.dwLowDateTime; ticks.HighPart = ft.dwHighDateTime;
    for (int i = 0; i < 3; ++i) {
        ULARGE_INTEGER dateTicks{};
        dateTicks.QuadPart = ticks.QuadPart + static_cast<int64_t>(i - 1) * 864000000000LL;
        FILETIME dateFt{dateTicks.LowPart, dateTicks.HighPart};
        SYSTEMTIME date{};
        if (!FileTimeToSystemTime(&dateFt, &date)) return {};
        wchar_t text[16]{};
        swprintf_s(text, L"%04u%02u%02u", date.wYear, date.wMonth, date.wDay);
        day.dates[i] = text;
        if (i == 1) {
            swprintf_s(text, L"%04u-%02u-%02u", date.wYear, date.wMonth, date.wDay);
            day.label = text;
        }
        if (i == 2) {
            if (!TzSpecificLocalTimeToSystemTime(nullptr, &date, &utc)) return {};
            day.end = NbaSystemTimeSeconds(utc);
        }
    }
    return day;
}
