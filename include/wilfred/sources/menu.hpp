#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/providers/provider.hpp"

#include <string>
#include <vector>

namespace wilfred {

// Desktop-only search over the focused application's menu tree.
//
// Enumeration costs tens to hundreds of milliseconds, so the result is cached
// for a few seconds and keyed by the foreground app's id. Items are exposed as
// metadata only; activation re-resolves the item by id.
class MenuProvider : public SearchProvider {
public:
  std::string id() const override { return "menu"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

void menu_forget_cache();

}  // namespace wilfred
