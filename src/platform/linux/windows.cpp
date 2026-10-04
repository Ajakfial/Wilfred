#include "wilfred/platform/native.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include "wilfred/platform/platform.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
// kinfo_proc via <sys/user.h> (DragonFly documents sys/user.h first,
// which pulls in sys/kinfo.h).
#include <sys/user.h>
#endif
#if defined(__OpenBSD__) || defined(__NetBSD__)
// kinfo_proc / kinfo_proc2 live in <sys/sysctl.h> here.
#include <sys/param.h>
#endif

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
#if defined(__FreeBSD__)
  // No /proc by default: single-process sysctl instead of /proc/<pid>/comm.
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  kinfo_proc kp{};
  std::size_t len = sizeof(kp);
  if (sysctl(mib, 4, &kp, &len, nullptr, 0) != 0) return {};
  return std::string(kp.ki_comm, strnlen(kp.ki_comm, sizeof(kp.ki_comm)));
#elif defined(__OpenBSD__)
  // Same shape as FreeBSD but struct kinfo_proc uses p_ field names.
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  kinfo_proc kp{};
  std::size_t len = sizeof(kp);
  if (sysctl(mib, 4, &kp, &len, nullptr, 0) != 0 || len == 0) return {};
  return std::string(kp.p_comm, strnlen(kp.p_comm, sizeof(kp.p_comm)));
#elif defined(__NetBSD__)
  // KERN_PROC2 with element size + count trailing the op.
  int mib[6] = {CTL_KERN, KERN_PROC2, KERN_PROC_PID, static_cast<int>(pid),
                (int)sizeof(struct kinfo_proc2), 0};
  struct kinfo_proc2 kp{};
  std::size_t len = sizeof(kp);
  if (sysctl(mib, 6, &kp, &len, nullptr, 0) != 0 || len == 0) return {};
  return std::string(kp.p_comm, strnlen(kp.p_comm, sizeof(kp.p_comm)));
#elif defined(__DragonFly__)
  // struct kinfo_proc via <sys/user.h>; kp_ field names (see sys/kinfo.h).
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
  kinfo_proc kp{};
  std::size_t len = sizeof(kp);
  if (sysctl(mib, 4, &kp, &len, nullptr, 0) != 0 || len == 0) return {};
  return std::string(kp.kp_comm, strnlen(kp.kp_comm, sizeof(kp.kp_comm)));
#else
  std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
  std::string name;
  std::getline(comm, name);
  return name;
#endif
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

namespace {

bool x_send_root_msg(Display* dpy, Window win, const char* msg_type, long l0, long l1, long l2) {
  Window root = DefaultRootWindow(dpy);
  Atom msg = XInternAtom(dpy, msg_type, False);
  XEvent ev{};
  ev.xclient.type = ClientMessage;
  ev.xclient.window = win;
  ev.xclient.message_type = msg;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = l0;
  ev.xclient.data.l[1] = l1;
  ev.xclient.data.l[2] = l2;
  return XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev) != 0;
}

bool x_has_wm_state(Display* dpy, Window win, const char* state) {
  Atom prop = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom want = XInternAtom(dpy, state, False);
  Atom actual = None;
  int fmt = 0;
  unsigned long nitems = 0, after = 0;
  unsigned char* data = nullptr;
  bool found = false;
  if (XGetWindowProperty(dpy, win, prop, 0, 64, False, XA_ATOM, &actual, &fmt, &nitems, &after,
                         &data) == Success &&
      data) {
    auto* atoms = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; ++i)
      if (atoms[i] == want) {
        found = true;
        break;
      }
  }
  if (data) XFree(data);
  return found;
}

}  // namespace

bool native_window_action(std::uint64_t id, NativeWindowOp op, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    error = "could not open display";
    return false;
  }
  Window win = static_cast<Window>(id);
  int screen = DefaultScreen(dpy);
  bool ok = false;
  switch (op) {
    case NativeWindowOp::Minimize:
      ok = XIconifyWindow(dpy, win, screen) != 0;
      if (!ok) error = "could not minimize window";
      break;
    case NativeWindowOp::Maximize:
      ok = x_send_root_msg(dpy, win, "_NET_WM_STATE", 1,
                           static_cast<long>(XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False)),
                           static_cast<long>(XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False)));
      if (ok)
        ok = x_send_root_msg(dpy, win, "_NET_WM_STATE", 1,
                             static_cast<long>(XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False)),
                             0);
      if (!ok) error = "could not maximize window";
      break;
    case NativeWindowOp::Restore: {
      // Clear maximized state; un-minimize via map+focus like native_focus_window.
      x_send_root_msg(dpy, win, "_NET_WM_STATE", 0,
                      static_cast<long>(XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False)),
                      static_cast<long>(XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False)));
      XMapRaised(dpy, win);
      XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
      ok = true;
      break;
    }
    case NativeWindowOp::Close:
      ok = x_send_root_msg(dpy, win, "_NET_CLOSE_WINDOW", CurrentTime, 0, 0);
      if (!ok) error = "could not close window";
      break;
    case NativeWindowOp::SnapLeft:
    case NativeWindowOp::SnapRight: {
      int sw = XDisplayWidth(dpy, screen);
      int sh = XDisplayHeight(dpy, screen);
      int half = sw / 2;
      int x = op == NativeWindowOp::SnapLeft ? 0 : half;
      XRaiseWindow(dpy, win);
      ok = XMoveResizeWindow(dpy, win, x, 0, static_cast<unsigned>(half),
                             static_cast<unsigned>(sh)) != 0;
      if (!ok) error = "could not snap window";
      break;
    }
  }
  XFlush(dpy);
  XCloseDisplay(dpy);
  return ok;
}

bool native_window_rect(std::uint64_t id, NativeWindowRect& rect, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    error = "could not open display";
    return false;
  }
  Window win = static_cast<Window>(id);
  Window root = DefaultRootWindow(dpy);
  XWindowAttributes attr{};
  if (!XGetWindowAttributes(dpy, win, &attr)) {
    error = "could not read window attributes";
    XCloseDisplay(dpy);
    return false;
  }
  int rx = 0, ry = 0;
  Window child = None;
  XTranslateCoordinates(dpy, win, root, 0, 0, &rx, &ry, &child);
  rect.x = rx;
  rect.y = ry;
  rect.w = attr.width;
  rect.h = attr.height;
  rect.maximized = x_has_wm_state(dpy, win, "_NET_WM_STATE_MAXIMIZED_VERT");
  XCloseDisplay(dpy);
  return true;
}

bool native_window_move(std::uint64_t id, int x, int y, int w, int h, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    error = "could not open display";
    return false;
  }
  Window win = static_cast<Window>(id);
  int use_w = w, use_h = h;
  if (use_w <= 0 || use_h <= 0) {
    XWindowAttributes attr{};
    if (!XGetWindowAttributes(dpy, win, &attr)) {
      error = "could not read window attributes";
      XCloseDisplay(dpy);
      return false;
    }
    if (use_w <= 0) use_w = attr.width;
    if (use_h <= 0) use_h = attr.height;
  }
  XRaiseWindow(dpy, win);
  bool ok = XMoveResizeWindow(dpy, win, x, y, static_cast<unsigned>(use_w),
                             static_cast<unsigned>(use_h)) != 0;
  XFlush(dpy);
  XCloseDisplay(dpy);
  if (!ok) {
    error = "could not move window";
    return false;
  }
  return true;
}

bool native_primary_work_area(NativeWorkArea& out, std::string& error) {
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    error = "could not open display";
    return false;
  }
  int scr = DefaultScreen(dpy);
  out.x = 0;
  out.y = 0;
  out.w = DisplayWidth(dpy, scr);
  out.h = DisplayHeight(dpy, scr);
  XCloseDisplay(dpy);
  if (out.w <= 0 || out.h <= 0) {
    error = "invalid work area";
    return false;
  }
  return true;
}

bool native_monitor_signature(std::string& sig, std::string& error) {
  // Prefer xrandr (multi-monitor aware); fall back to the default screen size.
  FILE* f = popen("xrandr --query 2>/dev/null", "r");
  std::vector<std::string> parts;
  if (f) {
    char line[512];
    while (fgets(line, sizeof(line), f)) {
      std::string s(line);
      // e.g. "HDMI-1 connected primary 1920x1080+0+0 (...)".
      auto con = s.find(" connected ");
      if (con == std::string::npos) continue;
      auto geom = s.find_first_of("0123456789", con);
      if (geom == std::string::npos) continue;
      auto end = s.find_first_of(" (", geom);
      std::string g = s.substr(geom, end == std::string::npos ? std::string::npos : end - geom);
      while (!g.empty() && (g.back() == ' ' || g.back() == '\n' || g.back() == '\r')) g.pop_back();
      if (!g.empty()) parts.push_back(g);
    }
    pclose(f);
  }
  if (parts.empty()) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
      error = "cannot enumerate monitors";
      return false;
    }
    int scr = DefaultScreen(dpy);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%dx%d@0,0", DisplayWidth(dpy, scr),
                  DisplayHeight(dpy, scr));
    XCloseDisplay(dpy);
    sig = buf;
    return true;
  }
  std::sort(parts.begin(), parts.end());
  sig.clear();
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) sig.push_back('|');
    sig += parts[i];
  }
  return true;
}

#else

std::vector<NativeWindowInfo> native_list_windows() { return {}; }

bool native_focus_window(std::uint64_t) { return false; }

bool native_window_action(std::uint64_t, NativeWindowOp, std::string& error) {
  error = "window management needs X11 (unsupported on Wayland-only builds)";
  return false;
}

bool native_window_rect(std::uint64_t, NativeWindowRect&, std::string& error) {
  error = "window management needs X11 (unsupported on Wayland-only builds)";
  return false;
}

bool native_window_move(std::uint64_t, int, int, int, int, std::string& error) {
  error = "window management needs X11 (unsupported on Wayland-only builds)";
  return false;
}

bool native_primary_work_area(NativeWorkArea&, std::string& error) {
  error = "window management needs X11 (unsupported on Wayland-only builds)";
  return false;
}

bool native_monitor_signature(std::string&, std::string& error) {
  error = "window management needs X11 (unsupported on Wayland-only builds)";
  return false;
}

#endif
#endif
}  // namespace wilfred
