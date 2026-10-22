// Wilfred Setup: a small Win32 + WebView2 host for the TypeScript wizard in
// installer/ui. It embeds the built wizard HTML and the MSI itself, then
// drives MsiInstallProductW with an external UI handler so the wizard can show
// real progress instead of the stock Windows Installer dialogs.
//
// Command line:
//   wilfred-setup.exe             show the wizard
//   wilfred-setup.exe /S          silent install (no UI)
//   wilfred-setup.exe --check     verify the embedded payload, then exit
//   wilfred-setup.exe --ui-check  verify WebView2 can load the wizard, then exit
//   wilfred-setup.exe --msi PATH  install a specific .msi (development)

#include <windows.h>

#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wrl.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "WebView2.h"
#include "json.h"
#include "msi_runner.h"
#include "resource.h"
#include "version.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {

constexpr wchar_t kWindowClass[] = L"WilfredSetupWindow";
constexpr wchar_t kWindowTitle[] = L"Wilfred Setup";
constexpr int kWindowWidth = 1000;
constexpr int kWindowHeight = 700;
constexpr UINT kMessageToUi = WM_APP + 1;
constexpr UINT_PTR kUiCheckTimer = 1;
constexpr UINT kUiCheckTimeoutMs = 20000;
constexpr size_t kNavigateToStringLimit = 2u * 1024u * 1024u;

HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HBRUSH g_background_brush = nullptr;

ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;

std::wstring g_html;
std::wstring g_version = WILFRED_SETUP_VERSION;
std::wstring g_install_dir;
std::wstring g_msi_override;
std::wstring g_payload_path;
std::wstring g_log_path;

bool g_payload_extracted = false;
bool g_payload_owned = false;
bool g_keep_payload = false;
bool g_installing = false;
bool g_launch_requested = true;
bool g_ui_check = false;
bool g_ui_ready = false;
int g_exit_code = 0;

struct UiMessage {
    std::wstring json;
    bool finished = false;
    bool success = false;
};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

HANDLE ConsoleHandle() {
    static HANDLE handle = INVALID_HANDLE_VALUE;
    if (handle == INVALID_HANDLE_VALUE) {
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            return INVALID_HANDLE_VALUE;
        }
        handle = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, nullptr);
    }
    return handle;
}

void PrintLine(const std::wstring& line) {
    HANDLE handle = ConsoleHandle();
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::wstring text = line + L"\r\n";
    DWORD written = 0;
    WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
}

std::wstring ModulePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return L"";
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring ModuleDirectory() {
    const std::wstring path = ModulePath();
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : path.substr(0, slash);
}

std::wstring LocalAppData() {
    PWSTR known = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &known)) &&
        known != nullptr) {
        std::wstring result(known);
        CoTaskMemFree(known);
        return result;
    }
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    return length > 0 ? std::wstring(buffer, length) : L"";
}

std::wstring TempPath(const std::wstring& filename) {
    wchar_t directory[MAX_PATH] = {};
    const DWORD length = GetTempPathW(MAX_PATH, directory);
    if (length == 0) {
        return filename;
    }
    return std::wstring(directory, length) + filename;
}

void ShowError(const std::wstring& text) {
    MessageBoxW(g_window, text.c_str(), kWindowTitle, MB_OK | MB_ICONERROR);
}

bool LoadResourceData(int id, const void** data, DWORD* size) {
    HRSRC resource = FindResourceW(g_instance, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (resource == nullptr) {
        return false;
    }
    const DWORD resourceSize = SizeofResource(g_instance, resource);
    HGLOBAL handle = LoadResource(g_instance, resource);
    if (handle == nullptr || resourceSize == 0) {
        return false;
    }
    void* pointer = LockResource(handle);
    if (pointer == nullptr) {
        return false;
    }
    *data = pointer;
    *size = resourceSize;
    return true;
}

std::wstring LoadHtmlResource() {
    const void* data = nullptr;
    DWORD size = 0;
    if (!LoadResourceData(IDR_SETUP_HTML, &data, &size)) {
        return L"";
    }
    const int chars = MultiByteToWideChar(CP_UTF8, 0, static_cast<const char*>(data),
                                          static_cast<int>(size), nullptr, 0);
    if (chars <= 0) {
        return L"";
    }
    std::wstring html(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, static_cast<const char*>(data), static_cast<int>(size), html.data(),
                        chars);
    return html;
}

std::wstring FindAdjacentMsi() {
    const std::wstring directory = ModuleDirectory();
    if (directory.empty()) {
        return L"";
    }
    WIN32_FIND_DATAW data = {};
    HANDLE search = FindFirstFileW((directory + L"\\*.msi").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) {
        return L"";
    }
    std::wstring fallback;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        const std::wstring path = directory + L"\\" + data.cFileName;
        if (fallback.empty()) {
            fallback = path;
        }
        if (_wcsicmp(data.cFileName, L"wilfred.msi") == 0 ||
            wcsncmp(data.cFileName, L"wilfred-", 8) == 0) {
            fallback = path;
            break;
        }
    } while (FindNextFileW(search, &data));
    FindClose(search);
    return fallback;
}

bool ExtractEmbeddedMsi() {
    if (g_payload_extracted) {
        return LooksLikeMsi(g_payload_path);
    }
    const void* data = nullptr;
    DWORD size = 0;
    if (!LoadResourceData(IDR_SETUP_MSI, &data, &size)) {
        return false;
    }
    g_payload_path = TempPath(L"wilfred-setup-" + std::to_wstring(GetCurrentProcessId()) + L".msi");
    HANDLE file = CreateFileW(g_payload_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        g_payload_path.clear();
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, data, size, &written, nullptr);
    CloseHandle(file);
    if (!ok || written != size) {
        DeleteFileW(g_payload_path.c_str());
        g_payload_path.clear();
        return false;
    }
    g_payload_extracted = true;
    g_payload_owned = true;
    return LooksLikeMsi(g_payload_path);
}

std::wstring EnsureMsiPath() {
    if (!g_msi_override.empty()) {
        return g_msi_override;
    }
    if (g_payload_extracted) {
        return g_payload_path;
    }
    if (ExtractEmbeddedMsi()) {
        return g_payload_path;
    }
    const std::wstring adjacent = FindAdjacentMsi();
    if (!adjacent.empty()) {
        g_payload_path = adjacent;
        g_payload_extracted = true;
        g_payload_owned = false;
        return g_payload_path;
    }
    return L"";
}

void CleanupPayload() {
    if (g_payload_owned && g_payload_extracted && !g_keep_payload && !g_payload_path.empty()) {
        DeleteFileW(g_payload_path.c_str());
    }
    g_payload_extracted = false;
}

bool InstallSucceeded(UINT code) {
    return code == ERROR_SUCCESS || code == ERROR_SUCCESS_REBOOT_REQUIRED ||
           code == ERROR_SUCCESS_REBOOT_INITIATED;
}

/* ------------------------------------------------------------------ */
/* Host -> wizard messages                                             */
/* ------------------------------------------------------------------ */

std::wstring MakeStatus(const std::wstring& text) {
    return L"{\"type\":\"status\",\"text\":\"" + JsonEscape(text) + L"\"}";
}

std::wstring MakeProgress(int percent, const std::wstring& action) {
    return L"{\"type\":\"progress\",\"percent\":" + std::to_wstring(percent) +
           L",\"action\":\"" + JsonEscape(action) + L"\"}";
}

std::wstring MakeFailed(const std::wstring& message, UINT code) {
    std::wstring text = message;
    if (text.empty()) {
        text = L"Windows Installer error " + std::to_wstring(code) + L".";
    }
    return L"{\"type\":\"failed\",\"message\":\"" + JsonEscape(text) + L"\",\"code\":" +
           std::to_wstring(code) + L"}";
}

void PostJsonToWebView(const std::wstring& json) {
    if (g_webview) {
        g_webview->PostWebMessageAsJson(json.c_str());
    }
}

void PostToUi(std::wstring json, bool finished = false, bool success = false) {
    if (g_window == nullptr) {
        return;
    }
    auto* message = new UiMessage{std::move(json), finished, success};
    PostMessageW(g_window, kMessageToUi, 0, reinterpret_cast<LPARAM>(message));
}

void SendInit() {
    const std::wstring json = L"{\"type\":\"init\",\"version\":\"" + JsonEscape(g_version) +
                              L"\",\"installDir\":\"" + JsonEscape(g_install_dir) + L"\"}";
    PostJsonToWebView(json);
}

void LaunchInstalledWilfred() {
    const std::wstring executable = g_install_dir + L"\\wilfred.exe";
    if (GetFileAttributesW(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    ShellExecuteW(g_window, L"open", executable.c_str(), nullptr, g_install_dir.c_str(),
                  SW_SHOWNORMAL);
}

/* ------------------------------------------------------------------ */
/* Installation                                                       */
/* ------------------------------------------------------------------ */

void StartInstall(bool launch) {
    if (g_installing) {
        return;
    }
    g_installing = true;
    g_launch_requested = launch;
    PostJsonToWebView(MakeStatus(L"Preparing installation"));

    std::thread([launch]() {
        const std::wstring msi = EnsureMsiPath();
        if (msi.empty()) {
            PostToUi(MakeFailed(L"The installer package is missing from this build. "
                                L"Re-download Wilfred Setup.",
                                ERROR_FILE_NOT_FOUND),
                     true, false);
            return;
        }

        const auto sink = [](const MsiEvent& event) {
            if (event.kind == MsiEventKind::Progress) {
                PostToUi(MakeProgress(event.percent, event.text));
            } else {
                PostToUi(MakeStatus(event.text));
            }
        };

        const MsiResult result = RunMsiInstall(msi, g_log_path, sink);
        if (InstallSucceeded(result.code)) {
            std::wstring json = L"{\"type\":\"done\",\"success\":true,\"launch\":";
            json += launch ? L"true" : L"false";
            json += L"}";
            PostToUi(std::move(json), true, true);
        } else {
            std::wstring message = result.error;
            if (!message.empty()) {
                message += L"\n\nLog: ";
                message += g_log_path;
            }
            PostToUi(MakeFailed(message, result.code), true, false);
        }
    }).detach();
}

/* ------------------------------------------------------------------ */
/* WebView2                                                           */
/* ------------------------------------------------------------------ */

void OnWebViewUnavailable(HRESULT result) {
    if (g_ui_check) {
        wchar_t line[128] = {};
        std::swprintf(line, 128, L"ui-check: WebView2 unavailable (0x%08lx)", static_cast<unsigned long>(result));
        PrintLine(line);
        g_exit_code = 2;
        if (g_window) {
            DestroyWindow(g_window);
        }
        return;
    }

    const std::wstring msi = EnsureMsiPath();
    if (msi.empty()) {
        ShowError(L"Wilfred Setup could not load its wizard and the installer package is missing.\n\n"
                  L"Please re-download Wilfred Setup.");
        if (g_window) {
            DestroyWindow(g_window);
        }
        return;
    }

    g_keep_payload = true;
    MessageBoxW(g_window,
                L"Wilfred Setup needs the Microsoft Edge WebView2 Runtime to show its wizard.\n\n"
                L"Setup will now open the standard Windows installer instead. You can install the "
                L"runtime later from https://go.microsoft.com/fwlink/p/?LinkId=2124703 and re-run "
                L"Wilfred Setup for the full experience.",
                kWindowTitle, MB_OK | MB_ICONINFORMATION);

    const std::wstring parameters = L"/i \"" + msi + L"\"";
    ShellExecuteW(nullptr, L"open", L"msiexec.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
    if (g_window) {
        DestroyWindow(g_window);
    }
}

void HandleUiMessage(const std::wstring& json);

void ConfigureWebView() {
    if (!g_controller || !g_webview) {
        return;
    }

    RECT bounds = {};
    GetClientRect(g_window, &bounds);
    g_controller->put_Bounds(bounds);

    ComPtr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(g_controller.As(&controller2))) {
        const COREWEBVIEW2_COLOR background = {255, 16, 17, 22};  // opaque #101116
        controller2->put_DefaultBackgroundColor(background);
    }

    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);
    }
    ComPtr<ICoreWebView2Settings3> settings3;
    if (settings && SUCCEEDED(settings.As(&settings3)) && settings3) {
        settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
    }

    EventRegistrationToken token = {};
    g_webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                LPWSTR uri = nullptr;
                bool allowed = false;
                if (SUCCEEDED(args->get_Uri(&uri)) && uri != nullptr) {
                    allowed = wcsncmp(uri, L"about:", 6) == 0 || wcsncmp(uri, L"data:", 5) == 0;
                    CoTaskMemFree(uri);
                }
                if (!allowed) {
                    args->put_Cancel(TRUE);
                }
                return S_OK;
            })
            .Get(),
        &token);

    g_webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                return S_OK;
            })
            .Get(),
        &token);

    if (g_html.size() > kNavigateToStringLimit) {
        OnWebViewUnavailable(E_OUTOFMEMORY);
        return;
    }

    g_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR json = nullptr;
                if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json != nullptr) {
                    HandleUiMessage(json);
                    CoTaskMemFree(json);
                }
                return S_OK;
            })
            .Get(),
        &token);

    g_webview->NavigateToString(g_html.c_str());
}

void HandleUiMessage(const std::wstring& json) {
    const std::wstring type = JsonStringValue(json, L"type");
    if (type == L"ready") {
        g_ui_ready = true;
        if (g_ui_check) {
            KillTimer(g_window, kUiCheckTimer);
            PrintLine(L"ui-check: wizard loaded");
            g_exit_code = 0;
            PostMessageW(g_window, WM_CLOSE, 0, 0);
            return;
        }
        SendInit();
    } else if (type == L"install") {
        StartInstall(JsonBoolValue(json, L"launch", true));
    } else if (type == L"close") {
        PostMessageW(g_window, WM_CLOSE, 0, 0);
    }
}

void InitWebView() {
    std::wstring userData = LocalAppData();
    userData = userData.empty() ? TempPath(L"wilfred-setup-webview2")
                                : userData + L"\\Wilfred\\SetupCache";
    SHCreateDirectoryExW(nullptr, userData.c_str(), nullptr);

    auto environmentHandler = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [userData](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
            if (FAILED(result) || environment == nullptr) {
                OnWebViewUnavailable(result);
                return S_OK;
            }
            environment->CreateCoreWebView2Controller(
                g_window,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [](HRESULT controllerResult,
                       ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(controllerResult) || controller == nullptr) {
                            OnWebViewUnavailable(controllerResult);
                            return S_OK;
                        }
                        g_controller = controller;
                        ComPtr<ICoreWebView2> webview;
                        if (FAILED(controller->get_CoreWebView2(&webview)) || !webview) {
                            OnWebViewUnavailable(E_FAIL);
                            return S_OK;
                        }
                        g_webview = webview;
                        ConfigureWebView();
                        return S_OK;
                    })
                    .Get());
            return S_OK;
        });

    const HRESULT result = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr, environmentHandler.Get());
    if (FAILED(result)) {
        OnWebViewUnavailable(result);
    }
}

/* ------------------------------------------------------------------ */
/* Window                                                             */
/* ------------------------------------------------------------------ */

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            if (g_controller) {
                RECT bounds = {};
                GetClientRect(hwnd, &bounds);
                g_controller->put_Bounds(bounds);
            }
            return 0;

        case WM_ERASEBKGND: {
            RECT bounds = {};
            GetClientRect(hwnd, &bounds);
            FillRect(reinterpret_cast<HDC>(wParam), &bounds, g_background_brush);
            return 1;
        }

        case kMessageToUi: {
            auto* pending = reinterpret_cast<UiMessage*>(lParam);
            if (pending != nullptr) {
                PostJsonToWebView(pending->json);
                if (pending->finished) {
                    g_installing = false;
                    if (pending->success) {
                        if (g_launch_requested) {
                            LaunchInstalledWilfred();
                        }
                        CleanupPayload();
                    }
                }
                delete pending;
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == kUiCheckTimer && g_ui_check && !g_ui_ready) {
                KillTimer(hwnd, kUiCheckTimer);
                PrintLine(L"ui-check: timed out waiting for the wizard");
                g_exit_code = 3;
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_CLOSE:
            if (g_installing) {
                MessageBeep(MB_ICONWARNING);
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(g_exit_code);
            return 0;

        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark,
                                     sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

HWND CreateMainWindow(int showCommand) {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = g_instance;
    windowClass.hIcon = LoadIconW(g_instance, MAKEINTRESOURCEW(IDI_APPICON));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;
    if (RegisterClassExW(&windowClass) == 0) {
        return nullptr;
    }

    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_MAXIMIZEBOX;
    RECT frame = {0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRectEx(&frame, style, FALSE, 0);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    const int x = std::max(0, (GetSystemMetrics(SM_CXSCREEN) - width) / 2);
    const int y = std::max(0, (GetSystemMetrics(SM_CYSCREEN) - height) / 2);

    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, style, x, y, width, height,
                                nullptr, nullptr, g_instance, nullptr);
    if (hwnd == nullptr) {
        return nullptr;
    }
    ApplyDarkTitleBar(hwnd);
    ShowWindow(hwnd, showCommand == SW_SHOWMAXIMIZED ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(hwnd);
    return hwnd;
}

/* ------------------------------------------------------------------ */
/* Modes                                                              */
/* ------------------------------------------------------------------ */

int RunSelfCheck() {
    int failures = 0;

    const void* html = nullptr;
    DWORD htmlSize = 0;
    if (!LoadResourceData(IDR_SETUP_HTML, &html, &htmlSize)) {
        PrintLine(L"check: wizard HTML resource is missing");
        ++failures;
    } else if (htmlSize > kNavigateToStringLimit) {
        PrintLine(L"check: wizard HTML is too large for NavigateToString");
        ++failures;
    } else {
        PrintLine(L"check: wizard HTML " + std::to_wstring(htmlSize) + L" bytes");
    }

    const std::wstring msi = EnsureMsiPath();
    if (msi.empty()) {
        PrintLine(L"check: installer package is missing");
        ++failures;
    } else {
        WIN32_FILE_ATTRIBUTE_DATA attributes = {};
        if (!GetFileAttributesExW(msi.c_str(), GetFileExInfoStandard, &attributes)) {
            PrintLine(L"check: installer package not readable");
            ++failures;
        } else if (!LooksLikeMsi(msi)) {
            PrintLine(L"check: installer package is not a Windows Installer database");
            ++failures;
        } else {
            const ULONGLONG bytes = (static_cast<ULONGLONG>(attributes.nFileSizeHigh) << 32) |
                                    attributes.nFileSizeLow;
            PrintLine(L"check: installer package " + std::to_wstring(bytes) + L" bytes");
        }
    }

    PrintLine(failures == 0 ? L"check: OK" : L"check: FAILED");
    return failures == 0 ? 0 : 1;
}

int RunSilentInstall() {
    const std::wstring msi = EnsureMsiPath();
    if (msi.empty()) {
        PrintLine(L"silent: installer package is missing");
        return 2;
    }
    const MsiResult result = RunMsiInstall(msi, g_log_path, [](const MsiEvent&) {});
    if (!InstallSucceeded(result.code)) {
        PrintLine(L"silent: installation failed (" + std::to_wstring(result.code) + L")");
    }
    CleanupPayload();
    return static_cast<int>(result.code);
}

void PrintHelp() {
    PrintLine(L"Wilfred Setup");
    PrintLine(L"Usage: wilfred-setup.exe [options]");
    PrintLine(L"  (no options)   Show the setup wizard");
    PrintLine(L"  /S             Silent install");
    PrintLine(L"  --check        Verify the embedded installer package and exit");
    PrintLine(L"  --ui-check     Verify the wizard can load in WebView2 and exit");
    PrintLine(L"  --msi <path>   Install a specific .msi instead of the embedded one");
    PrintLine(L"  --version      Print the setup version");
}

int RunGui(int showCommand) {
    g_html = LoadHtmlResource();
    if (g_html.empty()) {
        ShowError(L"Wilfred Setup is missing its wizard resources. This build is incomplete.");
        return 1;
    }

    g_window = CreateMainWindow(showCommand);
    if (g_window == nullptr) {
        ShowError(L"Could not create the setup window.");
        return 1;
    }

    if (g_ui_check) {
        SetTimer(g_window, kUiCheckTimer, kUiCheckTimeoutMs, nullptr);
    }

    InitWebView();

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_controller) {
        g_controller->Close();
        g_controller.Reset();
    }
    g_webview.Reset();
    return g_exit_code;
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE /*previous*/, PWSTR /*commandLine*/,
                      int showCommand) {
    g_instance = instance;
    g_background_brush = CreateSolidBrush(RGB(16, 17, 22));

    bool silent = false;
    bool check = false;
    std::vector<std::wstring> arguments;
    int argumentCount = 0;
    if (LPWSTR* rawArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int i = 1; i < argumentCount; ++i) {
            arguments.emplace_back(rawArguments[i]);
        }
        LocalFree(rawArguments);
    }

    for (size_t i = 0; i < arguments.size(); ++i) {
        const std::wstring& argument = arguments[i];
        if (argument == L"/S" || argument == L"-s" || argument == L"--silent") {
            silent = true;
        } else if (argument == L"--check") {
            check = true;
        } else if (argument == L"--ui-check") {
            check = true;
            g_ui_check = true;
        } else if (argument == L"--msi" && i + 1 < arguments.size()) {
            g_msi_override = arguments[++i];
        } else if (argument.rfind(L"--msi=", 0) == 0) {
            g_msi_override = argument.substr(6);
        } else if (argument == L"--version") {
            PrintLine(g_version);
            return 0;
        } else if (argument == L"--help" || argument == L"-h" || argument == L"/?") {
            PrintHelp();
            return 0;
        }
    }

    const std::wstring localAppData = LocalAppData();
    g_install_dir = localAppData.empty() ? L"%LOCALAPPDATA%\\Wilfred" : localAppData + L"\\Wilfred";
    g_log_path = TempPath(L"wilfred-setup.log");

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool comInitialized = SUCCEEDED(comResult);

    if (silent) {
        const int code = RunSilentInstall();
        if (comInitialized) {
            CoUninitialize();
        }
        return code;
    }

    if (check && !g_ui_check) {
        const int code = RunSelfCheck();
        CleanupPayload();
        if (comInitialized) {
            CoUninitialize();
        }
        return code;
    }

    if (g_ui_check) {
        if (RunSelfCheck() != 0) {
            CleanupPayload();
            if (comInitialized) {
                CoUninitialize();
            }
            return 4;
        }
    }

    const int code = RunGui(showCommand);
    CleanupPayload();
    DeleteObject(g_background_brush);
    if (comInitialized) {
        CoUninitialize();
    }
    return code;
}
