#include "wilfred/locale/locale.hpp"

#include "wilfred/config/yaml.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace wilfred {
namespace {

struct Entry {
  const char* key;
  const char* en;
};

// Compiled-in English. Every tr() key used in code MUST appear here;
// lang/*.yml files override these values (see lang/en.yml reference).
constexpr Entry kEnglish[] = {
    // --- Overlay chrome (also served via overlay_strings()) ---
    {"overlay.search_placeholder", "Search files, apps, and more"},
    {"overlay.empty_title_idle", "Search, calculate, or run a command"},
    {"overlay.empty_sub_idle", "Press {hotkey} anywhere to open Wilfred. Try one of these:"},
    {"overlay.empty_title_none", "No results for \u201c{q}\u201d"},
    {"overlay.empty_sub_none", "Check the spelling, or start with ? to search the web."},
    {"overlay.empty_sub_none_correction",
     "Press Tab to use the suggestion above, or start with ? to search the web."},
    {"overlay.correct_prefix", "Did you mean"},
    {"overlay.bar_results_one", "{n} result"},
    {"overlay.bar_results_other", "{n} results"},
    {"overlay.bar_fix", "Fix"},
    {"overlay.bar_complete", "Complete"},
    {"overlay.bar_details", "Details"},
    {"overlay.bar_actions", "Actions"},
    {"overlay.primary_open", "Open"},
    {"overlay.primary_copy_result", "Copy result"},
    {"overlay.primary_paste", "Paste"},
    {"overlay.primary_open_browser", "Open in browser"},
    {"overlay.primary_copy", "Copy"},
    {"overlay.primary_run", "Run"},
    {"overlay.primary_apply", "Apply"},
    {"overlay.primary_open_settings", "Open settings"},
    {"overlay.primary_install", "Install"},
    {"overlay.primary_open_app", "Open application"},
    {"overlay.primary_open_folder", "Open folder"},
    {"overlay.settings_title", "Settings"},
    {"overlay.settings_search", "Filter settings"},
    {"overlay.settings_saved", "Saved"},
    {"overlay.settings_restart", "Needs daemon restart"},
    {"overlay.settings_live", "Applies instantly"},
    {"overlay.settings_back", "Back to search"},
    {"overlay.settings_note",
     "Keys marked \u201c{live}\u201d apply on next summon; the rest {restart}."},
    // --- Result action labels ---
    {"action.open", "Open"},
    {"action.reveal", "Show in folder"},
    {"action.copy_path", "Copy path"},
    {"action.copy_name", "Copy name"},
    {"action.copy_posix", "Copy POSIX path"},
    {"action.copy_file_uri", "Copy file URL"},
    {"action.copy_wsl", "Copy WSL path"},
    {"action.copy_text", "Copy"},
    {"action.copy_text_alt", "Copy text"},
    {"action.copy", "Copy"},
    {"action.hash_file", "Copy SHA-256 hash"},
    {"action.compress_zip", "Compress to .zip"},
    {"action.move_to", "Move to \u2026 (uses clipboard path)"},
    {"action.move_to_short", "Move to \u2026"},
    {"action.bulk_rename", "Bulk rename\u2026 (uses clipboard pattern)"},
    {"action.bulk_rename_short", "Bulk rename\u2026"},
    {"action.new_from_template", "New from template\u2026"},
    {"action.new_from_template_short", "New from template\u2026"},
    {"action.run", "Run"},
    {"action.open_with_prefix", "Open with "},
    {"action.copy_install_cmd", "Copy install command"},
    {"action.pin_add", "Pin as favorite"},
    {"action.pin_remove", "Unpin favorite"},
    {"action.open_terminal", "Open terminal here"},
    {"action.open_editor", "Open in editor"},
    {"action.new_file", "New file here"},
    {"action.new_folder", "New folder here"},
    {"action.kill_process", "Kill process"},
    {"action.timer_stop", "Stop timer"},
    {"action.transcribe_run", "Transcribe"},
    {"action.convert_run", "Convert"},
    {"action.bgremove_run", "Remove background"},
    {"action.dictate_run", "Dictate"},
    {"action.paste", "Paste"},
    {"action.clip_pin", "Pin"},
    {"action.clip_unpin", "Unpin"},
    {"action.copy_url", "Copy URL"},
    {"action.capture", "Capture"},
    {"action.capture_reveal", "Capture + show in folder"},
    {"action.capture_copy", "Capture + copy path"},
    {"action.approve", "Approve"},
    {"action.install", "Install"},
    {"action.copy_example", "Copy example"},
    {"action.switch", "Switch"},
    {"action.copy_title", "Copy title"},
    {"action.copy_status", "Copy status"},
    {"action.apply", "Apply"},
    {"action.open_settings", "Open settings"},
    {"action.run_prefix", "Run "},
    {"action.apply_layout", "Apply layout"},
    {"action.tile", "Tile"},
    {"action.focus_window", "Focus window"},
    {"action.lock", "Lock"},
    {"action.sleep", "Sleep"},
    {"action.shutdown", "Shut down"},
    {"action.restart", "Restart"},
    {"action.logout", "Log out"},
    {"action.empty_trash", "Empty"},
    // --- Help mini (help.01..help.47 follow query-language order) ---
    {"help.01", "weather [city]  \u00b7  local forecast"},
    {"help.02", "time [zone]  \u00b7  clock and date"},
    {"help.03", "define <word> \u00b7 thesaurus <word>  \u00b7  offline dictionary"},
    {"help.04", "pin <text> \u00b7 pins \u00b7 unpin <text>  \u00b7  favorites"},
    {"help.05", "event add <summary> | <when>  \u00b7  new calendar event"},
    {"help.06", "contact add <name> [email]  \u00b7  new contact"},
    {"help.07", "rename <dir> <pattern>  \u00b7  bulk rename {n} {name} {ext}"},
    {"help.08", "template <kind> [name]  \u00b7  new from template"},
    {"help.09", "clips image \u00b7 clips path  \u00b7  image and path clips"},
    {"help.10", "timer 10m \u00b7 pomodoro \u00b7 stopwatch  \u00b7  focus timers"},
    {"help.11", "note <text> \u00b7 notes  \u00b7  quick notes"},
    {"help.12", "todo <task> \u00b7 todos \u00b7 todo done <id>  \u00b7  tasks"},
    {"help.13", "tz tokyo  \u00b7  world clock / zone convert"},
    {"help.14", "color #ff5500  \u00b7  hex / rgb / hsl"},
    {"help.15", "hex 255 \u00b7 dec 0xff \u00b7 base 16 255  \u00b7  number bases"},
    {"help.16", "bit and 12 10 \u00b7 bit not 5  \u00b7  bit tools"},
    {"help.17", "regex pattern text \u00b7 urlencode \u00b7 jwt <token>  \u00b7  text tools"},
    {"help.18", "json {\"a\":1} \u00b7 base64 \u00b7 sha256 \u00b7 uuid \u00b7 lorem"},
    {"help.19", "disk / disku  \u00b7  drive space"},
    {"help.20", "large [n] [dir] \u00b7 dupes [dir]  \u00b7  big + duplicate files"},
    {"help.21", "ram / cpu / swap  \u00b7  memory and load"},
    {"help.22", "process <name> \u00b7 kill <pid|name>  \u00b7  processes"},
    {"help.23", "media play \u00b7 next \u00b7 mute \u00b7 vol up  \u00b7  playback"},
    {"help.24", "ping <host> \u00b7 dns <host> \u00b7 myip  \u00b7  network"},
    {"help.25", "transcribe <file>  \u00b7  mp3/mp4 audio to text"},
    {"help.26", "convert <file> to <mp3|wav|ogg|png|jpg>  \u00b7  file conversion"},
    {"help.27", "bgremove <image> [tolerance] [#color]  \u00b7  transparent background"},
    {"help.28", "dictate [seconds]  \u00b7  mic to text via whisper"},
    {"help.29", "layout save <name> \u00b7 layout <name>  \u00b7  window layouts"},
    {"help.30", "windows [name]  \u00b7  switch to an open window"},
    {"help.31", "minimize/maximize <name> \u00b7 close window <name>"},
    {"help.32", "screenshot [fullscreen|window|region]"},
    {"help.33", "emoji [name]  \u00b7  emoji picker"},
    {"help.34", "symbol [name]  \u00b7  punctuation and signs"},
    {"help.35", "fx 100 usd to eur  \u00b7  currency conversion"},
    {"help.36", "lock / sleep / shutdown / restart / logout"},
    {"help.37", "empty trash  \u00b7  recycle bin"},
    {"help.38", "speedtest  \u00b7  live download and upload"},
    {"help.39", "battery / ip / hostname / uptime / user"},
    {"help.40", "clip / clips [type] [query]  \u00b7  clips url \u00b7 clips code"},
    {"help.41", "bm [query]  \u00b7  bookmarks, history, open tabs"},
    {"help.42", "workflow <name> \u00b7 run <name>  \u00b7  multi-step actions"},
    {"help.43", "ql <name> <args>  \u00b7  parameterized quicklinks"},
    {"help.44", "snip / ;keyword  \u00b7  text snippets"},
    {"help.45", "snip save <name>  \u00b7  save clipboard as snippet"},
    {"help.46", "os / cores / screen"},
    {"help.47", "macros  \u00b7  bang searches  (!yt cats)"},
    {"help.sub", "Mini commands \u00b7 enter copies"},
    // --- Define / thesaurus mini ---
    {"define.usage_title", "define <word>"},
    {"define.usage_sub", "Offline dictionary \u00b7 e.g. define resilient"},
    {"define.thes_title", "thesaurus <word>"},
    {"define.thes_sub", "Offline synonyms \u00b7 e.g. thesaurus happy"},
    {"define.syn_prefix", "Synonyms: {syn}"},
    {"define.sub_defined", "Definition \u00b7 enter copies"},
    {"define.sub_syn", "Thesaurus \u00b7 enter copies"},
    {"define.no_entry_title", "No definition for \u201c{word}\u201d"},
    {"define.no_entry_sub", "No offline entry \u00b7 enter copies the word"},
    {"define.no_exact_sub", "No exact entry \u00b7 did you mean: {sug}"},
    {"define.suggest_sub", "Suggestion \u00b7 enter looks up"},
    // --- Pins mini ---
    {"pins.empty_title", "No pins yet"},
    {"pins.empty_sub", "Type pin <path or title> to pin a favorite"},
    {"pins.item_title", "Pinned: {pin}"},
    {"pins.item_sub", "Favorite \u00b7 unpin {pin}"},
    {"pins.pin_usage", "pin <text>"},
    {"pins.pin_usage_sub", "Pin a path/title as favorite"},
    {"pins.unpin_usage", "unpin <text>"},
    {"pins.unpin_usage_sub", "Remove a favorite"},
    {"pins.pinned", "Pinned {target}"},
    {"pins.pinned_sub", "Favorites boost ranking via ranking.pinned"},
    {"pins.already", "Already pinned: {target}"},
    {"pins.already_sub", "Try pins to list"},
    {"pins.unpinned", "Unpinned {target}"},
    {"pins.unpinned_sub", "Favorite removed"},
    {"pins.no_match", "No pin matching {target}"},
    // --- Event / contact creation ---
    {"event.usage_title", "event add <summary> | <when>"},
    {"event.usage_sub", "Create a calendar event \u00b7 e.g. event add Team sync | tomorrow 10am"},
    {"event.created", "Event created: {summary}"},
    {"event.created_sub", "{dest} \u00b7 enter opens"},
    {"event.failed", "Cannot create event"},
    {"contact.usage_title", "contact add <name> [email] [phone]"},
    {"contact.usage_sub", "Create a contact \u00b7 e.g. contact add Jane Doe jane@x.com 555-0100"},
    {"contact.created", "Contact created: {name}"},
    {"contact.created_sub", "{dest} \u00b7 enter opens"},
    {"contact.failed", "Cannot create contact"},
    // --- Bulk file ops mini ---
    {"fileop.rename_usage", "rename <dir> <pattern>"},
    {"fileop.rename_sub",
     "Bulk rename with {n} {name} {ext} \u00b7 e.g. rename ./photos photo-{n}.jpg"},
    {"fileop.renamed", "Renamed {n} files"},
    {"fileop.renamed_sub", "{dir} \u00b7 {pattern} \u00b7 enter copies first path"},
    {"fileop.rename_failed", "Cannot rename"},
    {"fileop.move_title", "move <file> to <dir>"},
    {"fileop.move_sub", "Move files via result actions (open a file \u2192 move_to) or CLI"},
    {"fileop.template_usage", "template <kind> [name] [in <dir>]"},
    {"fileop.template_sub",
     "New from template \u00b7 empty, md, python, cpp, html, json, gitignore"},
    {"fileop.created", "Created {path}"},
    {"fileop.created_sub", "{kind} template \u00b7 enter opens"},
    {"fileop.template_failed", "Cannot create from template"},
    // --- Clips mini ---
    {"clips.clear_title", "Clear clipboard history"},
    {"clips.clear_sub", "Remove all unpinned clips"},
    {"clips.clear_action", "Clear"},
    {"clips.empty", "No clipboard history yet"},
    {"clips.empty_hint", "Copy text, then type clips"},
    {"clips.no_match", "No clips matching"},
    {"clips.no_match_hint", "Try clips to list all"},
    {"clips.no_type_match", "No {type} clips matching"},
    {"clips.pinned_sub", "Pinned clip \u00b7 enter copies"},
    {"clips.history_sub", "Clipboard history"},
    {"clips.type_sub", "{type} clip \u00b7 enter copies"},
    {"clips.kind_sub", "{kind} clip \u00b7 enter copies"},
};

const char* kOverlayKeys[] = {
    "overlay.search_placeholder",    "overlay.empty_title_idle",
    "overlay.empty_sub_idle",        "overlay.empty_title_none",
    "overlay.empty_sub_none",        "overlay.empty_sub_none_correction",
    "overlay.correct_prefix",        "overlay.bar_results_one",
    "overlay.bar_results_other",     "overlay.bar_fix",
    "overlay.bar_complete",          "overlay.bar_details",
    "overlay.bar_actions",           "overlay.primary_open",
    "overlay.primary_copy_result",   "overlay.primary_paste",
    "overlay.primary_open_browser",  "overlay.primary_copy",
    "overlay.primary_run",           "overlay.primary_apply",
    "overlay.primary_open_settings", "overlay.primary_install",
    "overlay.primary_open_app",      "overlay.primary_open_folder",
    "overlay.settings_title",        "overlay.settings_search",
    "overlay.settings_saved",        "overlay.settings_restart",
    "overlay.settings_live",         "overlay.settings_back",
    "overlay.settings_note",
};

std::string substitute(std::string text, const std::unordered_map<std::string, std::string>& args) {
  for (auto& [k, v] : args) {
    std::string ph = "{" + k + "}";
    std::size_t pos = 0;
    while ((pos = text.find(ph, pos)) != std::string::npos) {
      text.replace(pos, ph.size(), v);
      pos += v.size();
    }
  }
  return text;
}

}  // namespace

LocaleStore& LocaleStore::instance() {
  static LocaleStore inst;
  return inst;
}

bool LocaleStore::valid_code_syntax(const std::string& code) {
  if (code == "auto") return true;
  // "en", "pt-BR", "zh-Hans" (script part is ignored at runtime).
  std::size_t i = 0;
  while (i < code.size() && std::isalpha(static_cast<unsigned char>(code[i])))
    ++i;
  if (i < 2 || i > 3) return false;
  if (i == code.size()) return true;
  if (code[i] != '-' && code[i] != '_') return false;
  std::size_t j = i + 1;
  while (j < code.size() && std::isalnum(static_cast<unsigned char>(code[j])))
    ++j;
  return j > i + 1 && j == code.size();
}

std::vector<std::string> LocaleStore::shipped_codes() {
  return {"en", "de", "fr", "es", "pt", "it", "nl", "ja", "ko", "zh", "ru", "pl", "tr", "uk"};
}

std::vector<std::string> LocaleStore::english_keys() {
  std::vector<std::string> out;
  for (auto& e : kEnglish)
    out.emplace_back(e.key);
  return out;
}

std::string normalize_lang_code(const std::string& raw) {
  std::string s;
  for (char c : raw) {
    if (c == '_' || c == '-') break;
    if (c == '.' || c == '@' || c == ' ') break;
    s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (s.size() >= 3) break;
  }
  if (s == "eng") return "en";
  if (s == "deu" || s == "ger") return "de";
  if (s == "fra" || s == "fre") return "fr";
  if (s == "spa") return "es";
  if (s == "ita") return "it";
  if (s == "por") return "pt";
  if (s == "nld" || s == "dut") return "nl";
  if (s == "rus") return "ru";
  if (s == "pol") return "pl";
  if (s == "tur") return "tr";
  if (s == "ukr") return "uk";
  if (s == "kor") return "ko";
  if (s == "zho" || s == "chi") return "zh";
  if (s == "jpn") return "ja";
  if (s.size() == 2) return s;
  return "en";
}

std::string system_language_code() {
#ifdef _WIN32
  wchar_t buf[85];
  if (GetUserDefaultLocaleName(buf, 85)) return normalize_lang_code(wide_to_utf8(buf));
  return "en";
#elif defined(__APPLE__)
  CFLocaleRef loc = CFLocaleCopyCurrent();
  if (!loc) return "en";
  CFStringRef id = (CFStringRef)CFLocaleGetValue(loc, kCFLocaleIdentifier);
  char buf[32] = {0};
  std::string out = "en";
  if (id && CFStringGetCString(id, buf, sizeof(buf), kCFStringEncodingUTF8))
    out = normalize_lang_code(buf);
  CFRelease(loc);
  return out;
#else
  const char* vars[] = {nullptr, nullptr, nullptr, nullptr};
  // LANGUAGE is a colon-separated priority list; the rest are single values.
  vars[0] = std::getenv("LANGUAGE");
  vars[1] = std::getenv("LC_ALL");
  vars[2] = std::getenv("LC_MESSAGES");
  vars[3] = std::getenv("LANG");
  for (auto* v : vars) {
    if (!v || !*v) continue;
    std::string first = v;
    auto colon = first.find(':');
    if (colon != std::string::npos) first.resize(colon);
    if (first.empty() || first == "C" || first == "POSIX") continue;
    return normalize_lang_code(first);
  }
  return "en";
#endif
}

void LocaleStore::configure(const std::string& code) {
  std::lock_guard<std::mutex> lock(mu_);
  requested_ = code.empty() ? "auto" : code;
  effective_ = "en";
  strings_.clear();
}

static std::string catalog_path_for(const std::string& dir, const std::string& code) {
  return path_join(path_join(dir, "lang"), code + ".yml");
}

bool LocaleStore::load_from(const std::string& path) {
  std::string text;
  if (!read_file_all(path, text) || text.empty()) return false;
  YamlValue root;
  YamlError ye;
  if (!parse_yaml(text, root, ye) || !root.is_map()) return false;
  std::lock_guard<std::mutex> lock(mu_);
  bool any = false;
  for (auto& [k, v] : root.as_map()) {
    if (v.is_string() && !v.as_string().empty()) {
      strings_[k] = v.as_string();
      any = true;
    }
  }
  return any;
}

bool LocaleStore::load() {
  std::lock_guard<std::mutex> lock(mu_);
  strings_.clear();
  // Candidates: explicit region file first ("pt-BR"), then base ("pt").
  std::vector<std::string> cands;
  if (requested_ != "auto") {
    std::string raw;
    for (char c : requested_) {
      if (c == '_')
        raw.push_back('-');
      else
        raw.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (raw.size() > 2) cands.push_back(raw);
  }
  cands.push_back(requested_ == "auto" ? system_language_code() : normalize_lang_code(requested_));
  std::string exe = exe_directory();
  std::vector<std::string> search;
  search.push_back(path_join(config_directory(), "lang"));
  search.push_back("lang");  // CWD (dev checkouts, portable layouts)
  if (!exe.empty() && exe != ".") {
    search.push_back(path_join(path_join(exe, ".."), "share/wilfred/lang"));
    search.push_back(path_join(path_join(exe, "share"), "wilfred/lang"));
  }
  search.push_back("/usr/local/share/wilfred/lang");
  search.push_back("/usr/share/wilfred/lang");
  effective_ = "en";
  for (auto& c : cands) {
    if (c == "en") {
      effective_ = "en";
      return false;  // compiled-in English needs no file
    }
    for (auto& d : search) {
      std::string text;
      if (!read_file_all(catalog_path_for(d, c), text) || text.empty()) continue;
      YamlValue root;
      YamlError ye;
      if (!parse_yaml(text, root, ye) || !root.is_map()) continue;
      bool any = false;
      for (auto& [k, v] : root.as_map()) {
        if (v.is_string() && !v.as_string().empty()) {
          strings_[k] = v.as_string();
          any = true;
        }
      }
      if (any) {
        effective_ = c;
        return true;
      }
    }
  }
  return false;
}

std::string LocaleStore::requested_code() const {
  std::lock_guard<std::mutex> lock(mu_);
  return requested_;
}
std::string LocaleStore::code() const {
  std::lock_guard<std::mutex> lock(mu_);
  return effective_;
}

std::string LocaleStore::tr(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = strings_.find(key);
  if (it != strings_.end()) return it->second;
  for (auto& e : kEnglish)
    if (key == e.key) return e.en;
  return key;
}

std::string LocaleStore::tr(const std::string& key,
                            const std::unordered_map<std::string, std::string>& args) const {
  return substitute(tr(key), args);
}

std::unordered_map<std::string, std::string> LocaleStore::overlay_strings() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::unordered_map<std::string, std::string> out;
  for (auto* k : kOverlayKeys) {
    auto it = strings_.find(k);
    if (it != strings_.end()) {
      out[k] = it->second;
      continue;
    }
    for (auto& e : kEnglish)
      if (k == e.key) {
        out[k] = e.en;
        break;
      }
  }
  return out;
}

}  // namespace wilfred
