#pragma once

#include <string>

namespace wilfred {

enum class ScreenshotMode { Fullscreen, Window, Region };

std::string screenshot_mode_name(ScreenshotMode mode);
bool parse_screenshot_mode(const std::string& s, ScreenshotMode& out);
std::string screenshot_payload(ScreenshotMode mode);
bool parse_screenshot_payload(const std::string& payload, ScreenshotMode& out);

// Captures a screenshot and sets out_path to the saved file.
// Returns true on success. When the platform delegates to an interactive
// OS picker that manages its own files (e.g. Snipping Tool), returns true
// with out_path left empty. On failure returns false and sets error.
bool take_screenshot(ScreenshotMode mode, std::string& out_path, std::string& error);

}  // namespace wilfred
