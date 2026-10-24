struct NbaTeamChoice { const wchar_t* code; const wchar_t* name; };
constexpr NbaTeamChoice kNbaTeams[] = {
    {L"ATL",L"老鹰"},{L"BOS",L"凯尔特人"},{L"BKN",L"篮网"},{L"CHA",L"黄蜂"},
    {L"CHI",L"公牛"},{L"CLE",L"骑士"},{L"DAL",L"独行侠"},{L"DEN",L"掘金"},
    {L"DET",L"活塞"},{L"GS",L"勇士"},{L"HOU",L"火箭"},{L"IND",L"步行者"},
    {L"LAC",L"快船"},{L"LAL",L"湖人"},{L"MEM",L"灰熊"},{L"MIA",L"热火"},
    {L"MIL",L"雄鹿"},{L"MIN",L"森林狼"},{L"NO",L"鹈鹕"},{L"NY",L"尼克斯"},
    {L"OKC",L"雷霆"},{L"ORL",L"魔术"},{L"PHI",L"76人"},{L"PHX",L"太阳"},
    {L"POR",L"开拓者"},{L"SAC",L"国王"},{L"SA",L"马刺"},{L"TOR",L"猛龙"},
    {L"UTAH",L"爵士"},{L"WSH",L"奇才"}
};

std::wstring CanonicalNbaTeam(std::wstring value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    value = value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
    for (auto& c : value) c = static_cast<wchar_t>(towupper(c));
    if (value == L"GSW") value = L"GS";
    if (value == L"NOP") value = L"NO";
    if (value == L"NYK") value = L"NY";
    if (value == L"SAS") value = L"SA";
    if (value == L"UTA") value = L"UTAH";
    if (value == L"WAS") value = L"WSH";
    for (const auto& team : kNbaTeams)
        if (value == team.code || value == team.name) return team.code;
    return {};
}
