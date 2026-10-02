#pragma once

#include "wilfred/config/config.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace wilfred::import {

// One custom web search: `keyword` is the bang/trigger (e.g. "g"),
// `name` is the human label (e.g. "Google"), `url` is normalized to use
// {query} / {query_enc} / {clipboard} placeholders understood by Wilfred.
struct WebSearch {
  std::string name;
  std::string keyword;
  std::string url;
  std::string source;
};

struct Snippet {
  std::string trigger;
  std::string title;
  std::string body;
  std::string source;
};

struct AliasEntry {
  std::string from;
  std::string to;
};

struct ImportHotkey {
  bool has{false};
  std::vector<std::string> modifiers;
  std::string key;
  std::string raw;
};

// Normalized intermediate form produced by every launcher parser.
struct ImportedSettings {
  std::string source_id;
  std::string source_name;
  std::vector<WebSearch> searches;
  std::vector<Snippet> snippets;
  std::vector<AliasEntry> aliases;
  // Extra parameterized templates (command runners, app launchers, ...).
  // name -> template. Templates use {query}/{1}/{*}/{clipboard}.
  std::map<std::string, std::string> quicklinks;
  std::optional<std::string> default_search_template;
  ImportHotkey hotkey;
  std::optional<std::string> theme;
  std::vector<std::string> warnings;
};

struct LauncherInfo {
  std::string id;
  std::string name;
  std::string os;
  std::string description;
  std::string format;
  std::vector<std::string> candidates;
};

struct DetectedLauncher {
  LauncherInfo info;
  std::string found_path;
  bool is_directory{false};
};

struct ImportOptions {
  bool overwrite{false};
  bool dry_run{false};
  bool include_hotkey{true};
  bool include_searches{true};
  bool include_snippets{true};
  bool include_aliases{true};
  bool include_quicklinks{true};
  bool include_theme{true};
  bool include_browser{true};
};

struct ImportCounts {
  int macros_added{0};
  int macros_overwritten{0};
  int macros_skipped{0};
  int quicklinks_added{0};
  int quicklinks_overwritten{0};
  int quicklinks_skipped{0};
  int snippets_added{0};
  int snippets_overwritten{0};
  int snippets_skipped{0};
  int aliases_added{0};
  int aliases_overwritten{0};
  int aliases_skipped{0};
  bool hotkey_applied{false};
  bool theme_applied{false};
  bool browser_applied{false};
};

struct ImportOutcome {
  bool ok{false};
  std::string error;
  ImportCounts counts;
  std::vector<std::string> warnings;
  ImportedSettings preview;
};

std::vector<LauncherInfo> supported_launchers();
const LauncherInfo* find_launcher(const std::string& id);
std::vector<DetectedLauncher> detect_launchers();

// Parse raw file text for a known launcher id ("auto" runs generic
// detection across all formats). path_hint is used only for warnings.
ImportedSettings parse_text(const std::string& launcher_id, const std::string& text,
                            const std::string& path_hint, std::vector<std::string>& warnings);

// Parse a directory (used by Alfred.alfredpreferences bundles: every
// *.plist under the directory is scanned and merged).
ImportedSettings parse_directory(const std::string& launcher_id, const std::string& dir_path,
                                 std::vector<std::string>& warnings);

// Auto-detect launcher by path name and/or content, then parse.
// On success returns true and fills `out` + `detected_id`.
bool parse_file_auto(const std::string& path, ImportedSettings& out, std::string& detected_id,
                     std::string& error);

// Merge imported settings into a live Config. Returns per-category counts.
ImportCounts apply_settings(Config& cfg, const ImportedSettings& in, const ImportOptions& opts,
                            std::vector<std::string>& warnings);

// Config persistence used by import (lossy w.r.t. comments, preserves values).
std::string serialize_config(const Config& cfg);
bool save_config_file(const std::string& path, const Config& cfg, std::string& error);

// Helpers exposed for tests.
std::string normalize_url_template(std::string url);
std::string slugify_keyword(const std::string& s);
bool looks_like_search_url(const std::string& s);
bool parse_hotkey_string(const std::string& raw, ImportHotkey& out);
std::string hotkey_to_string(const ImportHotkey& hk);
std::string detect_id_for_path(const std::string& path, const std::string& content);

}  // namespace wilfred::import
