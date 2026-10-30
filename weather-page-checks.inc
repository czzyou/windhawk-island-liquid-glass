    assert(result.forecast[0].available && result.forecast[0].date == 20261016);
    assert(result.forecast[1].available && result.forecast[1].date == 20261017);
    assert(result.forecast[2].available && result.forecast[2].date == 20261018);
    assert(result.forecast[1].low == 14 && result.forecast[1].high == 25);
    assert(result.forecast[2].low == 1 && result.forecast[2].high == 17);
    assert(result.forecast[1].description == L"多云" && result.forecast[1].wind.find(L"东南风") != std::wstring::npos);
    assert(ParseChinaWeather(current,forecast,now,true,result));
    assert(std::abs(result.forecast[1].high - 77.0f) < 0.01f);
    assert(std::abs(result.forecast[1].low - 57.2f) < 0.01f);
    const std::string shortForecast = R"(var fc={"f":[{"fi":"10/16","fa":"00","fc":"18","fd":"6"}]};)";
    assert(ParseChinaWeather(current,shortForecast,now,false,result));
    assert(!result.forecast[1].available && !result.forecast[2].available);
    const std::string invalidFuture = R"(var fc={"f":[{"fi":"10/16","fa":"00","fc":"18","fd":"6"},
        {"fi":"10/17","fa":"01","fc":"10","fd":"20"},
        {"fi":"10/18","fa":"00","fc":"17","fd":"1"}]};)";
    assert(ParseChinaWeather(current,invalidFuture,now,false,result));
    assert(!result.forecast[1].available && result.forecast[2].available);
    SYSTEMTIME yearEnd{2026,12,0,31};
    assert(ChinaWeatherDateKey(ChinaWeatherDay(yearEnd,2)) == 20270102);
    SYSTEMTIME leapDay{2028,2,0,28};
    assert(ChinaWeatherDateKey(ChinaWeatherDay(leapDay,1)) == 20280229);
    assert(ChinaWeatherDateKey(ChinaWeatherDay(leapDay,2)) == 20280301);

    auto boundaryCurrent = current;
    boundaryCurrent.replace(boundaryCurrent.find("10月16日"), std::string("10月16日").size(), "10月30日");
    SYSTEMTIME boundary{2026,10,0,30,12,0,0,0};
    const auto november = ChinaWeatherDay(boundary, 2);
    assert(ChinaWeatherDateMatches(L"11/1", november));
    assert(ChinaWeatherDateMatches(L"11/01", november));
    for (const auto* invalid : {L"11/1x", L"11/001", L"11//1", L"11/2", L"", L"1/11"})
        assert(!ChinaWeatherDateMatches(invalid, november));
    for (const auto* date : {"11/1", "11/01"}) {
        const std::string boundaryForecast = std::string(R"(var fc={"f":[
            {"fi":"10/30","fa":"00","fc":"18","fd":"-4"},
            {"fi":"10/31","fa":"01","fc":"7","fd":"0"},
            {"fi":")") + date + R"(","fa":"03","fc":"12","fd":"-6"}]};)";
        WeatherSnapshot crossed;
        assert(ParseChinaWeather(boundaryCurrent,boundaryForecast,boundary,false,crossed));
        assert(crossed.forecast[2].available && crossed.forecast[2].date == 20261101);
        assert(crossed.forecast[2].low == -6 && crossed.forecast[2].high == 12);
    }
    SYSTEMTIME january{2027,1,0,1};
    assert(ChinaWeatherDateMatches(L"1/1", january) && ChinaWeatherDateMatches(L"01/01", january));
