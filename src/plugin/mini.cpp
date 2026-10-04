#include "wilfred/plugin/mini.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/plugin/trust.hpp"

namespace wilfred {
namespace {

SearchResult pcard(const std::string& title, const std::string& sub, const std::string& payload) {
  SearchResult r;
  r.title = title;
  r.subtitle = sub;
  r.payload = payload;
  r.path = payload;
  r.action = ResultAction::Mini;
  r.score = 10000;
  r.kind_label = "plugin";
  r.category = "plugins";
  return r;
}

std::string trim_ws(const std::string& s) {
  std::string o = s;
  while (!o.empty() && (o.front() == ' ' || o.front() == '\t')) o.erase(o.begin());
  while (!o.empty() && (o.back() == ' ' || o.back() == '\t' || o.back() == '\r')) o.pop_back();
  return o;
}

}  // namespace

std::vector<SearchResult> plugin_results(const std::string& remainder, const Config& cfg) {
  std::vector<SearchResult> out;
  PluginHost host;
  host.load(cfg);
  auto trust = plugin_trust_load();
  auto l = to_lower_utf8(trim_ws(remainder));

  auto is_pending = [&](const PluginManifest& m) {
    if (!m.enabled) return false;
    if (!cfg.plugins.require_approval) return false;
    std::string resolved = m.kind == "native" ? m.path : (m.command.empty() ? m.path : m.command);
    return !plugin_is_approved(trust, m.id, plugin_fingerprint(m.id, m.sha256, resolved),
                               m.permissions);
  };

  if (l.rfind("approve ", 0) == 0 || l == "approve") {
    auto id = trim_ws(remainder.size() > 7 ? remainder.substr(7) : "");
    if (id.empty() || to_lower_utf8(id) == "all") {
      int n = 0;
      for (auto& m : host.manifests())
        if (is_pending(m)) ++n;
      out.push_back(pcard(n ? "Approve all plugins (" + std::to_string(n) + ")" : "No pending plugins",
                          n ? "trust-on-first-use — records hash + permissions"
                            : "every installed plugin is approved",
                          "plugin_approve_all"));
      return out;
    }
    out.push_back(pcard("Approve plugin " + id, "records hash + permissions — enter approves",
                        "plugin_approve:" + id));
    return out;
  }

  // Default: pending approvals first, then installed list.
  bool any_pending = false;
  for (auto& m : host.manifests()) {
    if (!is_pending(m)) continue;
    any_pending = true;
    std::string sub = m.kind;
    if (!m.version.empty()) sub += " v" + m.version;
    if (!m.permissions.empty()) {
      sub += " ·";
      for (auto& p : m.permissions) sub += " " + p;
    }
    sub += " — enter approves";
    out.push_back(pcard("Approve " + m.id, sub, "plugin_approve:" + m.id));
  }
  if (any_pending) {
    out.push_back(pcard("Approve all plugins", "trust every pending plugin",
                        "plugin_approve_all"));
  }
  if (host.manifests().empty()) {
    out.push_back(pcard("No plugins installed", "drop a plugin.yml in " +
                                                     (cfg.plugins.directories.empty()
                                                          ? "<config>/plugins"
                                                          : cfg.plugins.directories.front()),
                        "config:open"));
    if (!cfg.plugins.registry.empty())
      out.push_back(pcard("Browse registry", "run: wilfred plugin list", "config:open"));
    return out;
  }
  for (auto& m : host.manifests()) {
    std::string status;
    if (!m.enabled) status = "disabled";
    else if (is_pending(m)) status = "pending approval";
    else status = "approved";
    std::string sub = m.kind + " · " + status;
    if (!m.version.empty()) sub += " · v" + m.version;
    out.push_back(pcard(m.id, sub, is_pending(m) ? ("plugin_approve:" + m.id) : "config:open"));
    if (out.size() >= 10) break;
  }
  return out;
}

}  // namespace wilfred
