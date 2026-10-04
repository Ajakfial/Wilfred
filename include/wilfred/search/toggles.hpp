#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <utility>
#include <vector>

namespace wilfred {

// Pure parsing helpers (unit-tested, no OS calls).
enum class ToggleOp { Status, On, Off, Toggle };

ToggleOp parse_toggle_arg(const std::string& remainder);
// Volume: returns true when remainder selects a concrete action.
// kind: "status" | "set" | "mute" | "unmute" | "toggle_mute" | "up" | "down"
bool parse_volume_arg(const std::string& remainder, std::string& kind, int& level);
// Brightness: kind "status" | "set" | "up" | "down"
bool parse_brightness_arg(const std::string& remainder, std::string& kind, int& level);

// Canonical settings page ids: "" (main), wifi, network, bluetooth,
// sound, display, battery, power, apps, privacy, update, about.
std::string normalize_settings_page(const std::string& remainder);
std::vector<std::pair<std::string, std::string>> settings_page_list();

// Mini result builders. Each calls the native_* status getters (fast,
// bounded) and returns actionable cards with category "toggle" or
// "settings". Payloads are `toggle:<what>:<op>` / `settings:<page>` /
// `config:<op>` and are executed in execute_result_action.
std::vector<SearchResult> wifi_results(const std::string& remainder, const Config& cfg);
std::vector<SearchResult> bluetooth_results(const std::string& remainder, const Config& cfg);
std::vector<SearchResult> volume_results(const std::string& remainder, const Config& cfg);
std::vector<SearchResult> brightness_results(const std::string& remainder, const Config& cfg);
std::vector<SearchResult> settings_results(const std::string& remainder, const Config& cfg);

// Execute a toggle/settings payload. Returns true when payload was handled
// (success = native call ok). error holds the failure reason.
bool execute_toggle_payload(const std::string& payload, const Config& cfg, std::string& error);

}  // namespace wilfred
