#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#if defined(WILFRED_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

namespace {

std::string popen_line(const char* cmd) {
  std::string out;
  FILE* f = popen(cmd, "r");
  if (!f) return out;
  char buf[1024];
  if (fgets(buf, sizeof(buf), f)) out = buf;
  pclose(f);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

std::string screenshot_dir_linux() {
  auto from_xdg = popen_line("xdg-user-dir PICTURES 2>/dev/null");
  std::string base;
  if (!from_xdg.empty() && from_xdg[0] == '/')
    base = from_xdg;
  else {
    const char* xdg_pics = std::getenv("XDG_PICTURES_DIR");
    if (xdg_pics && *xdg_pics)
      base = xdg_pics;
    else
      base = path_join(home_directory(), "Pictures");
  }
  return path_join(base, "Wilfred");
}

std::string next_path_linux(const std::string& dir, const char* ext) {
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
  localtime_r(&t, &tmv);
  char base[64];
  std::strftime(base, sizeof(base), "Screenshot-%Y%m%d-%H%M%S", &tmv);
  for (int i = 0; i < 100; ++i) {
    std::string name = base;
    if (i) name += "-" + std::to_string(i + 1);
    name += ext;
    auto full = path_join(dir, name);
    if (!fs::exists(fs::u8path(full), ec)) return full;
  }
  return path_join(dir, std::string(base) + ext);
}

bool have_tool(const char* name) {
  std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

std::string shell_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

bool run_and_check(const std::string& cmd, const std::string& path) {
  std::string full = cmd + " >/dev/null 2>&1";
  if (std::system(full.c_str()) != 0) return false;
  std::error_code ec;
  return fs::exists(fs::u8path(path), ec) && fs::file_size(fs::u8path(path), ec) > 0;
}

bool file_nonempty(const std::string& path) {
  std::error_code ec;
  return fs::exists(fs::u8path(path), ec) && fs::file_size(fs::u8path(path), ec) > 0;
}

#if defined(WILFRED_HAS_X11)
#pragma pack(push, 1)
struct BmpFileHeader {
  std::uint16_t type{0x4D42};
  std::uint32_t size{0};
  std::uint16_t reserved1{0};
  std::uint16_t reserved2{0};
  std::uint32_t off_bits{54};
};
struct BmpInfoHeader {
  std::uint32_t size{40};
  std::int32_t width{0};
  std::int32_t height{0};
  std::uint16_t planes{1};
  std::uint16_t bit_count{24};
  std::uint32_t compression{0};
  std::uint32_t image_size{0};
  std::int32_t x_ppm{0};
  std::int32_t y_ppm{0};
  std::uint32_t clr_used{0};
  std::uint32_t clr_important{0};
};
#pragma pack(pop)

bool x11_fullscreen_bmp(const std::string& path) {
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) return false;
  auto mask_shift = [](unsigned long mask) {
    int shift = 0;
    if (!mask) return 0;
    while ((mask & 1u) == 0) {
      mask >>= 1;
      ++shift;
    }
    return shift;
  };
  bool ok = false;
  Window root = DefaultRootWindow(dpy);
  XWindowAttributes attr{};
  if (XGetWindowAttributes(dpy, root, &attr) && attr.width > 0 && attr.height > 0) {
    int w = attr.width;
    int h = attr.height;
    XImage* img = XGetImage(dpy, root, 0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h),
                            AllPlanes, ZPixmap);
    if (img) {
      int r_shift = mask_shift(img->red_mask);
      int g_shift = mask_shift(img->green_mask);
      int b_shift = mask_shift(img->blue_mask);
      int stride = ((w * 3 + 3) / 4) * 4;
      std::vector<unsigned char> rows(static_cast<std::size_t>(stride) * h, 0);
      for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
          unsigned long px = XGetPixel(img, x, y);
          unsigned char r = static_cast<unsigned char>(((px & img->red_mask) >> r_shift) & 0xFF);
          unsigned char g = static_cast<unsigned char>(((px & img->green_mask) >> g_shift) & 0xFF);
          unsigned char b = static_cast<unsigned char>(((px & img->blue_mask) >> b_shift) & 0xFF);
          auto* dst = &rows[static_cast<std::size_t>(h - 1 - y) * stride + x * 3];
          dst[0] = b;
          dst[1] = g;
          dst[2] = r;
        }
      }
      BmpFileHeader fh;
      BmpInfoHeader ih;
      ih.width = w;
      ih.height = h;
      ih.image_size = static_cast<std::uint32_t>(rows.size());
      fh.size = fh.off_bits + ih.image_size;
      if (FILE* f = fopen(path.c_str(), "wb")) {
        ok = fwrite(&fh, 1, sizeof(fh), f) == sizeof(fh) &&
             fwrite(&ih, 1, sizeof(ih), f) == sizeof(ih) &&
             fwrite(rows.data(), 1, rows.size(), f) == rows.size();
        fclose(f);
      }
      XDestroyImage(img);
    }
  }
  XCloseDisplay(dpy);
  return ok && file_nonempty(path);
}
#endif

bool try_tool_fullscreen(const std::string& path) {
  auto q = shell_quote(path);
  if (have_tool("gnome-screenshot") && run_and_check("gnome-screenshot -f " + q, path)) return true;
  if (have_tool("scrot") && run_and_check("scrot " + q, path)) return true;
  if (have_tool("spectacle") && run_and_check("spectacle -b -n -o " + q, path)) return true;
  if (have_tool("maim") && run_and_check("maim " + q, path)) return true;
  if (have_tool("grim") && run_and_check("grim " + q, path)) return true;
  if (have_tool("import") && run_and_check("import -window root " + q, path)) return true;
  if (have_tool("xfce4-screenshooter") && run_and_check("xfce4-screenshooter -f -s " + q, path))
    return true;
  return false;
}

bool try_tool_window(const std::string& path) {
  auto q = shell_quote(path);
  if (have_tool("gnome-screenshot") && run_and_check("gnome-screenshot -w -f " + q, path))
    return true;
  if (have_tool("scrot") && run_and_check("scrot -u " + q, path)) return true;
  if (have_tool("spectacle") && run_and_check("spectacle -a -b -n -o " + q, path)) return true;
  if (have_tool("maim")) {
    // Focused-window capture needs the window id; fall back to interactive pick.
    if (have_tool("xdotool")) {
      std::string id = popen_line("xdotool getactivewindow 2>/dev/null");
      if (!id.empty() && run_and_check("maim -i " + id + " " + q, path)) return true;
    }
    if (run_and_check("maim -i root " + q, path)) return true;
  }
  if (have_tool("import") && run_and_check("import " + q, path)) return true;
  if (have_tool("xfce4-screenshooter") && run_and_check("xfce4-screenshooter -w -s " + q, path))
    return true;
  return false;
}

bool try_tool_region(const std::string& path) {
  auto q = shell_quote(path);
  if (have_tool("gnome-screenshot") && run_and_check("gnome-screenshot -a -f " + q, path))
    return true;
  if (have_tool("scrot") && run_and_check("scrot -s " + q, path)) return true;
  if (have_tool("spectacle") && run_and_check("spectacle -r -b -n -o " + q, path)) return true;
  if (have_tool("maim") && run_and_check("maim -s " + q, path)) return true;
  if (have_tool("grim") && have_tool("slurp")) {
    std::string geo = popen_line("slurp 2>/dev/null");
    if (!geo.empty() && run_and_check("grim -g " + shell_quote(geo) + " " + q, path)) return true;
  }
  if (have_tool("import") && run_and_check("import " + q, path)) return true;
  if (have_tool("xfce4-screenshooter") && run_and_check("xfce4-screenshooter -r -s " + q, path))
    return true;
  return false;
}

}  // namespace

std::string native_screenshot_save_directory() {
  return screenshot_dir_linux();
}

bool native_take_screenshot(NativeScreenshotMode mode, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  auto dir = screenshot_dir_linux();
  if (mode == NativeScreenshotMode::Fullscreen) {
    auto png = next_path_linux(dir, ".png");
    if (try_tool_fullscreen(png)) {
      out_path = png;
      return true;
    }
#if defined(WILFRED_HAS_X11)
    auto bmp = next_path_linux(dir, ".bmp");
    if (x11_fullscreen_bmp(bmp)) {
      out_path = bmp;
      return true;
    }
#endif
    error =
        "No screenshot tool found (install gnome-screenshot, scrot, grim, spectacle, maim, or "
        "ImageMagick)";
    return false;
  }
  auto png = next_path_linux(dir, ".png");
  bool ok = mode == NativeScreenshotMode::Window ? try_tool_window(png) : try_tool_region(png);
  if (ok) {
    out_path = png;
    return true;
  }
  error =
      "No screenshot tool found for this mode (install gnome-screenshot, scrot, grim+slurp, "
      "spectacle, maim, or ImageMagick)";
  return false;
}

}  // namespace wilfred
#else
namespace wilfred {
std::string native_screenshot_save_directory();
bool native_take_screenshot(NativeScreenshotMode, std::string&, std::string&);
}  // namespace wilfred
#endif
