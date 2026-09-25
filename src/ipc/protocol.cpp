#include "wilfred/ipc/protocol.hpp"

#include "wilfred/core/utf8.hpp"

#include <sstream>

namespace wilfred {

static std::string json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

static std::string json_get_string(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\"";
  auto pos = s.find(k);
  if (pos == std::string::npos) return {};
  pos = s.find(':', pos + k.size());
  if (pos == std::string::npos) return {};
  ++pos;
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
  if (pos >= s.size()) return {};
  if (s[pos] == '"') {
    ++pos;
    std::string v;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\' && pos + 1 < s.size()) {
        v.push_back(s[pos + 1]);
        pos += 2;
      } else
        v.push_back(s[pos++]);
    }
    return v;
  }
  std::string v;
  while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ' ') v.push_back(s[pos++]);
  return v;
}

std::string encode_response(const IpcResponse& r) {
  std::ostringstream os;
  os << "{\"ok\":" << (r.ok ? "true" : "false") << ",\"error\":\"" << json_escape(r.error)
     << "\",\"text\":\"" << json_escape(r.text) << "\",\"results\":[";
  bool first = true;
  for (auto& it : r.results) {
    if (!first) os << ',';
    first = false;
    os << "{\"id\":" << it.id << ",\"score\":" << it.score << ",\"title\":\""
       << json_escape(it.title) << "\",\"path\":\"" << json_escape(it.path) << "\",\"kind\":\""
       << json_escape(std::string(kind_name(it.kind))) << "\"}";
  }
  os << "]}";
  return os.str();
}

bool decode_request(const std::string& line, IpcRequest& out) {
  out = {};
  // Also accept: SEARCH<TAB>query
  if (line.rfind("SEARCH\t", 0) == 0) {
    out.cmd = "search";
    out.query = line.substr(7);
    return true;
  }
  if (line.rfind("LAUNCH\t", 0) == 0) {
    out.cmd = "launch";
    out.path = line.substr(7);
    return true;
  }
  if (line.rfind("STATUS", 0) == 0) {
    out.cmd = "status";
    return true;
  }
  out.cmd = json_get_string(line, "cmd");
  if (out.cmd.empty()) out.cmd = json_get_string(line, "command");
  out.query = json_get_string(line, "q");
  if (out.query.empty()) out.query = json_get_string(line, "query");
  out.path = json_get_string(line, "path");
  auto lim = json_get_string(line, "n");
  if (lim.empty()) lim = json_get_string(line, "limit");
  if (!lim.empty()) {
    try {
      out.limit = std::stoi(lim);
    } catch (...) {
    }
  }
  return !out.cmd.empty();
}

}  // namespace wilfred
