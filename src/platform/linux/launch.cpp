#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)
namespace fs = std::filesystem;

namespace {

bool have_tool(const char* name) {
  std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

std::string sh_quote(const std::string& s) {
  std::string o = "'";
  for (char c : s) {
    if (c == '\'')
      o += "'\\''";
    else
      o.push_back(c);
  }
  o.push_back('\'');
  return o;
}

std::string dquote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

std::string ext_mime(const std::string& path) {
  auto ext = path_extension(path);
  if (ext.empty() || ext[0] != '.') return {};
  ext = ext.substr(1);
  if (ext == "txt" || ext == "md" || ext == "markdown" || ext == "log" || ext == "ini" ||
      ext == "cfg" || ext == "conf" || ext == "yml" || ext == "yaml" || ext == "toml" ||
      ext == "csv" || ext == "tsv" || ext == "py" || ext == "c" || ext == "h" || ext == "cpp" ||
      ext == "hpp" || ext == "rs" || ext == "js" || ext == "ts" || ext == "java" || ext == "go" ||
      ext == "sh" || ext == "css" || ext == "tex")
    return "text/plain";
  if (ext == "json") return "application/json";
  if (ext == "html" || ext == "htm") return "text/html";
  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" ||
      ext == "svg" || ext == "webp" || ext == "ico" || ext == "tif" || ext == "tiff")
    return "image/*";
  if (ext == "mp3" || ext == "wav" || ext == "ogg" || ext == "flac" || ext == "m4a")
    return "audio/*";
  if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "webm")
    return "video/*";
  if (ext == "pdf") return "application/pdf";
  if (ext == "zip") return "application/zip";
  if (ext == "tar" || ext == "gz" || ext == "tgz" || ext == "bz2" || ext == "xz" || ext == "7z" ||
      ext == "rar")
    return "application/x-archive";
  return {};
}

struct DesktopEntry {
  std::string id;  // desktop file id, e.g. "org.gnome.gedit.desktop"
  std::string path;
  std::string name;
  std::string exec;
  std::vector<std::string> mimes;
  bool no_display{false};
};

std::string trim_ws(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
  std::size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return s.substr(i);
}

std::vector<std::string> xdg_app_dirs() {
  std::vector<std::string> out;
  if (const char* home = std::getenv("XDG_DATA_HOME"); home && *home) {
    out.push_back(path_join(home, "applications"));
  } else {
    out.push_back(path_join(home_directory(), ".local/share/applications"));
  }
  std::string dirs;
  if (const char* d = std::getenv("XDG_DATA_DIRS"); d && *d)
    dirs = d;
  else
    dirs = "/usr/local/share:/usr/share";
  std::string cur;
  for (char c : dirs + ":") {
    if (c == ':') {
      if (!cur.empty()) out.push_back(path_join(cur, "applications"));
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  return out;
}

const std::vector<DesktopEntry>& desktop_db() {
  static std::vector<DesktopEntry> db;
  static std::once_flag once;
  std::call_once(once, [] {
    for (auto& dir : xdg_app_dirs()) {
      std::error_code ec;
      if (!fs::is_directory(fs::u8path(dir), ec)) continue;
      for (auto it = fs::directory_iterator(fs::u8path(dir), ec);
           it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        auto p = it->path();
        if (p.extension() != fs::path(".desktop")) continue;
        std::error_code ec2;
        if (!it->is_regular_file(ec2)) continue;
        FILE* f = std::fopen(p.c_str(), "r");
        if (!f) continue;
        DesktopEntry e;
        {
          auto fn = p.filename().u8string();
          e.id = std::string(fn.begin(), fn.end());
          auto u8p = p.u8string();
          e.path = std::string(u8p.begin(), u8p.end());
        }
        bool in_entry = false;
        char line[2048];
        while (std::fgets(line, sizeof(line), f)) {
          std::string s = trim_ws(line);
          if (s.empty() || s[0] == '#') continue;
          if (s[0] == '[') {
            in_entry = s == "[Desktop Entry]";
            continue;
          }
          if (!in_entry) continue;
          auto eq = s.find('=');
          if (eq == std::string::npos) continue;
          auto key = s.substr(0, eq);
          auto val = s.substr(eq + 1);
          if (key == "Name" && e.name.empty())
            e.name = val;
          else if (key == "Exec" && e.exec.empty())
            e.exec = val;
          else if (key == "MimeType") {
            std::string cur_m;
            for (char c : val + ";") {
              if (c == ';') {
                if (!cur_m.empty()) e.mimes.push_back(cur_m);
                cur_m.clear();
              } else {
                cur_m.push_back(c);
              }
            }
          } else if (key == "NoDisplay" || key == "Hidden") {
            if (val == "true" || val == "1") e.no_display = true;
          }
        }
        std::fclose(f);
        if (!e.name.empty() && !e.exec.empty()) db.push_back(std::move(e));
      }
    }
  });
  return db;
}

// Preferred default app ids for a mime from mimeapps.list, in order.
std::vector<std::string> mime_defaults(const std::string& mime) {
  std::vector<std::string> out;
  auto cfg = path_join(config_directory(), "mimeapps.list");
  FILE* f = std::fopen(cfg.c_str(), "r");
  if (!f) return out;
  bool in_defaults = false;
  char line[1024];
  while (std::fgets(line, sizeof(line), f)) {
    std::string s = trim_ws(line);
    if (s.empty() || s[0] == '#') continue;
    if (s[0] == '[') {
      in_defaults = s == "[Default Applications]";
      continue;
    }
    if (!in_defaults) continue;
    auto eq = s.find('=');
    if (eq == std::string::npos) continue;
    if (s.substr(0, eq) != mime) continue;
    std::string cur;
    for (char c : s.substr(eq + 1) + ";") {
      if (c == ';') {
        if (!cur.empty()) out.push_back(cur);
        cur.clear();
      } else {
        cur.push_back(c);
      }
    }
  }
  std::fclose(f);
  return out;
}

bool mime_matches(const std::string& claimed, const std::string& want) {
  if (claimed == want) return true;
  auto slash = claimed.find('/');
  if (slash != std::string::npos && slash + 1 < claimed.size() && claimed[slash + 1] == '*')
    return want.rfind(claimed.substr(0, slash), 0) == 0;
  return false;
}

std::string expand_exec(const std::string& exec, const std::string& file) {
  std::string quoted = sh_quote(file);
  std::string o;
  for (std::size_t i = 0; i < exec.size(); ++i) {
    if (exec[i] == '%' && i + 1 < exec.size()) {
      char c = exec[i + 1];
      if (c == 'f' || c == 'F' || c == 'u' || c == 'U') {
        o += quoted;
        ++i;
        continue;
      }
      if (c == '%' || c == 'i' || c == 'c' || c == 'k' || c == 'd' || c == 'D' || c == 'n' ||
          c == 'N' || c == 'v' || c == 'm') {
        if (c == '%') o.push_back('%');
        ++i;
        continue;
      }
    }
    o.push_back(exec[i]);
  }
  return o;
}

}  // namespace

static bool sys(const std::string& cmd) { return std::system(cmd.c_str()) == 0; }

bool native_launch(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_reveal(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_open_url(const std::string& url) {
  std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_system_action(const std::string& id) {
  if (id == "lock") {
    return sys("loginctl lock-session >/dev/null 2>&1") ||
           sys("xdg-screensaver lock >/dev/null 2>&1") ||
           sys("gnome-screensaver-command -l >/dev/null 2>&1") ||
           sys("dm-tool lock >/dev/null 2>&1");
  }
  if (id == "sleep") return sys("systemctl suspend >/dev/null 2>&1 &") ||
                            sys("loginctl suspend >/dev/null 2>&1 &");
  if (id == "shutdown") return sys("systemctl poweroff >/dev/null 2>&1 &");
  if (id == "restart") return sys("systemctl reboot >/dev/null 2>&1 &");
  if (id == "logout")
    return sys("loginctl terminate-session \"$XDG_SESSION_ID\" >/dev/null 2>&1 &") ||
           sys("gnome-session-quit --logout --no-prompt >/dev/null 2>&1 &");
  if (id == "empty_trash")
    return sys("gio trash --empty >/dev/null 2>&1") ||
           sys("rm -rf \"$HOME/.local/share/Trash/files/\"* \"$HOME/.local/share/Trash/info/\"* "
               ">/dev/null 2>&1");
  return false;
}

std::vector<OpenWithApp> native_apps_for_file(const std::string& path, std::size_t max_apps) {
  std::vector<OpenWithApp> out;
  if (max_apps == 0 || path.empty()) return out;
  auto mime = ext_mime(path);
  if (mime.empty()) return out;
  const auto& db = desktop_db();
  auto push_entry = [&](const DesktopEntry& e) {
    if (out.size() >= max_apps || e.no_display) return;
    for (auto& o : out)
      if (o.name == e.name) return;
    OpenWithApp a;
    a.name = e.name;
    a.target = e.path;
    out.push_back(std::move(a));
  };
  // Honor the user's default first.
  for (auto& id : mime_defaults(mime)) {
    for (auto& e : db) {
      if (e.id != id) continue;
      bool ok = false;
      for (auto& m : e.mimes)
        if (mime_matches(m, mime)) {
          ok = true;
          break;
        }
      if (ok || e.mimes.empty()) push_entry(e);
    }
  }
  for (auto& e : db) {
    if (out.size() >= max_apps) break;
    for (auto& m : e.mimes) {
      if (mime_matches(m, mime)) {
        push_entry(e);
        break;
      }
    }
  }
  return out;
}

bool native_open_with(const std::string& target, const std::string& file) {
  if (target.empty() || file.empty()) return false;
  std::string exec;
  {
    FILE* f = std::fopen(target.c_str(), "r");
    if (!f) return false;
    bool in_entry = false;
    char line[2048];
    while (std::fgets(line, sizeof(line), f)) {
      std::string s = trim_ws(line);
      if (s.empty() || s[0] == '#') continue;
      if (s[0] == '[') {
        in_entry = s == "[Desktop Entry]";
        continue;
      }
      if (!in_entry) continue;
      if (s.rfind("Exec=", 0) == 0) {
        exec = s.substr(5);
        break;
      }
    }
    std::fclose(f);
  }
  if (exec.empty()) return false;
  std::string cmd = "(" + expand_exec(exec, file) + ") >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_open_terminal(const std::string& dir) {
  if (dir.empty()) return false;
  std::string q = dquote(dir);
  struct Cand {
    const char* bin;
    const char* flag;  // nullptr => rely on inherited cwd
  };
  static const Cand cands[] = {{"gnome-terminal", "--working-directory="},
                               {"konsole", "--workdir "},
                               {"xfce4-terminal", "--working-directory="},
                               {"mate-terminal", "--working-directory="},
                               {"x-terminal-emulator", "--working-directory="},
                               {"kitty", "--directory "},
                               {"wezterm start", "--cwd "},
                               {"foot", nullptr},
                               {"alacritty", nullptr},
                               {"xterm", nullptr},
                               {nullptr, nullptr}};
  for (auto* c = cands; c->bin; ++c) {
    std::string probe = c->bin;
    auto sp = probe.find(' ');
    if (sp != std::string::npos) probe.resize(sp);
    if (!have_tool(probe.c_str())) continue;
    std::string cmd = "cd " + sh_quote(dir) + " && " + c->bin;
    if (c->flag) cmd += std::string(" ") + c->flag + q;
    cmd += " >/dev/null 2>&1 &";
    if (sys(cmd)) return true;
  }
  return false;
}

bool native_open_editor(const std::string& dir) {
  if (dir.empty()) return false;
  if (have_tool("code") && sys("code --new-window " + dquote(dir) + " >/dev/null 2>&1 &"))
    return true;
  if (have_tool("codium") && sys("codium --new-window " + dquote(dir) + " >/dev/null 2>&1 &"))
    return true;
  return native_launch(dir);
}

#endif
}  // namespace wilfred
