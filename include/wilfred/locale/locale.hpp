#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

// Dependency-free UI localization.
//
// - English is compiled in and always available (unknown keys and unknown
//   languages fall back to English, never to an empty string).
// - `lang/<code>.yml` flat `key: "value"` mappings override English.
//   Lookup order: `<config dir>/lang/<code>.yml` (user overrides), then
//   `share/wilfred/lang/<code>.yml` next to the binary / install prefix /
//   CWD (shipped catalogs). Add a language by dropping a file — no code
//   changes needed (see docs/localization.md).
// - Values support `{name}` placeholders via the tr() overload with args.
// - `ui.language` accepts `auto` (OS locale), `en`, or any
//   two-letter code (`de`, `fr`, ...) optionally with region (`pt-BR`).
class LocaleStore {
 public:
  static LocaleStore& instance();

  // code: "auto" (default), "en", "de", ... Effective code resolves at
  // load(): requested -> base language -> "en".
  void configure(const std::string& code);
  // (Re)load the catalog for the configured code. Returns true when a
  // non-English catalog was loaded.
  bool load();
  // Load one catalog file directly (tests, embedding). Merges over English.
  bool load_from(const std::string& path);

  std::string requested_code() const;
  std::string code() const;  // effective code, e.g. "de" (or "en")

  std::string tr(const std::string& key) const;
  std::string tr(const std::string& key,
                 const std::unordered_map<std::string, std::string>& args) const;

  // Overlay chrome strings (placeholder, empty states, bar, primary
  // labels) for the show/config message. Never empty: English fallback.
  std::unordered_map<std::string, std::string> overlay_strings() const;

  static bool valid_code_syntax(const std::string& code);
  static std::vector<std::string> shipped_codes();  // {"en","de","fr","es"}
  // Every key in the compiled-in English table (for catalog validation).
  static std::vector<std::string> english_keys();

 private:
  LocaleStore() = default;
  mutable std::mutex mu_;
  std::string requested_{"auto"};
  std::string effective_{"en"};
  std::unordered_map<std::string, std::string> strings_;
};

// OS display language: "de", "fr", ... ("en" when unknown). Never empty.
std::string system_language_code();
// "de_DE.UTF-8" -> "de", "FR-fr" -> "fr", "eng" -> "en", "" -> "en".
std::string normalize_lang_code(const std::string& raw);

// Shorthand: LocaleStore::instance().tr(key).
inline std::string tr(const std::string& key) { return LocaleStore::instance().tr(key); }
inline std::string tr(const std::string& key,
                      const std::unordered_map<std::string, std::string>& args) {
  return LocaleStore::instance().tr(key, args);
}

}  // namespace wilfred
