// shortcut: Youdao's public web endpoints have no versioned contract; update if its response schema changes.
Outcome LookupSuggestions(const std::wstring& query, std::vector<LookupRecent>* out) {
    const auto path = L"/suggest?doctype=json&num=3&q=" + AsciiToWide(UrlEncodeComponent(query));
    const auto response = HttpGetEx(L"dict.youdao.com", path.c_str(), true, kLookupUserAgent, nullptr, 1500);
    if (!response.IsDefinitive()) return Outcome::Failed;
    if (response.status == 404) return Outcome::NotFound;
    try {
        using namespace winrt::Windows::Data::Json;
        const auto root = JsonObject::Parse(winrt::to_hstring(response.body));
        if (root.GetNamedObject(L"result").GetNamedNumber(L"code") != 200) return Outcome::Failed;
        std::vector<LookupRecent> entries;
        for (const auto& value : root.GetNamedObject(L"data").GetNamedArray(L"entries")) {
            const auto row = value.GetObject();
            const auto word = NormalizeQuery(std::wstring(row.GetNamedString(L"entry", L"")));
            if (word.empty() || word.size() > kMaxQueryChars ||
                std::any_of(entries.begin(), entries.end(), [&](const auto& item) { return item.query == word; })) continue;
            entries.push_back({word, word, ClipToChars(CollapseWhitespace(std::wstring(row.GetNamedString(L"explain", L""))), 100), L"有道词典"});
            if (entries.size() == 3) break;
        }
        *out = std::move(entries);
        return out->empty() ? Outcome::NotFound : Outcome::Found;
    } catch (...) { return Outcome::Failed; }
}

Outcome LookupText(const std::wstring& query, Result* out) {
    const auto path = L"/jsonapi?jsonversion=2&dicts=%7B%22count%22%3A2%2C%22dicts%22%3A%5B%5B%22ec%22%2C%22ce%22%5D%5D%7D&q=" +
        AsciiToWide(UrlEncodeComponent(query));
    const auto response = HttpGetEx(L"dict.youdao.com", path.c_str(), true, kLookupUserAgent, nullptr, 5000);
    if (!response.IsDefinitive()) return Outcome::Failed;
    if (response.status == 404) return Outcome::NotFound;
    try {
        using namespace winrt::Windows::Data::Json;
        const auto root = JsonObject::Parse(winrt::to_hstring(response.body));
        const bool english = root.HasKey(L"ec");
        if (!english && !root.HasKey(L"ce")) return Outcome::NotFound;
        const auto words = root.GetNamedObject(english ? L"ec" : L"ce").GetNamedArray(L"word");
        if (words.Size() == 0) return Outcome::NotFound;
        const auto word = words.GetAt(0).GetObject();
        Result result;
        result.title = query;
        result.source = L"有道词典";
        result.subtitle = english ? L"英 → 中" : L"中 → 英";
        for (const auto* key : {L"ukphone", L"usphone"}) {
            const std::wstring phone(word.GetNamedString(key, L""));
            if (!phone.empty()) result.subtitle += std::wstring(key[0] == L'u' && key[1] == L'k' ? L"  英 /" : L"  美 /") + phone + L"/";
        }
        for (const auto& value : word.GetNamedArray(L"trs")) {
            for (const auto& translation : value.GetObject().GetNamedArray(L"tr")) {
                const auto label = translation.GetObject().GetNamedObject(L"l");
                std::wstring text;
                for (const auto& item : label.GetNamedArray(L"i")) {
                    const std::wstring part(item.ValueType() == JsonValueType::String ? item.GetString() :
                        item.GetObject().GetNamedString(L"#text", L""));
                    if (part.empty()) continue;
                    if (!text.empty()) text += L"; ";
                    text += part;
                }
                if (!english) {
                    const std::wstring detail(label.GetNamedString(L"#tran", L""));
                    if (!detail.empty()) text += L" — " + detail;
                }
                text = CollapseWhitespace(text);
                if (text.empty()) continue;
                if (!result.body.empty()) result.body += L"\n";
                result.body += text;
                if (result.body.size() >= kMaxBodyChars) break;
            }
            if (result.body.size() >= kMaxBodyChars) break;
        }
        if (result.body.empty()) return Outcome::NotFound;
        result.body = ClipToChars(result.body, kMaxBodyChars);
        *out = std::move(result);
        return Outcome::Found;
    } catch (...) { return Outcome::Failed; }
}

void ApplySuggestions(uint64_t id, std::vector<LookupRecent> entries, bool returnToSearch = false) {
    std::lock_guard lock(g_stateMutex);
    if (id != g_lookupRequestSeq.load() || !g_state.lookup.active ||
        (!returnToSearch && g_state.lookup.status != LookupStatus::Search)) return;
    g_state.lookupSuggestions = std::move(entries);
    if (returnToSearch) {
        g_state.lookup.status = LookupStatus::Search;
        g_state.lookup.expiresAt = NowSeconds() + kPanelIdleSeconds;
    }
    g_layoutDirty = true;
}
