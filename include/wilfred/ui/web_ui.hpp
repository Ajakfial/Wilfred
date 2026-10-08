#pragma once

#include "wilfred/search/engine.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace wilfred {

std::string overlay_ui_dir();
std::string overlay_json_escape(const std::string& s);
const char* overlay_action_name(ResultAction a);
// Legacy shape (no assist). Kept for tests/back-compat; delegates to the
// assist-aware overload with empty correction/ghost/candidates.
std::string overlay_results_json(const std::vector<SearchResult>& items,
                                 const std::vector<std::string>& habits = {});
// Assist-aware shape consumed by the modern overlay (all platforms):
// {"type":"results","items":[...],"habits":[...],"correction":"...",
//  "ghost":"...","candidates":[...],"query":"..."}
std::string overlay_results_json(const std::vector<SearchResult>& items,
                                 const std::vector<std::string>& habits,
                                 const std::string& correction, const std::string& ghost,
                                 const std::vector<std::string>& candidates,
                                 const std::string& query = {});
bool overlay_json_field(const std::string& json, const char* key, std::string& out);

struct FilePreview {
  std::string title;
  std::string kind;
  std::string size_label;
  std::string modified_label;
  std::string text;            // head of text files / directory listing
  std::string image_data_url;  // data: URL for small images, else empty
  std::string error;
};

// Bounded, side-effect-free file preview for the overlay pane and CLI.
FilePreview build_file_preview(const std::string& path, std::size_t max_text = 2048,
                               std::size_t max_image = 102400);
std::string overlay_preview_json(const std::string& path);

// Overlay appearance driven by `ui:` config (accent, font_size). The service
// calls set_overlay_appearance(cfg) on config load; overlay backends send
// overlay_show_json() instead of a bare {"type":"show"} so the web UI can
// apply the custom theme without a separate config channel.
void set_overlay_appearance(const Config& cfg);
std::string overlay_show_json();

// Settings GUI channel (overlay SettingsPanel). Schema comes from
// config/settings.hpp; values are read from a fresh config load so the GUI
// never shows stale daemon state. Writes go through config_set_value
// (validated, atomic file update); ui.* keys additionally refresh the live
// appearance/locale globals so they apply on next summon.
std::string overlay_settings_json();
std::string overlay_settings_json_from(const Config& cfg);
std::string overlay_setting_set_result(const std::string& key, const std::string& value);

}  // namespace wilfred
