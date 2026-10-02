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
    bool snippets{true};
    bool plugins{true};
  } search;

  struct Index {
    std::vector<std::string> paths;
    std::vector<std::string> exclude;
    std::vector<std::string> exclude_globs;
    std::vector<std::string> system_directories;
    bool follow_symlinks{false};
    bool index_hidden{true};
    bool index_system{false};
    bool usn_scan{true};
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

  struct Clipboard {
    bool manager{true};
    int max_entries{200};
    bool persist{true};
  } clipboard;

  struct Hotkey {
    bool enabled{true};
    std::vector<std::string> modifiers{"ctrl", "alt"};
    std::string key{"W"};
    bool use_command_on_macos{true};
  } hotkey;

  // Extra system-wide hotkeys fired from anywhere, keyed by binding name.
  // Each runs one action: `show` toggles the overlay, `macro:<text>` opens
  // a macro/quicklink URL (clipboard feeds {clipboard} placeholders),
  // `system:<id>` runs a session command, `media:<id>` presses a media key,
  // `workflow:<name>` runs a named workflow against the clipboard.
  // Hotkey workflows should use targetless steps; file steps fail
  // gracefully per step.
  struct HotkeyBinding {
    std::string name;
    std::vector<std::string> modifiers{"ctrl", "alt"};
    std::string key;
    std::string run{"show"};
  };
  std::vector<HotkeyBinding> hotkeys;

  struct Browser {
    std::string provider{"auto"};
    std::string search_template{"https://www.google.com/search?q={query}"};
    bool library{true};
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

  struct Plugins {
    bool enabled{true};
    std::vector<std::string> directories;
    int timeout_ms{400};
  } plugins;

  struct Providers {
    bool semantic{false};
    double semantic_min_score{0.3};
    // vector | trigram | hybrid — which semantic backend to use when enabled.
    std::string semantic_backend{"hybrid"};
    int semantic_max_results{10};
  } providers;

  struct Embedding {
    bool enabled{false};
    // auto | hash | llamacpp | server. `auto` uses llama.cpp when a model or
    // endpoint is configured, otherwise the built-in hash embedder.
    std::string backend{"auto"};
    // Path to a local .gguf model used with llama.cpp (optional).
    std::string model;
    // Optional llama.cpp server endpoint, e.g. http://127.0.0.1:8080/embedding
    // (started via `llama-server -m model.gguf --embedding`).
    std::string endpoint;
    int dim{384};
    double min_score{0.45};
    int max_results{10};
  } embedding;

  struct Ai {
    bool enabled{false};
    // auto | openai | anthropic | gemini | groq
    std::string provider{"auto"};
    std::string model;
    std::string api_key;
    std::string endpoint;
    int max_tokens{1024};
    double temperature{0.7};
    int timeout_ms{30000};
  } ai;

  struct Sources {
    bool calendar{true};
    bool contacts{true};
    bool notes{true};
    std::vector<std::string> calendar_paths;
    std::vector<std::string> contacts_paths;
    std::vector<std::string> notes_paths;
    bool ocr{false};
    std::string ocr_languages{"eng"};
    int max_results{8};
  } sources;

  struct Transcription {
    // On-demand speech-to-text for audio files (`transcribe ...`).
    // Optional CLIs like OCR: whisper.cpp (`whisper-cli`) for recognition,
    // ffmpeg only when container audio (mp4/m4a/...) needs extracting.
    bool enabled{true};
    // Explicit whisper binary path or command name. Empty means probe PATH
    // for `whisper-cli`, then `whisper`.
    std::string binary;
    // Explicit whisper model file (.bin). Empty means probe
    // <data>/models for a ggml model.
    std::string model;
    // BCP-47-ish language hint for whisper (`en`, `de`, ...).
    // `auto` (default) leaves detection to the model.
    std::string language{"auto"};
    // Write `<audio>.txt` next to the source on success, in addition to
    // copying the transcript to the clipboard.
    bool save_txt{true};
    // Microphone device for `dictate` (ffmpeg syntax for this OS). Empty
    // means the OS default: dshow audio device name on Windows, avfoundation
    // audio index (or ":idx") on macOS, ALSA device on Linux.
    std::string mic;
  } transcription;

  struct Api {
    bool enabled{false};
    std::string bind{"127.0.0.1"};
    int port{17380};
    std::string token;
  } api;

  struct Sync {
    bool enabled{false};
    std::string url;
    std::string token;
    int interval_seconds{0};
    bool include_index{true};
  } sync;

  struct Snippets {
    bool expansion{true};
    std::string prefix{";"};
    bool auto_paste{false};
    bool global_expansion{false};
    std::unordered_map<std::string, std::string> items;
  } snippets;

  // Named multi-step workflows: workflow name -> ordered action ids.
  // Each step is an action id accepted by execute_result_action
  // (e.g. "copy_path", "reveal", "open"), joined with '+' at runtime.
  // Lowercased keys.
  std::unordered_map<std::string, std::vector<std::string>> workflows;
  // Parameterized quicklinks: name -> URL/path/command template.
  // Placeholders: {query} {query_enc} {clipboard} {clipboard_enc}
  // plus positional {1} {2} ... and {*} (all args). Lowercased keys.
  std::unordered_map<std::string, std::string> quicklinks;
  // Per-app extra context actions: app-name substring (lowercased)
  // -> additional action ids appended to file/app results.
  std::unordered_map<std::string, std::vector<std::string>> app_actions;

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
