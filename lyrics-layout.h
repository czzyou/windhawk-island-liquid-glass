struct LyricTextBlock {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    float top = 0, height = 0;
    UINT32 lineCount = 0;
};

Microsoft::WRL::ComPtr<IDWriteTextFormat> CreateLyricFormat(
    IDWriteFactory* factory, const std::wstring& family, float size, IDWriteFontFallback* fallback) {
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    const auto name = family.empty() ? L"Segoe UI" : family.c_str();
    HRESULT hr = factory->CreateTextFormat(name, nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, size, L"", &format);
    if (FAILED(hr)) return format;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, size * 1.24f, size);
    Microsoft::WRL::ComPtr<IDWriteTextFormat1> extended;
    if (fallback && SUCCEEDED(format.As(&extended))) extended->SetFontFallback(fallback);
    return format;
}

std::vector<LyricTextBlock> BuildLyricBlocks(IDWriteFactory* factory, IDWriteTextFormat* format,
    const std::vector<LyricsLine>& lines, float width, float gap) {
    std::vector<LyricTextBlock> blocks;
    if (!factory || !format || width <= 0) return blocks;
    float top = 0;
    for (const auto& line : lines) {
        LyricTextBlock block;
        if (IsLyricsCredit(line.text)) {
            block.top=top;
            blocks.push_back(std::move(block));
            continue;
        }
        const auto text = line.text.empty() ? std::wstring(L" ") : line.text;
        if (FAILED(factory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format,
                width, 16384.0f, &block.layout))) return {};
        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(block.layout->GetMetrics(&metrics))) return {};
        block.top = top;
        block.height = (std::max)(metrics.height, format->GetFontSize());
        block.lineCount = metrics.lineCount;
        top += block.height + gap;
        blocks.push_back(std::move(block));
    }
    return blocks;
}


float MeasureLongestLyricWidth(IDWriteFactory* factory, IDWriteTextFormat* format, const std::vector<LyricsLine>& lines) {
    if (!factory || !format) return -1;
    float widest=0;
    for (const auto& line : lines) {
        Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
        if (FAILED(factory->CreateTextLayout(line.text.c_str(),static_cast<UINT32>(line.text.size()),format,2000,128,&layout))) return -1;
        layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TEXT_METRICS metrics{};
        if (FAILED(layout->GetMetrics(&metrics))) return -1;
        widest=(std::max)(widest,metrics.widthIncludingTrailingWhitespace);
    }
    return widest;
}
float CollapsedLyricsWidth(float widest, float available) {
    return (std::min)((std::max)(0.0f,available),widest<0 ? 400.0f : std::ceil(widest+92.0f));
}

float CollapsedTitleWidth(float widest, float available) {
    return CollapsedLyricsWidth(widest, (std::min)(420.0f, available));
}

Microsoft::WRL::ComPtr<IDWriteTextLayout> CreateCollapsedTitleLayout(
    IDWriteFactory* factory, IDWriteTextFormat* format, const std::wstring& title) {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    if (FAILED(factory->CreateTextLayout(title.c_str(), static_cast<UINT32>(title.size()),
            format, 328, 26, &layout))) return layout;
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    if (FAILED(factory->CreateEllipsisTrimmingSign(format, &ellipsis)) ||
        FAILED(layout->SetTrimming(&trimming, ellipsis.Get()))) layout.Reset();
    return layout;
}
