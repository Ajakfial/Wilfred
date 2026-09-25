#include "wilfred/config/yaml.hpp"

#include <cctype>
#include <cmath>
#include <sstream>

namespace wilfred {
namespace {

struct Parser {
  std::string_view t;
  std::size_t i{0};
  int line{1};
  int col{1};
  YamlError* err{nullptr};

  char peek() const { return i < t.size() ? t[i] : '\0'; }
  char peek(std::size_t n) const { return i + n < t.size() ? t[i + n] : '\0'; }

  char get() {
    if (i >= t.size()) return '\0';
    char c = t[i++];
    if (c == '\n') {
      ++line;
      col = 1;
    } else {
      ++col;
    }
    return c;
  }

  void fail(const std::string& m) {
    if (err && err->message.empty()) {
      err->line = line;
      err->column = col;
      err->message = m;
    }
  }

  void skip_ws_inline() {
    while (peek() == ' ' || peek() == '\t') get();
  }

  void skip_comment() {
    if (peek() == '#')
      while (peek() && peek() != '\n') get();
  }

  int skip_blank_and_comments() {
    int indent = 0;
    for (;;) {
      skip_ws_inline();
      skip_comment();
      if (peek() == '\r') get();
      if (peek() == '\n') {
        get();
        indent = 0;
        continue;
      }
      indent = 0;
      std::size_t j = i;
      int ccol = 0;
      while (j < t.size() && (t[j] == ' ' || t[j] == '\t')) {
        ccol += (t[j] == '\t') ? 2 : 1;
        ++j;
      }
      (void)ccol;
      break;
    }
    int ind = 0;
    while (peek() == ' ') {
      get();
      ++ind;
    }
    while (peek() == '\t') {
      get();
      ind += 2;
    }
    return ind;
  }

  int current_indent() const {
    int ind = 0;
    std::size_t j = i;
    // look back to line start
    std::size_t k = i;
    while (k > 0 && t[k - 1] != '\n') --k;
    while (k < t.size() && t[k] == ' ') {
      ++ind;
      ++k;
    }
    while (k < t.size() && t[k] == '\t') {
      ind += 2;
      ++k;
    }
    (void)j;
    return ind;
  }

  std::string parse_quoted(char q) {
    get();  // quote
    std::string s;
    while (peek() && peek() != q) {
      char c = get();
      if (c == '\\') {
        char n = get();
        if (n == 'n') s.push_back('\n');
        else if (n == 't')
          s.push_back('\t');
        else if (n == 'r')
          s.push_back('\r');
        else
          s.push_back(n);
      } else {
        s.push_back(c);
      }
    }
    if (peek() == q) get();
    return s;
  }

  std::string parse_plain() {
    std::string s;
    while (peek() && peek() != '\n' && peek() != '#' && peek() != '\r') {
      s.push_back(get());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s;
  }

  YamlValue parse_scalar() {
    skip_ws_inline();
    YamlValue v;
    if (peek() == '"' || peek() == '\'') {
      v.data = parse_quoted(peek());
      return v;
    }
    if (peek() == '|' || peek() == '>') {
      get();
      skip_ws_inline();
      skip_comment();
      if (peek() == '\r') get();
      if (peek() == '\n') get();
      std::string s;
      int base = 0;
      bool first = true;
      while (peek()) {
        int ind = 0;
        while (peek() == ' ') {
          get();
          ++ind;
        }
        if (first) {
          base = ind;
          first = false;
        }
        if (ind < base && peek() && peek() != '\n') {
          // rewind spaces? we already consumed. For robustness, stop.
          break;
        }
        while (peek() && peek() != '\n' && peek() != '\r') s.push_back(get());
        if (peek() == '\r') get();
        if (peek() == '\n') {
          get();
          s.push_back('\n');
        } else
          break;
      }
      v.data = s;
      return v;
    }
    auto s = parse_plain();
    v.data = s;
    return v;
  }

  YamlValue parse_value(int indent);

  YamlValue parse_map(int indent) {
    YamlValue root;
    root.data = YamlMap{};
    auto& m = std::get<YamlMap>(root.data);
    while (i < t.size()) {
      int ind = skip_blank_and_comments();
      if (!peek()) break;
      if (ind < indent && peek() != '\0') {
        // unconsume indent is hard; if we skipped spaces of a shallower key, stop by checking
      }
      if (ind < indent) break;
      if (peek() == '-') break;
      skip_ws_inline();
      if (!peek() || peek() == '\n') continue;
      std::string key;
      if (peek() == '"' || peek() == '\'')
        key = parse_quoted(peek());
      else {
        while (peek() && peek() != ':' && peek() != '\n') key.push_back(get());
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
      }
      skip_ws_inline();
      if (peek() != ':') {
        fail("expected ':' after key '" + key + "'");
        break;
      }
      get();
      skip_ws_inline();
      skip_comment();
      if (peek() == '\r') get();
      if (peek() == '\n' || peek() == '\0') {
        if (peek() == '\n') get();
        m[key] = parse_value(indent + 2);
      } else {
        m[key] = parse_scalar();
        skip_ws_inline();
        skip_comment();
        if (peek() == '\r') get();
        if (peek() == '\n') get();
      }
    }
    return root;
  }

  YamlValue parse_list(int indent) {
    YamlValue root;
    root.data = YamlList{};
    auto& l = std::get<YamlList>(root.data);
    while (i < t.size()) {
      int ind = skip_blank_and_comments();
      if (!peek()) break;
      if (ind < indent) break;
      if (peek() != '-') break;
      get();
      skip_ws_inline();
      if (peek() == '\n' || peek() == '\0') {
        if (peek() == '\n') get();
        l.push_back(parse_value(indent + 2));
      } else if (peek() != '#' && looks_like_inline_map()) {
        // compact map in list item: key: value
        YamlValue item;
        item.data = YamlMap{};
        auto& im = std::get<YamlMap>(item.data);
        std::string key;
        while (peek() && peek() != ':' && peek() != '\n') key.push_back(get());
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (peek() == ':') get();
        skip_ws_inline();
        if (peek() == '\n' || peek() == '\0') {
          if (peek() == '\n') get();
          im[key] = parse_value(indent + 2);
        } else {
          im[key] = parse_scalar();
          if (peek() == '\n') get();
        }
        l.push_back(std::move(item));
      } else {
        l.push_back(parse_scalar());
        skip_ws_inline();
        skip_comment();
        if (peek() == '\r') get();
        if (peek() == '\n') get();
      }
    }
    return root;
  }

  bool looks_like_inline_map() const {
    std::size_t j = i;
    while (j < t.size() && t[j] != '\n' && t[j] != '#') {
      if (t[j] == ':') return true;
      ++j;
    }
    return false;
  }
};

YamlValue Parser::parse_value(int indent) {
  std::size_t save = i;
  int iline = line, icol = col;
  int ind = skip_blank_and_comments();
  (void)ind;
  if (peek() == '-') return parse_list(indent);
  // map vs scalar: if line has key:
  std::size_t j = i;
  bool map = false;
  if (peek() == '"' || peek() == '\'') {
    // could be scalar or keyed
  }
  while (j < t.size() && t[j] != '\n' && t[j] != '#') {
    if (t[j] == ':') {
      map = true;
      break;
    }
    if (t[j] == '-' && j == i) break;
    ++j;
  }
  if (map) {
    i = save;
    line = iline;
    col = icol;
    skip_blank_and_comments();
    return parse_map(indent);
  }
  i = save;
  line = iline;
  col = icol;
  skip_blank_and_comments();
  if (!peek()) {
    YamlValue v;
    v.data = std::string{};
    return v;
  }
  auto sc = parse_scalar();
  skip_ws_inline();
  skip_comment();
  if (peek() == '\r') get();
  if (peek() == '\n') get();
  return sc;
}

std::string dump(const YamlValue& v, int indent) {
  std::ostringstream os;
  auto pad = [&](int n) {
    for (int i = 0; i < n; ++i) os << ' ';
  };
  if (v.is_null()) {
    os << "null";
  } else if (v.is_string()) {
    os << v.as_string();
  } else if (v.is_list()) {
    if (v.as_list().empty()) {
      os << "[]";
    } else {
      os << '\n';
      for (auto& item : v.as_list()) {
        pad(indent);
        os << "- ";
        if (item.is_map() || item.is_list())
          os << dump(item, indent + 2);
        else
          os << dump(item, indent);
        if (!item.is_map() && !item.is_list()) os << '\n';
      }
    }
  } else if (v.is_map()) {
    os << '\n';
    for (auto& [k, val] : v.as_map()) {
      pad(indent);
      os << k << ": ";
      if (val.is_map() || val.is_list())
        os << dump(val, indent + 2);
      else
        os << dump(val, indent);
      if (!val.is_map() && !val.is_list()) os << '\n';
    }
  }
  return os.str();
}

}  // namespace

const YamlValue* YamlValue::get(std::string_view key) const {
  if (!is_map()) return nullptr;
  auto& m = as_map();
  auto it = m.find(std::string(key));
  if (it == m.end()) return nullptr;
  return &it->second;
}

std::string YamlValue::str(std::string_view key, std::string_view def) const {
  auto* v = get(key);
  if (!v || !v->is_string()) return std::string(def);
  return v->as_string();
}

static bool parse_bool_str(const std::string& s, bool& out) {
  std::string l;
  l.reserve(s.size());
  for (char c : s) l.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (l == "true" || l == "yes" || l == "on" || l == "1") {
    out = true;
    return true;
  }
  if (l == "false" || l == "no" || l == "off" || l == "0") {
    out = false;
    return true;
  }
  return false;
}

bool YamlValue::boolean(std::string_view key, bool def) const {
  auto* v = get(key);
  if (!v || !v->is_string()) return def;
  bool b = def;
  parse_bool_str(v->as_string(), b);
  return b;
}

std::int64_t YamlValue::integer(std::string_view key, std::int64_t def) const {
  auto* v = get(key);
  if (!v || !v->is_string()) return def;
  try {
    return std::stoll(v->as_string());
  } catch (...) {
    return def;
  }
}

double YamlValue::number(std::string_view key, double def) const {
  auto* v = get(key);
  if (!v || !v->is_string()) return def;
  try {
    return std::stod(v->as_string());
  } catch (...) {
    return def;
  }
}

std::vector<std::string> YamlValue::string_list(std::string_view key) const {
  std::vector<std::string> out;
  auto* v = get(key);
  if (!v) return out;
  if (v->is_list()) {
    for (auto& i : v->as_list()) {
      if (i.is_string()) out.push_back(i.as_string());
    }
  } else if (v->is_string() && !v->as_string().empty()) {
    out.push_back(v->as_string());
  }
  return out;
}

bool parse_yaml(std::string_view text, YamlValue& out, YamlError& err) {
  err = {};
  Parser p;
  p.t = text;
  p.err = &err;
  out = p.parse_value(0);
  return err.message.empty();
}

std::string yaml_to_string(const YamlValue& v) { return dump(v, 0); }

}  // namespace wilfred
