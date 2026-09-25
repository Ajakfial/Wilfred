#include "wilfred/ui/icon.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace wilfred {
namespace {

constexpr int kIconPx = 32;

std::mutex g_mu;
std::unordered_map<std::string, std::string> g_cache;

std::string base64_encode(const unsigned char* data, std::size_t len) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve(((len + 2) / 3) * 4);
  std::size_t i = 0;
  while (i + 2 < len) {
    unsigned n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(tbl[(n >> 6) & 63]);
    o.push_back(tbl[n & 63]);
    i += 3;
  }
  if (i < len) {
    unsigned n = data[i] << 16;
    if (i + 1 < len) n |= data[i + 1] << 8;
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(i + 1 < len ? tbl[(n >> 6) & 63] : '=');
    o.push_back('=');
  }
  return o;
}

std::string bmp_data_url(const unsigned char* bgra, int w, int h) {
  const std::uint32_t pixels = static_cast<std::uint32_t>(w * h * 4);
  const std::uint32_t off = 54;
  const std::uint32_t sz = off + pixels;
  std::vector<unsigned char> buf(sz, 0);
  buf[0] = 'B';
  buf[1] = 'M';
  buf[2] = static_cast<unsigned char>(sz);
  buf[3] = static_cast<unsigned char>(sz >> 8);
  buf[4] = static_cast<unsigned char>(sz >> 16);
  buf[5] = static_cast<unsigned char>(sz >> 24);
  buf[10] = 54;
  buf[14] = 40;
  buf[18] = static_cast<unsigned char>(w);
  buf[19] = static_cast<unsigned char>(w >> 8);
  buf[20] = static_cast<unsigned char>(w >> 16);
  buf[21] = static_cast<unsigned char>(w >> 24);
  int ih = -h;
  buf[22] = static_cast<unsigned char>(ih);
  buf[23] = static_cast<unsigned char>(ih >> 8);
  buf[24] = static_cast<unsigned char>(ih >> 16);
  buf[25] = static_cast<unsigned char>(ih >> 24);
  buf[26] = 1;
  buf[28] = 32;
  std::memcpy(buf.data() + 54, bgra, pixels);
  return "data:image/bmp;base64," + base64_encode(buf.data(), buf.size());
}

#ifdef _WIN32
std::string icon_from_hicon(HICON hicon) {
  if (!hicon) return {};
  HDC screen = GetDC(nullptr);
  HDC dc = CreateCompatibleDC(screen);
  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = kIconPx;
  bmi.bmiHeader.biHeight = -kIconPx;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  HGDIOBJ old = SelectObject(dc, dib);
  RECT rc{0, 0, kIconPx, kIconPx};
  HBRUSH bg = CreateSolidBrush(RGB(32, 34, 42));
  FillRect(dc, &rc, bg);
  DeleteObject(bg);
  IntersectClipRect(dc, 0, 0, kIconPx, kIconPx);
  SetStretchBltMode(dc, HALFTONE);
  DrawIconEx(dc, 0, 0, hicon, kIconPx, kIconPx, 0, nullptr, DI_NORMAL);
  std::string url;
  if (bits) url = bmp_data_url(static_cast<unsigned char*>(bits), kIconPx, kIconPx);
  SelectObject(dc, old);
  DeleteObject(dib);
  DeleteDC(dc);
  ReleaseDC(nullptr, screen);
  return url;
}

std::string win_file_icon(const std::string& path) {
  auto w = utf8_to_wide(path);
  SHFILEINFOW info{};
  DWORD_PTR ok = SHGetFileInfoW(w.c_str(), 0, &info, sizeof(info),
                                SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);
  if (!ok || !info.hIcon) {
    ok = SHGetFileInfoW(w.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON);
  }
  std::string url;
  if (info.hIcon) {
    url = icon_from_hicon(info.hIcon);
    DestroyIcon(info.hIcon);
  }
  return url;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
std::string linux_icon_file(const std::string& name) {
  if (name.empty()) return {};
  std::string file = name;
  if (file.find('/') != std::string::npos) {
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::u8path(file), ec)) return file;
  }
  const char* themes[] = {
      "/usr/share/icons/hicolor/32x32/apps/",
      "/usr/share/icons/hicolor/48x48/apps/",
      "/usr/share/pixmaps/",
      "/usr/share/icons/Adwaita/32x32/apps/",
  };
  const char* ext[] = {".png", ".svg", ".xpm", ""};
  for (auto* dir : themes) {
    for (auto* e : ext) {
      auto p = std::string(dir) + file + e;
      std::error_code ec;
      if (std::filesystem::exists(std::filesystem::u8path(p), ec)) return p;
    }
  }
  return {};
}

std::string file_to_data_url(const std::string& path) {
  std::string bytes;
  if (!read_file_all(path, bytes) || bytes.empty() || bytes.size() > 96 * 1024) return {};
  auto ext = to_lower_utf8(path_extension(path));
  const char* mime = "image/png";
  if (ext == ".svg") mime = "image/svg+xml";
  else if (ext == ".jpg" || ext == ".jpeg")
    mime = "image/jpeg";
  else if (ext == ".ico")
    mime = "image/x-icon";
  else if (ext == ".bmp")
    mime = "image/bmp";
  auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
  return std::string("data:") + mime + ";base64," + base64_encode(p, bytes.size());
}
#endif

}  // namespace

std::string file_icon_data_url(const std::string& path, FileKind kind) {
  if (path.empty()) return {};
  if (kind != FileKind::Application && kind != FileKind::Executable &&
      kind != FileKind::Shortcut && kind != FileKind::Directory) {
    auto ext = to_lower_utf8(path_extension(path));
    if (ext != ".exe" && ext != ".lnk" && ext != ".app" && ext != ".desktop") return {};
  }
  {
    std::lock_guard<std::mutex> lock(g_mu);
    auto it = g_cache.find(path);
    if (it != g_cache.end()) return it->second;
  }
  std::string url;
#ifdef _WIN32
  url = win_file_icon(path);
#elif defined(__APPLE__)
  (void)kind;
#else
  auto ext = to_lower_utf8(path_extension(path));
  if (ext == ".desktop") {
    std::string text;
    if (read_file_all(path, text)) {
      auto pos = text.find("\nIcon=");
      if (pos == std::string::npos && text.rfind("Icon=", 0) == 0) pos = 0;
      if (pos != std::string::npos) {
        if (pos == 0)
          pos = 5;
        else
          pos += 6;
        auto end = text.find('\n', pos);
        auto icon = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        while (!icon.empty() && (icon.back() == '\r' || icon.back() == ' ')) icon.pop_back();
        auto file = linux_icon_file(icon);
        if (!file.empty()) url = file_to_data_url(file);
      }
    }
  } else {
    auto file = linux_icon_file(path_stem(path));
    if (!file.empty()) url = file_to_data_url(file);
  }
#endif
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_cache.size() > 256) g_cache.clear();
  g_cache[path] = url;
  return url;
}

}  // namespace wilfred
