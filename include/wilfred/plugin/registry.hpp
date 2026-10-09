#pragma once

// Optional plugin registry index: a JSON document listing installable
// plugins. Disabled unless `plugins.registry` is set.
//
// Format:
//   {"plugins":[
//     {"id":"demo","version":"1.0","kind":"stdio",
//      "url":"https://example.com/plugins/demo.zip",
//      "sha256":"<hex>","description":"...","permissions":["network"]}
//   ]}
//
// `wilfred plugin list` prints entries; `install` downloads the artifact,
// verifies sha256, and writes plugins/<id>/plugin.yml so discovery picks it
// up on the next load. No auto-install, no auto-update.

#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

struct RegistryEntry {
  std::string id;
  std::string version;
  std::string kind;
  std::string url;
  std::string sha256;
  std::string description;
  std::vector<std::string> permissions;
  // Optional per-platform artifacts for native plugins:
  // artifacts: {windows: {url, sha256}, macos: {...}, linux: {...}}.
  // Install picks this OS and falls back to url/sha256.
  struct Artifact {
    std::string url;
    std::string sha256;
  };
  std::unordered_map<std::string, Artifact> artifacts;
};

std::string plugin_registry_artifact_os();  // "windows" | "macos" | "linux"
bool plugin_registry_resolve(const RegistryEntry& e, std::string& url, std::string& sha256);

std::vector<RegistryEntry> plugin_registry_parse(const std::string& body);
std::vector<RegistryEntry> plugin_registry_fetch(const std::string& url, int timeout_ms,
                                                 std::string& error);
bool plugin_registry_install(const RegistryEntry& entry, std::string& error);

int run_plugin_list();
int run_plugin_pending();
int run_plugin_install(const std::string& id);
int run_plugin_approve(const std::string& id, bool all);
int run_plugin_revoke(const std::string& id);

}  // namespace wilfred
