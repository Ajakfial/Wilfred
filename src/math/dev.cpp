#include "wilfred/math/expr.hpp"

#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cstdint>
#include <random>
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
  return false;
}

}  // namespace wilfred
