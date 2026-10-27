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
