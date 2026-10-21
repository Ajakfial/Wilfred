#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/providers/provider.hpp"

#include <string>
#include <vector>

namespace wilfred {

// Password-manager search backed by a CLI. Defaults to the 1Password CLI
// (`op`), configured via sources.vault_command.
//
// Only metadata reaches search results: item id, title, username. The secret
// is fetched when an item is activated, written to the clipboard, and cleared
// again after sources.vault_clear_after_secs. Secrets are never written to the
// index, history, provider cache, or logs.
class VaultProvider : public SearchProvider {
public:
  std::string id() const override { return "vault"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

// Resolves "vault:<id>", copies the secret, and arms the clipboard clearer.
// The secret is wiped from memory on both success and failure.
bool vault_copy_secret_to_clipboard(const Config& cfg, const std::string& item_ref,
                                    std::string& error);

// Drops the cached item list (sign-out, config change, tests).
void vault_forget_cache();

}  // namespace wilfred
