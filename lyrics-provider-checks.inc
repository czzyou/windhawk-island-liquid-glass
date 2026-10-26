void CheckLyricsProviders() {
    using namespace LyricsSources;
    assert(LyricsTextMatch(L"山雀", L"孤勇者") == 0);
    assert(LyricsTextMatch(L"山雀", L"山雀") == 2);
    assert(MatchScore({L"山雀", L"万能青年旅店", L"1", L"", 240}, L"山雀", L"万能青年旅店", 241) >= 0);
    assert(MatchScore({L"山雀", L"翻唱歌手", L"1", L"", 241}, L"山雀", L"万能青年旅店", 241) < 0);
    assert(MatchScore({L"山雀", L"万能青年旅店", L"1", L"", 260}, L"山雀", L"万能青年旅店", 241) < 0);
    assert(MatchScore({L"山雀 (Live)", L"万能青年旅店", L"1", L"", 241}, L"山雀", L"万能青年旅店", 241) < 0);
    assert(MatchScore({L"山雀 (伴奏)", L"万能青年旅店", L"1", L"", 241}, L"山雀", L"万能青年旅店", 241) < 0);
    assert(MatchScore({L"山雀", L"万能青年旅店", L"1", L"", 241}, L"山雀", L"", 241) >= 0);
    assert(MatchScore({L"山雀", L"万能青年旅店", L"1"}, L"山雀", L"", 241) < 0);
    assert(MatchScore({L"山雀", L"万能青年旅店", L"1", L"", std::numeric_limits<double>::infinity()}, L"山雀", L"万能青年旅店", 241) < 0);
    const std::string neSearch = R"({ "code": 200, "result": {"songs": [
        {"id":2,"name":"山雀","artists":[{"name":"错误歌手"}],"duration":241000},
        {"id":1,"name":"山雀","artists":[{"name":"万能青年旅店"}],"duration":241000}]}})";
    replies = {{true,200,neSearch},{true,200,R"({"code":200,"lrc":{"lyric":"[00:01.20]中文测试 🎵"}})"}};
    hosts.clear(); requestPaths.clear(); replyIndex = 0;
    LyricsPick ne; bool definitive = true;
    assert(FetchLyricsFromSources(L"山雀", L"万能青年旅店", L"", 241, &ne, &definitive));
    assert(definitive && ne.synced && ne.lines.size() == 1 && ne.lines[0].text == L"中文测试 🎵");
    assert(hosts.size() == 2 && hosts[0] == L"music.163.com" && requestPaths[1].ends_with(L"id=1"));
    replies = {{false,0,""},
        {true,200,R"({"code":0,"data":{"song":{"itemlist":[{"mid":"qq-id","name":"山雀","singer":"万能青年旅店"}]}}})"},
        {true,200,R"({"code":0,"songinfo":{"code":0,"data":{"track_info":{"name":"山雀","singer":[{"name":"万能青年旅店"}],"interval":241}}}})"},
        {true,200,R"({"retcode":0,"lyric":"[00:02.00]QQ result"})"}};
    hosts.clear(); requestPaths.clear(); replyIndex = 0;
    LyricsPick qq;
    assert(FetchLyricsFromSources(L"山雀", L"万能青年旅店", L"", 241, &qq, &definitive));
    assert(!definitive && qq.synced && qq.lines[0].text == L"QQ result");
    assert(hosts.size() == 4 && hosts[2] == L"u.y.qq.com" && hosts[3] == L"c.y.qq.com");
    replies = {{true,200,R"({"code":200,"result":{"songs":[]}})"},
        {true,200,R"({"code":0,"data":{"song":{"itemlist":[]}}})"},
        {true,200,R"({"status":1,"data":{"info":[{"songname":"山雀","singername":"万能青年旅店","hash":"recording-hash","duration":241}]}})"},
        {true,200,R"({"status":200,"candidates":[{"id":"clip","accesskey":"key","song":"山雀","singer":"万能青年旅店","duration":6000},{"id":"kg-id","accesskey":"a&b","song":"山雀","singer":"上传者","duration":241000}]})"},
        {true,200,R"({"status":200,"content":"WzAwOjAzLjAwXUt1Z291"})"}};
    hosts.clear(); requestPaths.clear(); replyIndex = 0;
    LyricsPick kg;
    assert(FetchLyricsFromSources(L"山雀", L"万能青年旅店", L"", 241, &kg, &definitive));
    assert(definitive && kg.synced && kg.lines[0].text == L"Kugou");
    assert(requestPaths[3].find(L"%20-%20") != std::wstring::npos && requestPaths[3].find(L"hash=recording-hash") != std::wstring::npos);
    assert(requestPaths[4].find(L"accesskey=a%26b") != std::wstring::npos && requestPaths[4].find(L"id=kg-id") != std::wstring::npos);
    replies = {{true,200,R"({"code":200,"result":{"songs":[]}})"},
        {true,200,R"({"code":0,"data":{"song":{"itemlist":[]}}})"},
        {true,200,R"({"status":1,"data":{"info":[]}})"},
        {true,404,""},{true,200,"[]"},{true,200,"[]"}};
    replyIndex = 0;
    LyricsPick missing;
    assert(!FetchLyricsFromSources(L"山雀", L"万能青年旅店", L"", 241, &missing, &definitive) && definitive);
    replies = {{true,200,"<html>blocked</html>"}}; replyIndex = 0;
    definitive = true;
    Fetch(Source::NetEase, L"山雀", L"万能青年旅店", 241, &missing, &definitive);
    assert(!definitive && !missing.found);
    replies = {{true,200,R"({"code":200,"result":{}})"}}; replyIndex = 0;
    definitive = true;
    Fetch(Source::NetEase, L"山雀", L"万能青年旅店", 241, &missing, &definitive);
    assert(!definitive && !missing.found);
    replies = {{true,200,R"({"status":1,"data":{"info":[{"songname":"山雀","singername":"万能青年旅店","hash":"hash","duration":241}]}})"},
        {true,200,R"({"status":200,"candidates":[{"id":"1","accesskey":"key","song":"山雀","singer":"万能青年旅店","duration":241000}]})"},
        {true,200,R"({"status":200,"content":"invalid%%%"})"}};
    replyIndex = 0; definitive = true;
    Fetch(Source::Kugou, L"山雀", L"万能青年旅店", 241, &missing, &definitive);
    assert(!definitive && !missing.found);
    LyricsPick plain; plain.found = true; plain.score = 200; plain.lines = {{0,L"plain"}};
    ConsiderLyricsBody(R"({"syncedLyrics":"[00:01]synced"})", L"Song", L"Artist", 0, true, &plain);
    assert(plain.synced && plain.lines[0].text == L"synced");
    ConsiderLyricsBody(R"({"plainLyrics":"plain","trackName":"Song","artistName":"Artist","duration":241})", L"Song", L"Artist", 241, false, &plain);
    assert(plain.synced && plain.lines[0].text == L"synced");
    replies = {{true,200,R"({"syncedLyrics":"[00:01]LRCLIB"})"}};
    replyIndex = 0; hosts.clear();
    LyricsPick english;
    assert(FetchLyricsFromSources(L"Song", L"Artist", L"", 241, &english, &definitive));
    assert(hosts.size() == 1 && hosts[0] == L"lrclib.net");
}
