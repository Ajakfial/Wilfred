#include "wilfred/search/content.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

namespace wilfred {

bool looks_like_text_extension(std::string_view ext) {
  auto e = to_lower_utf8(ext);
  if (e.empty()) return false;
  if (e[0] != '.') e.insert(e.begin(), '.');
  static const char* extra[] = {
      ".log",  ".csv",  ".json", ".xml",  ".html", ".htm",  ".md",   ".txt",  ".rst",
      ".toml", ".yml",  ".yaml", ".ini",  ".cfg",  ".conf", ".cpp",  ".cc",   ".cxx",
      ".c",    ".h",    ".hpp",  ".hh",   ".cs",   ".java", ".js",   ".jsx",  ".ts",
      ".tsx",  ".py",   ".rb",   ".go",   ".rs",   ".php",  ".swift",".kt",   ".scala",
      ".sql",  ".sh",   ".ps1",  ".bat",  ".css",  ".scss", ".less", ".vue",  ".svelte",
      ".cmake",".gradle",".pl",  ".lua",  ".r",    ".tex",  ".bib",  ".org",  ".rtf",
      ".svg",  ".gitignore", ".editorconfig", ".env", nullptr};
  for (auto** p = extra; *p; ++p)
    if (e == *p) return true;
  return false;
}

bool content_indexable(FileKind kind, std::string_view path) {
  if (kind == FileKind::Source || kind == FileKind::Config || kind == FileKind::Document)
    return true;
  return looks_like_text_extension(path_extension(path));
}

std::vector<std::string> extract_content_tokens(const std::string& text, int max_tokens) {
  return tokenize_content(text, max_tokens > 0 ? static_cast<std::size_t>(max_tokens) : 480);
}

}  // namespace wilfred
