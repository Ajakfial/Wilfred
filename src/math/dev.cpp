#include "wilfred/math/expr.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace wilfred {
namespace {

std::string trim_copy(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  return s;
}

void split_key(const std::string& s, std::string& key, std::string& rest) {
  auto l = to_lower_utf8(s);
  auto sp = l.find(' ');
  if (sp == std::string::npos) {
    key = l;
    rest.clear();
    return;
  }
  key = l.substr(0, sp);
  rest = trim_copy(s.substr(sp + 1));
}

std::string uuid_v4() {
  std::random_device rd;
  std::uint8_t b[16];
  for (int i = 0; i < 16; ++i) b[i] = static_cast<std::uint8_t>(rd() & 0xff);
  b[6] = static_cast<std::uint8_t>((b[6] & 0x0f) | 0x40);
  b[8] = static_cast<std::uint8_t>((b[8] & 0x3f) | 0x80);
  static const char* hex = "0123456789abcdef";
  std::string o;
  o.resize(36);
  int p = 0;
  auto put = [&](std::uint8_t v) {
    o[static_cast<std::size_t>(p++)] = hex[v >> 4];
    o[static_cast<std::size_t>(p++)] = hex[v & 0xf];
  };
  put(b[0]);
  put(b[1]);
  put(b[2]);
  put(b[3]);
  o[static_cast<std::size_t>(p++)] = '-';
  put(b[4]);
  put(b[5]);
  o[static_cast<std::size_t>(p++)] = '-';
  put(b[6]);
  put(b[7]);
  o[static_cast<std::size_t>(p++)] = '-';
  put(b[8]);
  put(b[9]);
  o[static_cast<std::size_t>(p++)] = '-';
  put(b[10]);
  put(b[11]);
  put(b[12]);
  put(b[13]);
  put(b[14]);
  put(b[15]);
  return o;
}

std::string b64_encode(const std::string& in) {
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve(((in.size() + 2) / 3) * 4);
  std::size_t i = 0;
  auto data = reinterpret_cast<const unsigned char*>(in.data());
  auto len = in.size();
  while (i + 2 < len) {
    unsigned n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(tbl[(n >> 6) & 63]);
    o.push_back(tbl[n & 63]);
    i += 3;
  }
  if (i < len) {
    unsigned n = data[i] << 16;
    if (i + 1 < len) n |= data[i + 1] << 8;
    o.push_back(tbl[(n >> 18) & 63]);
    o.push_back(tbl[(n >> 12) & 63]);
    o.push_back(i + 1 < len ? tbl[(n >> 6) & 63] : '=');
    o.push_back('=');
  }
  return o;
}

bool b64_decode(const std::string& in, std::string& out) {
  int dec[256];
  for (int& d : dec) d = -1;
  const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; i < 64; ++i) dec[static_cast<unsigned char>(tbl[i])] = i;
  std::vector<int> vals;
  vals.reserve(in.size());
  for (unsigned char c : in) {
    if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
    if (dec[c] < 0) return false;
    vals.push_back(dec[c]);
  }
  if (vals.empty()) {
    out.clear();
    return true;
  }
  out.clear();
  out.reserve(vals.size() * 3 / 4);
  for (std::size_t i = 0; i < vals.size(); i += 4) {
    int v0 = vals[i];
    int v1 = i + 1 < vals.size() ? vals[i + 1] : 0;
    int v2 = i + 2 < vals.size() ? vals[i + 2] : 0;
    int v3 = i + 3 < vals.size() ? vals[i + 3] : 0;
    out.push_back(static_cast<char>(((v0 << 2) | (v1 >> 4)) & 0xff));
    if (i + 2 < vals.size()) out.push_back(static_cast<char>(((v1 << 4) | (v2 >> 2)) & 0xff));
    if (i + 3 < vals.size()) out.push_back(static_cast<char>(((v2 << 6) | v3) & 0xff));
  }
  return true;
}

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::string sha256_hex(const std::string& msg) {
  static const std::uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
      0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
      0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
      0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
      0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
      0xc67178f2};
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<unsigned char> data(msg.begin(), msg.end());
  auto bitlen = static_cast<std::uint64_t>(msg.size()) * 8;
  data.push_back(0x80);
  while ((data.size() % 64) != 56) data.push_back(0);
  for (int i = 7; i >= 0; --i) data.push_back(static_cast<unsigned char>((bitlen >> (i * 8)) & 0xff));
  for (std::size_t off = 0; off < data.size(); off += 64) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(data[off + i * 4]) << 24) |
             (static_cast<std::uint32_t>(data[off + i * 4 + 1]) << 16) |
             (static_cast<std::uint32_t>(data[off + i * 4 + 2]) << 8) |
             static_cast<std::uint32_t>(data[off + i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      auto S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      auto ch = (e & f) ^ ((~e) & g);
      auto t1 = hh + S1 + ch + K[i] + w[i];
      auto S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      auto maj = (a & b) ^ (a & c) ^ (b & c);
      auto t2 = S0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  static const char* hex = "0123456789abcdef";
  std::string o;
  o.resize(64);
  for (int i = 0; i < 8; ++i) {
    for (int n = 0; n < 8; ++n) {
      auto shift = 28 - n * 4;
      o[static_cast<std::size_t>(i * 8 + n)] = hex[(h[i] >> shift) & 0xf];
    }
  }
  return o;
}

const char* kLorem[] = {
    "lorem",     "ipsum",     "dolor",    "sit",      "amet",     "consectetur", "adipiscing",
    "elit",      "sed",       "do",       "eiusmod",  "tempor",   "incididunt",  "ut",
    "labore",    "et",        "dolore",   "magna",    "aliqua",   "ut",          "enim",
    "ad",        "minim",     "veniam",   "quis",     "nostrud",  "exercitation","ullamco",
    "laboris",   "nisi",      "aliquip",  "ex",       "ea",       "commodo",     "consequat",
    "duis",      "aute",      "irure",    "in",       "reprehenderit", "voluptate", "velit",
    "esse",      "cillum",    nullptr};

std::string lorem_text(int words) {
  if (words < 1) words = 30;
  if (words > 400) words = 400;
  std::string o;
  int nwords = 0;
  for (auto** p = kLorem; *p; ++p) ++nwords;
  for (int i = 0; i < words; ++i) {
    if (i) o.push_back(' ');
    auto w = kLorem[i % nwords];
    if (i == 0) {
      o.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(w[0]))));
      o += (w + 1);
    } else
      o += w;
  }
  o.push_back('.');
  return o;
}

void skip_ws(const std::string& s, std::size_t& i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
}

bool emit_json(const std::string& s, std::size_t& i, std::string& pretty, std::string& compact,
               int indent);

bool emit_string(const std::string& s, std::size_t& i, std::string& pretty, std::string& compact) {
  if (i >= s.size() || s[i] != '"') return false;
  pretty.push_back('"');
  compact.push_back('"');
  ++i;
  while (i < s.size()) {
    char c = s[i++];
    pretty.push_back(c);
    compact.push_back(c);
    if (c == '\\') {
      if (i >= s.size()) return false;
      pretty.push_back(s[i]);
      compact.push_back(s[i]);
      ++i;
      continue;
    }
    if (c == '"') return true;
  }
  return false;
}

bool emit_json(const std::string& s, std::size_t& i, std::string& pretty, std::string& compact,
               int indent) {
  skip_ws(s, i);
  if (i >= s.size()) return false;
  char c = s[i];
  if (c == '"') return emit_string(s, i, pretty, compact);
  if (c == '{') {
    pretty.push_back('{');
    compact.push_back('{');
    ++i;
    skip_ws(s, i);
    if (i < s.size() && s[i] == '}') {
      pretty.push_back('}');
      compact.push_back('}');
      ++i;
      return true;
    }
    bool first = true;
    while (i < s.size()) {
      if (!first) {
        if (s[i] != ',') return false;
        pretty.push_back(',');
        compact.push_back(',');
        ++i;
        skip_ws(s, i);
      }
      first = false;
      pretty.push_back('\n');
      pretty.append(static_cast<std::size_t>(indent + 2), ' ');
      skip_ws(s, i);
      if (!emit_string(s, i, pretty, compact)) return false;
      skip_ws(s, i);
      if (i >= s.size() || s[i] != ':') return false;
      pretty += ": ";
      compact.push_back(':');
      ++i;
      if (!emit_json(s, i, pretty, compact, indent + 2)) return false;
      skip_ws(s, i);
      if (i < s.size() && s[i] == '}') {
        pretty.push_back('\n');
        pretty.append(static_cast<std::size_t>(indent), ' ');
        pretty.push_back('}');
        compact.push_back('}');
        ++i;
        return true;
      }
    }
    return false;
  }
  if (c == '[') {
    pretty.push_back('[');
    compact.push_back('[');
    ++i;
    skip_ws(s, i);
    if (i < s.size() && s[i] == ']') {
      pretty.push_back(']');
      compact.push_back(']');
      ++i;
      return true;
    }
    bool first = true;
    while (i < s.size()) {
      if (!first) {
        if (s[i] != ',') return false;
        pretty.push_back(',');
        compact.push_back(',');
        ++i;
      }
      first = false;
      pretty.push_back('\n');
      pretty.append(static_cast<std::size_t>(indent + 2), ' ');
      if (!emit_json(s, i, pretty, compact, indent + 2)) return false;
      skip_ws(s, i);
      if (i < s.size() && s[i] == ']') {
        pretty.push_back('\n');
        pretty.append(static_cast<std::size_t>(indent), ' ');
        pretty.push_back(']');
        compact.push_back(']');
        ++i;
        return true;
      }
    }
    return false;
  }
  if (s.compare(i, 4, "true") == 0) {
    pretty += "true";
    compact += "true";
    i += 4;
    return true;
  }
  if (s.compare(i, 5, "false") == 0) {
    pretty += "false";
    compact += "false";
    i += 5;
    return true;
  }
  if (s.compare(i, 4, "null") == 0) {
    pretty += "null";
    compact += "null";
    i += 4;
    return true;
  }
  if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
    auto start = i;
    if (c == '-') ++i;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i < s.size() && s[i] == '.') {
      ++i;
      if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
      ++i;
      if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
      if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) return false;
      while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    pretty.append(s, start, i - start);
    compact.append(s, start, i - start);
    return true;
  }
  return false;
}

bool json_forms(const std::string& in, std::string& pretty, std::string& compact) {
  std::size_t i = 0;
  pretty.clear();
  compact.clear();
  if (!emit_json(in, i, pretty, compact, 0)) return false;
  skip_ws(in, i);
  return i == in.size();
}

void ok_text(MathResult& out, const std::string& display) {
  out.ok = true;
  out.conversion = true;
  out.devutil = true;
  out.display = display;
}

bool parse_int_auto(const std::string& s, std::int64_t& out) {
  std::string t;
  for (char c : s) {
    if (c != '_' && c != ' ' && c != '\t' && c != ',') t.push_back(c);
  }
  if (t.empty()) return false;
  bool neg = false;
  std::size_t i = 0;
  if (t[0] == '+' || t[0] == '-') {
    neg = t[0] == '-';
    i = 1;
  }
  int base = 10;
  if (t.size() - i >= 2 && t[i] == '0' && (t[i + 1] == 'x' || t[i + 1] == 'X')) {
    base = 16;
    i += 2;
  } else if (t.size() - i >= 2 && t[i] == '0' && (t[i + 1] == 'b' || t[i + 1] == 'B')) {
    base = 2;
    i += 2;
  } else if (t.size() - i >= 2 && t[i] == '0' && (t[i + 1] == 'o' || t[i + 1] == 'O')) {
    base = 8;
    i += 2;
  } else if (t.size() - i >= 2 && t[i] == '#') {
    // #ff style handled elsewhere
    return false;
  }
  if (i >= t.size()) return false;
  std::int64_t v = 0;
  for (; i < t.size(); ++i) {
    char c = t[i];
    int d = -1;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else
      return false;
    if (d >= base) return false;
    if (v > (INT64_MAX - d) / base) return false;
    v = v * base + d;
  }
  out = neg ? -v : v;
  return true;
}

std::string to_base_str(std::int64_t v, int base, bool prefix) {
  bool neg = v < 0;
  std::uint64_t u = neg ? static_cast<std::uint64_t>(-(v + 1)) + 1 : static_cast<std::uint64_t>(v);
  const char* digits = "0123456789ABCDEF";
  std::string d;
  if (u == 0) d = "0";
  while (u) {
    d.push_back(digits[u % static_cast<std::uint64_t>(base)]);
    u /= static_cast<std::uint64_t>(base);
  }
  if (base == 2 || base == 8 || base == 16) {
    // digits are uppercase for hex; lower binary/octal unaffected.
  }
  std::reverse(d.begin(), d.end());
  // Lowercase hex digits for consistency with 0x style? Keep uppercase but
  // tests compare case-insensitively where needed.
  std::string o;
  if (neg) o.push_back('-');
  if (prefix) {
    if (base == 16) o += "0x";
    if (base == 2) o += "0b";
    if (base == 8) o += "0o";
  }
  o += d;
  return o;
}

std::string base_overview(std::int64_t v) {
  std::ostringstream os;
  os << std::to_string(v) << " = " << to_base_str(v, 16, true) << " = "
     << to_base_str(v, 2, true) << " = " << to_base_str(v, 8, true);
  return os.str();
}

bool is_hex_literal(const std::string& s) {
  if (s.size() < 3 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) return false;
  for (std::size_t i = 2; i < s.size(); ++i)
    if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
  return s.size() > 2;
}

bool is_bin_literal(const std::string& s) {
  if (s.size() < 3 || s[0] != '0' || (s[1] != 'b' && s[1] != 'B')) return false;
  for (std::size_t i = 2; i < s.size(); ++i)
    if (s[i] != '0' && s[i] != '1') return false;
  return s.size() > 2;
}

bool is_oct_literal(const std::string& s) {
  if (s.size() < 3 || s[0] != '0' || (s[1] != 'o' && s[1] != 'O')) return false;
  for (std::size_t i = 2; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '7') return false;
  return s.size() > 2;
}

std::string url_encode_str(const std::string& in) {
  static const char* hex = "0123456789ABCDEF";
  std::string o;
  for (unsigned char c : in) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~')
      o.push_back(static_cast<char>(c));
    else {
      o.push_back('%');
      o.push_back(hex[c >> 4]);
      o.push_back(hex[c & 0xf]);
    }
  }
  return o;
}

bool url_decode_str(const std::string& in, std::string& out) {
  out.clear();
  for (std::size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c == '+') {
      out.push_back(' ');
    } else if (c == '%') {
      if (i + 2 >= in.size()) return false;
      auto hv = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return -1;
      };
      int hi = hv(in[i + 1]), lo = hv(in[i + 2]);
      if (hi < 0 || lo < 0) return false;
      out.push_back(static_cast<char>((hi << 4) | lo));
      i += 2;
    } else {
      out.push_back(c);
    }
  }
  return true;
}

bool b64url_decode(const std::string& in, std::string& out) {
  std::string s = in;
  for (char& c : s) {
    if (c == '-') c = '+';
    if (c == '_') c = '/';
  }
  while (s.size() % 4) s.push_back('=');
  return b64_decode(s, out);
}

}  // namespace

bool convert_devutil(std::string_view expr, MathResult& out) {
  auto s = trim_copy(std::string(expr));
  if (s.empty()) return false;
  std::string key, rest;
  split_key(s, key, rest);

  if (key == "uuid" || key == "uuid4" || key == "guid" || key == "uuidv4") {
    if (!rest.empty() && rest != "v4" && rest != "4") return false;
    ok_text(out, uuid_v4());
    return true;
  }
  if (key == "base64" || key == "b64" || key == "base64encode" || key == "encode64" ||
      key == "b64e") {
    if (rest.empty()) return false;
    ok_text(out, b64_encode(rest));
    return true;
  }
  if (key == "base64decode" || key == "base64d" || key == "b64d" || key == "decode64" ||
      key == "b64decode") {
    if (rest.empty()) return false;
    std::string decoded;
    if (!b64_decode(rest, decoded)) return false;
    ok_text(out, decoded);
    return true;
  }
  if (key == "sha256" || key == "sha" || key == "hash") {
    if (rest.empty()) return false;
    ok_text(out, sha256_hex(rest));
    return true;
  }
  if (key == "lorem" || key == "ipsum" || key == "loremipsum") {
    int words = 30;
    if (!rest.empty()) {
      try {
        words = std::stoi(rest);
      } catch (...) {
        return false;
      }
    }
    ok_text(out, lorem_text(words));
    return true;
  }
  if (key == "json" || key == "prettyjson" || key == "jsonfmt" || key == "pretty") {
    auto payload = rest;
    auto pl = to_lower_utf8(payload);
    if (pl.rfind("pretty ", 0) == 0) payload = trim_copy(rest.substr(7));
    else if (pl.rfind("minify ", 0) == 0 || pl.rfind("compact ", 0) == 0)
      payload = trim_copy(rest.substr(pl.find(' ') + 1));
    if (payload.empty()) return false;
    std::string pretty, compact;
    if (!json_forms(payload, pretty, compact)) return false;
    if (pl.rfind("minify ", 0) == 0 || pl.rfind("compact ", 0) == 0)
      ok_text(out, compact);
    else
      ok_text(out, pretty);
    return true;
  }
  // --- Number-base tools: hex/dec/bin/oct/base + bare 0x/0b/0o literals ---
  if (key == "hex" || key == "dec" || key == "decimal" || key == "bin" || key == "binary" ||
      key == "oct" || key == "octal" || key == "base") {
    if (rest.empty()) return false;
    // `base <radix> <value>` form.
    if (key == "base") {
      std::string radix_s, val_s;
      split_key(rest, radix_s, val_s);
      if (val_s.empty()) return false;
      int radix = 0;
      try {
        radix = std::stoi(radix_s);
      } catch (...) {
        return false;
      }
      if (radix != 2 && radix != 8 && radix != 10 && radix != 16) return false;
      std::int64_t v = 0;
      if (!parse_int_auto(val_s, v)) return false;
      ok_text(out, to_base_str(v, radix, radix != 10));
      return true;
    }
    // `hex 255`, `dec 0xff`, `bin 10`, `oct 8` (+ `to` form: `255 to hex`).
    std::string val = rest;
    auto vl = to_lower_utf8(val);
    // Support "<value> to <base>" inside the rest, e.g. `hex 255 to bin`? Keep simple:
    // strip trailing " to <base>" if present and honor it as output base.
    int out_base = (key == "hex" ? 16 : key == "dec" || key == "decimal" ? 10 : key == "bin" || key == "binary" ? 2 : 8);
    auto to_pos = vl.rfind(" to ");
    if (to_pos != std::string::npos) {
      std::string want = trim_copy(val.substr(to_pos + 4));
      auto wl = to_lower_utf8(want);
      if (wl == "hex" || wl == "h") out_base = 16;
      else if (wl == "dec" || wl == "decimal" || wl == "d") out_base = 10;
      else if (wl == "bin" || wl == "binary" || wl == "b") out_base = 2;
      else if (wl == "oct" || wl == "octal" || wl == "o") out_base = 8;
      else
        return false;
      val = trim_copy(val.substr(0, to_pos));
    }
    // Also allow "<n> as <base>"? No, keep `to`.
    std::int64_t v = 0;
    if (!parse_int_auto(val, v)) {
      // Try "<value> in <base>"? No.
      return false;
    }
    if (out_base == 10 && (key == "hex" || key == "bin" || key == "oct")) {
      // `hex 255` historically means "show 255 in hex"; `dec 0xff` means "show in dec".
      // When input base == output base intent, still show overview for usefulness,
      // unless an explicit `to` was given.
      if (to_pos == std::string::npos) {
        ok_text(out, base_overview(v));
        return true;
      }
    }
    if (to_pos != std::string::npos || key == "dec" || key == "decimal") {
      if (out_base == 10)
        ok_text(out, std::to_string(v));
      else
        ok_text(out, to_base_str(v, out_base, true));
      return true;
    }
    ok_text(out, base_overview(v));
    return true;
  }
  // Bare literals: `0xff`, `0b1010`, `0o17` (+ `0xff to dec` style handled via math?).
  if (is_hex_literal(key) || is_bin_literal(key) || is_oct_literal(key)) {
    if (!rest.empty()) {
      // Support `0xff to dec` / `0b101 to hex`.
      auto rl = to_lower_utf8(rest);
      if (rl.rfind("to ", 0) == 0) {
        std::string want = trim_copy(rest.substr(3));
        auto wl = to_lower_utf8(want);
        std::int64_t v = 0;
        if (!parse_int_auto(key, v)) return false;
        if (wl == "dec" || wl == "decimal" || wl == "d" || wl == "10")
          ok_text(out, std::to_string(v));
        else if (wl == "hex" || wl == "h" || wl == "16")
          ok_text(out, to_base_str(v, 16, true));
        else if (wl == "bin" || wl == "binary" || wl == "b" || wl == "2")
          ok_text(out, to_base_str(v, 2, true));
        else if (wl == "oct" || wl == "octal" || wl == "o" || wl == "8")
          ok_text(out, to_base_str(v, 8, true));
        else
          return false;
        return true;
      }
      return false;
    }
    std::int64_t v = 0;
    if (!parse_int_auto(key, v)) return false;
    ok_text(out, base_overview(v));
    return true;
  }
  // --- Bit tools: `bit and/or/xor/not/shl/shr ...` ---
  if (key == "bit" || key == "bits" || key == "bitwise") {
    if (rest.empty()) return false;
    std::string op, args;
    split_key(rest, op, args);
    if (op == "and" || op == "or" || op == "xor" || op == "shl" || op == "shr" || op == "lshift" ||
        op == "rshift") {
      std::string a_s, b_s;
      split_key(args, a_s, b_s);
      if (a_s.empty() || b_s.empty()) return false;
      // b may contain extra tokens; only first two matter.
      auto sp = b_s.find(' ');
      if (sp != std::string::npos) b_s.resize(sp);
      std::int64_t a = 0, b = 0;
      if (!parse_int_auto(a_s, a) || !parse_int_auto(b_s, b)) return false;
      std::int64_t r = 0;
      const char* sym = "?";
      if (op == "and") {
        r = a & b;
        sym = "&";
      } else if (op == "or") {
        r = a | b;
        sym = "|";
      } else if (op == "xor") {
        r = a ^ b;
        sym = "^";
      } else {
        if (b < 0 || b > 62) return false;
        if (op == "shl" || op == "lshift") {
          r = a << b;
          sym = "<<";
        } else {
          r = a >> b;
          sym = ">>";
        }
      }
      std::ostringstream os;
      os << a_s << " " << sym << " " << b_s << " = " << r << " (" << to_base_str(r, 16, true)
         << " / " << to_base_str(r, 2, true) << ")";
      ok_text(out, os.str());
      return true;
    }
    if (op == "not" || op == "inv" || op == "complement") {
      if (args.empty()) return false;
      std::string a_s = args;
      auto sp = a_s.find(' ');
      if (sp != std::string::npos) a_s.resize(sp);
      std::int64_t a = 0;
      if (!parse_int_auto(a_s, a)) return false;
      std::int64_t r = ~a;
      std::ostringstream os;
      os << "~" << a_s << " = " << r << " (" << to_base_str(r, 16, true) << " / "
         << to_base_str(r, 2, true) << ")";
      ok_text(out, os.str());
      return true;
    }
    return false;
  }
  // --- URL codec ---
  if (key == "urlencode" || key == "urlenc" || key == "encodeurl" || key == "url_encode") {
    if (rest.empty()) return false;
    ok_text(out, url_encode_str(rest));
    return true;
  }
  if (key == "urldecode" || key == "urldec" || key == "decodeurl" || key == "url_decode") {
    if (rest.empty()) return false;
    std::string dec;
    if (!url_decode_str(rest, dec)) return false;
    ok_text(out, dec);
    return true;
  }
  if (key == "url" && !rest.empty()) {
    auto rl = to_lower_utf8(rest);
    if (rl.rfind("encode ", 0) == 0) {
      auto payload = trim_copy(rest.substr(7));
      if (payload.empty()) return false;
      ok_text(out, url_encode_str(payload));
      return true;
    }
    if (rl.rfind("decode ", 0) == 0) {
      auto payload = trim_copy(rest.substr(7));
      std::string dec;
      if (payload.empty() || !url_decode_str(payload, dec)) return false;
      ok_text(out, dec);
      return true;
    }
  }
  // --- JWT decode (header.payload.signature, base64url, no verification) ---
  if (key == "jwt") {
    std::string tok = rest;
    auto tl = to_lower_utf8(tok);
    if (tl.rfind("decode ", 0) == 0) tok = trim_copy(tok.substr(7));
    if (tok.empty()) return false;
    if (tok.find(' ') != std::string::npos) return false;
    auto p1 = tok.find('.');
    auto p2 = p1 == std::string::npos ? std::string::npos : tok.find('.', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;
    std::string h_b64 = tok.substr(0, p1);
    std::string p_b64 = tok.substr(p1 + 1, p2 - p1 - 1);
    std::string sig = tok.substr(p2 + 1);
    std::string h_json, p_json;
    if (!b64url_decode(h_b64, h_json) || !b64url_decode(p_b64, p_json)) return false;
    std::string h_pretty = h_json, p_pretty = p_json, dummy;
    {
      std::string hp, hc;
      if (json_forms(h_json, hp, hc)) h_pretty = hp;
      std::string pp, pc;
      if (json_forms(p_json, pp, pc)) p_pretty = pp;
    }
    std::ostringstream os;
    os << "header: " << h_pretty << "\npayload: " << p_pretty;
    if (!sig.empty()) os << "\nsignature: " << sig.substr(0, 16) << (sig.size() > 16 ? "..." : "");
    os << "\n(note: signature not verified)";
    ok_text(out, os.str());
    return true;
  }
  // --- Regex tester: `regex <pattern> <text>` (pattern = first token) ---
  if (key == "regex" || key == "regexp" || key == "re" || key == "regexi" || key == "rei") {
    if (rest.empty()) return false;
    bool icase = (key == "regexi" || key == "rei");
    std::string pattern, text;
    // Support quoted pattern: regex "a b" "a b c".
    if (!rest.empty() && (rest[0] == '"' || rest[0] == '\'')) {
      char q = rest[0];
      auto end = rest.find(q, 1);
      if (end == std::string::npos) return false;
      pattern = rest.substr(1, end - 1);
      text = trim_copy(rest.substr(end + 1));
      if (!text.empty() && (text[0] == '"' || text[0] == '\'')) {
        char q2 = text[0];
        if (text.size() >= 2 && text.back() == q2) text = text.substr(1, text.size() - 2);
      }
    } else {
      split_key(rest, pattern, text);
    }
    if (pattern.empty() || text.empty()) return false;
    // Support /pattern/flags form.
    std::string pat = pattern;
    if (pat.size() >= 2 && pat.front() == '/' ) {
      auto end = pat.rfind('/');
      if (end != std::string::npos && end > 0) {
        std::string flags = pat.substr(end + 1);
        pat = pat.substr(1, end - 1);
        for (char f : flags)
          if (f == 'i' || f == 'I') icase = true;
      }
    }
    try {
      auto flags = std::regex_constants::ECMAScript;
      if (icase) flags |= std::regex_constants::icase;
      std::regex re(pat, flags);
      std::smatch m;
      if (!std::regex_search(text, m, re)) {
        ok_text(out, "no match");
        return true;
      }
      std::ostringstream os;
      os << "match: " << m.str(0);
      for (std::size_t i = 1; i < m.size() && i <= 5; ++i) os << "\n$" << i << ": " << m.str(i);
      ok_text(out, os.str());
      return true;
    } catch (...) {
      return false;
    }
  }
  return false;
}

}  // namespace wilfred
