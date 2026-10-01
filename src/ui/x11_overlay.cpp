#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>

#if !defined(_WIN32) && !defined(__APPLE__) && defined(WILFRED_HAS_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif

namespace wilfred {

#if !defined(_WIN32) && !defined(__APPLE__)

class X11Overlay final : public OverlayUi {
public:
  OverlayQuery query;
  OverlaySubmit submit;
#ifdef WILFRED_HAS_X11
  Display* dpy{nullptr};
  Window win{0};
  bool vis{false};
  std::string text;
  std::vector<SearchResult> results;
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
    int w = 720, h = 480;
    int x = (DisplayWidth(dpy, s) - w) / 2;
    int y = DisplayHeight(dpy, s) / 5;
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, s), x, y, w, h, 1, BlackPixel(dpy, s),
                              0x16161A);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    XStoreName(dpy, win, "Wilfred");
    // Cross-platform parity: keep the overlay above normal windows, out of
    // the taskbar/pager, and focused when shown (mirrors TOPMOST on Windows
    // and NSFloatingWindowLevel on macOS).
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
    results.clear();
    speed_poll = {};
    XMapRaised(dpy, win);
    // Raise + focus so WMs that ignore _NET_WM_STATE_ABOVE still surface us.
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

  void draw() {
    if (!dpy) return;
    XClearWindow(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, nullptr);
    XSetForeground(dpy, gc, 0xF0F0F5);
    std::string prompt = "> " + text;
    XDrawString(dpy, win, gc, 16, 28, prompt.c_str(), static_cast<int>(prompt.size()));
    int y = 64;
    for (int i = 0; i < static_cast<int>(results.size()) && i < 9; ++i) {
      if (i == sel) {
        XSetForeground(dpy, gc, 0x2850A0);
        XFillRectangle(dpy, win, gc, 8, y - 16, 704, 40);
      }
      XSetForeground(dpy, gc, 0xF0F0F5);
      auto t = results[static_cast<std::size_t>(i)].title;
      if (t.size() > 80) t.resize(80);
      XDrawString(dpy, win, gc, 16, y, t.c_str(), static_cast<int>(t.size()));
      y += 44;
    }
    XFreeGC(dpy, gc);
    XFlush(dpy);
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
        if (ks == XK_Escape) hide();
        else if (ks == XK_Return && sel < static_cast<int>(results.size()) && submit) {
          auto& r = results[static_cast<std::size_t>(sel)];
          std::string act;
          if ((e.xkey.state & ShiftMask) || (e.xkey.state & Mod1Mask)) {
            if (r.actions.size() >= 2)
              act = r.actions[1].id;
            else if (!r.actions.empty())
              act = r.actions[0].id;
            else
              act = "reveal";
          }
          submit(r, act);
        } else if (ks == XK_Tab && sel < static_cast<int>(results.size()) && submit) {
          auto& r = results[static_cast<std::size_t>(sel)];
          std::string act = r.actions.size() >= 2 ? r.actions[1].id : (r.actions.empty() ? "reveal" : r.actions[0].id);
          submit(r, act);
        } else if (ks == XK_Down && !results.empty()) {
          sel = (sel + 1) % static_cast<int>(results.size());
          draw();
        } else if (ks == XK_Up && !results.empty()) {
          sel = (sel - 1 + static_cast<int>(results.size())) % static_cast<int>(results.size());
          draw();
        } else if (ks == XK_BackSpace) {
          if (!text.empty()) text.pop_back();
          if (query) results = query(text);
          sel = 0;
          draw();
        } else if (buf[0] >= 32) {
          text += buf[0];
          if (query) results = query(text);
          sel = 0;
          draw();
        }
      }
    }
    if (vis && query && speedtest_query(text)) {
      auto now = std::chrono::steady_clock::now();
      if (speed_poll.time_since_epoch().count() == 0 ||
          now - speed_poll >= std::chrono::milliseconds(350)) {
        speed_poll = now;
        results = query(text);
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
}  // namespace wilfred
