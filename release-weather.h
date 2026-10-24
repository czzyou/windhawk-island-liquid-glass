// IP lookup returns script text; extract the city identifier without executing it.
std::wstring ParseWeatherIPCity(const std::string& body) {
    const std::string prefix = "var id=\"";
    const auto start = body.find(prefix);
    if (start == std::string::npos) return {};
    const auto value = body.substr(start + prefix.size(), 10);
    if (value.size() != 10 || value[9] != '"' || value.substr(0, 3) != "101" ||
        !std::all_of(value.begin(), value.begin() + 9, [](char c) { return c >= '0' && c <= '9'; })) return {};
    return std::wstring(value.begin(), value.begin() + 9);
}

std::mutex g_weatherIPMutex;
std::wstring g_weatherIPCity;

std::wstring ResolvedWeatherCity(const std::wstring& setting) {
    if (!setting.empty()) return ChinaWeatherCityCode(setting);
    std::lock_guard lock(g_weatherIPMutex);
    return g_weatherIPCity;
}
