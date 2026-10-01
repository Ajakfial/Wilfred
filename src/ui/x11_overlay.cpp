#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/ui/web_ui.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <vector>

#if !defined(_WIN32) && !defined(__APPLE__)

// Modern Linux overlay: WebKitGTK (same HTML/JS as Windows/macOS) when
// available, otherwise an improved X11 canvas fallback. Both speak the
// identical overlay protocol (results + correction + ghost + candidates)
// so typo safety and autocomplete behave the same on every platform.

#ifdef WILFRED_HAS_WEBKIT
// WebKitGTK headers are pulled via pkg-config cflags (see CMakeLists).
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>
#endif

#if defined(WILFRED_HAS_X11) && !defined(WILFRED_HAS_WEBKIT)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

namespace wilfred {

#if defined(WILFRED_HAS_WEBKIT)
// ---------------------------------------------------------------- WebKitGTK

namespace {

struct WebkitState {
  OverlayQuery query;
  OverlaySubmit submit;
  GtkWidget* window{nullptr};
  WebKitWebView* web{nullptr};
  std::vector<SearchResult> results;
  OverlayResponse last;
  bool ready{false};
  bool want_show{false};
  bool visible{false};
};

static WebkitState g_wk;

static void wk_send_json(const std::string& json) {
  if (!g_wk.web) return;
  std::string js = "window.__wilfredNative && window.__wilfredNative(" + json + ");";
  webkit_web_view_run_javascript(g_wk.web, js.c_str(), nullptr, nullptr, nullptr);
}

static void wk_place(int w, int h) {
  if (!g_wk.window) return;
  GdkScreen* screen = gdk_screen_get_default();
  int sx = screen ? gdk_screen_get_width(screen) : 1280;
  int sy = screen ? gdk_screen_get_height(screen) : 800;
  w = std::clamp(w, 420, sx);
  h = std::clamp(h, 100, sy - 40);
  int x = (sx - w) / 2;
  int y = sy / 6;
  gtk_window_move(GTK_WINDOW(g_wk.window), x, y);
  gtk_window_resize(GTK_WINDOW(g_wk.window), w, h);
}

static void wk_handle_json(const std::string& json) {
  std::string type;
  overlay_json_field(json, "type", type);
  if (type == "ready") {
    g_wk.ready = true;
    if (g_wk.want_show) {
      g_wk.want_show = false;
      gtk_widget_show_all(g_wk.window);
      gtk_window_present(GTK_WINDOW(g_wk.window));
      gtk_widget_grab_focus(GTK_WIDGET(g_wk.web));
      g_wk.visible = true;
      wk_send_json("{\"type\":\"show\"}");
    }
    return;
  }
  if (type == "query") {
    std::string q;
    overlay_json_field(json, "q", q);
    OverlayResponse resp;
    resp.query = q;
    try {
      if (g_wk.query) resp = g_wk.query(q);
    } catch (...) {
    }
    g_wk.results = resp.results;
    g_wk.last = resp;
    wk_send_json(overlay_results_json(resp.results, {}, resp.correction, resp.ghost,
                                      resp.candidates, resp.query));
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
    }
    if (idx >= 0 && idx < static_cast<int>(g_wk.results.size()) && g_wk.submit)
      g_wk.submit(g_wk.results[static_cast<std::size_t>(idx)], action);
    return;
  }
  if (type == "preview") {
    std::string idxs;
    overlay_json_field(json, "index", idxs);
    int idx = -1;
    try {
      idx = std::stoi(idxs);
    } catch (...) {
    }
    if (idx >= 0 && idx < static_cast<int>(g_wk.results.size())) {
      const auto& r = g_wk.results[static_cast<std::size_t>(idx)];
      std::string path = r.path.empty() ? r.payload : r.path;
      if (!path.empty()) wk_send_json(overlay_preview_json(path));
    }
    return;
  }
  if (type == "hidden") {
    if (g_wk.window) gtk_widget_hide(g_wk.window);
    g_wk.visible = false;
    return;
  }
  if (type == "resize") {
    std::string ws, hs;
    overlay_json_field(json, "width", ws);
    overlay_json_field(json, "height", hs);
    int w = 760, h = 140;
    try {
      if (!ws.empty()) w = std::stoi(ws);
      if (!hs.empty()) h = std::stoi(hs);
    } catch (...) {
    }
    wk_place(w, h);
    return;
  }
}

static void wk_on_script_message(WebKitUserContentManager*, JSCValue* value, gpointer) {
  if (!value) return;
  char* str = jsc_value_to_string(value);
  if (!str) return;
  std::string json(str);
  g_free(str);
  wk_handle_json(json);
}

}  // namespace

class LinuxOverlay final : public OverlayUi {
 public:
  OverlayQuery query;
  OverlaySubmit submit;
  bool create() override {
    if (!gtk_init_check(nullptr, nullptr)) return false;
    g_wk.query = query;
    g_wk.submit = submit;
    g_wk.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_wk.window), "Wilfred");
    gtk_window_set_default_size(GTK_WINDOW(g_wk.window), 760, 140);
    gtk_window_set_decorated(GTK_WINDOW(g_wk.window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_wk.window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(g_wk.window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(g_wk.window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(g_wk.window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(g_wk.window), GTK_WIN_POS_CENTER);
    // Transparent-friendly dark frame; the web UI paints its own panel.
    GdkRGBA bg{0, 0, 0, 0};
    gtk_widget_override_background_color(g_wk.window, GTK_STATE_FLAG_NORMAL, &bg);

    WebKitWebView* web = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_wk.web = web;
    WebKitSettings* settings = webkit_web_view_get_settings(web);
    if (settings) {
      webkit_settings_set_enable_developer_extras(settings, FALSE);
      webkit_settings_set_enable_java(settings, FALSE);
    }
    WebKitUserContentManager* ucc =
        webkit_web_view_get_user_content_manager(web);
    webkit_user_content_manager_register_script_message_handler(ucc, "wilfred");
    g_signal_connect(ucc, "script-message-received::wilfred",
                     G_CALLBACK(wk_on_script_message), nullptr);
    gtk_container_add(GTK_CONTAINER(g_wk.window), GTK_WIDGET(web));
    auto dir = overlay_ui_dir();
    std::string html = path_join(dir, "index.html");
    std::string uri = "file://" + html;
    webkit_web_view_load_uri(web, uri.c_str());
    wk_place(760, 140);
    return true;
  }
  void show() override {
    g_wk.query = query;
    g_wk.submit = submit;
    if (!g_wk.window) return;
    g_wk.want_show = true;
    if (!g_wk.ready) return;
    g_wk.want_show = false;
    gtk_widget_show_all(g_wk.window);
    gtk_window_present(GTK_WINDOW(g_wk.window));
    if (g_wk.web) gtk_widget_grab_focus(GTK_WIDGET(g_wk.web));
    g_wk.visible = true;
    wk_send_json("{\"type\":\"show\"}");
  }
  void hide() override {
    if (g_wk.window) gtk_widget_hide(g_wk.window);
    g_wk.visible = false;
  }
  void destroy() override {
    if (g_wk.window) {
      gtk_widget_destroy(g_wk.window);
      g_wk.window = nullptr;
      g_wk.web = nullptr;
    }
  }
  bool visible() const override { return g_wk.visible; }
};

static LinuxOverlay* g_ov = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<LinuxOverlay>();
  g_ov = p.get();
  return p;
}

void overlay_bind(OverlayQuery q, OverlaySubmit s) {
  g_wk.query = q;
  g_wk.submit = s;
  if (g_ov) {
    g_ov->query = std::move(q);
    g_ov->submit = std::move(s);
    g_wk.query = g_ov->query;
    g_wk.submit = g_ov->submit;
  }
}

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {
  while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
}

#else
// ---------------------------------------------------------------- X11 fallback
// Improved canvas fallback used when WebKitGTK is unavailable (minimal
// containers, X11-only WMs). Still fully typo-safe: shows correction,
// ghost and candidates inline.

class X11Overlay final : public OverlayUi {
 public:
  OverlayQuery query;
  OverlaySubmit submit;
#ifdef WILFRED_HAS_X11
  Display* dpy{nullptr};
  Window win{0};
  bool vis{false};
  std::string text;
  OverlayResponse last;
  int sel{0};
  std::chrono::steady_clock::time_point speed_poll{};

  static bool speedtest_query(const std::string& q) {
    std::string t = q;
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!t.empty() && t.front() == ' ') t.erase(t.begin());
    while (!t.empty() && t.back() == ' ') t.pop_back();
    static const char* keys[] = {"speedtest", "speed-test", "speed_test", "netspeed",
                                 "bandwidth", "internetspeed", nullptr};
    for (auto** p = keys; *p; ++p) {
      std::string k = *p;
      if (t == k) return true;
      if (t.size() > k.size() && t.compare(0, k.size(), k) == 0 && t[k.size()] == ' ') {
        auto rest = t.substr(k.size() + 1);
        while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
        return rest.empty() || rest == "again" || rest == "retry" || rest == "new" ||
               rest == "rerun";
      }
    }
    return false;
  }

  bool create() override {
    dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;
    int s = DefaultScreen(dpy);
    int w = 760, h = 560;
    int x = (DisplayWidth(dpy, s) - w) / 2;
    int y = DisplayHeight(dpy, s) / 6;
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, s), x, y, static_cast<unsigned>(w),
                              static_cast<unsigned>(h), 1, BlackPixel(dpy, s), 0x171922);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    XStoreName(dpy, win, "Wilfred");
    Atom net_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom above = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    Atom skip_task = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom sticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    Atom states[] = {above, skip_task, skip_pager, sticky};
    XChangeProperty(dpy, win, net_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(states), 4);
    Atom win_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom dialog = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, win_type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&dialog), 1);
    XFlush(dpy);
    return true;
  }
  void show() override {
    if (!dpy) return;
    text.clear();
    last = OverlayResponse{};
    sel = 0;
    speed_poll = {};
    refresh();
    XMapRaised(dpy, win);
    XRaiseWindow(dpy, win);
    XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
    vis = true;
    draw();
  }
  void hide() override {
    if (dpy && win) XUnmapWindow(dpy, win);
    vis = false;
  }
  void destroy() override {
    if (dpy && win) XDestroyWindow(dpy, win);
    if (dpy) XCloseDisplay(dpy);
    dpy = nullptr;
  }
  bool visible() const override { return vis; }

  void refresh() {
    if (query) {
      try {
        last = query(text);
      } catch (...) {
        last = OverlayResponse{};
      }
    }
    sel = 0;
  }

  void draw() {
    if (!dpy) return;
    XClearWindow(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    auto draw_str = [&](int x, int y, const std::string& s, unsigned long color) {
      XSetForeground(dpy, gc, color);
      if (!s.empty()) XDrawString(dpy, win, gc, x, y, s.c_str(), static_cast<int>(s.size()));
    };
    // Search field with ghost suffix.
    std::string prompt = "> " + text;
    draw_str(16, 28, prompt, 0xF0F0F5);
    if (!last.ghost.empty() && last.ghost.size() > text.size()) {
      std::string suffix = last.ghost.substr(text.size());
      if (suffix.size() > 48) suffix.resize(48);
      int xoff = 16 + static_cast<int>(prompt.size()) * 7;
      draw_str(xoff, 28, suffix, 0x6C7490);
    }
    int y = 52;
    // Did-you-mean bar.
    if (!last.correction.empty()) {
      std::string c = "Did you mean " + last.correction + "?  (Tab to apply)";
      if (c.size() > 88) c.resize(88);
      draw_str(16, y, c, 0x8B97FF);
      y += 22;
    }
    // Candidates row.
    if (!last.candidates.empty()) {
      std::string cands;
      for (std::size_t i = 0; i < last.candidates.size() && i < 4; ++i) {
        if (i) cands += "  |  ";
        cands += last.candidates[i];
      }
      if (cands.size() > 96) cands.resize(96);
      draw_str(16, y, cands, 0x9AA2B8);
      y += 22;
    }
    y += 6;
    const auto& results = last.results;
    for (int i = 0; i < static_cast<int>(results.size()) && i < 9; ++i) {
      if (i == sel) {
        XSetForeground(dpy, gc, 0x2A3050);
        XFillRectangle(dpy, win, gc, 8, y - 16, 744, 40);
      }
      auto t = results[static_cast<std::size_t>(i)].title;
      if (t.size() > 80) t.resize(80);
      draw_str(16, y, t, i == sel ? 0xFFFFFF : 0xF0F0F5);
      y += 40;
      if (y > 540) break;
    }
    if (results.empty()) draw_str(16, y, "(no matches — typo-tolerant search active)", 0x6C7490);
    XFreeGC(dpy, gc);
    XFlush(dpy);
  }

  void accept_ghost_or_correction() {
    if (!last.ghost.empty() && last.ghost.size() > text.size()) {
      // Ghost extends the query: accept it.
      text = last.ghost;
      refresh();
      draw();
    } else if (!last.correction.empty()) {
      text = last.correction;
      refresh();
      draw();
    }
  }

  void pump() {
    if (!dpy) return;
    while (XPending(dpy)) {
      XEvent e;
      XNextEvent(dpy, &e);
      if (e.type == Expose) draw();
      if (e.type == KeyPress) {
        KeySym ks;
        char buf[32]{};
        XLookupString(&e.xkey, buf, sizeof(buf), &ks, nullptr);
        const auto& results = last.results;
        if (ks == XK_Escape) {
          hide();
        } else if (ks == XK_Return && sel < static_cast<int>(results.size()) && submit) {
          auto& r = results[static_cast<std::size_t>(sel)];
          std::string act;
          if ((e.xkey.state & ShiftMask) || (e.xkey.state & Mod1Mask)) {
            if (r.actions.size() >= 2)
              act = r.actions[1].id;
            else if (!r.actions.empty())
              act = r.actions[0].id;
          }
          submit(r, act);
        } else if (ks == XK_Tab) {
          // Typo-first: Tab applies correction/ghost when present (parity
          // with the web UI's Tab-to-apply), otherwise secondary action.
          if (!last.correction.empty() || (!last.ghost.empty() && last.ghost.size() > text.size())) {
            accept_ghost_or_correction();
          } else if (sel < static_cast<int>(results.size()) && submit) {
            auto& r = results[static_cast<std::size_t>(sel)];
            std::string act =
                r.actions.size() >= 2 ? r.actions[1].id : (r.actions.empty() ? "" : r.actions[0].id);
            submit(r, act);
          }
        } else if (ks == XK_Right && !last.ghost.empty() && last.ghost.size() > text.size()) {
          text = last.ghost;
          refresh();
          draw();
        } else if (ks == XK_Down && !results.empty()) {
          sel = (sel + 1) % static_cast<int>(results.size());
          draw();
        } else if (ks == XK_Up && !results.empty()) {
          sel = (sel - 1 + static_cast<int>(results.size())) % static_cast<int>(results.size());
          draw();
        } else if (ks == XK_BackSpace) {
          if (!text.empty()) text.pop_back();
          refresh();
          draw();
        } else if (buf[0] >= 32) {
          text += buf[0];
          refresh();
          draw();
        }
      }
    }
    if (vis && query && speedtest_query(text)) {
      auto now = std::chrono::steady_clock::now();
      if (speed_poll.time_since_epoch().count() == 0 ||
          now - speed_poll >= std::chrono::milliseconds(350)) {
        speed_poll = now;
        refresh();
        draw();
      }
    }
  }
#else
  bool create() override { return false; }
  void show() override {}
  void hide() override {}
  void destroy() override {}
  bool visible() const override { return false; }
#endif
};

static X11Overlay* g_ov = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<X11Overlay>();
  g_ov = p.get();
  return p;
}

void overlay_bind(OverlayQuery q, OverlaySubmit s) {
  if (!g_ov) return;
  g_ov->query = std::move(q);
  g_ov->submit = std::move(s);
}

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {
#ifdef WILFRED_HAS_X11
  if (g_ov) g_ov->pump();
#endif
}

#endif

#endif
}  // namespace wilfred
