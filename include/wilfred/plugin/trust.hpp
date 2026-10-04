#pragma once

// Trust-on-first-use store for plugins (<data>/plugins/trust.json).
//
// The first time a plugin id is seen its sha256 + permissions are recorded
// as pending. query()/execute() skip pending plugins while
// `plugins.require_approval` is true. `wilfred plugin approve <id>` (or the
// overlay `plugin_approve:<id>` action) marks it trusted; any later hash or
// permission change flips it back to pending.

#include <string>
#include <vector>

namespace wilfred {

struct TrustEntry {
  std::string id;
  std::string sha256;
  std::vector<std::string> permissions;
  bool approved{false};
};

std::string plugin_trust_path();
std::vector<TrustEntry> plugin_trust_load();
bool plugin_trust_save(const std::vector<TrustEntry>& entries);
// Fingerprint of a manifest: sha256 field when present, else the resolved
// native lib / stdio command path hash (empty when nothing hashes).
std::string plugin_fingerprint(const std::string& id, const std::string& sha_field,
                               const std::string& resolved_path);
bool plugin_is_approved(const std::vector<TrustEntry>& trust, const std::string& id,
                        const std::string& fingerprint,
                        const std::vector<std::string>& permissions);
bool plugin_trust_approve(const std::string& id, std::string& error);
bool plugin_trust_revoke(const std::string& id, std::string& error);

}  // namespace wilfred
