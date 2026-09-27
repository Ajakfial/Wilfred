#include "wilfred/core/json.hpp"

namespace wilfred {

std::string json_escape(const std::string& s) {
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
    else if (c == '\t')
      o += "\\t";
    else if (c < 0x20)
      continue;
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

std::string json_get_string(const std::string& s, const char* key) {
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
        char n = s[pos + 1];
        if (n == 'n')
          v.push_back('\n');
        else if (n == 't')
          v.push_back('\t');
        else if (n == 'r')
          v.push_back('\r');
        else
          v.push_back(n);
        pos += 2;
      } else
        v.push_back(s[pos++]);
    }
    return v;
  }
  std::string v;
  while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' && s[pos] != ' ')
    v.push_back(s[pos++]);
  return v;
}

int json_get_int(const std::string& s, const char* key, int def) {
  auto v = json_get_string(s, key);
  if (v.empty()) return def;
  try {
    return std::stoi(v);
  } catch (...) {
    return def;
  }
}

bool json_get_bool(const std::string& s, const char* key, bool def) {
  auto v = json_get_string(s, key);
  if (v.empty()) return def;
  return v == "true" || v == "1";
}

std::string json_extract_array(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\"";
  auto pos = s.find(k);
  if (pos == std::string::npos) return {};
  pos = s.find('[', pos + k.size());
  if (pos == std::string::npos) return {};
  int depth = 0;
  std::size_t start = pos;
  for (; pos < s.size(); ++pos) {
    if (s[pos] == '[')
      ++depth;
    else if (s[pos] == ']') {
      --depth;
      if (depth == 0) return s.substr(start, pos - start + 1);
    }
  }
  return {};
}

std::vector<std::string> json_object_array(const std::string& array_json) {
  std::vector<std::string> out;
  int depth = 0;
  std::size_t start = 0;
  bool in_str = false;
  bool esc = false;
  for (std::size_t i = 0; i < array_json.size(); ++i) {
    char c = array_json[i];
    if (in_str) {
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      continue;
    }
    if (c == '"') {
      in_str = true;
      continue;
    }
    if (c == '{') {
      if (depth == 0) start = i;
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) out.push_back(array_json.substr(start, i - start + 1));
    }
  }
  return out;
}

}  // namespace wilfred
