#include "wilfred/search/ocr.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace wilfred {
namespace {

std::string shell_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"') o += "\\\"";
    else
      o.push_back(c);
  }
  o.push_back('"');
  return o;
}

std::string collapse(const std::string& s, std::size_t cap) {
  std::string o;
  o.reserve(s.size() < cap ? s.size() : cap);
  bool ws = true;
  for (char c : s) {
    bool is_ws = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    if (is_ws) {
      if (!ws) {
        o.push_back(' ');
        ws = true;
        if (o.size() >= cap) break;
      }
    } else {
      o.push_back(c);
      ws = false;
      if (o.size() >= cap) break;
    }
  }
  while (!o.empty() && o.back() == ' ') o.pop_back();
  return o;
}

}  // namespace

bool is_image_extension(const std::string& path) {
  auto e = to_lower_utf8(path_extension(path));
  return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tif" ||
         e == ".tiff" || e == ".bmp" || e == ".webp" || e == ".gif";
}

bool is_ocr_candidate(const std::string& path) { return is_image_extension(path); }

bool tesseract_available() {
#ifdef _WIN32
  // `where tesseract` is quiet and fast.
  int rc = std::system("where tesseract >NUL 2>NUL");
  return rc == 0;
#else
  int rc = std::system("command -v tesseract >/dev/null 2>&1");
  return rc == 0;
#endif
}

bool extract_ocr_text(const std::string& path, std::string& out_text,
                      std::size_t max_bytes, const std::string& languages) {
  out_text.clear();
  if (max_bytes == 0 || path.empty()) return false;
  if (!is_ocr_candidate(path)) return false;
  if (!file_exists(path)) return false;
  std::string langs = languages.empty() ? "eng" : languages;
  // Keep shell metacharacters out of -l.
  std::string safe_langs;
  for (char c : langs) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' || c == '_')
      safe_langs.push_back(c);
  }
  if (safe_langs.empty()) safe_langs = "eng";
#ifdef _WIN32
  std::string cmd = "tesseract " + shell_quote(path) + " stdout -l " + safe_langs +
                    " --psm 6 2>NUL";
#else
  std::string cmd = "tesseract " + shell_quote(path) +
                    " stdout -l " + safe_langs + " --psm 6 2>/dev/null";
#endif
  FILE* f = POPEN(cmd.c_str(), "r");
  if (!f) return false;
  std::string raw;
  char buf[2048];
  while (fgets(buf, sizeof(buf), f)) {
    raw += buf;
    if (raw.size() > max_bytes * 2) break;
  }
  PCLOSE(f);
  auto collapsed = collapse(raw, max_bytes);
  int alpha = 0;
  for (unsigned char c : collapsed) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
      if (++alpha >= 3) break;
  }
  if (alpha < 3) return false;
  out_text = collapsed;
  return true;
}

}  // namespace wilfred
