#include "msi_runner.h"

#include <msi.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <vector>

namespace {

const std::function<void(const MsiEvent&)>* g_sink = nullptr;
std::wstring g_lastError;
std::wstring g_currentAction;
int g_percent = 0;

std::wstring Trim(std::wstring text) {
    while (!text.empty() && std::iswspace(text.front())) {
        text.erase(text.begin());
    }
    while (!text.empty() && std::iswspace(text.back())) {
        text.pop_back();
    }
    return text;
}

std::vector<std::wstring> SplitTab(const std::wstring& text) {
    std::vector<std::wstring> fields;
    size_t start = 0;
    while (true) {
        const size_t tab = text.find(L'\t', start);
        fields.push_back(text.substr(start, tab == std::wstring::npos ? std::wstring::npos : tab - start));
        if (tab == std::wstring::npos) {
            break;
        }
        start = tab + 1;
    }
    return fields;
}

std::vector<int> ParseIntegers(const std::wstring& text) {
    std::vector<int> values;
    const wchar_t* cursor = text.c_str();
    while (*cursor != L'\0' && values.size() < 8) {
        while (*cursor != L'\0' && !std::iswdigit(*cursor) && *cursor != L'-') {
            ++cursor;
        }
        if (*cursor == L'\0') {
            break;
        }
        wchar_t* end = nullptr;
        const long value = std::wcstol(cursor, &end, 10);
        if (end == cursor) {
            ++cursor;
            continue;
        }
        values.push_back(static_cast<int>(value));
        cursor = end;
    }
    return values;
}

void EmitStatus(const std::wstring& text) {
    if (g_sink == nullptr || text.empty()) {
        return;
    }
    MsiEvent event;
    event.kind = MsiEventKind::Status;
    event.text = text;
    (*g_sink)(event);
}

void EmitProgress() {
    if (g_sink == nullptr) {
        return;
    }
    MsiEvent event;
    event.kind = MsiEventKind::Progress;
    event.percent = std::min(g_percent, 99);
    event.text = g_currentAction;
    (*g_sink)(event);
}

int WINAPI ExternalUiHandler(void* /*context*/, UINT messageType, LPCWSTR message) {
    if (message == nullptr) {
        return 0;
    }
    switch (messageType) {
        case INSTALLMESSAGE_PROGRESS: {
            const std::vector<int> fields = ParseIntegers(message);
            if (fields.size() >= 2) {
                const int current = fields[0];
                const int total = fields[1];
                const bool reset = fields.size() >= 3 ? fields[2] == 0 : current == 0;
                if (reset) {
                    g_percent = 0;
                }
                if (total > 0) {
                    const int computed = static_cast<int>((100LL * current) / total);
                    g_percent = std::max(g_percent, std::min(computed, 99));
                }
                EmitProgress();
            }
            return 0;
        }
        case INSTALLMESSAGE_ACTIONSTART: {
            const std::vector<std::wstring> fields = SplitTab(message);
            std::wstring text;
            if (fields.size() >= 2 && !Trim(fields[1]).empty()) {
                text = Trim(fields[1]);
            } else if (!fields.empty()) {
                text = Trim(fields[0]);
            }
            g_currentAction = text;
            EmitStatus(text);
            return 0;
        }
        case INSTALLMESSAGE_INSTALLSTART:
            EmitStatus(L"Starting installation");
            return 0;
        case INSTALLMESSAGE_INSTALLEND:
            EmitStatus(L"Finishing up");
            return 0;
        case INSTALLMESSAGE_ERROR:
        case INSTALLMESSAGE_FATALEXIT: {
            std::wstring text = message;
            std::replace(text.begin(), text.end(), L'\t', L' ');
            text = Trim(text);
            if (g_lastError.empty() && !text.empty()) {
                g_lastError = text;
            }
            // Let Windows Installer continue/roll back on its own; the
            // process exit code and rollback state drive the UI.
            return IDOK;
        }
        case INSTALLMESSAGE_WARNING:
            return IDOK;
        default:
            return 0;
    }
}

}  // namespace

bool LooksLikeMsi(const std::wstring& path) {
    static const unsigned char kCompoundFileMagic[8] = {0xD0, 0xCF, 0x11, 0xE0,
                                                       0xA1, 0xB1, 0x1A, 0xE1};
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    unsigned char header[8] = {};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, header, sizeof(header), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(header)) {
        return false;
    }
    for (size_t i = 0; i < sizeof(header); ++i) {
        if (header[i] != kCompoundFileMagic[i]) {
            return false;
        }
    }
    return true;
}

MsiResult RunMsiInstall(const std::wstring& msiPath, const std::wstring& logPath,
                        const std::function<void(const MsiEvent&)>& sink) {
    MsiResult result;

    g_sink = &sink;
    g_lastError.clear();
    g_currentAction.clear();
    g_percent = 0;

    if (!LooksLikeMsi(msiPath)) {
        g_sink = nullptr;
        result.code = ERROR_BAD_FORMAT;
        result.error = L"The installer package is missing or corrupt. Re-download Wilfred Setup.";
        return result;
    }

    // No internal UI at all: the wizard owns the experience. The external UI
    // handler is what gives us progress and action text.
    MsiSetInternalUI(INSTALLUILEVEL_NONE, nullptr);
    MsiSetExternalUIW(ExternalUiHandler, INSTALLUILEVEL_PROGRESSONLY, nullptr);
    MsiEnableLogW(INSTALLLOGMODE_VERBOSE, logPath.c_str(), 0);

    const UINT code = MsiInstallProductW(msiPath.c_str(), L"REBOOT=ReallySuppress");

    MsiEnableLogW(0, nullptr, 0);
    MsiSetExternalUIW(nullptr, INSTALLUILEVEL_NONE, nullptr);
    g_sink = nullptr;

    result.code = code;
    if (code != ERROR_SUCCESS) {
        if (!g_lastError.empty()) {
            result.error = g_lastError;
        } else {
            wchar_t codeText[32] = {};
            std::swprintf(codeText, 32, L"%u", code);
            result.error = std::wstring(L"Windows Installer error ") + codeText + L".";
        }
    }
    return result;
}
