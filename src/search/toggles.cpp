#include "wilfred/search/toggles.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/search/setup.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace wilfred {
namespace {

std::string trim(const std::string& s) {
  std::string o = s;
  while (!o.empty() && (o.front() == ' ' || o.front() == '\t'))
    o.erase(o.begin());
  while (!o.empty() && (o.back() == ' ' || o.back() == '\t' || o.back() == '\r'))
    o.pop_back();
  return o;
}

SearchResult tcard(const std::string& title, const std::string& sub, const std::string& payload,
                   const std::string& label, const std::string& category, int meter = -1) {
  SearchResult r;
  r.title = title;
  r.subtitle = sub;
  r.payload = payload;
  r.path = payload;
  r.action = ResultAction::Mini;
  r.score = 10000;
  r.kind_label = label;
  r.category = category;
  r.meter = meter;
  return r;
}

}  // namespace

ToggleOp parse_toggle_arg(const std::string& remainder) {
  auto l = to_lower_utf8(trim(remainder));
  if (l.empty() || l == "status" || l == "state" || l == "list" || l == "show")
    return ToggleOp::Status;
  if (l == "on" || l == "enable" || l == "enabled" || l == "1" || l == "yes") return ToggleOp::On;
  if (l == "off" || l == "disable" || l == "disabled" || l == "0" || l == "no")
    return ToggleOp::Off;
  if (l == "toggle" || l == "switch" || l == "flip") return ToggleOp::Toggle;
  return ToggleOp::Status;
}

bool parse_volume_arg(const std::string& remainder, std::string& kind, int& level) {
  kind = "status";
  level = -1;
  auto t = trim(remainder);
  auto l = to_lower_utf8(t);
  if (l.empty() || l == "status" || l == "show" || l == "level") return true;
  if (l == "mute" || l == "muted") {
    kind = "mute";
    return true;
  }
  if (l == "unmute" || l == "unmuted" || l == "un-mute") {
    kind = "unmute";
    return true;
  }
  if (l == "toggle" || l == "toggle mute" || l == "mute toggle") {
    kind = "toggle_mute";
    return true;
  }
  if (l == "up" || l == "+" || l == "louder" || l == "increase" || l == "volup" ||
      l == "volume up") {
    kind = "up";
    return true;
  }
  if (l == "down" || l == "-" || l == "quieter" || l == "decrease" || l == "voldn" ||
      l == "volume down") {
    kind = "down";
    return true;
  }
  // Strip trailing %.
  std::string num = l;
  if (!num.empty() && num.back() == '%') num.pop_back();
  num = trim(num);
  // Allow "set 50", "50", "volume 50".
  for (const char* p : {"set ", "to ", "volume ", "vol "}) {
    std::string pre(p);
    if (num.rfind(pre, 0) == 0) {
      num = trim(num.substr(pre.size()));
      break;
    }
  }
  if (num.empty()) return true;
  // Must be all digits.
  for (char c : num)
    if (!std::isdigit(static_cast<unsigned char>(c))) return true;  // fall back to status
  try {
    int v = std::stoi(num);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    kind = "set";
    level = v;
    return true;
  } catch (...) {
    return true;
  }
}

bool parse_brightness_arg(const std::string& remainder, std::string& kind, int& level) {
  kind = "status";
  level = -1;
  auto t = trim(remainder);
  auto l = to_lower_utf8(t);
  if (l.empty() || l == "status" || l == "show" || l == "level") return true;
  if (l == "up" || l == "+" || l == "brighter" || l == "increase") {
    kind = "up";
    return true;
  }
  if (l == "down" || l == "-" || l == "dimmer" || l == "darker" || l == "decrease") {
    kind = "down";
    return true;
  }
  std::string num = l;
  if (!num.empty() && num.back() == '%') num.pop_back();
  num = trim(num);
  for (const char* p : {"set ", "to "}) {
    std::string pre(p);
    if (num.rfind(pre, 0) == 0) {
      num = trim(num.substr(pre.size()));
      break;
    }
  }
  if (num.empty()) return true;
  for (char c : num)
    if (!std::isdigit(static_cast<unsigned char>(c))) return true;
  try {
    int v = std::stoi(num);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    kind = "set";
    level = v;
    return true;
  } catch (...) {
    return true;
  }
}

std::string normalize_settings_page(const std::string& remainder) {
  auto l = to_lower_utf8(trim(remainder));
  if (l.empty() || l == "open" || l == "show" || l == "main" || l == "app") return {};
  if (l == "wifi" || l == "wi-fi" || l == "wlan" || l == "wireless") return "wifi";
  if (l == "network" || l == "networks" || l == "internet" || l == "ethernet" || l == "vpn")
    return "network";
  if (l == "bluetooth" || l == "bt" || l == "blue tooth") return "bluetooth";
  if (l == "sound" || l == "audio" || l == "volume" || l == "speaker") return "sound";
  if (l == "display" || l == "screen" || l == "monitor" || l == "brightness") return "display";
  if (l == "battery") return "battery";
  if (l == "power" || l == "sleep" || l == "energy") return "power";
  if (l == "apps" || l == "applications" || l == "programs") return "apps";
  if (l == "privacy" || l == "security" || l == "permissions") return "privacy";
  if (l == "update" || l == "updates" || l == "windows update") return "update";
  if (l == "about" || l == "system" || l == "info") return "about";
  // Unknown text: return it lowercased so native_open_settings can still try
  // a best-effort mapping; the platform layer returns false when unknown.
  return l;
}

std::vector<std::pair<std::string, std::string>> settings_page_list() {
  return {{"", "Main settings"},      {"wifi", "Wi-Fi"},     {"network", "Network"},
          {"bluetooth", "Bluetooth"}, {"sound", "Sound"},    {"display", "Display"},
          {"battery", "Battery"},     {"power", "Power"},    {"apps", "Apps"},
          {"privacy", "Privacy"},     {"update", "Updates"}, {"about", "About"}};
}

std::vector<SearchResult> wifi_results(const std::string& remainder, const Config&) {
  std::vector<SearchResult> out;
  auto op = parse_toggle_arg(remainder);
  bool enabled = false;
  std::string detail, err;
  bool ok = native_wifi_status(enabled, detail, err);
  std::string status = !ok ? ("Unavailable" + (err.empty() ? "" : " · " + err))
                           : (enabled ? "On" : "Off") + (detail.empty() ? "" : " · " + detail);
  if (op == ToggleOp::Status) {
    out.push_back(tcard("Wi-Fi: " + status, "wifi on | wifi off | wifi toggle — enter toggles",
                        "toggle:wifi:toggle", "wifi", "toggle"));
    if (ok) {
      out.push_back(tcard("Turn Wi-Fi on", "wifi on", "toggle:wifi:on", "wifi", "toggle"));
      out.push_back(tcard("Turn Wi-Fi off", "wifi off", "toggle:wifi:off", "wifi", "toggle"));
      std::string lerr;
      auto nets = native_wifi_list(lerr);
      for (std::size_t i = 0; i < nets.size() && i < 5; ++i)
        out.push_back(tcard(nets[i], "Nearby network", "toggle:wifi:toggle", "wifi", "toggle"));
    }
    out.push_back(tcard("Open Wi-Fi settings", "settings wifi — enter opens", "settings:wifi",
                        "settings", "settings"));
    return out;
  }
  std::string payload = op == ToggleOp::On    ? "toggle:wifi:on"
                        : op == ToggleOp::Off ? "toggle:wifi:off"
                                              : "toggle:wifi:toggle";
  std::string label = op == ToggleOp::On    ? "Turn Wi-Fi on"
                      : op == ToggleOp::Off ? "Turn Wi-Fi off"
                                            : "Toggle Wi-Fi (now " + status + ")";
  out.push_back(tcard(label, "Wi-Fi · " + status + " — enter applies", payload, "wifi", "toggle"));
  return out;
}

std::vector<SearchResult> bluetooth_results(const std::string& remainder, const Config&) {
  std::vector<SearchResult> out;
  auto op = parse_toggle_arg(remainder);
  bool enabled = false;
  std::string detail, err;
  bool ok = native_bluetooth_status(enabled, detail, err);
  std::string status = !ok ? ("Unavailable" + (err.empty() ? "" : " · " + err))
                           : (enabled ? "On" : "Off") + (detail.empty() ? "" : " · " + detail);
  if (op == ToggleOp::Status) {
    out.push_back(tcard("Bluetooth: " + status,
                        "bluetooth on | bluetooth off | bluetooth toggle — enter toggles",
                        "toggle:bluetooth:toggle", "bluetooth", "toggle"));
    if (ok) {
      out.push_back(
          tcard("Turn Bluetooth on", "bluetooth on", "toggle:bluetooth:on", "bluetooth", "toggle"));
      out.push_back(tcard("Turn Bluetooth off", "bluetooth off", "toggle:bluetooth:off",
                          "bluetooth", "toggle"));
    }
    out.push_back(tcard("Open Bluetooth settings", "settings bluetooth — enter opens",
                        "settings:bluetooth", "settings", "settings"));
    return out;
  }
  std::string payload = op == ToggleOp::On    ? "toggle:bluetooth:on"
                        : op == ToggleOp::Off ? "toggle:bluetooth:off"
                                              : "toggle:bluetooth:toggle";
  std::string label = op == ToggleOp::On    ? "Turn Bluetooth on"
                      : op == ToggleOp::Off ? "Turn Bluetooth off"
                                            : "Toggle Bluetooth (now " + status + ")";
  out.push_back(
      tcard(label, "Bluetooth · " + status + " — enter applies", payload, "bluetooth", "toggle"));
  return out;
}

std::vector<SearchResult> volume_results(const std::string& remainder, const Config&) {
  std::vector<SearchResult> out;
  std::string kind;
  int level = -1;
  parse_volume_arg(remainder, kind, level);
  int cur = -1;
  bool muted = false;
  std::string err;
  bool ok = native_volume_status(cur, muted, err);
  char curbuf[96];
  if (!ok)
    std::snprintf(curbuf, sizeof(curbuf), "Unavailable%s",
                  err.empty() ? "" : (" · " + err).c_str());
  else if (muted)
    std::snprintf(curbuf, sizeof(curbuf), "Muted (level %d%%)", cur);
  else
    std::snprintf(curbuf, sizeof(curbuf), "%d%%%s", cur, muted ? " muted" : "");
  std::string curstr(curbuf);
  if (kind == "status") {
    out.push_back(tcard("Volume: " + curstr,
                        "volume 50 | volume mute | volume up — enter toggles mute",
                        "toggle:volume:mute_toggle", "volume", "toggle", cur));
    if (ok) {
      for (int v : {25, 50, 75, 100}) {
        char t[64];
        std::snprintf(t, sizeof(t), "Set volume to %d%%", v);
        out.push_back(tcard(t, "volume " + std::to_string(v),
                            "toggle:volume:set:" + std::to_string(v), "volume", "toggle", v));
      }
      out.push_back(tcard(muted ? "Unmute" : "Mute", muted ? "volume unmute" : "volume mute",
                          muted ? "toggle:volume:unmute" : "toggle:volume:mute", "volume", "toggle",
                          cur));
    }
    out.push_back(tcard("Open sound settings", "settings sound — enter opens", "settings:sound",
                        "settings", "settings"));
    return out;
  }
  std::string payload, label;
  if (kind == "set") {
    payload = "toggle:volume:set:" + std::to_string(level);
    label = "Set volume to " + std::to_string(level) + "% (now " + curstr + ")";
  } else if (kind == "mute") {
    payload = "toggle:volume:mute";
    label = "Mute (now " + curstr + ")";
  } else if (kind == "unmute") {
    payload = "toggle:volume:unmute";
    label = "Unmute (now " + curstr + ")";
  } else if (kind == "toggle_mute") {
    payload = "toggle:volume:mute_toggle";
    label = std::string(muted ? "Unmute" : "Mute") + " (now " + curstr + ")";
  } else if (kind == "up") {
    payload = "toggle:volume:up";
    label = "Volume up (now " + curstr + ")";
  } else {
    payload = "toggle:volume:down";
    label = "Volume down (now " + curstr + ")";
  }
  out.push_back(tcard(label, "Volume · " + curstr + " — enter applies", payload, "volume", "toggle",
                      level >= 0 ? level : cur));
  return out;
}

std::vector<SearchResult> brightness_results(const std::string& remainder, const Config&) {
  std::vector<SearchResult> out;
  std::string kind;
  int level = -1;
  parse_brightness_arg(remainder, kind, level);
  int cur = -1;
  std::string err;
  bool ok = native_brightness_status(cur, err);
  std::string curstr =
      !ok ? ("Unavailable" + (err.empty() ? "" : " · " + err)) : (std::to_string(cur) + "%");
  if (kind == "status") {
    out.push_back(tcard("Brightness: " + curstr,
                        "brightness 70 | brightness up — enter opens display settings",
                        "settings:display", "brightness", "toggle", cur));
    if (ok) {
      for (int v : {25, 50, 75, 100}) {
        char t[64];
        std::snprintf(t, sizeof(t), "Set brightness to %d%%", v);
        out.push_back(tcard(t, "brightness " + std::to_string(v),
                            "toggle:brightness:set:" + std::to_string(v), "brightness", "toggle",
                            v));
      }
    }
    return out;
  }
  std::string payload, label;
  if (kind == "set") {
    payload = "toggle:brightness:set:" + std::to_string(level);
    label = "Set brightness to " + std::to_string(level) + "% (now " + curstr + ")";
  } else if (kind == "up") {
    payload = "toggle:brightness:up";
    label = "Brightness up (now " + curstr + ")";
  } else {
    payload = "toggle:brightness:down";
    label = "Brightness down (now " + curstr + ")";
  }
  out.push_back(tcard(label, "Brightness · " + curstr + " — enter applies", payload, "brightness",
                      "toggle", level >= 0 ? level : cur));
  return out;
}

std::vector<SearchResult> settings_results(const std::string& remainder, const Config& cfg) {
  std::vector<SearchResult> out;
  auto t = trim(remainder);
  auto tl = to_lower_utf8(t);
  // `settings edit <key> <value>` / `settings get <key>` edit wilfred.yml
  // in place (same payloads as the `config` mini; executed in actions.cpp).
  if (tl.rfind("edit ", 0) == 0 || tl.rfind("set ", 0) == 0) {
    auto rest = trim(t.substr(t.find(' ') + 1));
    auto sp = rest.find(' ');
    if (sp == std::string::npos) {
      out.push_back(tcard("Usage: settings edit <section.key> <value>",
                          "e.g. settings edit search.max_results 40", "", "settings", "settings"));
      return out;
    }
    auto key = trim(rest.substr(0, sp));
    auto val = trim(rest.substr(sp + 1));
    std::string cur, err;
    bool known = config_get_value(cfg, to_lower_utf8(key), cur, err);
    out.push_back(
        tcard("Set " + key + " to " + val,
              known ? ("now: " + cur + " — enter applies") : err + " — enter tries anyway",
              "config:set:" + key + "=" + val, "settings", "settings"));
    return out;
  }
  if (tl.rfind("get ", 0) == 0) {
    auto key = to_lower_utf8(trim(t.substr(4)));
    std::string cur, err;
    if (config_get_value(cfg, key, cur, err))
      out.push_back(tcard(key + " = " + cur, "enter copies the value", "config:get:" + key,
                          "settings", "settings"));
    else
      out.push_back(tcard("Unknown setting: " + key, err, "", "settings", "settings"));
    return out;
  }
  if (tl == "reset" || tl == "reset defaults" || tl == "restore defaults") {
    out.push_back(tcard("Reset wilfred.yml to defaults",
                        "backs up to .pre-reset.bak — enter resets", "config:reset", "settings",
                        "settings"));
    return out;
  }
  auto page = normalize_settings_page(remainder);
  if (!remainder.empty() && trim(remainder).size() > 0 && !page.empty()) {
    std::string label = page;
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(label[0]));
    out.push_back(tcard("Open " + label + " settings", "settings " + page + " — enter opens",
                        "settings:" + page, "settings", "settings"));
    return out;
  }
  if (!trim(remainder).empty() && page.empty()) {
    // Bare `settings` with no page handled below; `settings <unknown>` already
    // returned above only when page non-empty, so this is the main list.
  }
  if (trim(remainder).empty()) {
    out.push_back(
        tcard("Settings", "Main settings — enter opens", "settings:", "settings", "settings"));
    for (auto& [id, label] : settings_page_list()) {
      if (id.empty()) continue;
      out.push_back(tcard(label + " settings", "settings " + id + " — enter opens",
                          "settings:" + id, "settings", "settings"));
    }
    out.push_back(tcard("Edit wilfred.yml", "settings config — open in editor", "config:open",
                        "settings", "settings"));
    out.push_back(tcard("Validate wilfred.yml", "settings config validate — enter checks",
                        "config:validate", "settings", "settings"));
    out.push_back(tcard("First-run setup", "setup — check roots, hotkey, browser", "setup:wizard",
                        "setup", "setup"));
    return out;
  }
  // `settings <page>` with empty normalized page = main settings.
  out.push_back(
      tcard("Settings", "Main settings — enter opens", "settings:", "settings", "settings"));
  return out;
}

bool execute_toggle_payload(const std::string& payload, const Config&, std::string& error) {
  if (payload.rfind("toggle:wifi:", 0) == 0) {
    auto op = payload.substr(12);
    if (op == "on") return native_wifi_set(true, error);
    if (op == "off") return native_wifi_set(false, error);
    bool cur = false, dummy_on = false;
    std::string detail;
    if (!native_wifi_status(cur, detail, error)) return false;
    (void)dummy_on;
    return native_wifi_set(!cur, error);
  }
  if (payload.rfind("toggle:bluetooth:", 0) == 0) {
    auto op = payload.substr(17);
    if (op == "on") return native_bluetooth_set(true, error);
    if (op == "off") return native_bluetooth_set(false, error);
    bool cur = false;
    std::string detail;
    if (!native_bluetooth_status(cur, detail, error)) return false;
    return native_bluetooth_set(!cur, error);
  }
  if (payload.rfind("toggle:volume:", 0) == 0) {
    auto op = payload.substr(14);
    if (op.rfind("set:", 0) == 0) {
      try {
        return native_volume_set(std::stoi(op.substr(4)), error);
      } catch (...) {
        error = "bad volume level";
        return false;
      }
    }
    if (op == "mute") return native_volume_mute(true, error);
    if (op == "unmute") return native_volume_mute(false, error);
    if (op == "mute_toggle") {
      int lvl = 0;
      bool muted = false;
      if (!native_volume_status(lvl, muted, error)) return false;
      return native_volume_mute(!muted, error);
    }
    if (op == "up" || op == "down") {
      int lvl = 0;
      bool muted = false;
      if (!native_volume_status(lvl, muted, error)) return false;
      if (muted) {
        if (!native_volume_mute(false, error)) return false;
      }
      int next = op == "up" ? lvl + 10 : lvl - 10;
      if (next < 0) next = 0;
      if (next > 100) next = 100;
      return native_volume_set(next, error);
    }
    error = "unknown volume op '" + op + "'";
    return false;
  }
  if (payload.rfind("toggle:brightness:", 0) == 0) {
    auto op = payload.substr(18);
    if (op.rfind("set:", 0) == 0) {
      try {
        return native_brightness_set(std::stoi(op.substr(4)), error);
      } catch (...) {
        error = "bad brightness level";
        return false;
      }
    }
    if (op == "up" || op == "down") {
      int cur = -1;
      if (!native_brightness_status(cur, error)) return false;
      int next = op == "up" ? cur + 10 : cur - 10;
      if (next < 0) next = 0;
      if (next > 100) next = 100;
      return native_brightness_set(next, error);
    }
    error = "unknown brightness op '" + op + "'";
    return false;
  }
  if (payload.rfind("settings:", 0) == 0) {
    return native_open_settings(payload.substr(9), error);
  }
  return false;
}

}  // namespace wilfred
