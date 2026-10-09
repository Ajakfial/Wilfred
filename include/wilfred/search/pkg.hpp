#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

// System package-manager search (winget, brew, apt, choco, flatpak,
// pacman). Fires only on explicit `<manager> <query>` invocations — never
// on plain file searches — so there is no per-keystroke cost and no
// surprise network traffic. Each manager shells out to its own CLI with
// non-interactive flags under a hard timeout; a missing/slow tool yields
// no results instead of an error.
struct PkgHit {
  std::string name;     // display name, e.g. "Mozilla Firefox"
  std::string id;       // install id, e.g. "Mozilla.Firefox"
  std::string version;  // may be empty when the tool omits it
  std::string source;   // repo/source column, may be empty
};

struct PkgIntent {
  bool matched{false};
  std::string manager;  // canonical id: winget | brew | apt | choco | flatpak | pacman
  std::string query;    // remainder after the manager prefix
};

// Pure helpers (unit-tested, no subprocesses).
bool pkg_is_manager(const std::string& word);
PkgIntent pkg_parse_intent(const std::string& text);
std::vector<std::string> pkg_known_managers();
// Managers with a usable CLI on PATH right now (fast `where`/`command -v`
// probes, no network). Empty on mobile.
std::vector<std::string> pkg_detected_managers(const Config& cfg);
// Table/list parsers, one per tool output shape.
std::vector<PkgHit> pkg_parse_winget(const std::string& out);
std::vector<PkgHit> pkg_parse_brew(const std::string& out);
std::vector<PkgHit> pkg_parse_apt(const std::string& out);
std::vector<PkgHit> pkg_parse_choco(const std::string& out);
std::vector<PkgHit> pkg_parse_flatpak(const std::string& out);
std::vector<PkgHit> pkg_parse_pacman(const std::string& out);
// Shell command a user would type to install the hit (copied on copy_text).
std::string pkg_install_command(const std::string& manager, const std::string& id,
                                const std::string& source = "");

// Mini cards for `packages [filter]`: one row per known manager with
// detected status, plus usage examples. Empty remainder lists all.
std::vector<SearchResult> pkg_managers_results(const std::string& remainder, const Config& cfg);
// Actually install (blocking; generous timeout — installs take minutes).
// Returns false with error set on failure or when the manager needs an
// interactive terminal (apt without polkit).
bool pkg_install(const std::string& manager, const std::string& id, std::string& error);

class PkgProvider : public SearchProvider {
public:
  std::string id() const override { return "pkg"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

}  // namespace wilfred
