#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {

std::string fold_ascii(std::string_view s) {
  std::string o;
  o.reserve(s.size());
  for (unsigned char c : s) o.push_back(static_cast<char>(std::tolower(c)));
  return o;
}

std::string to_lower_utf8(std::string_view s) {
  // Full Unicode case fold is not required for launcher ranking; ASCII fold plus
  // pass-through of non-ASCII keeps matching stable for CJK and accented names.
  std::string o;
  o.reserve(s.size());
  for (std::size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 128) {
      o.push_back(static_cast<char>(std::tolower(c)));
      ++i;
    } else {
      o.push_back(static_cast<char>(c));
      ++i;
    }
  }
  return o;
}

std::string normalize_query(std::string_view s) {
  std::string o;
  o.reserve(s.size());
  bool sp = false;
  for (unsigned char c : s) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!o.empty() && !sp) {
        o.push_back(' ');
        sp = true;
      }
    } else {
      o.push_back(static_cast<char>(std::tolower(c)));
      sp = false;
    }
  }
  if (!o.empty() && o.back() == ' ') o.pop_back();
  return o;
}

bool starts_with_ci(std::string_view hay, std::string_view needle) {
  if (needle.size() > hay.size()) return false;
  for (std::size_t i = 0; i < needle.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(hay[i])) !=
        std::tolower(static_cast<unsigned char>(needle[i])))
      return false;
  }
  return true;
}

bool contains_ci(std::string_view hay, std::string_view needle) {
  if (needle.empty()) return true;
  if (needle.size() > hay.size()) return false;
  auto h = to_lower_utf8(hay);
  auto n = to_lower_utf8(needle);
  return h.find(n) != std::string::npos;
}

std::vector<std::uint32_t> utf8_codepoints(std::string_view s) {
  std::vector<std::uint32_t> out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    std::uint32_t cp = 0;
    int n = 1;
    if (c < 0x80) {
      cp = c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
      n = 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
      cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(s[i + 2]) & 0x3F);
      n = 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
      cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(s[i + 3]) & 0x3F);
      n = 4;
    } else {
      cp = c;
    }
    out.push_back(cp);
    i += static_cast<std::size_t>(n);
  }
  return out;
}

#ifdef _WIN32
std::wstring utf8_to_wide(std::string_view s) {
  if (s.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  std::wstring w(static_cast<std::size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
  return w;
}

std::string wide_to_utf8(std::wstring_view s) {
  if (s.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0,
                              nullptr, nullptr);
  std::string o(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), o.data(), n, nullptr,
                      nullptr);
  return o;
}
#endif

}  // namespace wilfred
