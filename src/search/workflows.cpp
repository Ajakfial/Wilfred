#include "wilfred/search/workflows.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/macros.hpp"

#include <algorithm>
#include <cctype>

namespace wilfred {
namespace {

std::string trim_copy(const std::string& s) {
  std::string o = s;
  while (!o.empty() && (o.front() == ' ' || o.front() == '\t')) o.erase(o.begin());
  while (!o.empty() && (o.back() == ' ' || o.back() == '\t')) o.pop_back();
  return o;
}

std::vector<std::string> split_args(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  bool in_q = false;
  char q = 0;
  for (char c : s) {
    if (!in_q && (c == '"' || c == '\'')) {
      in_q = true;
      q = c;
      continue;
    }
    if (in_q && c == q) {
      in_q = false;
      continue;
    }
    if (!in_q && (c == ' ' || c == '\t')) {
      if (!cur.empty()) {
        out.push_back(cur);
        cur.clear();
      }
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

}  // namespace

QuicklinkMatch match_quicklink(const std::string& query, const Config& cfg) {
  QuicklinkMatch out;
  if (cfg.quicklinks.empty()) return out;
  std::string s = trim_copy(query);
  if (s.empty()) return out;
  std::string body = s;
  // Explicit `ql <name> <args>` form.
  auto l = to_lower_utf8(s);
  if (l.rfind("ql ", 0) == 0 || l.rfind("quicklink ", 0) == 0 || l.rfind("link ", 0) == 0) {
    auto sp = s.find(' ');
    body = trim_copy(s.substr(sp + 1));
    if (body.empty()) return out;
    // Fall through to name/args split below.
  } else if (!s.empty() && (s[0] == '!' || s[0] == '/')) {
    body = trim_copy(s.substr(1));
  } else {
    // Bare `name args` or `name:args` only when name is a known quicklink.
    // This keeps file search intact for unknown words.
  }
  std::string name, args;
  auto colon = body.find(':');
  auto space = body.find(' ');
  if (colon != std::string::npos && (space == std::string::npos || colon < space) && colon > 0 &&
      colon < 48) {
    name = to_lower_utf8(trim_copy(body.substr(0, colon)));
    args = trim_copy(body.substr(colon + 1));
  } else if (space != std::string::npos) {
    name = to_lower_utf8(trim_copy(body.substr(0, space)));
    args = trim_copy(body.substr(space + 1));
  } else {
    name = to_lower_utf8(trim_copy(body));
  }
  auto it = cfg.quicklinks.find(name);
  if (it == cfg.quicklinks.end()) return out;
  // Bare single-word invocation without args and without explicit ql!/:/ prefix
  // only counts when the template needs no args (rare). Otherwise require an
  // explicit form so file search keeps working.
  bool explicit_form = (!s.empty() && (s[0] == '!' || s[0] == '/')) ||
                       l.rfind("ql ", 0) == 0 || l.rfind("quicklink ", 0) == 0 ||
                       l.rfind("link ", 0) == 0 || colon != std::string::npos ||
                       space != std::string::npos;
  if (!explicit_form && args.empty()) {
    // Allow it: quicklink with no args still opens the template with empty query.
  }
  out.matched = true;
  out.name = name;
  out.tmpl = it->second;
  out.args = args;
  return out;
}

std::string expand_quicklink(const std::string& tmpl, const std::string& args,
                             const std::string& clipboard) {
  auto parts = split_args(args);
  std::string all = args;
  std::string enc_all = percent_encode(all);
  std::string clip_enc = percent_encode(clipboard);
  std::string out = tmpl;
  auto subst = [](std::string s, const std::string& key, const std::string& val) {
    std::string o;
    std::size_t i = 0;
    while (i < s.size()) {
      auto p = s.find(key, i);
      if (p == std::string::npos) {
        o.append(s, i, std::string::npos);
        break;
      }
      o.append(s, i, p - i);
      o += val;
      i = p + key.size();
    }
    return o;
  };
  // Numbered args first: {1_enc} then {1}, so {1} does not eat the suffix.
  for (int i = static_cast<int>(parts.size()); i >= 1; --i) {
    out = subst(out, "{" + std::to_string(i) + "_enc}", percent_encode(parts[i - 1]));
    out = subst(out, "{" + std::to_string(i) + "}", parts[i - 1]);
  }
  // Missing positional args expand to empty.
  for (int i = 1; i <= 9; ++i) {
    out = subst(out, "{" + std::to_string(i) + "_enc}", "");
    out = subst(out, "{" + std::to_string(i) + "}", "");
  }
  out = subst(out, "{*_enc}", enc_all);
  out = subst(out, "{*}", all);
  out = subst(out, "{clipboard_enc}", clip_enc);
  out = subst(out, "{clip_enc}", clip_enc);
  out = subst(out, "{query_enc}", enc_all);
  out = subst(out, "{q_enc}", enc_all);
  out = subst(out, "{clipboard}", clipboard);
  out = subst(out, "{clip}", clipboard);
  out = subst(out, "{query}", enc_all);
  out = subst(out, "{q}", enc_all);
  return out;
}

std::vector<SearchResult> quicklink_results(const QuicklinkMatch& m, const std::string& clipboard) {
  std::vector<SearchResult> out;
  if (!m.matched) return out;
  SearchResult r;
  auto url = expand_quicklink(m.tmpl, m.args, clipboard);
  r.title = m.args.empty() ? ("Quicklink · " + m.name) : (m.name + " · " + m.args);
  r.subtitle = url;
  r.path = url;
  r.payload = url;
  r.action = (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0)
                 ? ResultAction::WebSearch
                 : ResultAction::Open;
  r.score = 9750;
  r.kind_label = "quicklink";
  r.category = "quicklink";
  out.push_back(std::move(r));
  return out;
}

bool is_workflow_query(const std::string& query, const Config& cfg, std::string& out_name) {
  if (cfg.workflows.empty()) return false;
  std::string s = trim_copy(query);
  auto l = to_lower_utf8(s);
  std::string rest;
  if (l.rfind("workflow ", 0) == 0)
    rest = trim_copy(s.substr(9));
  else if (l.rfind("workflows", 0) == 0 && (l.size() == 9 || l[9] == ' ')) {
    rest = trim_copy(l.size() > 9 ? s.substr(9) : "");
    if (rest.empty()) {
      out_name.clear();  // list mode
      return true;
    }
  } else if (l.rfind("flow ", 0) == 0)
    rest = trim_copy(s.substr(5));
  else if (l.rfind("run ", 0) == 0)
    rest = trim_copy(s.substr(4));
  else
    return false;
  if (rest.empty()) {
    out_name.clear();
    return true;
  }
  // Workflow name is the first token; extra tokens are ignored (target query).
  auto sp = rest.find(' ');
  std::string name = sp == std::string::npos ? rest : rest.substr(0, sp);
  name = to_lower_utf8(trim_copy(name));
  auto it = cfg.workflows.find(name);
  if (it == cfg.workflows.end()) return false;
  out_name = name;
  return true;
}

std::vector<SearchResult> workflow_results(const std::string& name, const Config& cfg) {
  std::vector<SearchResult> out;
  if (name.empty()) {
    // List all workflows.
    for (auto& [k, steps] : cfg.workflows) {
      SearchResult r;
      std::string chain;
      for (std::size_t i = 0; i < steps.size(); ++i) {
        if (i) chain += " + ";
        chain += steps[i];
      }
      r.title = "Workflow · " + k;
      r.subtitle = chain + "  ·  type `workflow " + k + "` to run";
      r.payload = "workflow:" + k;
      r.path = r.payload;
      r.action = ResultAction::None;
      r.score = 9900;
      r.kind_label = "workflow";
      r.category = "workflow";
      out.push_back(std::move(r));
    }
    if (out.empty()) {
      SearchResult r;
      r.title = "No workflows configured";
      r.subtitle = "Add workflows: in wilfred.yml";
      r.action = ResultAction::None;
      out.push_back(std::move(r));
    }
    return out;
  }
  auto it = cfg.workflows.find(to_lower_utf8(name));
  if (it == cfg.workflows.end()) return out;
  std::string chain;
  for (std::size_t i = 0; i < it->second.size(); ++i) {
    if (i) chain += "+";
    chain += it->second[i];
  }
  SearchResult r;
  r.title = "Run workflow · " + name;
  r.subtitle = chain;
  r.payload = "workflow:" + to_lower_utf8(name);
  r.path = r.payload;
  r.action = ResultAction::None;
  r.score = 9950;
  r.kind_label = "workflow";
  r.category = "workflow";
  r.actions.push_back({"workflow:" + to_lower_utf8(name), "Run"});
  out.push_back(std::move(r));
  return out;
}

std::string workflow_chain(const std::string& name, const Config& cfg) {
  auto it = cfg.workflows.find(to_lower_utf8(name));
  if (it == cfg.workflows.end()) return {};
  std::string chain;
  for (std::size_t i = 0; i < it->second.size(); ++i) {
    if (i) chain += "+";
    chain += it->second[i];
  }
  return chain;
}

}  // namespace wilfred
