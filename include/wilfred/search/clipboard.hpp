#pragma once

#include <optional>
#include <string>
#include <vector>

namespace wilfred {

struct ClipboardSnapshot {
  std::string text;
  std::vector<std::string> paths;
};

ClipboardSnapshot read_clipboard();
bool write_clipboard(const std::string& text);
std::vector<std::string> clipboard_history_texts();

// Tests inject clipboard without touching the OS pasteboard.
void set_clipboard_override(std::optional<ClipboardSnapshot> snap);

std::vector<std::string> clipboard_path_hints(const ClipboardSnapshot& snap);
bool clipboard_text_matches(const std::string& query, const std::string& text);
std::string clipboard_preview(const std::string& text, std::size_t max_chars = 90);

}  // namespace wilfred
