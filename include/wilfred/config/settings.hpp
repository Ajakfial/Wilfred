#pragma once

// Machine-readable schema for the settings editor (overlay GUI, `settings`
// mini, `wilfred config-set`). Single source of truth for which dotted keys
// exist, their value types, secrets, live-apply behavior, and enum options.
//
// Conventions (must stay in sync with config/config.cpp validation):
// - `secret` keys are write-only over every channel: getters return "" and
//   the GUI renders a password field (values are never echoed back).
// - `live` keys take effect without a daemon restart (overlay appearance and
//   locale today); everything else needs a restart after saving.
// - Nested maps (workflows, quicklinks, app_actions, hotkeys, ...) stay
//   file-edited and are intentionally absent here.

#include <string>
#include <vector>

namespace wilfred {

enum class SettingType { Bool, Int, Double, Str, List };

struct SettingMeta {
  std::string key;  // e.g. "search.max_results"
  SettingType type;
  bool secret{false};
  bool live{false};
  // Non-empty for select-style editors (e.g. {"dark", "light"}).
  std::vector<std::string> options;
};

// Every settable key, grouped by section in GUI order.
const std::vector<SettingMeta>& all_settings();
// True + meta when `dotted` is a known setting.
bool setting_meta(const std::string& dotted, SettingMeta& out);

}  // namespace wilfred
