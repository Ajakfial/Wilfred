#include "wilfred/platform/native.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <string>

#if defined(WILFRED_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace wilfred {

namespace {

#if defined(WILFRED_HAS_X11)
unsigned char channel_of(unsigned long pixel, unsigned long mask) {
  if (!mask) return 0;
  int shift = 0;
  while ((mask & 1u) == 0) {
    mask >>= 1;
    ++shift;
  }
  return static_cast<unsigned char>(((pixel & mask) >> shift) & 0xFFu);
}

bool sample_x11(int x, int y, NativeColor& out, std::string& error) {
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    // Wayland: no root window to read. A screencopy portal implementation is
    // the only supported route, and it does not exist yet.
    error = "Screen color sampling needs an X11 display";
    return false;
  }
  bool ok = false;
  XImage* img = XGetImage(dpy, DefaultRootWindow(dpy), x, y, 1, 1, AllPlanes, ZPixmap);
  if (!img) {
    error = "Could not read the pixel at that point";
  } else {
    const unsigned long px = XGetPixel(img, 0, 0);
    out.r = channel_of(px, img->red_mask);
    out.g = channel_of(px, img->green_mask);
    out.b = channel_of(px, img->blue_mask);
    XDestroyImage(img);
    ok = true;
  }
  XCloseDisplay(dpy);
  return ok;
}
#endif

}  // namespace

bool native_color_at_point(int x, int y, NativeColor& out, std::string& error) {
  error.clear();
#if defined(WILFRED_HAS_X11)
  return sample_x11(x, y, out, error);
#else
  (void)x;
  (void)y;
  (void)out;
  error = "Screen color sampling is not available in this build";
  return false;
#endif
}

bool native_color_at_cursor(NativeColor& out, std::string& error) {
  error.clear();
#if defined(WILFRED_HAS_X11)
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    error = "Screen color sampling needs an X11 display";
    return false;
  }
  Window root = DefaultRootWindow(dpy);
  Window root_ret = 0;
  Window child = 0;
  int root_x = 0;
  int root_y = 0;
  int win_x = 0;
  int win_y = 0;
  unsigned int mask = 0;
  const bool queried =
      XQueryPointer(dpy, root, &root_ret, &child, &root_x, &root_y, &win_x, &win_y, &mask) != 0;
  XCloseDisplay(dpy);
  if (!queried) {
    error = "Could not read the cursor position";
    return false;
  }
  return sample_x11(root_x, root_y, out, error);
#else
  (void)out;
  error = "Screen color sampling is not available in this build";
  return false;
#endif
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_color_at_point(int, int, NativeColor&, std::string&);
bool native_color_at_cursor(NativeColor&, std::string&);
}  // namespace wilfred
#endif
