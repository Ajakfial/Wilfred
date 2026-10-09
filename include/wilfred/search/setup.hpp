#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

// First-run wizard + `settings` config cards.
//
// The wizard is deliberately non-interactive inside the overlay: `setup`
// cards report what is missing (index roots, hotkey, browser template) and
// offer one-key actions to open/reveal/validate wilfred.yml. The real
// prompting happens in `wilfred setup` on the terminal (run_setup_wizard).
std::vector<SearchResult> setup_results(const std::string& remainder, const Config& cfg);
std::vector<SearchResult> config_results(const std::string& remainder, const Config& cfg);

// New-version card backed by the daemon's background startup check
// (`startup_update_check` + `pending_update`). Read-only: Enter copies the
// release download link, installing stays `wilfred update` on the terminal.
std::vector<SearchResult> update_results(const std::string& remainder, const Config& cfg);

// Validate wilfred.yml text with the same loader the daemon uses.
// Returns true when valid; message holds the human-readable error or "ok".
bool validate_config_text(const std::string& text, std::string& message);

// One-line status used by `wilfred status` and the setup cards.
std::string setup_summary(const Config& cfg);

// Typed settings editor (overlay `settings edit/get` + CLI
// `wilfred config-get/set`). Dotted keys are `section.key`, e.g.
// `search.max_results`, `browser.search_template`, `hotkey.key`.
// Lists use comma-separated values, e.g. `index.paths`.
bool config_get_value(const Config& cfg, const std::string& dotted, std::string& out,
                      std::string& error);
bool config_set_value(const std::string& dotted, const std::string& value, std::string& error);
bool config_reset_default(std::string& error);

// Terminal wizard: prompts for index roots / hotkey / browser template,
// writes missing keys back to wilfred.yml (never overwrites existing keys
// unless `overwrite` is true). Returns exit code (0 = ok).
int run_setup_wizard(bool overwrite);

// CLI helpers.
int run_config_validate();
int run_config_open();
int run_config_path();
int run_config_get(const std::string& dotted);
int run_config_set(const std::string& dotted, const std::string& value);
int run_config_reset();

}  // namespace wilfred
