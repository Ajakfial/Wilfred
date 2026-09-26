#include "wilfred/ipc/protocol.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/index/record.hpp"

#include <sstream>

namespace wilfred {

static const char* ipc_action_name(ResultAction a) {
  switch (a) {
    case ResultAction::Reveal:
      return "reveal";
    case ResultAction::Copy:
      return "copy";
    case ResultAction::WebSearch:
      return "web";
    case ResultAction::Calculate:
      return "calc";
    case ResultAction::Convert:
      return "convert";
    case ResultAction::None:
      return "none";
    case ResultAction::Habit:
      return "habit";
    case ResultAction::Mini:
      return "mini";
    case ResultAction::Expand:
      return "expand";
    case ResultAction::Plugin:
      return "plugin";
    default:
      return "open";
  }
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
       << json_escape(it.kind_label.empty() ? std::string(kind_name(it.kind)) : it.kind_label)
       << "\",\"subtitle\":\"" << json_escape(it.subtitle) << "\",\"action\":\""
       << ipc_action_name(it.action) << "\",\"category\":\"" << json_escape(it.category)
       << "\",\"payload\":\"" << json_escape(it.payload) << "\",\"plugin\":\""
       << json_escape(it.plugin_id) << "\"";
    if (!it.actions.empty()) {
      os << ",\"actions\":[";
      bool af = true;
      for (auto& a : it.actions) {
        if (!af) os << ',';
        af = false;
        os << "{\"id\":\"" << json_escape(a.id) << "\",\"label\":\"" << json_escape(a.label) << "\"}";
      }
      os << "]";
    }
    os << "}";
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
  if (line.rfind("BACKUP", 0) == 0) {
    out.cmd = "backup";
    if (line.size() > 7 && line[6] == '\t') out.path = line.substr(7);
    return true;
  }
  if (line.rfind("RESTORE", 0) == 0) {
    out.cmd = "restore";
    if (line.size() > 8 && line[7] == '\t') out.path = line.substr(8);
    return true;
  }
  out.cmd = json_get_string(line, "cmd");
  if (out.cmd.empty()) out.cmd = json_get_string(line, "command");
  out.query = json_get_string(line, "q");
  if (out.query.empty()) out.query = json_get_string(line, "query");
  out.path = json_get_string(line, "path");
  out.action = json_get_string(line, "action");
  out.token = json_get_string(line, "token");
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
