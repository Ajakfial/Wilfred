#include "wilfred/platform/native.hpp"

#ifdef _WIN32
// windows.h first: keeps the Win32 macro surface contained.
#include <windows.h>

#include <string>

namespace wilfred {

namespace {

// GetPixel wants screen coordinates in the DC's space. GetCursorPos fills the
// same space, so the pair stays consistent whether or not the process is
// DPI aware -- under virtualization both are scaled together, which means the
// sampled pixel is the one the user sees, just reported in scaled units.
bool sample_point(int x, int y, NativeColor& out, std::string& error) {
  HDC h = GetDC(nullptr);
  if (!h) {
    error = "Could not access the screen";
    return false;
  }
  const COLORREF px = GetPixel(h, x, y);
  ReleaseDC(nullptr, h);
  if (px == CLR_INVALID) {
    error = "Could not read the pixel at that point";
    return false;
  }
  out.r = GetRValue(px);
  out.g = GetGValue(px);
  out.b = GetBValue(px);
  return true;
}

}  // namespace

bool native_color_at_point(int x, int y, NativeColor& out, std::string& error) {
  error.clear();
  return sample_point(x, y, out, error);
}

bool native_color_at_cursor(NativeColor& out, std::string& error) {
  error.clear();
  POINT p{};
  if (!GetCursorPos(&p)) {
    error = "Could not read the cursor position";
    return false;
  }
  return sample_point(p.x, p.y, out, error);
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_color_at_point(int, int, NativeColor&, std::string&);
bool native_color_at_cursor(NativeColor&, std::string&);
}  // namespace wilfred
#endif
