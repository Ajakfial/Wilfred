#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

struct RankingWeights {
  int exact_name{1200};
  int prefix_name{700};
  int substring_name{420};
  int fuzzy_name{280};
  int acronym{640};
  int path_component{180};
  int extension{90};
  int application{350};
  int recency{220};
  int frequency{260};
  int previous_selection{400};
  int word_boundary{160};
  int token_proximity{140};
  int directory_bonus{40};
  int alias{500};
  int learned_choice{520};
  int context_parent{200};
  int context_extension{90};
  int access_recency{150};
  int clipboard_overlap{240};
  int content_hit{190};
  int hour_affinity{70};
};

struct Config {
  struct Search {
    bool include_system_files{false};
    bool include_hidden_files{true};
    bool show_system_in_results{false};
    int max_results{40};
    int debounce_ms{12};
    bool web_search_fallback{true};
    bool treat_urls_as_open{true};
    int min_query_length{1};
    bool fuzzy{true};
    bool acronyms{true};
    bool context_aware{true};
    bool clipboard{true};
    bool minis{true};
    bool macros{true};
  } search;

  struct Index {
    std::vector<std::string> paths;
    std::vector<std::string> exclude;
    std::vector<std::string> exclude_globs;
    std::vector<std::string> system_directories;
    bool follow_symlinks{false};
    bool index_hidden{true};
    bool index_system{false};
    std::uint64_t max_file_size_bytes{0};
    bool content_indexing{true};
    std::uint64_t content_max_bytes{131072};
    int content_max_tokens{480};
    int workers{0};
    int cpu_percent_limit{45};
    int memory_limit_mb{384};
    int batch_size{2048};
    int debounce_fs_ms{80};
    int rescan_interval_seconds{0};
    int persist_every_records{50000};
    std::uint64_t wal_compact_bytes{8 * 1024 * 1024};
    std::vector<std::string> ext_include;
    std::vector<std::string> ext_exclude;
  } index;

  RankingWeights ranking;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> macros;
  std::unordered_map<std::string, std::vector<std::string>> scopes;
  std::unordered_map<std::string, std::string> custom_metadata;

  struct History {
    bool enabled{true};
    int max_entries{8000};
    bool persist{true};
  } history;

  struct Hotkey {
    bool enabled{true};
    std::vector<std::string> modifiers{"ctrl", "alt"};
    std::string key{"W"};
    bool use_command_on_macos{true};
  } hotkey;

  struct Browser {
    std::string provider{"auto"};
    std::string search_template{"https://www.google.com/search?q={query}"};
  } browser;

  struct Logging {
    std::string level{"info"};
    std::string file;
    std::uint64_t max_file_bytes{2 * 1024 * 1024};
  } logging;

  struct Ui {
    std::string theme{"dark"};
    int max_visible{9};
    int width{720};
  } ui;

  std::string source_path;
};

struct ConfigError {
  std::string message;
};

bool load_config_text(const std::string& text, Config& out, ConfigError& err);
bool load_config_file(const std::string& path, Config& out, ConfigError& err);
Config load_or_create_user_config(ConfigError& err);
bool path_is_excluded(const Config& cfg, const std::string& path, const std::string& name);
bool extension_allowed(const Config& cfg, const std::string& ext);
bool is_system_path(const Config& cfg, const std::string& path);

}  // namespace wilfred
