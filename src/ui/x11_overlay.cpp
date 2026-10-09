#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/ui/wayland.hpp"
#include "wilfred/ui/web_ui.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if !defined(_WIN32) && !defined(__APPLE__)

// Modern Linux overlay: WebKitGTK (same HTML/JS as Windows/macOS) when
// available, with an X11 canvas fallback for minimal containers and
// X11-only WMs. On Wayland sessions the WebKit window additionally becomes
// a layer-shell surface (gtk-layer-shell, when built in): top-anchored,
// always on top, exclusive keyboard — the things plain GTK hints cannot do
// under a Wayland compositor. Both speak the identical overlay protocol
// (results + correction + ghost + candidates) so typo safety and
// autocomplete behave the same on every platform. When both backends are
// compiled in, WebKit is tried first and the X11 canvas is used
// automatically if WebKit cannot start (missing runtime, no display,
// gtk_init failure).

#ifdef WILFRED_HAS_WEBKIT
// WebKitGTK headers are pulled via pkg-config cflags (see CMakeLists).
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <jsc/jsc.h>
#endif

#ifdef WILFRED_HAS_LAYER_SHELL
// gtk-layer-shell (pkg-config gtk-layer-shell-0): turns the GTK window into
// a native Wayland layer-shell surface. Optional at build time and
// self-disabling at runtime on compositors without the protocol.
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#if defined(WILFRED_HAS_X11)
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
  bool fit{false};  // user dragged an edge: stop content-hugging, fill the window
};

static WebkitState g_wk;

static void wk_send_json(const std::string& json) {
  if (!g_wk.web) return;
  std::string js = "window.__wilfredNative && window.__wilfredNative(" + json + ");";
  webkit_web_view_run_javascript(g_wk.web, js.c_str(), nullptr, nullptr, nullptr);
}

// Undecorated GTK windows have no resize handles, so we provide an invisible
// grip band at the window edge (the web UI keeps a transparent shadow margin
// there, so it never overlaps the panel). Pressing in the band starts a
// WM-driven resize; hovering it shows the matching resize cursor.
static constexpr int kGrip = 8;
static constexpr int kMinW = 420;
static constexpr int kMinH = 100;      // auto-size (content-hugging) minimum
static constexpr int kMinDragH = 220;  // minimum once the user resizes

static bool wk_edge_at(double x, double y, GdkWindowEdge* edge) {
  if (!g_wk.window) return false;
  int w = gtk_widget_get_allocated_width(g_wk.window);
  int h = gtk_widget_get_allocated_height(g_wk.window);
  const bool left = x < kGrip, right = x >= w - kGrip;
  const bool top = y < kGrip, bottom = y >= h - kGrip;
  if (top && left) *edge = GDK_WINDOW_EDGE_NORTH_WEST;
  else if (top && right) *edge = GDK_WINDOW_EDGE_NORTH_EAST;
  else if (bottom && left) *edge = GDK_WINDOW_EDGE_SOUTH_WEST;
  else if (bottom && right) *edge = GDK_WINDOW_EDGE_SOUTH_EAST;
  else if (left) *edge = GDK_WINDOW_EDGE_WEST;
  else if (right) *edge = GDK_WINDOW_EDGE_EAST;
  else if (top) *edge = GDK_WINDOW_EDGE_NORTH;
  else if (bottom) *edge = GDK_WINDOW_EDGE_SOUTH;
  else return false;
  return true;
}

static const char* wk_edge_cursor(GdkWindowEdge e) {
  switch (e) {
    case GDK_WINDOW_EDGE_NORTH_WEST: return "nw-resize";
    case GDK_WINDOW_EDGE_NORTH_EAST: return "ne-resize";
    case GDK_WINDOW_EDGE_SOUTH_WEST: return "sw-resize";
    case GDK_WINDOW_EDGE_SOUTH_EAST: return "se-resize";
    case GDK_WINDOW_EDGE_WEST: return "w-resize";
    case GDK_WINDOW_EDGE_EAST: return "e-resize";
    case GDK_WINDOW_EDGE_NORTH: return "n-resize";
    default: return "s-resize";
  }
}

// Event coordinates are relative to the web view, which fills the window.
static gboolean wk_on_button_press(GtkWidget*, GdkEventButton* ev, gpointer) {
  GdkWindowEdge edge;
  if (ev->button != 1 || !wk_edge_at(ev->x, ev->y, &edge)) return FALSE;
  if (!g_wk.fit) {
    g_wk.fit = true;
    GdkGeometry geo{};
    geo.min_width = kMinW;
    geo.min_height = kMinDragH;
    gtk_window_set_geometry_hints(GTK_WINDOW(g_wk.window), nullptr, &geo, GDK_HINT_MIN_SIZE);
    wk_send_json("{\"type\":\"fit\"}");
  }
  gtk_window_begin_resize_drag(GTK_WINDOW(g_wk.window), edge, static_cast<gint>(ev->button),
                               static_cast<gint>(ev->x_root), static_cast<gint>(ev->y_root),
                               ev->time);
  return TRUE;  // swallow: the page must not also see this click
}

static gboolean wk_on_motion(GtkWidget* widget, GdkEventMotion* ev, gpointer) {
  GdkWindow* gw = gtk_widget_get_window(widget);
  if (!gw) return FALSE;
  GdkWindowEdge edge;
  if (wk_edge_at(ev->x, ev->y, &edge)) {
    GdkCursor* c = gdk_cursor_new_from_name(gdk_window_get_display(gw), wk_edge_cursor(edge));
    gdk_window_set_cursor(gw, c);
    if (c) g_object_unref(c);
    return TRUE;
  }
  gdk_window_set_cursor(gw, nullptr);
  return FALSE;
}


static void wk_place(int w, int h) {
  if (!g_wk.window || g_wk.fit) return;  // never fight the user's size
  GdkScreen* screen = gdk_screen_get_default();
  int sx = screen ? gdk_screen_get_width(screen) : 1280;
  int sy = screen ? gdk_screen_get_height(screen) : 800;
  w = std::clamp(w, kMinW, sx);
  h = std::clamp(h, kMinH, sy - 40);
  int x = (sx - w) / 2;
  int y = sy / 6;
  gtk_window_move(GTK_WINDOW(g_wk.window), x, y);
  gtk_window_resize(GTK_WINDOW(g_wk.window), w, h);
}

#ifdef WILFRED_HAS_LAYER_SHELL
// Wayland native path: promote the GTK window to a layer-shell surface.
// Called from create(), before the window is ever realized (a layer-shell
// requirement). Anchored TOP only, so the compositor centers the bar
// horizontally; the top margin mirrors wk_place's sy/6 offset. TOP layer +
// auto exclusive zone keeps maximized windows from covering it; EXCLUSIVE
// keyboard routes keys to the overlay while visible (launcher convention,
// cf. wofi/bemenu). Safe no-ops when the checks fail: off-Wayland sessions
// never reach the library, and gtk-layer-shell ignores compositors without
// the protocol (notably GNOME, which has no layer-shell support).
static void wk_maybe_layer_shell() {
  if (!g_wk.window) return;
  if (!wayland_session_hint()) return;
  // Standalone-safe: does its own roundtrip, so unsupported compositors
  // (notably GNOME) bail out here before any window state is touched.
  if (!gtk_layer_is_supported()) return;
  GtkWindow* win = GTK_WINDOW(g_wk.window);
  gtk_layer_init_for_window(win);
  gtk_layer_set_namespace(win, "wilfred");
  gtk_layer_set_layer(win, GTK_LAYER_SHELL_LAYER_TOP);
  gtk_layer_set_anchor(win, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  GdkScreen* screen = gdk_screen_get_default();
  int sy = screen ? gdk_screen_get_height(screen) : 800;
  gtk_layer_set_margin(win, GTK_LAYER_SHELL_EDGE_TOP, sy / 6);
  gtk_layer_set_keyboard_mode(win, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
  gtk_layer_auto_exclusive_zone_enable(win);
}
#else
static void wk_maybe_layer_shell() {}
#endif

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
      wk_send_json(overlay_show_json());
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
  if (type == "settings-get") {
    wk_send_json(overlay_settings_json());
    return;
  }
  if (type == "setting-set") {
    std::string key, value;
    overlay_json_field(json, "key", key);
    overlay_json_field(json, "value", value);
    wk_send_json(overlay_setting_set_result(key, value));
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

// Async provider push: runs on the GTK main loop (g_idle_add callbacks do),
// so webkit_web_view_run_javascript stays on its home thread.
static gboolean wk_push_idle(gpointer data) {
  std::unique_ptr<OverlayResponse> resp(static_cast<OverlayResponse*>(data));
  if (!resp || !g_wk.web) return G_SOURCE_REMOVE;
  g_wk.results = resp->results;
  g_wk.last = *resp;
  wk_send_json(overlay_results_json(resp->results, {}, resp->correction, resp->ghost,
                                    resp->candidates, resp->query, true));
  return G_SOURCE_REMOVE;
}

static void wk_push_results(const OverlayResponse& resp) {
  if (!g_wk.web) return;
  g_idle_add(wk_push_idle, new OverlayResponse(resp));
}

}  // namespace

class LinuxWebkitOverlay final : public OverlayUi {
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
    gtk_window_set_resizable(GTK_WINDOW(g_wk.window), TRUE);
    // Transparent-friendly dark frame; the web UI paints its own panel.
    GdkRGBA bg{0, 0, 0, 0};
    gtk_widget_override_background_color(g_wk.window, GTK_STATE_FLAG_NORMAL, &bg);

    WebKitWebView* web = WEBKIT_WEB_VIEW(webkit_web_view_new());
    if (!web) {
      if (g_wk.window) {
        gtk_widget_destroy(g_wk.window);
        g_wk.window = nullptr;
      }
      return false;
    }
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
    gtk_widget_add_events(GTK_WIDGET(web), GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(web, "button-press-event", G_CALLBACK(wk_on_button_press), nullptr);
    g_signal_connect(web, "motion-notify-event", G_CALLBACK(wk_on_motion), nullptr);
    auto dir = overlay_ui_dir();
    std::string html = path_join(dir, "index.html");
    std::string uri = "file://" + html;
    webkit_web_view_load_uri(web, uri.c_str());
    wk_place(760, 140);
    wk_maybe_layer_shell();
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
    wk_send_json(overlay_show_json());
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
  void pump() {
    while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
  }
};

#endif  // WILFRED_HAS_WEBKIT

#if defined(WILFRED_HAS_X11)
// ---------------------------------------------------------------- X11 fallback
// Canvas fallback used when WebKitGTK is unavailable or fails to start
// (minimal containers, X11-only WMs). Still fully typo-safe: shows
// correction, ghost and candidates inline. Keyboard map mirrors the web UI:
// Esc back/dismiss, Tab fix/complete, Ctrl+K actions, Ctrl+U clear,
// arrows/Home/End/PgUp/PgDn move, Ctrl+N/P move, Ctrl+1-9 quick open,
// Enter submit (Shift/Alt secondary), F3 details.

// Async provider inbox: the worker thread deposits late results here;
// X11Overlay::pump() (UI thread) merges them into the live query.
static std::mutex g_push_mu;
static OverlayResponse g_push_inbox;
static bool g_push_pending = false;

static void push_inbox_store(const OverlayResponse& resp) {
  std::lock_guard<std::mutex> lock(g_push_mu);
  g_push_inbox = resp;
  g_push_pending = true;
}

static bool push_inbox_take(OverlayResponse& out) {
  std::lock_guard<std::mutex> lock(g_push_mu);
  if (!g_push_pending) return false;
  g_push_pending = false;
  out = std::move(g_push_inbox);
  return true;
}

class X11Overlay final : public OverlayUi {
 public:
  OverlayQuery query;
  OverlaySubmit submit;
  Display* dpy{nullptr};
  Window win{0};
  bool vis{false};
  std::string text;
  OverlayResponse last;
  int sel{0};
  bool preview_open{false};
  FilePreview preview;
  bool correction_dismissed{false};
  bool menu_open{false};
  int menu_sel{0};
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
    preview_open = false;
    preview = FilePreview{};
    correction_dismissed = false;
    menu_open = false;
    menu_sel = 0;
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
    menu_open = false;
    menu_sel = 0;
    correction_dismissed = false;
    if (preview_open) rebuild_preview();
  }

  void rebuild_preview() {
    preview = FilePreview{};
    if (!preview_open) return;
    if (sel < 0 || sel >= static_cast<int>(last.results.size())) return;
    const auto& r = last.results[static_cast<std::size_t>(sel)];
    std::string path = r.path.empty() ? r.payload : r.path;
    if (path.empty() || path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
      preview.title = r.title;
      preview.text = r.subtitle;
      return;
    }
    try {
      preview = build_file_preview(path);
    } catch (...) {
      preview = FilePreview{};
    }
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
    // Did-you-mean bar (Esc dismisses, Tab applies).
    if (!correction_dismissed && !last.correction.empty()) {
      std::string c = "Did you mean " + last.correction + "?  (Tab to apply, Esc to dismiss)";
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
    // Actions popover (Ctrl+K / Tab when no assist).
    const auto& results = last.results;
    if (menu_open && sel >= 0 && sel < static_cast<int>(results.size())) {
      const auto& acts = results[static_cast<std::size_t>(sel)].actions;
      std::string head = "Actions (Esc to close):";
      draw_str(16, y, head, 0x8B97FF);
      y += 20;
      for (int j = 0; j < static_cast<int>(acts.size()) && j < 6; ++j) {
        std::string label = (j == menu_sel ? "> " : "  ") + acts[static_cast<std::size_t>(j)].label;
        if (label.size() > 80) label.resize(80);
        draw_str(24, y, label, j == menu_sel ? 0xFFFFFF : 0x9AA2B8);
        y += 18;
        if (y > 500) break;
      }
      y += 6;
    }
    int limit = preview_open ? 4 : 9;
    for (int i = 0; i < static_cast<int>(results.size()) && i < limit; ++i) {
      if (i == sel) {
        XSetForeground(dpy, gc, 0x2A3050);
        XFillRectangle(dpy, win, gc, 8, y - 16, 744, 40);
      }
      auto t = results[static_cast<std::size_t>(i)].title;
      if (t.size() > 80) t.resize(80);
      draw_str(16, y, t, i == sel ? 0xFFFFFF : 0xF0F0F5);
      y += 40;
      if (y > 470) break;
    }
    if (results.empty()) draw_str(16, y, "(no matches — typo-tolerant search active)", 0x6C7490);
    // Details pane (F3).
    if (preview_open) {
      y += 4;
      std::string head = preview.title.empty() ? "Details" : preview.title;
      if (head.size() > 80) head.resize(80);
      draw_str(16, y, head, 0xFFFFFF);
      y += 18;
      std::string meta;
      if (!preview.kind.empty()) meta += preview.kind;
      if (!preview.size_label.empty()) {
        if (!meta.empty()) meta += "  ";
        meta += preview.size_label;
      }
      if (!preview.modified_label.empty()) {
        if (!meta.empty()) meta += "  ";
        meta += preview.modified_label;
      }
      if (!meta.empty()) {
        if (meta.size() > 88) meta.resize(88);
        draw_str(16, y, meta, 0x9AA2B8);
        y += 18;
      }
      if (!preview.error.empty()) {
        std::string e = preview.error;
        if (e.size() > 88) e.resize(88);
        draw_str(16, y, e, 0x6C7490);
      } else if (!preview.text.empty()) {
        std::size_t pos = 0;
        for (int line = 0; line < 5 && pos < preview.text.size(); ++line) {
          auto nl = preview.text.find('\n', pos);
          std::string row = preview.text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
          if (row.size() > 88) row.resize(88);
          // XDrawString needs non-empty printable content; skip empty rows.
          if (!row.empty()) draw_str(16, y, row, 0xF0F0F5);
          y += 16;
          if (nl == std::string::npos) break;
          pos = nl + 1;
        }
      }
    }
    // Shortcut footer (mirrors the web ActionBar).
    draw_str(16, 545, "Tab fix  Ctrl+K actions  Ctrl+U clear  F3 details  Esc back", 0x6C7490);
    XFreeGC(dpy, gc);
    XFlush(dpy);
  }

  bool has_assist() const {
    if (!correction_dismissed && !last.correction.empty()) return true;
    return !last.ghost.empty() && last.ghost.size() > text.size();
  }

  void accept_ghost_or_correction() {
    if (!last.ghost.empty() && last.ghost.size() > text.size()) {
      text = last.ghost;
      refresh();
      draw();
    } else if (!correction_dismissed && !last.correction.empty()) {
      text = last.correction;
      refresh();
      draw();
    }
  }

  void toggle_menu() {
    if (menu_open) {
      menu_open = false;
      return;
    }
    if (sel >= 0 && sel < static_cast<int>(last.results.size())) {
      const auto& acts = last.results[static_cast<std::size_t>(sel)].actions;
      if (!acts.empty()) {
        menu_open = true;
        menu_sel = 0;
      }
    }
  }

  void toggle_preview() {
    preview_open = !preview_open;
    if (preview_open)
      rebuild_preview();
    else
      preview = FilePreview{};
    draw();
  }

  void move_sel(int delta) {
    const auto& results = last.results;
    if (results.empty()) return;
    menu_open = false;
    menu_sel = 0;
    sel = (sel + delta + static_cast<int>(results.size())) % static_cast<int>(results.size());
    if (preview_open) rebuild_preview();
    draw();
  }

  void jump_sel(int idx) {
    const auto& results = last.results;
    if (results.empty()) return;
    menu_open = false;
    menu_sel = 0;
    sel = std::clamp(idx, 0, static_cast<int>(results.size()) - 1);
    if (preview_open) rebuild_preview();
    draw();
  }

  void submit_at(int idx, const std::string& act) {
    const auto& results = last.results;
    if (idx < 0 || idx >= static_cast<int>(results.size()) || !submit) return;
    submit(results[static_cast<std::size_t>(idx)], act);
  }

  void submit_secondary_at(int idx) {
    const auto& results = last.results;
    if (idx < 0 || idx >= static_cast<int>(results.size())) return;
    const auto& r = results[static_cast<std::size_t>(idx)];
    std::string act;
    if (r.actions.size() >= 2)
      act = r.actions[1].id;
    else if (!r.actions.empty())
      act = r.actions[0].id;
    submit_at(idx, act);
  }

  void pump() {
    if (!dpy) return;
    // Late provider results: same query still showing → swap in place and
    // keep the keyboard selection (clamped); anything else is stale.
    OverlayResponse inbox;
    if (push_inbox_take(inbox) && inbox.query == text) {
      last = inbox;
      if (last.results.empty()) {
        sel = 0;
      } else {
        sel = std::clamp(sel, 0, static_cast<int>(last.results.size()) - 1);
      }
      if (preview_open) rebuild_preview();
      if (vis) draw();
    }
    while (XPending(dpy)) {
      XEvent e;
      XNextEvent(dpy, &e);
      if (e.type == Expose) draw();
      if (e.type == KeyPress) {
        KeySym ks;
        char buf[32]{};
        XLookupString(&e.xkey, buf, sizeof(buf), &ks, nullptr);
        const auto& results = last.results;
        bool ctrl = (e.xkey.state & ControlMask) != 0;
        bool mod = (e.xkey.state & (ControlMask | Mod4Mask)) != 0;
        bool shift = (e.xkey.state & ShiftMask) != 0;
        bool alt = (e.xkey.state & Mod1Mask) != 0;
        if (ks == XK_Escape) {
          if (menu_open) {
            menu_open = false;
            draw();
          } else if (!correction_dismissed && !last.correction.empty()) {
            correction_dismissed = true;
            draw();
          } else if (preview_open) {
            preview_open = false;
            preview = FilePreview{};
            draw();
          } else {
            hide();
          }
        } else if (mod && (ks == XK_k || ks == XK_K)) {
          // Raycast convention: Ctrl+K opens the actions popover.
          toggle_menu();
          draw();
        } else if (ctrl && (ks == XK_u || ks == XK_U)) {
          // Clear the query (Ctrl+K now opens actions).
          if (!text.empty()) {
            text.clear();
            refresh();
            draw();
          } else if (menu_open || preview_open) {
            menu_open = false;
            preview_open = false;
            preview = FilePreview{};
            draw();
          }
        } else if (ks == XK_Tab || ks == XK_ISO_Left_Tab) {
          // Typo-first: Tab applies correction/ghost when present (parity
          // with the web UI), otherwise toggles the actions popover.
          if (has_assist()) {
            accept_ghost_or_correction();
          } else if (menu_open) {
            menu_open = false;
            draw();
          } else if (sel < static_cast<int>(results.size())) {
            toggle_menu();
            draw();
          }
        } else if (ks == XK_F3) {
          toggle_preview();
        } else if ((ks == XK_Return || ks == XK_KP_Enter) && sel < static_cast<int>(results.size()) &&
                   submit) {
          if (menu_open) {
            const auto& acts = results[static_cast<std::size_t>(sel)].actions;
            std::string act;
            if (menu_sel >= 0 && menu_sel < static_cast<int>(acts.size()))
              act = acts[static_cast<std::size_t>(menu_sel)].id;
            submit_at(sel, act);
          } else {
            if (shift || alt)
              submit_secondary_at(sel);
            else
              submit_at(sel, "");
          }
        } else if (mod && ks >= XK_1 && ks <= XK_9) {
          // Quick open: Ctrl/Cmd+1..9 opens that row.
          int pos = static_cast<int>(ks - XK_1);
          if (pos < static_cast<int>(results.size()) && submit) submit_at(pos, "");
        } else if (mod && (ks == XK_n || ks == XK_N)) {
          move_sel(1);
        } else if (mod && (ks == XK_p || ks == XK_P)) {
          move_sel(-1);
        } else if (ks == XK_Home || (mod && ks == XK_Up)) {
          jump_sel(0);
        } else if (ks == XK_End || (mod && ks == XK_Down)) {
          jump_sel(static_cast<int>(results.size()) - 1);
        } else if (ks == XK_Page_Down) {
          if (!results.empty()) jump_sel(sel + 8);
        } else if (ks == XK_Page_Up) {
          if (!results.empty()) jump_sel(sel - 8);
        } else if (menu_open && (ks == XK_Down || ks == XK_Up) && !mod) {
          // The actions popover is a vertical list: arrows move inside it.
          const auto& acts = results[static_cast<std::size_t>(sel)].actions;
          if (!acts.empty()) {
            int n = static_cast<int>(acts.size());
            menu_sel = (menu_sel + (ks == XK_Down ? 1 : -1) + n) % n;
            draw();
          }
        } else if (menu_open && (ks == XK_Right || ks == XK_Left)) {
          const auto& acts = results[static_cast<std::size_t>(sel)].actions;
          if (!acts.empty()) {
            int n = static_cast<int>(acts.size());
            menu_sel = (menu_sel + (ks == XK_Right ? 1 : -1) + n) % n;
            draw();
          }
        } else if (ks == XK_Right && !menu_open && !last.ghost.empty() &&
                   last.ghost.size() > text.size()) {
          text = last.ghost;
          refresh();
          draw();
        } else if (ks == XK_Down && !results.empty()) {
          move_sel(1);
        } else if (ks == XK_Up && !results.empty()) {
          move_sel(-1);
        } else if (ks == XK_BackSpace || ks == XK_Delete) {
          if (!text.empty()) {
            text.pop_back();
            refresh();
            draw();
          } else if (menu_open || preview_open) {
            menu_open = false;
            if (preview_open) {
              preview_open = false;
              preview = FilePreview{};
            }
            draw();
          }
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
};

#endif  // WILFRED_HAS_X11

#if defined(WILFRED_HAS_WEBKIT) && defined(WILFRED_HAS_X11)
// ------------------------------------------------------- Runtime fallback
// Both backends compiled: try WebKitGTK first (full HTML UI), fall back to
// the X11 canvas when WebKit cannot start. Shortcuts match in both paths.

class LinuxOverlay final : public OverlayUi {
 public:
  OverlayQuery query;
  OverlaySubmit submit;
  std::unique_ptr<OverlayUi> inner;

  bool create() override {
    {
      auto wk = std::make_unique<LinuxWebkitOverlay>();
      wk->query = query;
      wk->submit = submit;
      if (wk->create()) {
        inner = std::move(wk);
        return true;
      }
    }
    {
      auto x = std::make_unique<X11Overlay>();
      x->query = query;
      x->submit = submit;
      if (x->create()) {
        inner = std::move(x);
        return true;
      }
    }
    return false;
  }
  void show() override {
    if (!inner && !create()) return;
    sync_inner();
    inner->show();
  }
  void hide() override {
    if (inner) inner->hide();
  }
  void destroy() override {
    if (inner) inner->destroy();
  }
  bool visible() const override { return inner ? inner->visible() : false; }

  void sync_inner() {
    if (!inner) return;
    if (auto* wk = dynamic_cast<LinuxWebkitOverlay*>(inner.get())) {
      wk->query = query;
      wk->submit = submit;
    } else if (auto* x = dynamic_cast<X11Overlay*>(inner.get())) {
      x->query = query;
      x->submit = submit;
    }
  }
  void pump_inner() {
    if (!inner) return;
    if (auto* wk = dynamic_cast<LinuxWebkitOverlay*>(inner.get())) {
      wk->pump();
    } else if (auto* x = dynamic_cast<X11Overlay*>(inner.get())) {
      x->pump();
    }
  }
};

static LinuxOverlay* g_ov = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<LinuxOverlay>();
  g_ov = p.get();
  return p;
}

void overlay_bind(OverlayQuery q, OverlaySubmit s) {
  if (g_ov) {
    g_ov->query = q;
    g_ov->submit = s;
    g_ov->sync_inner();
    g_wk.query = g_ov->query;
    g_wk.submit = g_ov->submit;
  } else {
    g_wk.query = std::move(q);
    g_wk.submit = std::move(s);
  }
}

void overlay_push_results(const OverlayResponse& resp) {
  // Backend is chosen at create() time; fan out to both sinks guarded at
  // delivery (webkit nil-guards, canvas checks the live query at drain).
  wk_push_results(resp);
  push_inbox_store(resp);
}

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {
  if (g_ov) g_ov->pump_inner();
}

#elif defined(WILFRED_HAS_WEBKIT)
// ------------------------------------------------------- WebKitGTK only

static LinuxWebkitOverlay* g_ov = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<LinuxWebkitOverlay>();
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

void overlay_push_results(const OverlayResponse& resp) { wk_push_results(resp); }

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {
  while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
}

#elif defined(WILFRED_HAS_X11)
// ------------------------------------------------------- X11 fallback only

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

void overlay_push_results(const OverlayResponse& resp) { push_inbox_store(resp); }

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {
  if (g_ov) g_ov->pump();
}

#else
// ------------------------------------------------------- No Linux UI backend

std::unique_ptr<OverlayUi> create_overlay() { return nullptr; }

void overlay_bind(OverlayQuery, OverlaySubmit) {}

void overlay_push_results(const OverlayResponse&) {}

void overlay_set_quit(std::function<void()>) {}

void overlay_pump() {}

#endif

}  // namespace wilfred

#endif
