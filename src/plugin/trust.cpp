#include "wilfred/plugin/trust.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/search/file_ops.hpp"

#include <sstream>

namespace wilfred {

std::string plugin_trust_path() { return path_join(path_join(data_directory(), "plugins"), "trust.json"); }

std::vector<TrustEntry> plugin_trust_load() {
  std::vector<TrustEntry> out;
  std::string blob;
  if (!read_file_all(plugin_trust_path(), blob) || blob.empty()) return out;
  auto arr = json_extract_array(blob, "trust");
  if (arr.empty()) return out;
  for (auto& obj : json_object_array(arr)) {
    TrustEntry e;
    e.id = json_get_string(obj, "id");
    if (e.id.empty()) continue;
    e.sha256 = to_lower_utf8(json_get_string(obj, "sha256"));
    e.approved = json_get_bool(obj, "approved", false);
    auto parr = json_extract_array(obj, "permissions");
    for (auto& p : json_object_array(parr)) {
      (void)p;
    }
    // permissions may be a string array: parse manually.
    std::string plist = parr;
    std::size_t pos = 0;
    while ((pos = plist.find('"', pos)) != std::string::npos) {
      auto end = plist.find('"', pos + 1);
      if (end == std::string::npos) break;
      auto v = plist.substr(pos + 1, end - pos - 1);
      if (!v.empty()) e.permissions.push_back(to_lower_utf8(v));
      pos = end + 1;
    }
    out.push_back(std::move(e));
  }
  return out;
}

bool plugin_trust_save(const std::vector<TrustEntry>& entries) {
  std::ostringstream os;
  os << "{\"trust\":[";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (i) os << ",";
    auto& e = entries[i];
    os << "{\"id\":\"" << json_escape(e.id) << "\",\"sha256\":\"" << json_escape(e.sha256)
       << "\",\"approved\":" << (e.approved ? "true" : "false") << ",\"permissions\":[";
    for (std::size_t j = 0; j < e.permissions.size(); ++j) {
      if (j) os << ",";
      os << "\"" << json_escape(e.permissions[j]) << "\"";
    }
    os << "]}";
  }
  os << "]}";
  auto blob = os.str();
  create_directories(path_join(data_directory(), "plugins"));
  return write_file_atomic(plugin_trust_path(), blob.data(), blob.size());
}

std::string plugin_fingerprint(const std::string& id, const std::string& sha_field,
                               const std::string& resolved_path) {
  (void)id;
  auto fp = to_lower_utf8(sha_field);
  if (!fp.empty()) return fp;
  if (!resolved_path.empty()) {
    std::string hex, err;
    if (sha256_file(resolved_path, hex, err) && !hex.empty()) return to_lower_utf8(hex);
  }
  return {};
}

static bool same_perms(const std::vector<std::string>& a, const std::vector<std::string>& b) {
  if (a.size() != b.size()) return false;
  for (auto& x : a) {
    bool found = false;
    for (auto& y : b)
      if (to_lower_utf8(x) == to_lower_utf8(y)) {
        found = true;
        break;
      }
    if (!found) return false;
  }
  return true;
}

bool plugin_is_approved(const std::vector<TrustEntry>& trust, const std::string& id,
                        const std::string& fingerprint,
                        const std::vector<std::string>& permissions) {
  for (auto& e : trust) {
    if (e.id != id) continue;
    if (!e.approved) return false;
    if (!fingerprint.empty() && !e.sha256.empty() && e.sha256 != to_lower_utf8(fingerprint))
      return false;
    if (!same_perms(e.permissions, permissions)) return false;
    return true;
  }
  return false;
}

bool plugin_trust_approve(const std::string& id, std::string& error) {
  // Resolve current fingerprint from discovered manifests is done by the
  // caller (registry approve path); this low-level helper records an
  // approval when the caller already knows hash+permissions. Kept for tests.
  auto trust = plugin_trust_load();
  for (auto& e : trust) {
    if (e.id == id) {
      e.approved = true;
      if (!plugin_trust_save(trust)) {
        error = "cannot write trust store";
        return false;
      }
      return true;
    }
  }
  error = "unknown plugin '" + id + "' (run: wilfred plugin pending)";
  return false;
}

bool plugin_trust_revoke(const std::string& id, std::string& error) {
  auto trust = plugin_trust_load();
  for (auto& e : trust) {
    if (e.id == id) {
      e.approved = false;
      if (!plugin_trust_save(trust)) {
        error = "cannot write trust store";
        return false;
      }
      return true;
    }
  }
  error = "unknown plugin '" + id + "'";
  return false;
}

}  // namespace wilfred
