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

namespace {
unsigned int json_hex4(const std::string& s, std::size_t pos) {
  unsigned int v = 0;
  for (int i = 0; i < 4; ++i) {
    char c = s[pos + static_cast<std::size_t>(i)];
    unsigned int d = 0;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0xFFFFFFFFu;
    v = (v << 4) | d;
  }
  return v;
}

bool json_is_hex4(const std::string& s, std::size_t pos) {
  if (pos + 4 > s.size()) return false;
  for (int i = 0; i < 4; ++i) {
    char c = s[pos + static_cast<std::size_t>(i)];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return false;
  }
  return true;
}

void json_append_utf8(std::string& out, unsigned int cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}
}  // namespace

std::string json_get_string(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\"";
  auto pos = s.find(k);
  if (pos == std::string::npos) return {};
  pos = s.find(':', pos + k.size());
  if (pos == std::string::npos) return {};
  ++pos;
  while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t'))
    ++pos;
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
        else if (n == 'b')
          v.push_back('\b');
        else if (n == 'f')
          v.push_back('\f');
        else if (n == '"' || n == '\\' || n == '/')
          v.push_back(n);
        else if (n == 'u' && pos + 6 <= s.size() && json_is_hex4(s, pos + 2)) {
          unsigned int cp = json_hex4(s, pos + 2);
          // Handle UTF-16 surrogate pairs.
          if (cp >= 0xD800 && cp <= 0xDBFF && pos + 12 <= s.size() &&
              s[pos + 6] == '\\' && s[pos + 7] == 'u' && json_is_hex4(s, pos + 8)) {
            unsigned int lo = json_hex4(s, pos + 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
              pos += 12;
              json_append_utf8(v, cp);
              continue;
            }
          }
          if (cp >= 0xD800 && cp <= 0xDFFF) {
            // Lone surrogate: emit replacement character.
            json_append_utf8(v, 0xFFFDu);
          } else {
            json_append_utf8(v, cp);
          }
          pos += 6;
          continue;
        } else {
          v.push_back(n);
        }
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

std::string json_extract_object(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\"";
  auto pos = s.find(k);
  if (pos == std::string::npos) return {};
  pos = s.find('{', pos + k.size());
  if (pos == std::string::npos) return {};
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  std::size_t start = pos;
  for (; pos < s.size(); ++pos) {
    char c = s[pos];
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
    if (c == '{')
      ++depth;
    else if (c == '}') {
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
