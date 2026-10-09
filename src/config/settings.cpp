#include "wilfred/config/settings.hpp"

namespace wilfred {
namespace {

#define B(k) \
  { k, SettingType::Bool }
#define I(k) \
  { k, SettingType::Int }
#define D(k) \
  { k, SettingType::Double }
#define S(k) \
  { k, SettingType::Str }
#define L(k) \
  { k, SettingType::List }

// Keep in sync with config/config.cpp validation ranges. Enum options drive
// select editors; free-text otherwise.
const SettingMeta kTable[] = {
    // search
    B("search.include_system_files"),
    B("search.include_hidden_files"),
    B("search.show_system_in_results"),
    B("search.web_search_fallback"),
    B("search.treat_urls_as_open"),
    B("search.fuzzy"),
    B("search.acronyms"),
    B("search.context_aware"),
    B("search.clipboard"),
    B("search.minis"),
    B("search.macros"),
    B("search.snippets"),
    B("search.plugins"),
    B("search.async_providers"),
    I("search.max_results"),
    I("search.debounce_ms"),
    I("search.min_query_length"),
    // index
    L("index.paths"),
    S("index.format"),
    B("index.follow_symlinks"),
    B("index.index_hidden"),
    B("index.index_system"),
    B("index.usn_scan"),
    B("index.content_indexing"),
    I("index.workers"),
    I("index.cpu_percent_limit"),
    I("index.memory_limit_mb"),
    I("index.batch_size"),
    I("index.debounce_fs_ms"),
    I("index.rescan_interval_seconds"),
    I("index.persist_every_records"),
    I("index.content_max_tokens"),
    L("index.exclude"),
    L("index.exclude_globs"),
    L("index.system_directories"),
    // ranking (all weights)
    I("ranking.exact_name"),
    I("ranking.prefix_name"),
    I("ranking.substring_name"),
    I("ranking.fuzzy_name"),
    I("ranking.acronym"),
    I("ranking.path_component"),
    I("ranking.extension"),
    I("ranking.application"),
    I("ranking.recency"),
    I("ranking.frequency"),
    I("ranking.previous_selection"),
    I("ranking.word_boundary"),
    I("ranking.token_proximity"),
    I("ranking.directory_bonus"),
    I("ranking.alias"),
    I("ranking.learned_choice"),
    I("ranking.context_parent"),
    I("ranking.context_extension"),
    I("ranking.access_recency"),
    I("ranking.clipboard_overlap"),
    I("ranking.content_hit"),
    I("ranking.hour_affinity"),
    I("ranking.pinned"),
    // hotkey
    B("hotkey.enabled"),
    L("hotkey.modifiers"),
    S("hotkey.key"),
    B("hotkey.use_command_on_macos"),
    // browser
    S("browser.provider"),
    S("browser.search_template"),
    B("browser.library"),
    // logging
    S("logging.level"),
    S("logging.file"),
    I("logging.max_file_bytes"),
    // ui (live: applies on next summon, no restart)
    S("ui.theme"),
    I("ui.max_visible"),
    I("ui.width"),
    S("ui.accent"),
    I("ui.font_size"),
    S("ui.language"),
    // plugins
    B("plugins.enabled"),
    L("plugins.directories"),
    I("plugins.timeout_ms"),
    S("plugins.registry"),
    B("plugins.require_approval"),
    // providers / embedding / ai
    B("providers.semantic"),
    D("providers.semantic_min_score"),
    S("providers.semantic_backend"),
    I("providers.semantic_max_results"),
    D("providers.semantic_vector_weight"),
    D("providers.semantic_trigram_weight"),
    B("embedding.enabled"),
    S("embedding.backend"),
    S("embedding.model"),
    S("embedding.endpoint"),
    I("embedding.dim"),
    D("embedding.min_score"),
    I("embedding.max_results"),
    B("ai.enabled"),
    S("ai.provider"),
    S("ai.model"),
    S("ai.api_key"),
    S("ai.endpoint"),
    I("ai.max_tokens"),
    D("ai.temperature"),
    I("ai.timeout_ms"),
    // sources
    B("sources.calendar"),
    B("sources.contacts"),
    B("sources.notes"),
    L("sources.calendar_paths"),
    L("sources.contacts_paths"),
    L("sources.notes_paths"),
    B("sources.ocr"),
    S("sources.ocr_languages"),
    I("sources.max_results"),
    // remotes
    B("remotes.enabled"),
    I("remotes.timeout_ms"),
    I("remotes.max_results"),
    // packages
    B("packages.enabled"),
    I("packages.max_results"),
    I("packages.timeout_ms"),
    // os_search (official OS index federation)
    B("os_search.enabled"),
    I("os_search.max_results"),
    I("os_search.timeout_ms"),
    S("os_search.backend"),
    // transcription
    B("transcription.enabled"),
    S("transcription.binary"),
    S("transcription.model"),
    S("transcription.language"),
    B("transcription.save_txt"),
    S("transcription.mic"),
    // api / sync
    B("api.enabled"),
    S("api.bind"),
    I("api.port"),
    S("api.token"),
    B("sync.enabled"),
    S("sync.url"),
    S("sync.token"),
    I("sync.interval_seconds"),
    B("sync.include_index"),
    B("sync.encrypt"),
    S("sync.password"),
    S("sync.key_file"),
    // snippets / history / clipboard / layouts
    B("snippets.expansion"),
    S("snippets.prefix"),
    B("snippets.auto_paste"),
    B("snippets.global_expansion"),
    B("history.enabled"),
    I("history.max_entries"),
    B("history.persist"),
    B("clipboard.manager"),
    I("clipboard.max_entries"),
    B("clipboard.persist"),
    B("layouts.auto_apply"),
    S("layouts.auto_layout"),
    // pins
    L("pins"),
};

void apply_options(SettingMeta& m) {
  const std::string& k = m.key;
  if (k == "ui.theme")
    m.options = {"dark", "light"};
  else if (k == "ui.language")
    m.options = {"auto", "en", "de", "fr", "es", "pt", "it", "nl",
                 "ja", "ko", "zh", "ru", "pl", "tr", "uk"};
  else if (k == "logging.level")
    m.options = {"error", "warn", "info", "debug"};
  else if (k == "embedding.backend")
    m.options = {"auto", "hash", "server", "llamacpp"};
  else if (k == "ai.provider")
    m.options = {"auto", "openai", "anthropic", "gemini", "groq"};
  else if (k == "providers.semantic_backend")
    m.options = {"hybrid", "vector", "trigram"};
  else if (k == "os_search.backend")
    m.options = {"auto", "spotlight", "windows_search", "tracker",
                 "baloo", "locate", "everything"};
  else if (k == "index.format")
    m.options = {"auto", "v2", "v3"};
}

void apply_flags(SettingMeta& m) {
  if (m.key == "sync.password" || m.key == "sync.token" || m.key == "api.token" ||
      m.key == "ai.api_key")
    m.secret = true;
  if (m.key.rfind("ui.", 0) == 0) m.live = true;
}

#undef B
#undef I
#undef D
#undef S
#undef L

}  // namespace

const std::vector<SettingMeta>& all_settings() {
  static std::vector<SettingMeta> table;
  if (table.empty()) {
    for (auto& e : kTable) {
      SettingMeta m{e.key, e.type, false, false, {}};
      apply_options(m);
      apply_flags(m);
      table.push_back(std::move(m));
    }
  }
  return table;
}

bool setting_meta(const std::string& dotted, SettingMeta& out) {
  for (auto& m : all_settings())
    if (m.key == dotted) {
      out = m;
      return true;
    }
  return false;
}

}  // namespace wilfred
