#include "wilfred/plugin/registry.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/plugin/trust.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/remote.hpp"
#include "wilfred/updater/updater.hpp"

#include <cstdio>
#include <filesystem>
#include <iostream>

namespace wilfred {
namespace fs = std::filesystem;

std::vector<RegistryEntry> plugin_registry_parse(const std::string& body) {
  std::vector<RegistryEntry> out;
  if (body.empty() || body.size() > 1024 * 1024) return out;
  auto arr = json_extract_array(body, "plugins");
  std::string src = arr.empty() ? body : arr;
  if (src.empty()) return out;
  for (auto& obj : json_object_array(src)) {
    RegistryEntry e;
    e.id = json_get_string(obj, "id");
    if (e.id.empty()) continue;
    e.version = json_get_string(obj, "version");
    e.kind = to_lower_utf8(json_get_string(obj, "kind"));
    if (e.kind.empty()) e.kind = "stdio";
    e.url = json_get_string(obj, "url");
    if (e.url.empty()) continue;
    e.sha256 = to_lower_utf8(json_get_string(obj, "sha256"));
    e.description = json_get_string(obj, "description");
    auto parr = json_extract_array(obj, "permissions");
    std::size_t pos = 0;
    while ((pos = parr.find('"', pos)) != std::string::npos) {
      auto end = parr.find('"', pos + 1);
      if (end == std::string::npos) break;
      auto v = parr.substr(pos + 1, end - pos - 1);
      if (!v.empty()) e.permissions.push_back(to_lower_utf8(v));
      pos = end + 1;
    }
    out.push_back(std::move(e));
    if (out.size() >= 100) break;
  }
  return out;
}

std::vector<RegistryEntry> plugin_registry_fetch(const std::string& url, int timeout_ms,
                                                std::string& error) {
  error.clear();
  if (url.empty()) {
    error = "plugins.registry is not configured";
    return {};
  }
  auto body = remote_fetch(url, timeout_ms > 0 ? timeout_ms : 10000);
  if (body.empty()) {
    error = "cannot fetch registry (network or empty index)";
    return {};
  }
  auto entries = plugin_registry_parse(body);
  if (entries.empty()) error = "registry has no plugins";
  return entries;
}

static std::string registry_url(const Config& cfg) { return cfg.plugins.registry; }

int run_plugin_list() {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  auto url = registry_url(cfg);
  if (url.empty()) {
    // No registry: list installed manifests instead.
    PluginHost host;
    host.load(cfg);
    auto& ms = host.manifests();
    if (ms.empty()) {
      std::cout << "no plugins installed (" << default_plugin_directories().front() << ")\n"
                << "set plugins.registry to browse a registry index.\n";
      return 0;
    }
    for (auto& m : ms)
      std::cout << m.id << "  [" << m.kind << "]" << (m.enabled ? "" : " (disabled)") << "\n";
    return 0;
  }
  std::string fetch_err;
  auto entries = plugin_registry_fetch(url, cfg.plugins.timeout_ms, fetch_err);
  if (entries.empty()) {
    std::cerr << fetch_err << "\n";
    return 1;
  }
  for (auto& e : entries) {
    std::cout << e.id;
    if (!e.version.empty()) std::cout << "  v" << e.version;
    std::cout << "  [" << e.kind << "]";
    if (!e.description.empty()) std::cout << " - " << e.description;
    std::cout << "\n";
  }
  return 0;
}

int run_plugin_pending() {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  PluginHost host;
  host.load(cfg);
  auto trust = plugin_trust_load();
  bool any = false;
  for (auto& m : host.manifests()) {
    if (!m.enabled) continue;
    std::string resolved = m.kind == "native" ? m.path : m.command;
    auto fp = plugin_fingerprint(m.id, m.sha256, resolved);
    if (cfg.plugins.require_approval && !plugin_is_approved(trust, m.id, fp, m.permissions)) {
      any = true;
      std::cout << m.id;
      if (!m.version.empty()) std::cout << "  v" << m.version;
      std::cout << "  [" << m.kind << "]";
      if (!m.permissions.empty()) {
        std::cout << "  permissions:";
        for (auto& p : m.permissions) std::cout << " " << p;
      }
      if (!fp.empty()) std::cout << "  sha256:" << fp.substr(0, 12) << "...";
      std::cout << "\n";
    }
  }
  if (!any) std::cout << "no pending plugins\n";
  return 0;
}

bool plugin_registry_install(const RegistryEntry& entry, std::string& error) {
  error.clear();
  if (entry.id.empty() || entry.url.empty()) {
    error = "invalid registry entry";
    return false;
  }
  for (char c : entry.id) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.';
    if (!ok) {
      error = "unsafe plugin id";
      return false;
    }
  }
  auto dir = path_join(path_join(data_directory(), "plugins"), entry.id);
  create_directories(dir);
  auto tmp = path_join(dir, ".download");
  std::string dl_err;
  bool dl_ok = download_file(entry.url, tmp, nullptr, nullptr, 30000,
                             entry.url.rfind("https://", 0) == 0 || entry.url.rfind("http://", 0) == 0
                                 ? &dl_err
                                 : nullptr);
  if (!dl_ok) {
    error = dl_err.empty() ? "download failed" : dl_err;
    return false;
  }
  if (!entry.sha256.empty()) {
    std::string hex, herr;
    if (!sha256_file(tmp, hex, herr) || to_lower_utf8(hex) != to_lower_utf8(entry.sha256)) {
      remove_file(tmp);
      error = "sha256 mismatch (not installing)";
      return false;
    }
  }
  bool is_archive = entry.url.size() > 4 &&
                    to_lower_utf8(entry.url.substr(entry.url.size() - 4)) == ".zip";
  std::string artifact;
  if (is_archive) {
    std::string xerr;
    if (!extract_archive(tmp, dir, &xerr)) {
      remove_file(tmp);
      error = xerr.empty() ? "cannot extract archive" : xerr;
      return false;
    }
    remove_file(tmp);
    artifact = dir;
  } else {
    auto base = entry.url.substr(entry.url.find_last_of("/\\") + 1);
    auto qm = base.find('?');
    if (qm != std::string::npos) base.resize(qm);
    if (base.empty() || base.size() > 128) base = entry.id;
    artifact = path_join(dir, base);
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(artifact), ec);
    if (ec) {
      error = "cannot stage artifact";
      return false;
    }
#ifndef _WIN32
    fs::permissions(fs::u8path(artifact),
                    fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::add, ec);
#endif
  }
  // Write a manifest so discovery picks it up.
  std::string yml = "id: " + entry.id + "\nkind: " + (entry.kind.empty() ? "stdio" : entry.kind) +
                    "\n";
  if ((entry.kind.empty() || entry.kind == "stdio"))
    yml += "command: " + artifact + "\n";
  else
    yml += "path: " + artifact + "\n";
  if (!entry.version.empty()) yml += "version: " + entry.version + "\n";
  if (!entry.sha256.empty()) yml += "sha256: " + entry.sha256 + "\n";
  if (!entry.description.empty()) yml += "# " + entry.description + "\n";
  if (!entry.permissions.empty()) {
    yml += "permissions: [";
    for (std::size_t i = 0; i < entry.permissions.size(); ++i) {
      if (i) yml += ", ";
      yml += entry.permissions[i];
    }
    yml += "]\n";
  }
  if (!entry.url.empty()) yml += "# origin: " + entry.url + "\n";
  auto mf = path_join(dir, "plugin.yml");
  if (!write_file_atomic(mf, yml.data(), yml.size())) {
    error = "cannot write manifest";
    return false;
  }
  return true;
}

int run_plugin_install(const std::string& id) {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  auto url = registry_url(cfg);
  if (url.empty()) {
    std::cerr << "plugins.registry is not configured\n";
    return 1;
  }
  std::string fetch_err;
  auto entries = plugin_registry_fetch(url, cfg.plugins.timeout_ms, fetch_err);
  for (auto& e : entries) {
    if (e.id == id) {
      std::string ierr;
      if (!plugin_registry_install(e, ierr)) {
        std::cerr << ierr << "\n";
        return 1;
      }
      std::cout << "installed " << e.id;
      if (!e.version.empty()) std::cout << " v" << e.version;
      std::cout << " — approve with: wilfred plugin approve " << e.id << "\n";
      return 0;
    }
  }
  std::cerr << "unknown plugin '" + id + "'\n";
  return 1;
}

int run_plugin_approve(const std::string& id, bool all) {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  PluginHost host;
  host.load(cfg);
  auto trust = plugin_trust_load();
  bool changed = false;
  int n = 0;
  for (auto& m : host.manifests()) {
    if (!all && m.id != id) continue;
    if (!m.enabled) continue;
    std::string resolved = m.kind == "native" ? m.path : m.command;
    if (resolved.empty()) resolved = m.path;
    auto fp = plugin_fingerprint(m.id, m.sha256, resolved);
    bool found = false;
    for (auto& e : trust) {
      if (e.id == m.id) {
        e.sha256 = fp;
        e.permissions = m.permissions;
        e.approved = true;
        found = true;
        break;
      }
    }
    if (!found) {
      TrustEntry e;
      e.id = m.id;
      e.sha256 = fp;
      e.permissions = m.permissions;
      e.approved = true;
      trust.push_back(std::move(e));
    }
    changed = true;
    ++n;
    if (!all) break;
  }
  if (!changed) {
    std::cerr << "unknown plugin '" + id + "'\n";
    return 1;
  }
  if (!plugin_trust_save(trust)) {
    std::cerr << "cannot write trust store\n";
    return 1;
  }
  std::cout << "approved " << (all ? std::to_string(n) + " plugins" : id) << "\n";
  return 0;
}

int run_plugin_revoke(const std::string& id) {
  std::string err;
  if (!plugin_trust_revoke(id, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << "revoked " << id << "\n";
  return 0;
}

}  // namespace wilfred
