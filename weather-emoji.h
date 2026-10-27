std::wstring WeatherEmoji(WeatherVisual visual, bool night, float temperature = NAN, bool fahrenheit = false) {
    std::wstring emoji;
    switch (visual) {
        case WeatherVisual::Clear: emoji = night ? L"🌙" : L"☀️"; break;
        case WeatherVisual::PartlyCloudy: emoji = night ? L"🌙☁️" : L"⛅"; break;
        case WeatherVisual::Cloudy: emoji = L"☁️"; break;
        case WeatherVisual::Fog: emoji = L"🌫️"; break;
        case WeatherVisual::Storm: emoji = L"⛈️"; break;
        case WeatherVisual::Rain: emoji = L"🌧️"; break;
        case WeatherVisual::Snow: emoji = L"🌨️"; break;
        default: return L"🌡️";
    }
    const float celsius = fahrenheit ? (temperature - 32.0f) / 1.8f : temperature;
    if (celsius <= 0.0f) emoji += L"🥶";
    else if (celsius >= 35.0f) emoji += L"🥵";
    return emoji;
}
