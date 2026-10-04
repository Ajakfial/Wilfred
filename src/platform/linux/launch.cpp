#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/platform/platform.hpp"

#include <cctype>
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
#ifdef WILFRED_BSD
  // No systemd/logind on BSD: lock via screensavers, power via shutdown(8).
  // Logout needs a session-manager hook and stays unsupported. Sleep is
  // per-OS below (acpiconf on FreeBSD/DragonFly, zzz on OpenBSD, ACPI
  // sleep state on NetBSD).
  if (id == "lock") {
    return sys("xdg-screensaver lock >/dev/null 2>&1") ||
           sys("xscreensaver-command -lock >/dev/null 2>&1");
  }
  if (id == "sleep") {
#if defined(__FreeBSD__)
    return sys("acpiconf -s 3 >/dev/null 2>&1");
#elif defined(__OpenBSD__)
    return sys("zzz >/dev/null 2>&1");
#elif defined(__NetBSD__)
    // suspend-to-RAM via the ACPI sleep state (NetBSD power mgmt guide).
    return sys("sysctl -w hw.acpi.sleep.state=3 >/dev/null 2>&1");
#elif defined(__DragonFly__)
    return sys("acpiconf -s 3 >/dev/null 2>&1");
#else
    return false;
#endif
  }
  if (id == "shutdown") return sys("shutdown -p now >/dev/null 2>&1 &");
  if (id == "restart") return sys("shutdown -r now >/dev/null 2>&1 &");
  if (id == "logout") return false;
  // empty_trash falls through to the shared gio/rm implementation below.
#endif
  if (id == "lock") {
    return sys("loginctl lock-session >/dev/null 2>&1") ||
           sys("xdg-screensaver lock >/dev/null 2>&1") ||
           sys("xscreensaver-command -lock >/dev/null 2>&1") ||
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

namespace {

std::string lin_exec(const std::string& cmd) {
  std::string full = cmd + " 2>/dev/null";
  FILE* f = popen(full.c_str(), "r");
  if (!f) return {};
  std::string out;
  char buf[1024];
  while (fgets(buf, sizeof(buf), f)) {
    out += buf;
    if (out.size() > 32768) break;
  }
  pclose(f);
  return out;
}

std::string lin_lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string lin_trim(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' ||
                        s.front() == '\r'))
    s.erase(s.begin());
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
  return s;
}

}  // namespace

bool native_wifi_status(bool& enabled, std::string& detail, std::string& error) {
#ifdef WILFRED_BSD
  error = "Wi-Fi control is not supported on this BSD yet (use ifconfig)";
  return false;
#else
  if (have_tool("nmcli")) {
    auto out = lin_trim(lin_lower(lin_exec("nmcli -t -f WIFI g")));
    if (out == "enabled" || out == "enable") {
      enabled = true;
    } else if (out == "disabled" || out == "disable") {
      enabled = false;
    } else if (!out.empty()) {
      enabled = out.find("disab") == std::string::npos;
    } else {
      error = "nmcli did not report Wi-Fi state";
      return false;
    }
    std::string ssid = lin_trim(lin_exec("nmcli -t -f ACTIVE,SSID dev wifi | grep '^yes:' | cut -d: -f2 | head -n 1"));
    if (ssid.empty()) ssid = lin_trim(lin_exec("iwgetid -r"));
    detail = ssid.empty() ? "nmcli" : ssid;
    return true;
  }
  // rfkill fallback (no SSID).
  auto rf = lin_exec("rfkill list wifi");
  if (rf.empty()) {
    error = "no nmcli or rfkill found (install NetworkManager)";
    return false;
  }
  auto low = lin_lower(rf);
  enabled = low.find("soft blocked: yes") == std::string::npos;
  detail = "rfkill";
  return true;
#endif
}

bool native_wifi_set(bool enabled, std::string& error) {
#ifdef WILFRED_BSD
  error = "Wi-Fi control is not supported on this BSD yet (use ifconfig)";
  return false;
#else
  if (have_tool("nmcli")) {
    if (sys(std::string("nmcli radio wifi ") + (enabled ? "on" : "off") + " >/dev/null 2>&1"))
      return true;
  }
  if (have_tool("rfkill")) {
    if (sys(std::string("rfkill ") + (enabled ? "unblock" : "block") + " wifi >/dev/null 2>&1"))
      return true;
  }
  error = "cannot change Wi-Fi (need nmcli/rfkill + permissions)";
  return false;
#endif
}

std::vector<std::string> native_wifi_list(std::string& error) {
  std::vector<std::string> out;
#ifdef WILFRED_BSD
  error = "Wi-Fi scan is not supported on this BSD yet";
  return out;
#else
  if (!have_tool("nmcli")) {
    error = "nmcli not found";
    return out;
  }
  auto txt = lin_exec("nmcli -t -f SSID,SIGNAL dev wifi list --rescan no 2>/dev/null | head -n 20");
  std::string cur;
  for (char c : txt + "\n") {
    if (c == '\n') {
      auto p = cur.find(':');
      std::string ssid = p == std::string::npos ? cur : cur.substr(0, p);
      ssid = lin_trim(ssid);
      if (!ssid.empty() && ssid != "--") out.push_back(ssid);
      if (out.size() >= 8) break;
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (out.empty()) error = "no networks found";
  return out;
#endif
}

bool native_bluetooth_status(bool& enabled, std::string& detail, std::string& error) {
#ifdef WILFRED_BSD
  error = "Bluetooth control is not supported on this BSD yet";
  return false;
#else
  if (have_tool("bluetoothctl")) {
    auto out = lin_lower(lin_exec("bluetoothctl show"));
    auto p = out.find("powered:");
    if (p != std::string::npos) {
      auto v = out.substr(p + 8, 8);
      enabled = v.find("yes") != std::string::npos;
      detail = "bluetoothctl";
      return true;
    }
  }
  auto rf = lin_exec("rfkill list bluetooth");
  if (!rf.empty()) {
    auto low = lin_lower(rf);
    enabled = low.find("soft blocked: yes") == std::string::npos;
    detail = "rfkill";
    return true;
  }
  error = "no bluetoothctl or rfkill found (install bluez)";
  return false;
#endif
}

bool native_bluetooth_set(bool enabled, std::string& error) {
#ifdef WILFRED_BSD
  error = "Bluetooth control is not supported on this BSD yet";
  return false;
#else
  if (have_tool("bluetoothctl")) {
    if (sys(std::string("bluetoothctl power ") + (enabled ? "on" : "off") +
            " >/dev/null 2>&1")) {
      // Verify best-effort.
      bool cur = false;
      std::string detail;
      std::string ignore;
      if (native_bluetooth_status(cur, detail, ignore) && cur == enabled) return true;
      return true;
    }
  }
  if (have_tool("rfkill")) {
    if (sys(std::string("rfkill ") + (enabled ? "unblock" : "block") + " bluetooth >/dev/null 2>&1"))
      return true;
  }
  error = "cannot change Bluetooth (need bluetoothctl/rfkill + permissions)";
  return false;
#endif
}

bool native_volume_status(int& level, bool& muted, std::string& error) {
  level = -1;
  muted = false;
#ifdef WILFRED_BSD
  // OSS mixer: `mixer vol` prints "vol.volume=0.75:0.75".
  auto out = lin_exec("mixer vol");
  if (!out.empty()) {
    auto p = out.find('=');
    if (p != std::string::npos) {
      try {
        float f = std::stof(out.substr(p + 1));
        level = static_cast<int>(f * 100 + 0.5f);
        muted = level == 0;
        return true;
      } catch (...) {
      }
    }
  }
  error = "cannot read volume (no mixer found)";
  return false;
#else
  if (have_tool("wpctl")) {
    auto out = lin_lower(lin_exec("wpctl get-volume @DEFAULT_AUDIO_SINK@"));
    // "Volume: 0.50 [MUTED]" or "Volume: 0.50".
    auto p = out.find("volume:");
    if (p != std::string::npos) {
      try {
        float f = std::stof(out.substr(p + 7));
        level = static_cast<int>(f * 100 + 0.5f);
        muted = out.find("muted") != std::string::npos;
        return true;
      } catch (...) {
      }
    }
  }
  if (have_tool("pactl")) {
    auto vol = lin_exec("pactl get-sink-volume @DEFAULT_SINK@");
    auto mute = lin_lower(lin_exec("pactl get-sink-mute @DEFAULT_SINK@"));
    // "... / 50% / ..." — take the first NN%.
    int pct = -1;
    for (std::size_t i = 0; i + 1 < vol.size(); ++i) {
      if (vol[i] == '%' && i > 0) {
        std::size_t j = i;
        while (j > 0 && (std::isdigit(static_cast<unsigned char>(vol[j - 1])) ||
                         vol[j - 1] == ' '))
          --j;
        std::string num;
        for (std::size_t k = j; k < i; ++k)
          if (std::isdigit(static_cast<unsigned char>(vol[k]))) num.push_back(vol[k]);
        if (!num.empty()) {
          try {
            pct = std::stoi(num);
          } catch (...) {
          }
          break;
        }
      }
    }
    if (pct >= 0) {
      level = pct;
      muted = mute.find("yes") != std::string::npos;
      return true;
    }
  }
  if (have_tool("amixer")) {
    auto out = lin_exec("amixer -M sget Master");
    int pct = -1;
    bool off = false;
    for (std::size_t i = 0; i + 1 < out.size(); ++i) {
      if (out[i] == '%' ) {
        std::size_t j = i;
        while (j > 0 && std::isdigit(static_cast<unsigned char>(out[j - 1]))) --j;
        try {
          pct = std::stoi(out.substr(j, i - j));
        } catch (...) {
        }
        break;
      }
    }
    auto low = lin_lower(out);
    off = low.find("[off]") != std::string::npos;
    if (pct >= 0) {
      level = pct;
      muted = off;
      return true;
    }
  }
  error = "cannot read volume (tried wpctl, pactl, amixer)";
  return false;
#endif
}

bool native_volume_set(int level, std::string& error) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
#ifdef WILFRED_BSD
  if (have_tool("mixer")) {
    float f = static_cast<float>(level) / 100.0f;
    char cmd[128];
    std::snprintf(cmd, sizeof(cmd), "mixer vol %.2f >/dev/null 2>&1", static_cast<double>(f));
    if (sys(cmd)) return true;
  }
  error = "cannot set volume (no mixer found)";
  return false;
#else
  if (have_tool("wpctl")) {
    char cmd[160];
    std::snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%% >/dev/null 2>&1",
                  level);
    if (sys(cmd)) {
      // Ensure unmuted when setting a positive level.
      if (level > 0) sys("wpctl set-mute @DEFAULT_AUDIO_SINK@ 0 >/dev/null 2>&1");
      return true;
    }
  }
  if (have_tool("pactl")) {
    char cmd[160];
    std::snprintf(cmd, sizeof(cmd), "pactl set-sink-volume @DEFAULT_SINK@ %d%% >/dev/null 2>&1",
                  level);
    if (sys(cmd)) {
      if (level > 0) sys("pactl set-sink-mute @DEFAULT_SINK@ 0 >/dev/null 2>&1");
      return true;
    }
  }
  if (have_tool("amixer")) {
    char cmd[160];
    std::snprintf(cmd, sizeof(cmd), "amixer -q -M sset Master %d%% unmute >/dev/null 2>&1", level);
    if (sys(cmd)) return true;
  }
  if (have_tool("pamixer")) {
    char cmd[160];
    std::snprintf(cmd, sizeof(cmd), "pamixer --set-volume %d --unmute >/dev/null 2>&1", level);
    if (sys(cmd)) return true;
  }
  error = "cannot set volume (tried wpctl, pactl, amixer, pamixer)";
  return false;
#endif
}

bool native_volume_mute(bool mute, std::string& error) {
#ifdef WILFRED_BSD
  if (have_tool("mixer")) {
    if (mute) {
      if (sys("mixer vol 0 >/dev/null 2>&1")) return true;
    } else {
      if (sys("mixer vol 0.5 >/dev/null 2>&1")) return true;
    }
  }
  error = "cannot change mute (no mixer found)";
  return false;
#else
  if (have_tool("wpctl")) {
    if (sys(std::string("wpctl set-mute @DEFAULT_AUDIO_SINK@ ") + (mute ? "1" : "0") +
            " >/dev/null 2>&1"))
      return true;
  }
  if (have_tool("pactl")) {
    if (sys(std::string("pactl set-sink-mute @DEFAULT_SINK@ ") + (mute ? "1" : "0") +
            " >/dev/null 2>&1"))
      return true;
  }
  if (have_tool("amixer")) {
    if (sys(std::string("amixer -q sset Master ") + (mute ? "mute" : "unmute") +
            " >/dev/null 2>&1"))
      return true;
  }
  if (have_tool("pamixer")) {
    if (sys(std::string("pamixer ") + (mute ? "--mute" : "--unmute") + " >/dev/null 2>&1"))
      return true;
  }
  error = "cannot change mute (tried wpctl, pactl, amixer, pamixer)";
  return false;
#endif
}

bool native_brightness_status(int& percent, std::string& error) {
  percent = -1;
  if (have_tool("brightnessctl")) {
    auto cur = lin_trim(lin_exec("brightnessctl get"));
    auto max = lin_trim(lin_exec("brightnessctl max"));
    try {
      int c = std::stoi(cur);
      int m = std::stoi(max);
      if (m > 0) {
        percent = static_cast<int>(c * 100 / m);
        return true;
      }
    } catch (...) {
    }
  }
  if (have_tool("light")) {
    auto out = lin_trim(lin_exec("light -G"));
    try {
      percent = static_cast<int>(std::stof(out) + 0.5f);
      return true;
    } catch (...) {
    }
  }
  if (have_tool("xbacklight")) {
    auto out = lin_trim(lin_exec("xbacklight -get"));
    try {
      percent = static_cast<int>(std::stof(out) + 0.5f);
      return true;
    } catch (...) {
    }
  }
  // sysfs fallback: first backlight device.
  auto devs = lin_exec("ls /sys/class/backlight 2>/dev/null");
  {
    std::string name;
    for (char c : devs) {
      if (c == '\n' || c == ' ' || c == '\t') {
        if (!name.empty()) break;
      } else {
        name.push_back(c);
      }
    }
    // Re-list to get the first token cleanly.
    name.clear();
    for (char c : devs) {
      if (c == '\n' || c == ' ' || c == '\t' || c == '\r') break;
      name.push_back(c);
    }
    if (!name.empty()) {
      auto cur = lin_trim(lin_exec("cat /sys/class/backlight/" + name + "/brightness"));
      auto max = lin_trim(lin_exec("cat /sys/class/backlight/" + name + "/max_brightness"));
      try {
        int c = std::stoi(cur);
        int m = std::stoi(max);
        if (m > 0) {
          percent = static_cast<int>(c * 100 / m);
          return true;
        }
      } catch (...) {
      }
    }
  }
  error = "cannot read brightness (need brightnessctl, light, xbacklight, or /sys/class/backlight)";
  return false;
}

bool native_brightness_set(int percent, std::string& error) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  if (have_tool("brightnessctl")) {
    if (sys("brightnessctl set " + std::to_string(percent) + "% >/dev/null 2>&1")) return true;
  }
  if (have_tool("light")) {
    if (sys("light -S " + std::to_string(percent) + " >/dev/null 2>&1")) return true;
  }
  if (have_tool("xbacklight")) {
    if (sys("xbacklight -set " + std::to_string(percent) + " >/dev/null 2>&1")) return true;
  }
  // sysfs direct write (needs permissions on the brightness file).
  auto devs = lin_exec("ls /sys/class/backlight 2>/dev/null");
  std::string name;
  for (char c : devs) {
    if (c == '\n' || c == ' ' || c == '\t' || c == '\r') break;
    name.push_back(c);
  }
  if (!name.empty()) {
    auto maxs = lin_trim(lin_exec("cat /sys/class/backlight/" + name + "/max_brightness"));
    try {
      int m = std::stoi(maxs);
      int v = static_cast<int>(percent * m / 100);
      if (sys("echo " + std::to_string(v) + " > /sys/class/backlight/" + name +
              "/brightness 2>/dev/null"))
        return true;
    } catch (...) {
    }
  }
  error = "cannot set brightness (need brightnessctl, light, xbacklight, or writable sysfs)";
  return false;
}

bool native_open_settings(const std::string& page, std::string& error) {
  auto try_cmd = [](const std::string& c) { return sys(c + " >/dev/null 2>&1 &"); };
  // GNOME.
  if (have_tool("gnome-control-center")) {
    std::string arg;
    if (page.empty()) arg = "";
    else if (page == "wifi") arg = " wifi";
    else if (page == "network") arg = " network";
    else if (page == "bluetooth") arg = " bluetooth";
    else if (page == "sound") arg = " sound";
    else if (page == "display") arg = " display";
    else if (page == "battery" || page == "power") arg = " power";
    else if (page == "apps") arg = " applications";
    else if (page == "privacy") arg = " privacy";
    else if (page == "update") arg = " info-overview";
    else if (page == "about") arg = " info-overview";
    else {
      error = "unknown settings page '" + page + "'";
      return false;
    }
    if (try_cmd("gnome-control-center" + arg)) return true;
  }
  // KDE.
  if (have_tool("systemsettings") || have_tool("systemsettings5")) {
    std::string bin = have_tool("systemsettings") ? "systemsettings" : "systemsettings5";
    (void)page;
    if (try_cmd(bin)) return true;
  }
  if (have_tool("xfce4-settings-manager")) {
    if (try_cmd("xfce4-settings-manager")) return true;
  }
  error = "no settings app found (need gnome-control-center, systemsettings, or xfce4-settings-manager)";
  return false;
}

#endif
}  // namespace wilfred
