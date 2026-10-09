#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace wilfred {
namespace fs = std::filesystem;

namespace {

std::string screenshot_dir_win() {
  PWSTR w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &w)) && w) {
    std::string s = path_join(wide_to_utf8(w), "Wilfred");
    CoTaskMemFree(w);
    return s;
  }
  return path_join(path_join(home_directory(), "Pictures"), "Wilfred");
}

std::string next_path_win(const std::string& dir, const char* ext) {
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
  localtime_s(&tmv, &t);
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

bool save_hbitmap_bmp(HBITMAP hbmp, HDC hdc, const std::string& path) {
  BITMAP bm{};
  if (!GetObjectW(hbmp, sizeof(bm), &bm)) return false;
  BITMAPINFOHEADER bi{};
  bi.biSize = sizeof(bi);
  bi.biWidth = bm.bmWidth;
  bi.biHeight = -bm.bmHeight;  // top-down
  bi.biPlanes = 1;
  bi.biBitCount = 24;
  bi.biCompression = BI_RGB;
  int stride = ((bm.bmWidth * 3 + 3) / 4) * 4;
  std::vector<unsigned char> bits(static_cast<std::size_t>(stride) * bm.bmHeight);
  BITMAPINFO info{};
  info.bmiHeader = bi;
  if (!GetDIBits(hdc, hbmp, 0, static_cast<UINT>(bm.bmHeight), bits.data(), &info, DIB_RGB_COLORS))
    return false;
  BITMAPFILEHEADER fh{};
  fh.bfType = 0x4D42;
  fh.bfOffBits = sizeof(fh) + sizeof(bi);
  fh.bfSize = fh.bfOffBits + static_cast<DWORD>(bits.size());
  FILE* f = nullptr;
  if (_wfopen_s(&f, utf8_to_wide(path).c_str(), L"wb") != 0 || !f) return false;
  bool ok = fwrite(&fh, 1, sizeof(fh), f) == sizeof(fh) &&
            fwrite(&bi, 1, sizeof(bi), f) == sizeof(bi) &&
            fwrite(bits.data(), 1, bits.size(), f) == bits.size();
  fclose(f);
  return ok;
}

bool capture_rect_to_bmp(int x, int y, int w, int h, const std::string& path, std::string& error) {
  if (w <= 0 || h <= 0) {
    error = "Nothing to capture";
    return false;
  }
  HDC h_screen = GetDC(nullptr);
  if (!h_screen) {
    error = "Could not access the screen";
    return false;
  }
  HDC h_mem = CreateCompatibleDC(h_screen);
  HBITMAP h_bmp = CreateCompatibleBitmap(h_screen, w, h);
  bool ok = false;
  if (h_mem && h_bmp) {
    HGDIOBJ old = SelectObject(h_mem, h_bmp);
    if (BitBlt(h_mem, 0, 0, w, h, h_screen, x, y, SRCCOPY | CAPTUREBLT)) {
      if (save_hbitmap_bmp(h_bmp, h_screen, path))
        ok = true;
      else
        error = "Could not write screenshot file";
    } else {
      error = "Screen capture failed";
    }
    SelectObject(h_mem, old);
  } else {
    error = "Could not allocate capture buffer";
  }
  if (h_bmp) DeleteObject(h_bmp);
  if (h_mem) DeleteDC(h_mem);
  ReleaseDC(nullptr, h_screen);
  return ok;
}

bool launch_snipping_ui() {
  // Modern Snipping Tool overlay first, classic Snipping Tool as fallback.
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", L"ms-screenclip:", nullptr, nullptr, SW_SHOWNORMAL));
  if (rc > 32) return true;
  rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", L"snippingtool.exe", nullptr, nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

}  // namespace

std::string native_screenshot_save_directory() {
  return screenshot_dir_win();
}

bool native_take_screenshot(NativeScreenshotMode mode, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  if (mode == NativeScreenshotMode::Region) {
    if (launch_snipping_ui()) return true;
    error = "Could not open the snipping tool";
    return false;
  }
  auto dir = screenshot_dir_win();
  auto path = next_path_win(dir, ".bmp");
  if (mode == NativeScreenshotMode::Fullscreen) {
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (w <= 0 || h <= 0) {
      w = GetSystemMetrics(SM_CXSCREEN);
      h = GetSystemMetrics(SM_CYSCREEN);
      x = 0;
      y = 0;
    }
    if (capture_rect_to_bmp(x, y, w, h, path, error)) {
      out_path = path;
      return true;
    }
    return false;
  }
  // Window: capture the foreground window.
  HWND fg = GetForegroundWindow();
  if (!fg || !IsWindow(fg)) {
    error = "No active window to capture";
    return false;
  }
  RECT rc{};
  if (!GetWindowRect(fg, &rc)) {
    error = "Could not measure the active window";
    return false;
  }
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) {
    error = "Active window has no size";
    return false;
  }
  if (capture_rect_to_bmp(rc.left, rc.top, w, h, path, error)) {
    out_path = path;
    return true;
  }
  return false;
}

}  // namespace wilfred
#else
namespace wilfred {
std::string native_screenshot_save_directory();
bool native_take_screenshot(NativeScreenshotMode, std::string&, std::string&);
}  // namespace wilfred
#endif
