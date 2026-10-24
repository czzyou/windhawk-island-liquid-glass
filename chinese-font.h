inline Microsoft::WRL::ComPtr<IDWriteFontFallback> CreateIslandChineseFontFallback(IDWriteFactory* factory) {
    Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
    Microsoft::WRL::ComPtr<IDWriteFontFallbackBuilder> builder;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> systemFallback, fallback;
    if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory2))) ||
        FAILED(factory2->CreateFontFallbackBuilder(&builder))) return {};
    // Keep Latin text in the configured family; prioritize PingFang for CJK only.
    constexpr DWRITE_UNICODE_RANGE ranges[] = {
        {0x2E80, 0x303F}, {0x3100, 0x312F}, {0x31A0, 0x31BF},
        {0x3400, 0x9FFF}, {0xF900, 0xFAFF}, {0xFE10, 0xFE1F},
        {0xFE30, 0xFE4F}, {0xFF00, 0xFFEF}, {0x20000, 0x323AF},
    };
    const wchar_t* families[] = {L"PingFang SC", L"苹方_常规"};
    if (FAILED(builder->AddMapping(ranges, ARRAYSIZE(ranges), families, ARRAYSIZE(families),
                                  nullptr, nullptr, nullptr, 1.0f)) ||
        FAILED(factory2->GetSystemFontFallback(&systemFallback)) ||
        FAILED(builder->AddMappings(systemFallback.Get())) ||
        FAILED(builder->CreateFontFallback(&fallback))) return {};
    return fallback;
}
