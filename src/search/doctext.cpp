#include "wilfred/search/doctext.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/updater/inflate.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace wilfred {
namespace {

std::uint16_t rd16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t rd32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Find a central-directory entry by name predicate; decompress it (stored or
// deflated) into out. Returns false when absent or undecodable.
bool zip_find_entry(const std::vector<std::uint8_t>& data, const std::string& prefix,
                    const std::string& suffix, std::string& out) {
  if (data.size() < 22) return false;
  std::size_t eocd = std::string::npos;
  for (std::size_t i = data.size() - 22; i + 22 <= data.size(); --i) {
    if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 5 && data[i + 3] == 6) {
      eocd = i;
      break;
    }
    if (i == 0) break;
  }
  if (eocd == std::string::npos) return false;
  const std::uint8_t* e = data.data() + eocd;
  std::uint16_t total = rd16(e + 10);
  std::uint32_t off = rd32(e + 16);
  bool found = false;
  std::uint32_t local = 0, comp = 0;
  std::uint16_t method = 0;
  for (std::uint16_t i = 0; i < total; ++i) {
    if (off + 46 > data.size()) return false;
    const std::uint8_t* c = data.data() + off;
    if (rd32(c) != 0x02014b50) return false;
    std::uint16_t nl = rd16(c + 28), el = rd16(c + 30), cl = rd16(c + 32);
    std::string name;
    name.assign(reinterpret_cast<const char*>(data.data() + off + 46), nl);
    bool match = name.size() >= prefix.size() + suffix.size() &&
                 name.compare(0, prefix.size(), prefix) == 0 &&
                 name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    std::uint32_t lo = rd32(c + 42);
    off += 46 + nl + el + cl;
    if (!match) continue;
    method = rd16(c + 10);
    comp = rd32(c + 20);
    local = lo;
    found = true;
    break;
  }
  if (!found || local + 30 > data.size()) return false;
  const std::uint8_t* lh = data.data() + local;
  if (rd32(lh) != 0x04034b50) return false;
  std::uint32_t data_off = local + 30 + rd16(lh + 26) + rd16(lh + 28);
  if (data_off + comp > data.size()) return false;
  if (method == 0) {
    out.assign(reinterpret_cast<const char*>(data.data() + data_off), comp);
    return true;
  }
  if (method == 8) {
    std::vector<std::uint8_t> dec;
    if (!inflate_decompress(data.data() + data_off, comp, dec)) return false;
    out.assign(reinterpret_cast<const char*>(dec.data()), dec.size());
    return true;
  }
  return false;
}

std::string decode_entities(std::string s) {
  auto rep = [&](const char* from, const char* to) {
    std::string f(from);
    std::size_t p = 0;
    while ((p = s.find(f, p)) != std::string::npos) {
      s.replace(p, f.size(), to);
      p += std::char_traits<char>::length(to);
    }
  };
  rep("&amp;", "&");
  rep("&lt;", "<");
  rep("&gt;", ">");
  rep("&quot;", "\"");
  rep("&apos;", "'");
  rep("&nbsp;", " ");
  return s;
}

// Strip <...> tags, skipping script/style content. Keeps block boundaries.
std::string strip_xml_tags(const std::string& xml, bool drop_code_blocks) {
  std::string o;
  o.reserve(xml.size() / 2);
  std::size_t i = 0;
  int skip_depth = 0;  // inside script/style when drop_code_blocks
  while (i < xml.size()) {
    if (xml[i] == '<') {
      auto e = xml.find('>', i);
      if (e == std::string::npos) break;
      std::string tag = xml.substr(i + 1, e - i - 1);
      bool closing = !tag.empty() && tag[0] == '/';
      std::string low;
      for (std::size_t t = closing ? 1 : 0; t < tag.size(); ++t) {
        char c = tag[t];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/') break;
        low.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      }
      if (closing) low = "/" + low;
      if (drop_code_blocks) {
        if (low == "script" || low == "style") ++skip_depth;
        if ((low == "/script" || low == "/style") && skip_depth > 0) --skip_depth;
      }
      if (skip_depth == 0 &&
          (low == "p" || low == "/p" || low == "br" || low == "div" || low == "/div" ||
           low == "tr" || low == "li" || low == "/li" || low == "h1" || low == "h2" || low == "h3"))
        o.push_back(' ');
      i = e + 1;
    } else if (skip_depth == 0) {
      o.push_back(xml[i++]);
    } else {
      ++i;
    }
  }
  return decode_entities(o);
}

std::string collapse_ws(std::string s, std::size_t cap) {
  std::string o;
  o.reserve(std::min(s.size(), cap));
  bool ws = true;
  for (char c : s) {
    bool is_ws = c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
    if (is_ws) {
      if (!ws) {
        o.push_back(' ');
        ws = true;
        if (o.size() >= cap) break;
      }
    } else {
      o.push_back(c);
      ws = false;
      if (o.size() >= cap) break;
    }
  }
  while (!o.empty() && o.back() == ' ')
    o.pop_back();
  return o;
}

bool has_token_chars(const std::string& s) {
  int alpha = 0;
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      if (++alpha >= 3) return true;
    }
  }
  return false;
}

bool read_bytes(const std::string& path, std::vector<std::uint8_t>& data, std::size_t cap) {
  std::string text;
  if (!read_file_all(path, text)) return false;
  if (text.size() > cap) text.resize(cap);
  data.assign(text.begin(), text.end());
  return true;
}

bool extract_office(const std::vector<std::uint8_t>& data, const std::string& ext, std::string& out,
                    std::size_t cap) {
  std::string acc;
  auto grab = [&](const std::string& prefix, const std::string& suffix) {
    std::string part;
    if (zip_find_entry(data, prefix, suffix, part)) {
      acc += strip_xml_tags(part, false);
      acc.push_back(' ');
    }
  };
  if (ext == ".docx") {
    grab("word/document", ".xml");
    grab("word/header", ".xml");
    grab("word/footer", ".xml");
    grab("word/footnotes", ".xml");
    grab("word/endnotes", ".xml");
    grab("word/comments", ".xml");
  } else if (ext == ".xlsx") {
    grab("xl/sharedStrings", ".xml");
    grab("xl/worksheets/sheet", ".xml");
  } else if (ext == ".pptx") {
    grab("ppt/slides/slide", ".xml");
    grab("ppt/notesSlides/notesSlide", ".xml");
  } else {  // .odt .ods .odp
    grab("", "content.xml");
  }
  out = collapse_ws(acc, cap);
  return has_token_chars(out);
}

bool extract_pdf(const std::vector<std::uint8_t>& data, std::string& out, std::size_t cap) {
  std::string acc;
  acc.reserve(8192);
  std::size_t i = 0;
  auto is_ws = [](unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
  };
  while (i < data.size()) {
    // Find the next "stream" keyword.
    std::size_t s = std::string::npos;
    for (std::size_t k = i; k + 6 < data.size(); ++k) {
      if (data[k] == 's' && data[k + 1] == 't' && data[k + 2] == 'r' && data[k + 3] == 'e' &&
          data[k + 4] == 'a' && data[k + 5] == 'm') {
        s = k;
        break;
      }
      if (k > i + 65536) break;  // give up scanning far ahead
    }
    if (s == std::string::npos) break;
    std::size_t body = s + 6;
    while (body < data.size() && is_ws(data[body]))
      ++body;
    auto e = std::string::npos;
    for (std::size_t k = body; k + 9 < data.size() && k < body + 4 * 1024 * 1024; ++k) {
      if (data[k] == 'e' && data[k + 1] == 'n' && data[k + 2] == 'd' && data[k + 3] == 's' &&
          data[k + 4] == 't' && data[k + 5] == 'r' && data[k + 6] == 'e' && data[k + 7] == 'a' &&
          data[k + 8] == 'm') {
        e = k;
        break;
      }
    }
    if (e == std::string::npos || e <= body) {
      i = body;
      continue;
    }
    std::string payload;
    {
      std::vector<std::uint8_t> dec;
      if (inflate_decompress(data.data() + body, e - body, dec) && !dec.empty())
        payload.assign(reinterpret_cast<const char*>(dec.data()), dec.size());
      else
        payload.assign(reinterpret_cast<const char*>(data.data() + body), e - body);
    }
    // Literal (escaped) and hex strings.
    for (std::size_t k = 0; k < payload.size(); ++k) {
      if (payload[k] == '(') {
        std::string lit;
        ++k;
        int depth = 1;
        while (k < payload.size() && depth > 0) {
          char c = payload[k];
          if (c == '\\' && k + 1 < payload.size()) {
            char n = payload[k + 1];
            if (n == 'n')
              lit.push_back('\n');
            else if (n == 'r')
              lit.push_back('\r');
            else if (n == 't')
              lit.push_back('\t');
            else if (n >= '0' && n <= '7') {
              int v = 0, d = 0;
              while (d < 3 && k + 1 + d < payload.size() && payload[k + 1 + d] >= '0' &&
                     payload[k + 1 + d] <= '7') {
                v = v * 8 + (payload[k + 1 + d] - '0');
                ++d;
              }
              lit.push_back(static_cast<char>(v));
              k += d;
            } else
              lit.push_back(n);
            k += 2;
          } else if (c == '(') {
            ++depth;
            lit.push_back(c);
            ++k;
          } else if (c == ')') {
            if (--depth == 0) {
              ++k;
              break;
            }
            lit.push_back(c);
            ++k;
          } else {
            lit.push_back(c);
            ++k;
          }
        }
        acc += lit;
        acc.push_back(' ');
      } else if (payload[k] == '<' && k + 1 < payload.size() && payload[k + 1] != '<') {
        std::string hex;
        ++k;
        while (k < payload.size() && payload[k] != '>') {
          if (!is_ws(static_cast<unsigned char>(payload[k]))) hex.push_back(payload[k]);
          ++k;
        }
        std::string bytes;
        for (std::size_t h = 0; h + 1 < hex.size(); h += 2) {
          auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
          };
          int hi = nib(hex[h]), lo = nib(hex[h + 1]);
          if (hi < 0 || lo < 0) break;
          bytes.push_back(static_cast<char>((hi << 4) | lo));
        }
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
            static_cast<unsigned char>(bytes[1]) == 0xFF) {
          for (std::size_t b = 2; b + 1 < bytes.size(); b += 2) {
            unsigned v = (static_cast<unsigned char>(bytes[b]) << 8) |
                         static_cast<unsigned char>(bytes[b + 1]);
            if (v < 0x80)
              acc.push_back(static_cast<char>(v));
            else if (v < 0x800) {
              acc.push_back(static_cast<char>(0xC0 | (v >> 6)));
              acc.push_back(static_cast<char>(0x80 | (v & 0x3F)));
            } else {
              acc.push_back(static_cast<char>(0xE0 | (v >> 12)));
              acc.push_back(static_cast<char>(0x80 | ((v >> 6) & 0x3F)));
              acc.push_back(static_cast<char>(0x80 | (v & 0x3F)));
            }
          }
        } else {
          acc += bytes;
        }
        acc.push_back(' ');
      }
      if (acc.size() > cap * 2) break;
    }
    i = e + 9;
    if (acc.size() > cap * 2) break;
  }
  out = collapse_ws(acc, cap);
  return has_token_chars(out);
}

bool extract_rtf(const std::string& text, std::string& out, std::size_t cap) {
  std::string o;
  o.reserve(std::min(text.size(), cap));
  std::size_t i = 0;
  int ignore_depth = -1;
  int depth = 0;
  auto is_dest = [](const std::string& w) {
    return w == "fonttbl" || w == "colortbl" || w == "stylesheet" || w == "info" || w == "header" ||
           w == "footer" || w == "footnote" || w == "pict" || w == "object" || w == "themedata" ||
           w == "colorschememapping";
  };
  while (i < text.size()) {
    char c = text[i];
    if (c == '{') {
      ++depth;
      ++i;
    } else if (c == '}') {
      if (ignore_depth >= 0 && depth <= ignore_depth) ignore_depth = -1;
      if (depth > 0) --depth;
      ++i;
    } else if (c == '\\' && i + 1 < text.size()) {
      char n = text[i + 1];
      if (n == '\\' || n == '{' || n == '}') {
        if (ignore_depth < 0) o.push_back(n);
        i += 2;
      } else if (n == '\'') {
        if (i + 3 < text.size() && ignore_depth < 0) {
          auto hex = text.substr(i + 2, 2);
          unsigned v = 0;
          if (std::sscanf(hex.c_str(), "%x", &v) == 1) o.push_back(static_cast<char>(v));
        }
        i += 4;
      } else if (n == '*') {
        ignore_depth = depth;
        i += 2;
      } else if ((n >= 'a' && n <= 'z') || (n >= 'A' && n <= 'Z')) {
        std::size_t j = i + 1;
        while (j < text.size() &&
               ((text[j] >= 'a' && text[j] <= 'z') || (text[j] >= 'A' && text[j] <= 'Z')))
          ++j;
        std::string word = text.substr(i + 1, j - i - 1);
        std::size_t k = j;
        bool neg = false;
        if (k < text.size() && text[k] == '-') {
          neg = true;
          ++k;
        }
        while (k < text.size() && text[k] >= '0' && text[k] <= '9')
          ++k;
        if (k < text.size() && text[k] == ' ') ++k;
        (void)neg;
        if (ignore_depth < 0) {
          if (is_dest(word))
            ignore_depth = depth;
          else if (word == "par" || word == "line" || word == "tab")
            o.push_back(' ');
          else if (word == "emdash")
            o += "--";
          else if (word == "endash")
            o += "-";
        }
        i = k;
      } else {
        ++i;
      }
    } else {
      if (ignore_depth < 0 && (c == '\n' || c == '\r'))
        o.push_back(' ');
      else if (ignore_depth < 0)
        o.push_back(c);
      ++i;
    }
    if (o.size() >= cap) break;
  }
  out = collapse_ws(o, cap);
  return has_token_chars(out);
}

}  // namespace

bool is_document_extension(const std::string& path) {
  auto e = to_lower_utf8(path_extension(path));
  return e == ".pdf" || e == ".docx" || e == ".xlsx" || e == ".pptx" || e == ".odt" ||
         e == ".ods" || e == ".odp" || e == ".rtf" || e == ".html" || e == ".htm";
}

bool extract_document_text(const std::string& path, std::string& out_text, std::size_t max_bytes) {
  out_text.clear();
  if (max_bytes == 0) return false;
  auto e = to_lower_utf8(path_extension(path));
  std::size_t cap = std::min<std::size_t>(max_bytes, 4 * 1024 * 1024);
  if (e == ".rtf") {
    std::string text;
    if (!read_file_all(path, text)) return false;
    if (text.size() > cap) text.resize(cap);
    return extract_rtf(text, out_text, max_bytes);
  }
  if (e == ".html" || e == ".htm") {
    std::string text;
    if (!read_file_all(path, text)) return false;
    if (text.size() > cap) text.resize(cap);
    out_text = collapse_ws(strip_xml_tags(text, true), max_bytes);
    return has_token_chars(out_text);
  }
  std::vector<std::uint8_t> data;
  if (!read_bytes(path, data, cap) || data.empty()) return false;
  if (e == ".pdf") return extract_pdf(data, out_text, max_bytes);
  return extract_office(data, e, out_text, max_bytes);
}

}  // namespace wilfred
