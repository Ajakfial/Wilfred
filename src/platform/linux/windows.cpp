#include "wilfred/platform/native.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if defined(WILFRED_HAS_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#endif

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

#if defined(WILFRED_HAS_X11)

namespace {

std::string x_window_utf8_name(Display* dpy, Window win) {
  Atom net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
  Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
  Atom actual = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char* prop = nullptr;
  if (XGetWindowProperty(dpy, win, net_wm_name, 0, 1024, False, utf8, &actual, &fmt, &nitems,
                         &after, &prop) == Success &&
      prop && nitems) {
    std::string s(reinterpret_cast<char*>(prop), nitems);
    XFree(prop);
    return s;
  }
  if (prop) XFree(prop);
  char* name = nullptr;
  if (XFetchName(dpy, win, &name) && name) {
    std::string s(name);
    XFree(name);
    return s;
  }
  return {};
}

std::string owner_from_pid(unsigned long pid) {
  if (!pid) return {};
  std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
  std::string name;
  std::getline(comm, name);
  return name;
}

unsigned long x_window_pid(Display* dpy, Window win) {
  Atom atom = XInternAtom(dpy, "_NET_WM_PID", False);
  Atom actual = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char* prop = nullptr;
  unsigned long pid = 0;
  if (XGetWindowProperty(dpy, win, atom, 0, 1, False, XA_CARDINAL, &actual, &fmt, &nitems, &after,
                         &prop) == Success &&
      prop && nitems) {
    pid = *reinterpret_cast<unsigned long*>(prop);
  }
  if (prop) XFree(prop);
  return pid;
}

std::vector<Window> x_client_list(Display* dpy) {
  std::vector<Window> out;
  Window root = DefaultRootWindow(dpy);
  Atom atom = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
  Atom actual = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char* prop = nullptr;
  auto fetch = [&](Atom a) {
    if (prop) {
      XFree(prop);
      prop = nullptr;
    }
    nitems = 0;
    return XGetWindowProperty(dpy, root, a, 0, 4096, False, XA_WINDOW, &actual, &fmt, &nitems,
                              &after, &prop) == Success &&
           prop && nitems;
  };
  if (!fetch(atom)) {
    atom = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    if (!fetch(atom)) {
      if (prop) XFree(prop);
      return out;
    }
  }
  auto* wins = reinterpret_cast<Window*>(prop);
  out.assign(wins, wins + nitems);
  XFree(prop);
  return out;
}

}  // namespace

std::vector<NativeWindowInfo> native_list_windows() {
  std::vector<NativeWindowInfo> out;
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) return out;
  auto wins = x_client_list(dpy);
  for (auto it = wins.rbegin(); it != wins.rend(); ++it) {
    Window win = *it;
    auto title = x_window_utf8_name(dpy, win);
    if (title.empty() || title == "Wilfred") continue;
    NativeWindowInfo w;
    w.id = static_cast<std::uint64_t>(win);
    w.title = std::move(title);
    w.owner = owner_from_pid(x_window_pid(dpy, win));
    out.push_back(std::move(w));
  }
  XCloseDisplay(dpy);
  return out;
}

bool native_focus_window(std::uint64_t id) {
  if (!id) return false;
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) return false;
  Window win = static_cast<Window>(id);
  Window root = DefaultRootWindow(dpy);
  Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  XEvent ev{};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win;
  ev.xclient.message_type = active;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = 2;
  ev.xclient.data.l[1] = CurrentTime;
  ev.xclient.data.l[2] = 0;
  XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
  XMapRaised(dpy, win);
  XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
  XFlush(dpy);
  XCloseDisplay(dpy);
  return true;
}

#else

std::vector<NativeWindowInfo> native_list_windows() { return {}; }

bool native_focus_window(std::uint64_t) { return false; }

#endif
#endif
}  // namespace wilfred
