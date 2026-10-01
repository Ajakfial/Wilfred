#include "wilfred/ui/web_ui.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/ui/icon.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#include <cstdlib>
#include <unistd.h>
#else
#include <climits>
#include <unistd.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

std::string overlay_json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c < 0x20)
      continue;
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

const char* overlay_action_name(ResultAction a) {
  switch (a) {
    case ResultAction::Reveal:
      return "reveal";
    case ResultAction::Copy:
      return "copy";
    case ResultAction::WebSearch:
      return "web";
    case ResultAction::Calculate:
      return "calc";
    case ResultAction::Convert:
      return "convert";
    case ResultAction::None:
      return "none";
    case ResultAction::Habit:
      return "habit";
    case ResultAction::Mini:
      return "mini";
    case ResultAction::Expand:
      return "expand";
    case ResultAction::Plugin:
      return "plugin";
    case ResultAction::SwitchWindow:
      return "open";
    case ResultAction::System:
      return "open";
    case ResultAction::Screenshot:
      return "open";
    default:
      return "open";
  }
}

std::string overlay_results_json(const std::vector<SearchResult>& items,
                                 const std::vector<std::string>& habits) {
  std::string o = "{\"type\":\"results\",\"items\":[";
  bool first = true;
  for (auto& it : items) {
    if (!first) o += ',';
    first = false;
    o += "{\"title\":\"";
    o += overlay_json_escape(it.title);
    o += "\",\"subtitle\":\"";
    o += overlay_json_escape(it.subtitle);
    o += "\",\"path\":\"";
    o += overlay_json_escape(it.path);
    o += "\",\"kind\":\"";
    o += overlay_json_escape(it.kind_label.empty() ? std::string(kind_name(it.kind)) : it.kind_label);
    o += "\",\"action\":\"";
    o += overlay_action_name(it.action);
    o += "\",\"category\":\"";
    o += overlay_json_escape(it.category);
    o += "\",\"payload\":\"";
    o += overlay_json_escape(it.payload);
    o += "\"";
    if (!it.plugin_id.empty()) {
      o += ",\"plugin\":\"";
      o += overlay_json_escape(it.plugin_id);
      o += "\"";
    }
    if (!it.actions.empty()) {
      o += ",\"actions\":[";
      bool af = true;
      for (auto& a : it.actions) {
        if (!af) o += ',';
        af = false;
        o += "{\"id\":\"";
        o += overlay_json_escape(a.id);
        o += "\",\"label\":\"";
        o += overlay_json_escape(a.label);
        o += "\"}";
      }
      o += "]";
    }
    if (it.meter >= 0) {
      o += ",\"meter\":";
      o += std::to_string(it.meter);
    }
    if (it.category != "mini" && it.category != "macro" && it.category != "clipboard" &&
        it.category != "window" && it.category != "system" && it.category != "screenshot" &&
        it.action != ResultAction::Copy && it.action != ResultAction::Calculate &&
        it.action != ResultAction::Convert && it.action != ResultAction::WebSearch &&
        it.action != ResultAction::Habit && it.action != ResultAction::SwitchWindow &&
        it.action != ResultAction::System && it.action != ResultAction::Screenshot) {
      auto icon = file_icon_data_url(it.path, it.kind);
      if (!icon.empty()) {
        o += ",\"icon\":\"";
        o += overlay_json_escape(icon);
        o += "\"";
      }
    }
    o += "}";
  }
  o += "],\"habits\":[";
  first = true;
  for (auto& h : habits) {
    if (h.empty()) continue;
    if (!first) o += ',';
    first = false;
    o += "\"";
    o += overlay_json_escape(h);
    o += "\"";
  }
  o += "]}";
  return o;
}

bool overlay_json_field(const std::string& json, const char* key, std::string& out) {  std::string k = std::string("\"") + key + "\"";
  auto pos = json.find(k);
  if (pos == std::string::npos) return false;
  pos = json.find(':', pos + k.size());
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  if (pos >= json.size()) return false;
  if (json[pos] == '"') {
    ++pos;
    out.clear();
    while (pos < json.size() && json[pos] != '"') {
      if (json[pos] == '\\' && pos + 1 < json.size()) {
        out.push_back(json[pos + 1]);
        pos += 2;
      } else
        out.push_back(json[pos++]);
    }
    return true;
  }
  out.clear();
  while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ' ')
    out.push_back(json[pos++]);
  return !out.empty();
}

static std::string exe_directory() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (!n) return ".";
  return path_parent(wide_to_utf8(std::wstring(buf, n)));
#elif defined(__APPLE__)
  char buf[1024];
  uint32_t sz = sizeof(buf);
  if (_NSGetExecutablePath(buf, &sz) != 0) return ".";
  char real[PATH_MAX];
  if (!realpath(buf, real)) return path_parent(buf);
  return path_parent(real);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  buf[n] = 0;
  return path_parent(buf);
#endif
}

std::string overlay_ui_dir() {
  std::error_code ec;
  std::vector<std::string> roots = {exe_directory()};
  auto cwd = fs::current_path(ec);
  if (!ec) roots.push_back(cwd.string());
  auto cur = exe_directory();
  for (int i = 0; i < 4; ++i) {
    cur = path_parent(cur);
    if (cur.empty()) break;
    roots.push_back(cur);
  }
  for (auto& root : roots) {
    if (root.empty()) continue;
    auto dir = path_join(path_join(root, "ui"), "overlay");
    auto html = path_join(dir, "index.html");
    if (fs::exists(fs::u8path(html), ec)) return dir;
  }
  return path_join(path_join(exe_directory(), "ui"), "overlay");
}

namespace {

std::string preview_b64(const std::string& bytes) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve(((bytes.size() + 2) / 3) * 4);
  std::size_t i = 0;
  auto* d = reinterpret_cast<const unsigned char*>(bytes.data());
  while (i + 2 < bytes.size()) {
    unsigned n = (static_cast<unsigned>(d[i]) << 16) | (static_cast<unsigned>(d[i + 1]) << 8) |
                 d[i + 2];
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(tbl[(n >> 6) & 63]);
    o.push_back(tbl[n & 63]);
    i += 3;
  }
  if (i < bytes.size()) {
    unsigned n = static_cast<unsigned>(d[i]) << 16;
    if (i + 1 < bytes.size()) n |= static_cast<unsigned>(d[i + 1]) << 8;
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(i + 1 < bytes.size() ? tbl[(n >> 6) & 63] : '=');
    o.push_back('=');
  }
  return o;
}

std::string preview_human_bytes(std::uint64_t n) {
  const char* u[] = {"B", "KB", "MB", "GB", "TB"};
  double v = static_cast<double>(n);
  int i = 0;
  while (v >= 1024.0 && i < 4) {
    v /= 1024.0;
    ++i;
  }
  char buf[48];
  if (i == 0)
    std::snprintf(buf, sizeof(buf), "%llu %s", static_cast<unsigned long long>(n), u[i]);
  else
    std::snprintf(buf, sizeof(buf), "%.1f %s", v, u[i]);
  return buf;
}

std::string preview_mtime(std::int64_t unix) {
  if (unix <= 0) return {};
  std::time_t t = static_cast<std::time_t>(unix);
  std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  char buf[32];
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmv) == 0) return {};
  return buf;
}

// Keep printable text; drop NULs/control bytes so JSON stays sane.
std::string preview_scrub(std::string s, std::size_t cap) {
  std::string o;
  o.reserve(std::min(s.size(), cap));
  for (unsigned char c : s) {
    if (c == '\n' || c == '\t' || (c >= 32 && c < 127) || c >= 128)
      o.push_back(static_cast<char>(c));
    else if (c == '\r')
      o.push_back('\n');
    if (o.size() >= cap) break;
  }
  return o;
}

const char* sniff_image_mime(const std::string& head) {
  auto u = reinterpret_cast<const unsigned char*>(head.data());
  auto n = head.size();
  if (n >= 8 && u[0] == 0x89 && u[1] == 'P' && u[2] == 'N' && u[3] == 'G') return "image/png";
  if (n >= 3 && u[0] == 0xFF && u[1] == 0xD8 && u[2] == 0xFF) return "image/jpeg";
  if (n >= 6 && u[0] == 'G' && u[1] == 'I' && u[2] == 'F') return "image/gif";
  if (n >= 12 && u[0] == 'R' && u[1] == 'I' && u[2] == 'F' && u[3] == 'F' && u[8] == 'W' &&
      u[9] == 'E' && u[10] == 'B' && u[11] == 'P')
    return "image/webp";
  if (n >= 2 && u[0] == 'B' && u[1] == 'M') return "image/bmp";
  if (n >= 4 && u[0] == 0 && u[1] == 0 && u[2] == 1 && u[3] == 0) return "image/x-icon";
  return nullptr;
}

}  // namespace

FilePreview build_file_preview(const std::string& path, std::size_t max_text,
                               std::size_t max_image) {
  FilePreview pv;
  pv.title = path_filename(path);
  if (pv.title.empty()) pv.title = path;
  if (path.empty() || !file_exists(path)) {
    pv.error = "No such file";
    return pv;
  }
  auto st = stat_path(path);
  pv.kind = std::string(kind_name(classify_path(path, st.is_dir, st.is_exec)));
  pv.size_label = st.is_dir ? "" : preview_human_bytes(st.size);
  pv.modified_label = preview_mtime(st.mtime);
  if (st.is_dir) {
    std::error_code ec;
    std::size_t shown = 0, total = 0;
    std::string list;
    for (auto it = fs::directory_iterator(fs::u8path(path), ec);
         it != fs::directory_iterator() && shown < 20; it.increment(ec)) {
      if (ec) break;
      ++total;
      auto fn_u8 = it->path().filename().u8string();
      std::string fn(fn_u8.begin(), fn_u8.end());
      std::error_code ec2;
      bool dir = it->is_directory(ec2);
      if (!list.empty()) list.push_back('\n');
      list += dir ? fn + "/" : fn;
      ++shown;
    }
    // Count the rest cheaply for the header line.
    if (shown == 20) {
      for (auto it = fs::directory_iterator(fs::u8path(path), ec);
           it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        ++total;
        if (total > 100000) break;
      }
    }
    pv.text = std::to_string(total) + (total == 1 ? " item\n" : " items\n") + list;
    return pv;
  }
  if (st.size > 0 && st.size <= static_cast<std::uint64_t>(max_image)) {
    std::string bytes;
    if (read_file_all(path, bytes) && bytes.size() == st.size) {
      if (auto mime = sniff_image_mime(bytes)) {
        pv.image_data_url = std::string("data:") + mime + ";base64," + preview_b64(bytes);
        return pv;
      }
    }
  }
  // Text head: read a bounded window, bail on binary.
  {
    std::string bytes;
    if (!read_file_all(path, bytes)) {
      pv.error = "Unreadable file";
      return pv;
    }
    std::size_t window = std::min(bytes.size(), max_text * 4 + 512);
    std::string_view head(bytes.data(), window);
    if (looks_binary(head)) {
      pv.text = "";
      return pv;
    }
    pv.text = preview_scrub(std::string(head), max_text);
  }
  return pv;
}

std::string overlay_preview_json(const std::string& path) {
  auto pv = build_file_preview(path);
  std::string o = "{\"type\":\"preview\",\"title\":\"";
  o += overlay_json_escape(pv.title);
  o += "\",\"kind\":\"";
  o += overlay_json_escape(pv.kind);
  o += "\",\"size\":\"";
  o += overlay_json_escape(pv.size_label);
  o += "\",\"modified\":\"";
  o += overlay_json_escape(pv.modified_label);
  o += "\",\"text\":\"";
  o += overlay_json_escape(pv.text);
  o += "\",\"image\":\"";
  o += overlay_json_escape(pv.image_data_url);
  o += "\",\"error\":\"";
  o += overlay_json_escape(pv.error);
  o += "\"}";
  return o;
}

}  // namespace wilfred
