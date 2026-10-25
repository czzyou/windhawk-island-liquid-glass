// Adapted from devcode90/Dynamic-Island-for-Windows, MIT.
// Upstream: 7fe506e81e4e72460251aa60110437794dbc88da (1.4.0).
bool IsUrlSafeChar(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

std::string UrlEncodeComponent(const std::wstring& value) {
    const int len = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(len), '\0');
    if (len > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), len, nullptr, nullptr);

    std::string out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : utf8) {
        if (IsUrlSafeChar(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0xF]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

std::wstring AsciiToWide(const std::string& s) {
    std::wstring w;
    w.reserve(s.size());
    for (char c : s) {
        w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    }
    return w;
}

// Unescapes a raw JSON string value (still containing \n, \", \uXXXX etc.)
// and decodes it from UTF-8 into a wide string.
std::wstring JsonStringUnescape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            switch (next) {
                case 'n': out.push_back('\n'); ++i; break;
                case 'r': out.push_back('\r'); ++i; break;
                case 't': out.push_back('\t'); ++i; break;
                case '"': out.push_back('"'); ++i; break;
                case '\\': out.push_back('\\'); ++i; break;
                case '/': out.push_back('/'); ++i; break;
                case 'u': {
                    if (i + 5 < raw.size()) {
                        std::string hexNarrow = raw.substr(i + 2, 4);
                        std::wstring hexWide = AsciiToWide(hexNarrow);
                        wchar_t code = static_cast<wchar_t>(wcstoul(hexWide.c_str(), nullptr, 16));
                        char utf8buf[4] = {};
                        int outLen = WideCharToMultiByte(CP_UTF8, 0, &code, 1, utf8buf,
                                                         sizeof(utf8buf), nullptr, nullptr);
                        if (outLen > 0) {
                            out.append(utf8buf, outLen);
                        }
                        i += 5;
                    }
                    break;
                }
                default:
                    out.push_back(next);
                    ++i;
                    break;
            }
        } else {
            out.push_back(c);
        }
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, nullptr, 0);
    std::wstring wout;
    if (wlen > 0) {
        wout.resize(wlen - 1);
        MultiByteToWideChar(CP_UTF8, 0, out.c_str(), -1, wout.data(), wlen);
    }
    return wout;
}

// Finds "fieldName":"....." in a JSON blob and returns the raw (still
// escaped) contents between the quotes. Returns false if missing or null.
bool ExtractJsonStringField(const std::string& json, const char* fieldKey, std::string* outRaw) {
    const std::string key = std::string("\"") + fieldKey + "\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) {
        return false;
    }
    pos += key.size();

    std::string result;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            result.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            result.push_back(c);
            escaped = true;
            continue;
        }
        if (c == '"') {
            *outRaw = result;
            return true;
        }
        result.push_back(c);
    }
    return false;
}

// Splits a top-level JSON array "[ {...}, {...} ]" into the substrings for
// each object, respecting nested braces and quoted strings.
std::vector<std::string> SplitJsonObjectArray(const std::string& json) {
    std::vector<std::string> objects;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    size_t objectStart = std::string::npos;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            if (depth == 0) {
                objectStart = i;
            }
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(json.substr(objectStart, i - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}
