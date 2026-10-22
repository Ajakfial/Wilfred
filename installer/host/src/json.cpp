#include "json.h"

#include <cwchar>
#include <cwctype>

namespace {

size_t SkipWhitespace(const std::wstring& text, size_t index) {
    while (index < text.size() && std::iswspace(text[index])) {
        ++index;
    }
    return index;
}

size_t FindValueStart(const std::wstring& json, const wchar_t* key) {
    const std::wstring needle = L"\"" + std::wstring(key) + L"\"";
    size_t position = json.find(needle);
    if (position == std::wstring::npos) {
        return std::wstring::npos;
    }
    position = json.find(L':', position + needle.size());
    if (position == std::wstring::npos) {
        return std::wstring::npos;
    }
    return SkipWhitespace(json, position + 1);
}

}  // namespace

std::wstring JsonEscape(const std::wstring& value) {
    std::wstring out;
    out.reserve(value.size() + 8);
    for (const wchar_t c : value) {
        switch (c) {
            case L'"':
                out += L"\\\"";
                break;
            case L'\\':
                out += L"\\\\";
                break;
            case L'\n':
                out += L"\\n";
                break;
            case L'\r':
                out += L"\\r";
                break;
            case L'\t':
                out += L"\\t";
                break;
            default:
                if (c < 0x20) {
                    wchar_t buffer[7] = {};
                    std::swprintf(buffer, 7, L"\\u%04x", static_cast<unsigned>(c));
                    out += buffer;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::wstring JsonStringValue(const std::wstring& json, const wchar_t* key) {
    size_t position = FindValueStart(json, key);
    if (position == std::wstring::npos || position >= json.size() || json[position] != L'"') {
        return L"";
    }
    ++position;

    std::wstring out;
    while (position < json.size()) {
        const wchar_t c = json[position++];
        if (c == L'"') {
            break;
        }
        if (c != L'\\' || position >= json.size()) {
            out += c;
            continue;
        }
        const wchar_t escaped = json[position++];
        switch (escaped) {
            case L'n':
                out += L'\n';
                break;
            case L'r':
                out += L'\r';
                break;
            case L't':
                out += L'\t';
                break;
            case L'b':
                out += L'\b';
                break;
            case L'f':
                out += L'\f';
                break;
            case L'u': {
                if (position + 4 <= json.size()) {
                    wchar_t hex[5] = {};
                    for (int i = 0; i < 4; ++i) {
                        hex[i] = json[position + i];
                    }
                    wchar_t* end = nullptr;
                    const long code = std::wcstol(hex, &end, 16);
                    if (end == hex + 4) {
                        out += static_cast<wchar_t>(code);
                        position += 4;
                        break;
                    }
                }
                out += L'u';
                break;
            }
            default:
                out += escaped;
                break;
        }
    }
    return out;
}

bool JsonBoolValue(const std::wstring& json, const wchar_t* key, bool fallback) {
    const size_t position = FindValueStart(json, key);
    if (position == std::wstring::npos) {
        return fallback;
    }
    if (json.compare(position, 4, L"true") == 0) {
        return true;
    }
    if (json.compare(position, 5, L"false") == 0) {
        return false;
    }
    return fallback;
}
