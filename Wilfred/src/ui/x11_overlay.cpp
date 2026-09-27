#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>

#if !defined(_WIN32) && !defined(__APPLE__) && defined(WILFRED_HAS_X11)
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
    return true;
  }
  void show() override {
    if (!dpy) return;
    text.clear();
    results.clear();
    XMapRaised(dpy, win);
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
