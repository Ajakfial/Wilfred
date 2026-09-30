#include "wilfred/math/expr.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wilfred {
namespace {

std::string trim_copy(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  return s;
}

bool is_hex_char(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;
}

bool parse_hex_color(std::string_view raw, int& r, int& g, int& b, int& a, bool& has_a) {
  std::string s = trim_copy(std::string(raw));
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s = s.substr(2);
  if (!s.empty() && s.front() == '#') s.erase(s.begin());
  if (s.size() != 3 && s.size() != 4 && s.size() != 6 && s.size() != 8) return false;
  for (char c : s)
    if (!is_hex_char(c)) return false;
  auto byte_at = [&](std::size_t i) {
    return hex_val(s[i]) * 16 + hex_val(s[i + 1]);
  };
  auto nibble = [&](std::size_t i) {
    int v = hex_val(s[i]);
    return v * 16 + v;
  };
  has_a = false;
  a = 255;
  if (s.size() == 3) {
    r = nibble(0);
    g = nibble(1);
    b = nibble(2);
    return true;
  }
  if (s.size() == 4) {
    r = nibble(0);
    g = nibble(1);
    b = nibble(2);
    a = nibble(3);
    has_a = true;
    return true;
  }
  if (s.size() == 6) {
    r = byte_at(0);
    g = byte_at(2);
    b = byte_at(4);
    return true;
  }
  r = byte_at(0);
  g = byte_at(2);
  b = byte_at(4);
  a = byte_at(6);
  has_a = true;
  return true;
}

struct ColorNum {
  double v{0};
  bool pct{false};
};

bool parse_num_list(const std::string& inner, std::vector<ColorNum>& nums) {
  nums.clear();
  std::size_t i = 0;
  while (i < inner.size()) {
    while (i < inner.size() && (inner[i] == ' ' || inner[i] == '\t' || inner[i] == ',' ||
                                inner[i] == '/' || inner[i] == ';'))
      ++i;
    if (i >= inner.size()) break;
    std::size_t start = i;
    if (inner[i] == '+' || inner[i] == '-') ++i;
    bool any = false;
    while (i < inner.size() && (std::isdigit(static_cast<unsigned char>(inner[i])) || inner[i] == '.')) {
      any = true;
      ++i;
    }
    if (!any) return false;
    ColorNum n;
    try {
      n.v = std::stod(inner.substr(start, i - start));
    } catch (...) {
      return false;
    }
    if (i < inner.size() && inner[i] == '%') {
      n.pct = true;
      ++i;
    }
    nums.push_back(n);
  }
  return !nums.empty();
}

int rgb_channel(const ColorNum& n) {
  double v = n.v;
  if (n.pct) v = v / 100.0 * 255.0;
  return std::clamp(static_cast<int>(std::lround(v)), 0, 255);
}

double unit_or_pct(const ColorNum& n) {
  if (n.pct || n.v > 1.0) return std::clamp(n.v / 100.0, 0.0, 1.0);
  return std::clamp(n.v, 0.0, 1.0);
}

void rgb_to_hsl(int r, int g, int b, double& h, double& s, double& l) {
  double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
  double max = std::max(rf, std::max(gf, bf));
  double min = std::min(rf, std::min(gf, bf));
  l = (max + min) / 2.0;
  if (max == min) {
    h = 0;
    s = 0;
    return;
  }
  double d = max - min;
  s = l > 0.5 ? d / (2.0 - max - min) : d / (max + min);
  if (max == rf)
    h = std::fmod((gf - bf) / d + (gf < bf ? 6.0 : 0.0), 6.0) * 60.0;
  else if (max == gf)
    h = ((bf - rf) / d + 2.0) * 60.0;
  else
    h = ((rf - gf) / d + 4.0) * 60.0;
  if (h < 0) h += 360.0;
}

void rgb_to_hsv(int r, int g, int b, double& h, double& s, double& v) {
  double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
  double max = std::max(rf, std::max(gf, bf));
  double min = std::min(rf, std::min(gf, bf));
  v = max;
  double d = max - min;
  s = max == 0 ? 0 : d / max;
  if (d == 0) {
    h = 0;
    return;
  }
  if (max == rf)
    h = std::fmod((gf - bf) / d + (gf < bf ? 6.0 : 0.0), 6.0) * 60.0;
  else if (max == gf)
    h = ((bf - rf) / d + 2.0) * 60.0;
  else
    h = ((rf - gf) / d + 4.0) * 60.0;
  if (h < 0) h += 360.0;
}

void rgb_to_cmyk(int r, int g, int b, double& c, double& m, double& y, double& k) {
  double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
  k = 1.0 - std::max(rf, std::max(gf, bf));
  if (k >= 1.0 - 1e-12) {
    c = m = y = 0;
    k = 1;
    return;
  }
  c = (1.0 - rf - k) / (1.0 - k);
  m = (1.0 - gf - k) / (1.0 - k);
  y = (1.0 - bf - k) / (1.0 - k);
}

double hue_to_rgb(double p, double q, double t) {
  if (t < 0) t += 1;
  if (t > 1) t -= 1;
  if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
  if (t < 0.5) return q;
  if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
  return p;
}

void hsl_to_rgb(double h, double s, double l, int& r, int& g, int& b) {
  h = std::fmod(h, 360.0);
  if (h < 0) h += 360.0;
  s = std::clamp(s, 0.0, 1.0);
  l = std::clamp(l, 0.0, 1.0);
  if (s == 0) {
    int v = static_cast<int>(std::lround(l * 255.0));
    r = g = b = std::clamp(v, 0, 255);
    return;
  }
  double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  double p = 2 * l - q;
  double hk = h / 360.0;
  r = std::clamp(static_cast<int>(std::lround(hue_to_rgb(p, q, hk + 1.0 / 3.0) * 255.0)), 0, 255);
  g = std::clamp(static_cast<int>(std::lround(hue_to_rgb(p, q, hk) * 255.0)), 0, 255);
  b = std::clamp(static_cast<int>(std::lround(hue_to_rgb(p, q, hk - 1.0 / 3.0) * 255.0)), 0, 255);
}

void hsv_to_rgb(double h, double s, double v, int& r, int& g, int& b) {
  h = std::fmod(h, 360.0);
  if (h < 0) h += 360.0;
  s = std::clamp(s, 0.0, 1.0);
  v = std::clamp(v, 0.0, 1.0);
  double c = v * s;
  double x = c * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
  double m = v - c;
  double rf = 0, gf = 0, bf = 0;
  if (h < 60) {
    rf = c;
    gf = x;
  } else if (h < 120) {
    rf = x;
    gf = c;
  } else if (h < 180) {
    gf = c;
    bf = x;
  } else if (h < 240) {
    gf = x;
    bf = c;
  } else if (h < 300) {
    rf = x;
    bf = c;
  } else {
    rf = c;
    bf = x;
  }
  r = std::clamp(static_cast<int>(std::lround((rf + m) * 255.0)), 0, 255);
  g = std::clamp(static_cast<int>(std::lround((gf + m) * 255.0)), 0, 255);
  b = std::clamp(static_cast<int>(std::lround((bf + m) * 255.0)), 0, 255);
}

void hwb_to_rgb(double h, double w, double blk, int& r, int& g, int& b) {
  w = std::clamp(w, 0.0, 1.0);
  blk = std::clamp(blk, 0.0, 1.0);
  if (w + blk >= 1.0) {
    double gsv = w / (w + blk);
    int v = std::clamp(static_cast<int>(std::lround(gsv * 255.0)), 0, 255);
    r = g = b = v;
    return;
  }
  hsv_to_rgb(h, 1.0, 1.0, r, g, b);
  auto mix = [&](int ch) {
    double x = ch / 255.0;
    x = x * (1.0 - w - blk) + w;
    return std::clamp(static_cast<int>(std::lround(x * 255.0)), 0, 255);
  };
  r = mix(r);
  g = mix(g);
  b = mix(b);
}

void cmyk_to_rgb(double c, double m, double y, double k, int& r, int& g, int& b) {
  c = std::clamp(c, 0.0, 1.0);
  m = std::clamp(m, 0.0, 1.0);
  y = std::clamp(y, 0.0, 1.0);
  k = std::clamp(k, 0.0, 1.0);
  r = std::clamp(static_cast<int>(std::lround(255.0 * (1.0 - c) * (1.0 - k))), 0, 255);
  g = std::clamp(static_cast<int>(std::lround(255.0 * (1.0 - m) * (1.0 - k))), 0, 255);
  b = std::clamp(static_cast<int>(std::lround(255.0 * (1.0 - y) * (1.0 - k))), 0, 255);
}

const std::unordered_map<std::string, std::pair<int, int>>& named_colors() {
  static const auto m = [] {
    std::unordered_map<std::string, std::pair<int, int>> u;
    auto add = [&](const char* name, int rgb) { u.emplace(name, std::pair<int, int>{rgb, 255}); };
    add("aliceblue", 0xf0f8ff);
    add("antiquewhite", 0xfaebd7);
    add("aqua", 0x00ffff);
    add("aquamarine", 0x7fffd4);
    add("azure", 0xf0ffff);
    add("beige", 0xf5f5dc);
    add("bisque", 0xffe4c4);
    add("black", 0x000000);
    add("blanchedalmond", 0xffebcd);
    add("blue", 0x0000ff);
    add("blueviolet", 0x8a2be2);
    add("brown", 0xa52a2a);
    add("burlywood", 0xdeb887);
    add("cadetblue", 0x5f9ea0);
    add("chartreuse", 0x7fff00);
    add("chocolate", 0xd2691e);
    add("coral", 0xff7f50);
    add("cornflowerblue", 0x6495ed);
    add("cornsilk", 0xfff8dc);
    add("crimson", 0xdc143c);
    add("cyan", 0x00ffff);
    add("darkblue", 0x00008b);
    add("darkcyan", 0x008b8b);
    add("darkgoldenrod", 0xb8860b);
    add("darkgray", 0xa9a9a9);
    add("darkgreen", 0x006400);
    add("darkgrey", 0xa9a9a9);
    add("darkkhaki", 0xbdb76b);
    add("darkmagenta", 0x8b008b);
    add("darkolivegreen", 0x556b2f);
    add("darkorange", 0xff8c00);
    add("darkorchid", 0x9932cc);
    add("darkred", 0x8b0000);
    add("darksalmon", 0xe9967a);
    add("darkseagreen", 0x8fbc8f);
    add("darkslateblue", 0x483d8b);
    add("darkslategray", 0x2f4f4f);
    add("darkslategrey", 0x2f4f4f);
    add("darkturquoise", 0x00ced1);
    add("darkviolet", 0x9400d3);
    add("deeppink", 0xff1493);
    add("deepskyblue", 0x00bfff);
    add("dimgray", 0x696969);
    add("dimgrey", 0x696969);
    add("dodgerblue", 0x1e90ff);
    add("firebrick", 0xb22222);
    add("floralwhite", 0xfffaf0);
    add("forestgreen", 0x228b22);
    add("fuchsia", 0xff00ff);
    add("gainsboro", 0xdcdcdc);
    add("ghostwhite", 0xf8f8ff);
    add("gold", 0xffd700);
    add("goldenrod", 0xdaa520);
    add("gray", 0x808080);
    add("green", 0x008000);
    add("greenyellow", 0xadff2f);
    add("grey", 0x808080);
    add("honeydew", 0xf0fff0);
    add("hotpink", 0xff69b4);
    add("indianred", 0xcd5c5c);
    add("indigo", 0x4b0082);
    add("ivory", 0xfffff0);
    add("khaki", 0xf0e68c);
    add("lavender", 0xe6e6fa);
    add("lavenderblush", 0xfff0f5);
    add("lawngreen", 0x7cfc00);
    add("lemonchiffon", 0xfffacd);
    add("lightblue", 0xadd8e6);
    add("lightcoral", 0xf08080);
    add("lightcyan", 0xe0ffff);
    add("lightgoldenrodyellow", 0xfafad2);
    add("lightgray", 0xd3d3d3);
    add("lightgreen", 0x90ee90);
    add("lightgrey", 0xd3d3d3);
    add("lightpink", 0xffb6c1);
    add("lightsalmon", 0xffa07a);
    add("lightseagreen", 0x20b2aa);
    add("lightskyblue", 0x87cefa);
    add("lightslategray", 0x778899);
    add("lightslategrey", 0x778899);
    add("lightsteelblue", 0xb0c4de);
    add("lightyellow", 0xffffe0);
    add("lime", 0x00ff00);
    add("limegreen", 0x32cd32);
    add("linen", 0xfaf0e6);
    add("magenta", 0xff00ff);
    add("maroon", 0x800000);
    add("mediumaquamarine", 0x66cdaa);
    add("mediumblue", 0x0000cd);
    add("mediumorchid", 0xba55d3);
    add("mediumpurple", 0x9370db);
    add("mediumseagreen", 0x3cb371);
    add("mediumslateblue", 0x7b68ee);
    add("mediumspringgreen", 0x00fa9a);
    add("mediumturquoise", 0x48d1cc);
    add("mediumvioletred", 0xc71585);
    add("midnightblue", 0x191970);
    add("mintcream", 0xf5fffa);
    add("mistyrose", 0xffe4e1);
    add("moccasin", 0xffe4b5);
    add("navajowhite", 0xffdead);
    add("navy", 0x000080);
    add("oldlace", 0xfdf5e6);
    add("olive", 0x808000);
    add("olivedrab", 0x6b8e23);
    add("orange", 0xffa500);
    add("orangered", 0xff4500);
    add("orchid", 0xda70d6);
    add("palegoldenrod", 0xeee8aa);
    add("palegreen", 0x98fb98);
    add("paleturquoise", 0xafeeee);
    add("palevioletred", 0xdb7093);
    add("papayawhip", 0xffefd5);
    add("peachpuff", 0xffdab9);
    add("peru", 0xcd853f);
    add("pink", 0xffc0cb);
    add("plum", 0xdda0dd);
    add("powderblue", 0xb0e0e6);
    add("purple", 0x800080);
    add("rebeccapurple", 0x663399);
    add("red", 0xff0000);
    add("rosybrown", 0xbc8f8f);
    add("royalblue", 0x4169e1);
    add("saddlebrown", 0x8b4513);
    add("salmon", 0xfa8072);
    add("sandybrown", 0xf4a460);
    add("seagreen", 0x2e8b57);
    add("seashell", 0xfff5ee);
    add("sienna", 0xa0522d);
    add("silver", 0xc0c0c0);
    add("skyblue", 0x87ceeb);
    add("slateblue", 0x6a5acd);
    add("slategray", 0x708090);
    add("slategrey", 0x708090);
    add("snow", 0xfffafa);
    add("springgreen", 0x00ff7f);
    add("steelblue", 0x4682b4);
    add("tan", 0xd2b48c);
    add("teal", 0x008080);
    add("thistle", 0xd8bfd8);
    add("tomato", 0xff6347);
    add("turquoise", 0x40e0d0);
    add("violet", 0xee82ee);
    add("wheat", 0xf5deb3);
    add("white", 0xffffff);
    add("whitesmoke", 0xf5f5f5);
    add("yellow", 0xffff00);
    add("yellowgreen", 0x9acd32);
    add("transparent", 0x000000);
    u["transparent"].second = 0;
    return u;
  }();
  return m;
}

bool named_rgb(std::string_view raw, int& r, int& g, int& b, int& a) {
  auto n = to_lower_utf8(trim_copy(std::string(raw)));
  n.erase(std::remove(n.begin(), n.end(), ' '), n.end());
  auto it = named_colors().find(n);
  if (it == named_colors().end()) return false;
  int rgb = it->second.first;
  r = (rgb >> 16) & 255;
  g = (rgb >> 8) & 255;
  b = rgb & 255;
  a = it->second.second;
  return true;
}

std::string fmt_hex(int r, int g, int b, int a, bool has_a) {
  char buf[16];
  if (has_a && a != 255)
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
  else
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return buf;
}

std::string fmt_rgb(int r, int g, int b, int a, bool has_a) {
  char buf[48];
  if (has_a && a != 255)
    std::snprintf(buf, sizeof(buf), "rgba(%d, %d, %d, %.3g)", r, g, b, a / 255.0);
  else
    std::snprintf(buf, sizeof(buf), "rgb(%d, %d, %d)", r, g, b);
  return buf;
}

std::string fmt_hsl(int r, int g, int b, int a, bool has_a) {
  double h = 0, s = 0, l = 0;
  rgb_to_hsl(r, g, b, h, s, l);
  char buf[64];
  if (has_a && a != 255)
    std::snprintf(buf, sizeof(buf), "hsla(%.0f, %.0f%%, %.0f%%, %.3g)", h, s * 100.0, l * 100.0,
                  a / 255.0);
  else
    std::snprintf(buf, sizeof(buf), "hsl(%.0f, %.0f%%, %.0f%%)", h, s * 100.0, l * 100.0);
  return buf;
}

std::string fmt_hsv(int r, int g, int b) {
  double h = 0, s = 0, v = 0;
  rgb_to_hsv(r, g, b, h, s, v);
  char buf[48];
  std::snprintf(buf, sizeof(buf), "hsv(%.0f, %.0f%%, %.0f%%)", h, s * 100.0, v * 100.0);
  return buf;
}

std::string fmt_cmyk(int r, int g, int b) {
  double c = 0, m = 0, y = 0, k = 0;
  rgb_to_cmyk(r, g, b, c, m, y, k);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "cmyk(%.0f%%, %.0f%%, %.0f%%, %.0f%%)", c * 100.0, m * 100.0,
                y * 100.0, k * 100.0);
  return buf;
}

std::string target_of(std::string s) {
  auto l = to_lower_utf8(s);
  auto pos = l.find(" to ");
  std::size_t n = 4;
  if (pos == std::string::npos) {
    pos = l.find(" in ");
    n = 4;
  }
  if (pos == std::string::npos) return {};
  auto t = trim_copy(l.substr(pos + n));
  t.erase(std::remove(t.begin(), t.end(), ' '), t.end());
  if (t.rfind("as", 0) == 0) t = t.substr(2);
  return t;
}

std::string source_of(std::string s) {
  auto l = to_lower_utf8(s);
  while (!l.empty() && (l.front() == ' ' || l.front() == '\t'))
    l.erase(l.begin());
  if (l.rfind("convert ", 0) == 0) l = l.substr(8);
  if (l.rfind("color ", 0) == 0) l = l.substr(6);
  if (l.rfind("colour ", 0) == 0) l = l.substr(7);
  if (l.rfind("hex ", 0) == 0) l = l.substr(4);
  auto pos = l.find(" to ");
  if (pos == std::string::npos) pos = l.find(" in ");
  if (pos == std::string::npos) return trim_copy(l);
  return trim_copy(l.substr(0, pos));
}

bool parse_func_color(const std::string& src, int& r, int& g, int& b, int& a, bool& has_a) {
  auto l = to_lower_utf8(trim_copy(src));
  has_a = false;
  a = 255;
  auto open = l.find('(');
  auto close = l.rfind(')');
  std::string fn;
  std::string inner;
  if (open != std::string::npos && close != std::string::npos && close > open) {
    fn = trim_copy(l.substr(0, open));
    inner = l.substr(open + 1, close - open - 1);
  } else if (l.rfind("rgb ", 0) == 0 || l.rfind("rgba ", 0) == 0 || l.rfind("hsl ", 0) == 0 ||
             l.rfind("hsla ", 0) == 0 || l.rfind("hsv ", 0) == 0 || l.rfind("hsb ", 0) == 0 ||
             l.rfind("hwb ", 0) == 0 || l.rfind("cmyk ", 0) == 0) {
    auto sp = l.find(' ');
    fn = l.substr(0, sp);
    inner = l.substr(sp + 1);
  } else {
    return false;
  }
  std::vector<ColorNum> nums;
  if (!parse_num_list(inner, nums)) return false;
  if ((fn == "cmyk") ? nums.size() < 4 : nums.size() < 3) return false;
  if (fn != "cmyk" && nums.size() >= 4) {
    has_a = true;
    if (nums[3].pct)
      a = std::clamp(static_cast<int>(std::lround(nums[3].v / 100.0 * 255.0)), 0, 255);
    else if (nums[3].v <= 1.0)
      a = std::clamp(static_cast<int>(std::lround(nums[3].v * 255.0)), 0, 255);
    else
      a = std::clamp(static_cast<int>(std::lround(nums[3].v)), 0, 255);
  }
  if (fn == "rgb" || fn == "rgba") {
    r = rgb_channel(nums[0]);
    g = rgb_channel(nums[1]);
    b = rgb_channel(nums[2]);
    return true;
  }
  if (fn == "hsl" || fn == "hsla") {
    hsl_to_rgb(nums[0].v, unit_or_pct(nums[1]), unit_or_pct(nums[2]), r, g, b);
    return true;
  }
  if (fn == "hsv" || fn == "hsb") {
    hsv_to_rgb(nums[0].v, unit_or_pct(nums[1]), unit_or_pct(nums[2]), r, g, b);
    return true;
  }
  if (fn == "hwb") {
    hwb_to_rgb(nums[0].v, unit_or_pct(nums[1]), unit_or_pct(nums[2]), r, g, b);
    return true;
  }
  if (fn == "cmyk") {
    cmyk_to_rgb(unit_or_pct(nums[0]), unit_or_pct(nums[1]), unit_or_pct(nums[2]),
                unit_or_pct(nums[3]), r, g, b);
    return true;
  }
  return false;
}

}  // namespace

bool convert_color(std::string_view expr, MathResult& out) {
  auto src = source_of(std::string(expr));
  auto tgt = target_of(std::string(expr));
  if (src.empty()) return false;
  int r = 0, g = 0, b = 0, a = 255;
  bool has_a = false;
  bool ok = parse_hex_color(src, r, g, b, a, has_a);
  if (!ok) ok = parse_func_color(src, r, g, b, a, has_a);
  if (!ok) {
    ok = named_rgb(src, r, g, b, a);
    if (ok && a != 255) has_a = true;
  }
  if (!ok) return false;
  out.ok = true;
  out.conversion = true;
  out.color = true;
  out.currency = false;
  out.datetime = false;
  out.value = r * 65536.0 + g * 256.0 + b;
  out.color_hex = fmt_hex(r, g, b, a, has_a);
  out.color_rgb = fmt_rgb(r, g, b, a, has_a);
  out.color_hsl = fmt_hsl(r, g, b, a, has_a);
  out.color_hsv = fmt_hsv(r, g, b);
  out.color_cmyk = fmt_cmyk(r, g, b);
  if (tgt == "rgb" || tgt == "rgba")
    out.display = out.color_rgb;
  else if (tgt == "hsl" || tgt == "hsla")
    out.display = out.color_hsl;
  else if (tgt == "hsv" || tgt == "hsb")
    out.display = out.color_hsv;
  else if (tgt == "cmyk")
    out.display = out.color_cmyk;
  else if (tgt == "hex" || tgt == "html" || tgt == "#" || tgt == "hexadecimal")
    out.display = out.color_hex;
  else
    out.display = out.color_hex + "  ·  " + out.color_rgb + "  ·  " + out.color_hsl;
  out.error.clear();
  return true;
}

}  // namespace wilfred
