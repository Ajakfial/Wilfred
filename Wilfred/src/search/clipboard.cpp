#include "wilfred/search/clipboard.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <cstdio>
#include <memory>
#else
#include <cstdio>
#include <memory>
#endif

namespace wilfred {
namespace {

std::mutex mu;
std::optional<ClipboardSnapshot> override_snap;
std::vector<std::string> history_ring;

void remember_text(const std::string& text) {
  if (text.empty()) return;
  history_ring.erase(std::remove(history_ring.begin(), history_ring.end(), text), history_ring.end());
  history_ring.insert(history_ring.begin(), text);
  if (history_ring.size() > 12) history_ring.resize(12);
}

#ifdef _WIN32
ClipboardSnapshot read_os_clipboard() {
  ClipboardSnapshot snap;
  if (!OpenClipboard(nullptr)) return snap;
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (h) {
    auto* w = static_cast<wchar_t*>(GlobalLock(h));
    if (w) {
      snap.text = wide_to_utf8(w);
      GlobalUnlock(h);
    }
  }
  HANDLE drop = GetClipboardData(CF_HDROP);
  if (drop) {
    auto hdrop = static_cast<HDROP>(drop);
    UINT n = DragQueryFileW(hdrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < n; ++i) {
      UINT len = DragQueryFileW(hdrop, i, nullptr, 0);
      std::wstring buf(len + 1, L'\0');
      DragQueryFileW(hdrop, i, buf.data(), len + 1);
      buf.resize(len);
      auto p = wide_to_utf8(buf);
      if (!p.empty()) snap.paths.push_back(std::move(p));
    }
  }
  CloseClipboard();
  if (snap.text.size() > 16384) snap.text.resize(16384);
  return snap;
}

bool write_os_clipboard(const std::string& text) {
  auto w = utf8_to_wide(text);
  auto bytes = (w.size() + 1) * sizeof(wchar_t);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!mem) return false;
  auto* dst = static_cast<wchar_t*>(GlobalLock(mem));
  if (!dst) {
    GlobalFree(mem);
    return false;
  }
  memcpy(dst, w.c_str(), bytes);
  GlobalUnlock(mem);
  if (!OpenClipboard(nullptr)) {
    GlobalFree(mem);
    return false;
  }
  EmptyClipboard();
  if (!SetClipboardData(CF_UNICODETEXT, mem)) {
    CloseClipboard();
    GlobalFree(mem);
    return false;
  }
  CloseClipboard();
  return true;
}
#else
static std::string popen_read(const char* cmd) {
  std::string out;
  FILE* f = popen(cmd, "r");
  if (!f) return out;
  char buf[1024];
  while (fgets(buf, sizeof(buf), f)) out += buf;
  pclose(f);
  if (out.size() > 16384) out.resize(16384);
  return out;
}

ClipboardSnapshot read_os_clipboard() {
  ClipboardSnapshot snap;
#ifdef __APPLE__
  snap.text = popen_read("pbpaste 2>/dev/null");
#else
  snap.text = popen_read("wl-paste -n 2>/dev/null");
  if (snap.text.empty()) snap.text = popen_read("xclip -selection clipboard -o 2>/dev/null");
#endif
  while (!snap.text.empty() && (snap.text.back() == '\n' || snap.text.back() == '\r'))
    snap.text.pop_back();
  return snap;
}

bool write_os_clipboard(const std::string& text) {
#ifdef __APPLE__
  FILE* f = popen("pbcopy", "w");
#else
  FILE* f = popen("wl-copy 2>/dev/null || xclip -selection clipboard", "w");
#endif
  if (!f) return false;
  fwrite(text.data(), 1, text.size(), f);
  return pclose(f) == 0;
}
#endif

}  // namespace

void set_clipboard_override(std::optional<ClipboardSnapshot> snap) {
  std::lock_guard<std::mutex> lock(mu);
  override_snap = std::move(snap);
  if (override_snap) remember_text(override_snap->text);
}

ClipboardSnapshot read_clipboard() {
  std::lock_guard<std::mutex> lock(mu);
  if (override_snap) {
    remember_text(override_snap->text);
    return *override_snap;
  }
  auto snap = read_os_clipboard();
  remember_text(snap.text);
  return snap;
}

std::vector<std::string> clipboard_history_texts() {
  std::lock_guard<std::mutex> lock(mu);
  return history_ring;
}

static bool looks_like_fs_path(const std::string& s) {
  if (s.size() < 2) return false;
  if (s[0] == '/' || s[0] == '~') return true;
  if (s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' &&
      (s[2] == '\\' || s[2] == '/'))
    return true;
  if (s.rfind("\\\\", 0) == 0) return true;
  return file_exists(s);
}

std::vector<std::string> clipboard_path_hints(const ClipboardSnapshot& snap) {
  std::vector<std::string> out = snap.paths;
  std::string cur;
  auto flush = [&] {
    while (!cur.empty() && (cur.back() == '"' || cur.back() == '\'' || cur.back() == ' '))
      cur.pop_back();
    while (!cur.empty() && (cur.front() == '"' || cur.front() == '\'' || cur.front() == ' '))
      cur.erase(cur.begin());
    if (cur.size() >= 2 && looks_like_fs_path(cur)) {
      bool dup = false;
      for (auto& e : out)
        if (e == cur) dup = true;
      if (!dup) out.push_back(cur);
    }
    cur.clear();
  };
  for (char c : snap.text) {
    if (c == '\n' || c == '\r' || c == '\t')
      flush();
    else
      cur.push_back(c);
  }
  flush();
  return out;
}

bool clipboard_text_matches(const std::string& query, const std::string& text) {
  auto q = fold_search(normalize_query(query));
  if (q.size() < 2 || text.empty()) return false;
  auto folded = fold_search(text);
  if (folded.find(q) != std::string::npos) return true;
  auto toks = tokenize_name(q);
  bool any = false;
  for (auto& tok : toks) {
    if (tok.size() < 2) continue;
    any = true;
    if (folded.find(tok) == std::string::npos) return false;
  }
  return any;
}

std::string clipboard_preview(const std::string& text, std::size_t max_chars) {
  std::string show = text;
  for (char& c : show)
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  if (show.size() > max_chars) show = show.substr(0, max_chars - 3) + "...";
  return show;
}

bool write_clipboard(const std::string& text) {
  {
    std::lock_guard<std::mutex> lock(mu);
    if (override_snap) {
      override_snap->text = text;
      remember_text(text);
      return true;
    }
  }
  return write_os_clipboard(text);
}

}  // namespace wilfred
