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

// Validate wilfred.yml text with the same loader the daemon uses.
// Returns true when valid; message holds the human-readable error or "ok".
bool validate_config_text(const std::string& text, std::string& message);

// One-line status used by `wilfred status` and the setup cards.
std::string setup_summary(const Config& cfg);

// Terminal wizard: prompts for index roots / hotkey / browser template,
// writes missing keys back to wilfred.yml (never overwrites existing keys
// unless `overwrite` is true). Returns exit code (0 = ok).
int run_setup_wizard(bool overwrite);

// CLI helpers.
int run_config_validate();
int run_config_open();
int run_config_path();

}  // namespace wilfred
