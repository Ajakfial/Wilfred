#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/ui/web_ui.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wrl.h>
#include <wrl/event.h>
#include <WebView2.h>
#endif

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wilfred {
#ifdef _WIN32

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static OverlayQuery g_query;
static OverlaySubmit g_submit;
static std::function<void()> g_quit;
static HWND g_hwnd = nullptr;
static bool g_visible = false;
static bool g_ready = false;
static bool g_want_show = false;
static bool g_tray = false;
static NOTIFYICONDATAW g_nid{};
static std::atomic<std::uint64_t> g_qid{0};
static std::mutex g_res_mu;
static std::vector<SearchResult> g_results;
static constexpr UINT WM_WILFRED_RESULTS = WM_APP + 7;
static constexpr UINT WM_WILFRED_READY = WM_APP + 8;
static constexpr UINT WM_WILFRED_TRAY = WM_APP + 9;
static constexpr UINT ID_TRAY_SHOW = 1;
static constexpr UINT ID_TRAY_QUIT = 2;

static ComPtr<ICoreWebView2Environment> g_env;
static ComPtr<ICoreWebView2Controller> g_ctrl;
static ComPtr<ICoreWebView2> g_web;

static void post_json(const std::string& json) {
  if (!g_web) return;
  auto w = utf8_to_wide(json);
  g_web->PostWebMessageAsJson(w.c_str());
}

static void layout_webview() {
  if (!g_ctrl || !g_hwnd) return;
  RECT rc{};
  GetClientRect(g_hwnd, &rc);
  g_ctrl->put_Bounds(rc);
}

static void place_window(int w, int h) {
  int sx = GetSystemMetrics(SM_CXSCREEN);
  int sy = GetSystemMetrics(SM_CYSCREEN);
  w = std::clamp(w, 420, sx);
  h = std::clamp(h, 100, sy - 40);
  int x = (sx - w) / 2;
  int y = sy / 6;
  SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
  layout_webview();
}

static void do_query(const std::string& q) {
  auto id = ++g_qid;
  std::thread([id, q] {
    std::vector<SearchResult> r;
    try {
      if (g_query) r = g_query(q);
    } catch (...) {
      r.clear();
    }
    if (id != g_qid.load()) return;
    {
      std::lock_guard<std::mutex> lock(g_res_mu);
      g_results = std::move(r);
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_WILFRED_RESULTS, 0, 0);
  }).detach();
}

static void accept_index(int idx, const std::string& action) {
  std::lock_guard<std::mutex> lock(g_res_mu);
  if (idx >= 0 && idx < static_cast<int>(g_results.size()) && g_submit)
    g_submit(g_results[static_cast<std::size_t>(idx)], action);
}

static void hide_now() {
  if (g_hwnd) ShowWindow(g_hwnd, SW_HIDE);
  g_visible = false;
  g_want_show = false;
}

static void show_now() {
  if (!g_hwnd) return;
  g_want_show = true;
  if (!g_ready) return;
  g_want_show = false;
  SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
  ShowWindow(g_hwnd, SW_SHOW);
  SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  SetForegroundWindow(g_hwnd);
  g_visible = true;
  post_json("{\"type\":\"show\"}");
}

static void tray_remove() {
  if (!g_tray) return;
  Shell_NotifyIconW(NIM_DELETE, &g_nid);
  g_tray = false;
}

static void tray_add() {
  tray_remove();
  g_nid = {};
  g_nid.cbSize = sizeof(g_nid);
  g_nid.hWnd = g_hwnd;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  g_nid.uCallbackMessage = WM_WILFRED_TRAY;
  g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  lstrcpynW(g_nid.szTip, L"Wilfred  (Ctrl+Alt+W)", ARRAYSIZE(g_nid.szTip));
  g_tray = Shell_NotifyIconW(NIM_ADD, &g_nid) == TRUE;
}

static void request_quit() {
  hide_now();
  tray_remove();
  if (g_quit) g_quit();
}

static void tray_popup() {
  POINT pt{};
  GetCursorPos(&pt);
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, L"Show");
  AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Quit");
  SetForegroundWindow(g_hwnd);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_hwnd, nullptr);
  PostMessageW(g_hwnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
  if (cmd == ID_TRAY_SHOW) show_now();
  else if (cmd == ID_TRAY_QUIT)
    request_quit();
}

static void handle_web_message(const std::string& json) {
  std::string type;
  overlay_json_field(json, "type", type);
  if (type == "ready") {
    g_ready = true;
    if (g_hwnd) PostMessageW(g_hwnd, WM_WILFRED_READY, 0, 0);
    return;
  }
  if (type == "query") {
    std::string q;
    overlay_json_field(json, "q", q);
    do_query(q);
    return;
  }
  if (type == "submit") {
    std::string idxs, action;
    overlay_json_field(json, "index", idxs);
    overlay_json_field(json, "action", action);
    int idx = 0;
    try {
      idx = std::stoi(idxs);
    } catch (...) {
      idx = 0;
    }
    accept_index(idx, action);
    return;
  }
  if (type == "hidden") {
    hide_now();
    return;
  }
  if (type == "resize") {
    std::string ws, hs;
    overlay_json_field(json, "width", ws);
    overlay_json_field(json, "height", hs);
    int w = 704, h = 120;
    try {
      if (!ws.empty()) w = std::stoi(ws);
      if (!hs.empty()) h = std::stoi(hs);
    } catch (...) {
    }
    place_window(w, h);
  }
}

static void init_webview(ICoreWebView2Controller* ctrl) {
  g_ctrl = ctrl;
  ctrl->get_CoreWebView2(&g_web);
  if (!g_web) return;

  ComPtr<ICoreWebView2Controller2> c2;
  if (SUCCEEDED(ctrl->QueryInterface(IID_PPV_ARGS(&c2))) && c2) {
    COREWEBVIEW2_COLOR clear{0, 0, 0, 0};
    c2->put_DefaultBackgroundColor(clear);
  }

  ComPtr<ICoreWebView2Settings> settings;
  if (SUCCEEDED(g_web->get_Settings(&settings)) && settings) {
    settings->put_AreDefaultContextMenusEnabled(FALSE);
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDefaultScriptDialogsEnabled(FALSE);
  }

  auto dir = utf8_to_wide(overlay_ui_dir());
  ComPtr<ICoreWebView2_3> web3;
  if (SUCCEEDED(g_web.As(&web3)) && web3) {
    web3->SetVirtualHostNameToFolderMapping(L"wilfred.ui", dir.c_str(),
                                            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
    g_web->Navigate(L"https://wilfred.ui/index.html");
  } else {
    std::wstring uri = L"file:///" + dir + L"/index.html";
    for (auto& ch : uri)
      if (ch == L'\\') ch = L'/';
    g_web->Navigate(uri.c_str());
  }

  g_web->add_WebMessageReceived(
      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            if (!args) return S_OK;
            std::string json;
            LPWSTR raw = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
              json = wide_to_utf8(raw);
              CoTaskMemFree(raw);
            } else if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
              json = wide_to_utf8(raw);
              CoTaskMemFree(raw);
            } else {
              return S_OK;
            }
            handle_web_message(json);
            return S_OK;
          })
          .Get(),
      nullptr);

  g_ctrl->put_IsVisible(TRUE);
  layout_webview();
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_NCHITTEST:
      return HTCLIENT;
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      layout_webview();
      return 0;
    case WM_ACTIVATE:
      if (LOWORD(w) == WA_INACTIVE && g_visible) hide_now();
      return 0;
    case WM_WILFRED_RESULTS: {
      std::vector<SearchResult> snap;
      {
        std::lock_guard<std::mutex> lock(g_res_mu);
        snap = g_results;
      }
      post_json(overlay_results_json(snap));
      return 0;
    }
    case WM_WILFRED_READY:
      if (g_want_show) show_now();
      return 0;
    case WM_WILFRED_TRAY:
      if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) tray_popup();
      else if (l == WM_LBUTTONUP || l == WM_LBUTTONDBLCLK)
        show_now();
      return 0;
    case WM_DESTROY:
      tray_remove();
      g_web.Reset();
      g_ctrl.Reset();
      g_env.Reset();
      g_hwnd = nullptr;
      return 0;
    default:
      break;
  }
  return DefWindowProcW(h, m, w, l);
}

class WinOverlay final : public OverlayUi {
public:
  OverlayQuery query;
  OverlaySubmit submit;
  bool create() override {
    g_query = query;
    g_submit = submit;
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WilfredOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);

    int width = 704, height = 140;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"",
        WS_POPUP, (sx - width) / 2, sy / 6, width, height, nullptr, nullptr, wc.hInstance,
        nullptr);
    if (!g_hwnd) return false;
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);
    tray_add();

    auto data = utf8_to_wide(path_join(data_directory(), "webview"));
    create_directories(path_join(data_directory(), "webview"));
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, data.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
              if (FAILED(result) || !env) {
                log_error("ui", "WebView2 runtime is missing or failed to start");
                return result;
              }
              g_env = env;
              env->CreateCoreWebView2Controller(
                  g_hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                              [](HRESULT result2, ICoreWebView2Controller* ctrl) -> HRESULT {
                                if (FAILED(result2) || !ctrl) {
                                  log_error("ui", "failed to create WebView2 controller");
                                  return result2;
                                }
                                init_webview(ctrl);
                                return S_OK;
                              })
                              .Get());
              return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
      log_error("ui", "CreateCoreWebView2EnvironmentWithOptions failed");
      return false;
    }
    return true;
  }
  void show() override { show_now(); }
  void hide() override { hide_now(); }
  void destroy() override {
    tray_remove();
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
    g_web.Reset();
    g_ctrl.Reset();
    g_env.Reset();
  }
  bool visible() const override { return g_visible; }
};

static WinOverlay* g_overlay = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<WinOverlay>();
  g_overlay = p.get();
  return p;
}

void overlay_bind(OverlayQuery q, OverlaySubmit s) {
  if (g_overlay) {
    g_overlay->query = std::move(q);
    g_overlay->submit = std::move(s);
    g_query = g_overlay->query;
    g_submit = g_overlay->submit;
  }
}

void overlay_set_quit(std::function<void()> fn) { g_quit = std::move(fn); }

void overlay_pump() {}

#else
std::unique_ptr<OverlayUi> create_overlay();
#endif
}  // namespace wilfred
