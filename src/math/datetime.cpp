#include "wilfred/math/expr.hpp"

#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {
namespace {

using namespace std::chrono;

std::string trim_copy(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
  return s;
}

std::string norm_token(std::string s) {
  s = to_lower_utf8(trim_copy(std::move(s)));
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    if (c == ' ' || c == '.' || c == '_' || c == '-' || c == '/') continue;
    o.push_back(c);
  }
  return o;
}

enum class DstRule { None, US, EU, AU, NZ };

struct Zone {
  int std_min{0};
  int dst_extra{0};
  DstRule dst{DstRule::None};
  const char* label{"UTC"};
};

bool nth_weekday(int y, unsigned m, unsigned n, unsigned wd, sys_days& out) {
  auto ym = year{y} / month{m} / weekday{wd}[n];
  if (!ym.ok()) return false;
  out = sys_days{ym};
  return true;
}

bool last_weekday(int y, unsigned m, unsigned wd, sys_days& out) {
  auto ym = year{y} / month{m} / weekday{wd}[last];
  if (!ym.ok()) return false;
  out = sys_days{ym};
  return true;
}

bool us_dst(sys_seconds utc, int std_min) {
  auto local = utc + minutes{std_min};
  auto dp = floor<days>(local);
  year_month_day ymd{dp};
  int y = static_cast<int>(ymd.year());
  sys_days start, end;
  if (!nth_weekday(y, 3, 2, 0, start) || !nth_weekday(y, 11, 1, 0, end)) return false;
  return dp >= start && dp < end;
}

bool eu_dst(sys_seconds utc) {
  auto dp = floor<days>(utc);
  year_month_day ymd{dp};
  int y = static_cast<int>(ymd.year());
  sys_days start, end;
  if (!last_weekday(y, 3, 0, start) || !last_weekday(y, 10, 0, end)) return false;
  auto begin = sys_seconds{start} + hours{1};
  auto finish = sys_seconds{end} + hours{1};
  return utc >= begin && utc < finish;
}

bool au_dst(sys_seconds utc, int std_min) {
  auto local = utc + minutes{std_min};
  auto dp = floor<days>(local);
  year_month_day ymd{dp};
  int y = static_cast<int>(ymd.year());
  sys_days oct, apr;
  if (!nth_weekday(y, 10, 1, 0, oct) || !nth_weekday(y, 4, 1, 0, apr)) return false;
  return dp >= oct || dp < apr;
}

bool nz_dst(sys_seconds utc, int std_min) {
  auto local = utc + minutes{std_min};
  auto dp = floor<days>(local);
  year_month_day ymd{dp};
  int y = static_cast<int>(ymd.year());
  sys_days sep, apr;
  if (!last_weekday(y, 9, 0, sep) || !nth_weekday(y, 4, 1, 0, apr)) return false;
  return dp >= sep || dp < apr;
}

int zone_offset_at(const Zone& z, std::int64_t utc_sec) {
  auto utc = sys_seconds{seconds{utc_sec}};
  bool dst = false;
  switch (z.dst) {
    case DstRule::US:
      dst = us_dst(utc, z.std_min);
      break;
    case DstRule::EU:
      dst = eu_dst(utc);
      break;
    case DstRule::AU:
      dst = au_dst(utc, z.std_min);
      break;
    case DstRule::NZ:
      dst = nz_dst(utc, z.std_min);
      break;
    case DstRule::None:
      dst = false;
      break;
  }
  return z.std_min + (dst ? z.dst_extra : 0);
}

const std::unordered_map<std::string, Zone>& zones() {
  static const auto m = [] {
    std::unordered_map<std::string, Zone> u;
    auto add = [&](const Zone& z, std::initializer_list<const char*> names) {
      for (auto n : names)
        u.emplace(norm_token(n), z);
    };
    add({0, 0, DstRule::None, "UTC"}, {"utc", "gmt", "z", "zulu", "universal"});
    add({0, 60, DstRule::EU, "London"},
        {"london", "uk", "britain", "england", "europelondon"});
    add({60, 0, DstRule::None, "CET"}, {"cet", "westafrica"});
    add({60, 60, DstRule::EU, "Paris"},
        {"paris", "berlin", "amsterdam", "madrid", "rome", "brussels", "prague", "vienna",
         "zurich", "stockholm", "oslo", "copenhagen", "warsaw", "budapest", "europe"});
    add({120, 60, DstRule::EU, "Helsinki"},
        {"helsinki", "athens", "bucharest", "kyiv", "kiev", "riga", "tallinn", "sofia"});
    add({120, 0, DstRule::None, "Cairo"}, {"cairo", "eet", "southafrica", "johannesburg", "harare"});
    add({180, 0, DstRule::None, "Moscow"}, {"moscow", "istanbul", "msk"});
    add({-300, 0, DstRule::None, "EST"}, {"est"});
    add({-240, 0, DstRule::None, "EDT"}, {"edt"});
    add({-360, 0, DstRule::None, "CST"}, {"cst"});
    add({-300, 0, DstRule::None, "CDT"}, {"cdt"});
    add({-420, 0, DstRule::None, "MST"}, {"mst"});
    add({-360, 0, DstRule::None, "MDT"}, {"mdt"});
    add({-480, 0, DstRule::None, "PST"}, {"pst"});
    add({-420, 0, DstRule::None, "PDT"}, {"pdt"});
    add({-300, 60, DstRule::US, "Eastern"},
        {"et", "eastern", "nyc", "newyork", "boston", "miami", "atlanta", "toronto",
         "montreal", "washington", "philadelphia", "americanewyork", "usaeastern"});
    add({-360, 60, DstRule::US, "Central"},
        {"ct", "central", "chicago", "dallas", "houston", "winnipeg", "mexico", "mexicocity"});
    add({-420, 60, DstRule::US, "Mountain"}, {"mt", "mountain", "denver", "calgary", "edmonton"});
    add({-420, 0, DstRule::None, "Phoenix"}, {"phoenix", "arizona"});
    add({-480, 60, DstRule::US, "Pacific"},
        {"pt", "pacific", "la", "losangeles", "seattle", "portland", "vancouver", "sanfrancisco",
         "sf"});
    add({-540, 60, DstRule::US, "Alaska"}, {"akst", "akdt", "alaska", "anchorage"});
    add({-600, 0, DstRule::None, "Hawaii"}, {"hst", "hawaii", "honolulu"});
    add({540, 0, DstRule::None, "Tokyo"}, {"jst", "tokyo", "japan", "osaka", "asiatokyo"});
    add({540, 0, DstRule::None, "Seoul"}, {"kst", "seoul", "korea"});
    add({480, 0, DstRule::None, "China"},
        {"cstchina", "shanghai", "beijing", "hongkong", "taipei", "singapore", "perth", "manila",
         "hkt", "sgt", "cstcn"});
    add({330, 0, DstRule::None, "India"},
        {"ist", "india", "mumbai", "delhi", "kolkata", "bangalore", "chennai", "hyderabad"});
    add({480, 0, DstRule::None, "AWST"}, {"awst"});
    add({600, 60, DstRule::AU, "Sydney"}, {"aest", "aedt", "sydney", "melbourne", "canberra"});
    add({600, 0, DstRule::None, "Brisbane"}, {"brisbane", "queensland"});
    add({240, 0, DstRule::None, "Dubai"}, {"gst", "dubai", "abudhabi", "muscat"});
    add({720, 60, DstRule::NZ, "Auckland"}, {"nzst", "nzdt", "auckland", "wellington", "nz"});
    add({180, 0, DstRule::None, "East Africa"}, {"eat", "nairobi", "doha", "riyadh"});
    add({420, 0, DstRule::None, "Bangkok"}, {"ict", "bangkok", "jakarta", "hanoi", "saigon",
                                            "hochiminh", "wib"});
    add({-180, 0, DstRule::None, "São Paulo"},
        {"saopaulo", "brasilia", "brt", "rio", "saobrazil", "americasaopaulo"});
    add({-180, 0, DstRule::None, "Buenos Aires"}, {"buenosaires", "art", "americabuenosaires"});
    add({0, 60, DstRule::EU, "Lisbon"}, {"weti", "lisbon", "europelisbon", "portugal"});
    add({0, 0, DstRule::None, "Casablanca"}, {"casablanca", "morocco"});
    add({60, 0, DstRule::None, "Lagos"}, {"wat", "lagos", "africalagos"});
    add({-240, 60, DstRule::US, "Atlantic"},
        {"atlantic", "halifax", "americahalifax"});
    add({-240, 0, DstRule::None, "AST"}, {"ast", "puertorico"});
    add({-180, 0, DstRule::None, "ADT"}, {"adt"});
    add({-150, 60, DstRule::US, "Newfoundland"},
        {"newfoundland", "stjohns", "americastjohns"});
    add({-210, 0, DstRule::None, "NST"}, {"nst"});
    add({-150, 0, DstRule::None, "NDT"}, {"ndt"});
    return u;
  }();
  return m;
}

bool parse_utc_offset(const std::string& n, Zone& z) {
  if (n.size() < 4) return false;
  std::string p = n;
  if (p.rfind("utc", 0) == 0)
    p = p.substr(3);
  else if (p.rfind("gmt", 0) == 0)
    p = p.substr(3);
  else
    return false;
  if (p.empty()) {
    z = {0, 0, DstRule::None, "UTC"};
    return true;
  }
  if (p.front() != '+' && p.front() != '-') return false;
  int sign = p.front() == '-' ? -1 : 1;
  p.erase(p.begin());
  int hours = 0, mins = 0;
  auto colon = p.find(':');
  try {
    if (colon == std::string::npos) {
      if (p.size() <= 2)
        hours = std::stoi(p);
      else if (p.size() == 4) {
        hours = std::stoi(p.substr(0, 2));
        mins = std::stoi(p.substr(2, 2));
      } else {
        hours = std::stoi(p);
      }
    } else {
      hours = std::stoi(p.substr(0, colon));
      mins = std::stoi(p.substr(colon + 1));
    }
  } catch (...) {
    return false;
  }
  if (hours > 14 || mins >= 60) return false;
  z.std_min = sign * (hours * 60 + mins);
  z.dst_extra = 0;
  z.dst = DstRule::None;
  z.label = "UTC offset";
  return true;
}

bool find_zone(std::string_view raw, Zone& z) {
  auto n = norm_token(std::string(raw));
  if (n.empty()) return false;
  auto it = zones().find(n);
  if (it != zones().end()) {
    z = it->second;
    return true;
  }
  return parse_utc_offset(n, z);
}

const char* weekday_name(unsigned wd) {
  static const char* n[] = {"Sunday",   "Monday", "Tuesday",  "Wednesday",
                            "Thursday", "Friday", "Saturday"};
  return n[wd % 7];
}

const char* month_name(unsigned m) {
  static const char* n[] = {"January", "February", "March",     "April",   "May",      "June",
                            "July",    "August",   "September", "October", "November", "December"};
  return n[(m - 1) % 12];
}

sys_seconds unix_to_sys(std::int64_t s) { return sys_seconds{seconds{s}}; }

std::int64_t sys_to_unix(sys_seconds t) { return duration_cast<seconds>(t.time_since_epoch()).count(); }

void civil_from_unix(std::int64_t s, int& y, unsigned& mo, unsigned& d, int& h, int& mi, int& se) {
  auto tp = unix_to_sys(s);
  auto dp = floor<days>(tp);
  year_month_day ymd{dp};
  y = static_cast<int>(ymd.year());
  mo = static_cast<unsigned>(ymd.month());
  d = static_cast<unsigned>(ymd.day());
  auto tod = tp - sys_seconds{dp};
  auto hh = duration_cast<hours>(tod);
  auto mm = duration_cast<minutes>(tod - hh);
  auto ss = duration_cast<seconds>(tod - hh - mm);
  h = static_cast<int>(hh.count());
  mi = static_cast<int>(mm.count());
  se = static_cast<int>(ss.count());
}

std::int64_t unix_from_civil(int y, unsigned mo, unsigned d, int h, int mi, int se) {
  auto dp = sys_days{year{y} / month{mo} / day{d}};
  return sys_to_unix(sys_seconds{dp} + hours{h} + minutes{mi} + seconds{se});
}

unsigned weekday_from_unix(std::int64_t s) {
  weekday wd{floor<days>(unix_to_sys(s))};
  return wd.c_encoding();  // 0 = Sunday
}

std::string format_clock(int h, int mi, int se, bool with_sec) {
  int h12 = h % 12;
  if (h12 == 0) h12 = 12;
  const char* ap = h < 12 ? "AM" : "PM";
  char buf[32];
  if (with_sec)
    std::snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", h12, mi, se, ap);
  else
    std::snprintf(buf, sizeof(buf), "%d:%02d %s", h12, mi, ap);
  return buf;
}

std::string format_date(int y, unsigned mo, unsigned d, unsigned wd) {
  char buf[80];
  std::snprintf(buf, sizeof(buf), "%s, %s %u, %d", weekday_name(wd), month_name(mo), d, y);
  return buf;
}

std::string format_iso_date(int y, unsigned mo, unsigned d) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02u-%02u", y, mo, d);
  return buf;
}

std::string format_offset(int mins) {
  char buf[16];
  int sign = mins < 0 ? -1 : 1;
  int a = std::abs(mins);
  std::snprintf(buf, sizeof(buf), "%c%02d:%02d", sign < 0 ? '-' : '+', a / 60, a % 60);
  return buf;
}

bool parse_int_span(const std::string& s, std::size_t& i, int& v) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  if (i >= s.size()) return false;
  std::size_t start = i;
  if (s[i] == '+' || s[i] == '-') ++i;
  bool any = false;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
    any = true;
    ++i;
  }
  if (!any) {
    i = start;
    return false;
  }
  try {
    v = std::stoi(s.substr(start, i - start));
  } catch (...) {
    return false;
  }
  return true;
}

bool parse_clock(const std::string& s, std::size_t& i, int& h, int& mi, int& se) {
  std::size_t save = i;
  int hour = 0;
  if (!parse_int_span(s, i, hour)) return false;
  mi = 0;
  se = 0;
  bool had_colon = false;
  if (i < s.size() && s[i] == ':') {
    had_colon = true;
    ++i;
    if (!parse_int_span(s, i, mi)) {
      i = save;
      return false;
    }
    if (i < s.size() && s[i] == ':') {
      ++i;
      if (!parse_int_span(s, i, se)) {
        i = save;
        return false;
      }
    }
  }
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  bool pm = false, am = false;
  if (i + 1 < s.size()) {
    char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    char b = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i + 1])));
    if (a == 'p' && b == 'm') {
      pm = true;
      i += 2;
    } else if (a == 'a' && b == 'm') {
      am = true;
      i += 2;
    }
  }
  if (hour < 0 || mi < 0 || mi > 59 || se < 0 || se > 59) {
    i = save;
    return false;
  }
  if (am || pm) {
    if (hour > 12) {
      i = save;
      return false;
    }
    if (pm && hour != 12) hour += 12;
    if (am && hour == 12) hour = 0;
  } else if (!had_colon || hour > 23) {
    i = save;
    return false;
  }
  h = hour;
  return true;
}

bool parse_iso_date(const std::string& s, std::size_t& i, int& y, unsigned& mo, unsigned& d) {
  std::size_t save = i;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  if (i + 9 >= s.size()) {
    i = save;
    return false;
  }
  if (!(std::isdigit(static_cast<unsigned char>(s[i])) &&
        std::isdigit(static_cast<unsigned char>(s[i + 1])) &&
        std::isdigit(static_cast<unsigned char>(s[i + 2])) &&
        std::isdigit(static_cast<unsigned char>(s[i + 3])) && (s[i + 4] == '-' || s[i + 4] == '/') &&
        std::isdigit(static_cast<unsigned char>(s[i + 5])) &&
        std::isdigit(static_cast<unsigned char>(s[i + 6])) && s[i + 7] == s[i + 4] &&
        std::isdigit(static_cast<unsigned char>(s[i + 8])) &&
        std::isdigit(static_cast<unsigned char>(s[i + 9])))) {
    i = save;
    return false;
  }
  try {
    y = std::stoi(s.substr(i, 4));
    mo = static_cast<unsigned>(std::stoi(s.substr(i + 5, 2)));
    d = static_cast<unsigned>(std::stoi(s.substr(i + 8, 2)));
  } catch (...) {
    i = save;
    return false;
  }
  if (mo < 1 || mo > 12 || d < 1 || d > 31) {
    i = save;
    return false;
  }
  auto ymd = year{y} / month{mo} / day{d};
  if (!ymd.ok()) {
    i = save;
    return false;
  }
  i += 10;
  return true;
}

int month_from_name(const std::string& s, std::size_t i, std::size_t& len) {
  static const char* names[] = {"january", "february", "march",     "april",   "may",      "june",
                                "july",    "august",   "september", "october", "november", "december"};
  static const char* abbr[] = {"jan", "feb", "mar", "apr", "may", "jun",
                               "jul", "aug", "sep", "oct", "nov", "dec"};
  auto rest = to_lower_utf8(s.substr(i));
  for (int m = 0; m < 12; ++m) {
    auto n = std::string_view(names[m]);
    if (rest.rfind(n, 0) == 0) {
      std::size_t after = i + n.size();
      if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) continue;
      len = n.size();
      return m + 1;
    }
  }
  for (int m = 0; m < 12; ++m) {
    auto n = std::string_view(abbr[m]);
    if (rest.rfind(n, 0) == 0) {
      std::size_t after = i + n.size();
      if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) continue;
      len = n.size();
      return m + 1;
    }
  }
  return 0;
}

int weekday_from_name(const std::string& s, std::size_t i, std::size_t& len) {
  static const char* names[] = {"sunday",   "monday", "tuesday", "wednesday",
                                "thursday", "friday", "saturday"};
  static const char* abbr[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  auto rest = to_lower_utf8(s.substr(i));
  for (int w = 0; w < 7; ++w) {
    auto n = std::string_view(names[w]);
    if (rest.rfind(n, 0) == 0) {
      std::size_t after = i + n.size();
      if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) continue;
      len = n.size();
      return w;
    }
  }
  for (int w = 0; w < 7; ++w) {
    auto n = std::string_view(abbr[w]);
    if (rest.rfind(n, 0) == 0) {
      std::size_t after = i + n.size();
      if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) continue;
      len = n.size();
      return w;
    }
  }
  return -1;
}

bool parse_written_date(const std::string& s, std::size_t& i, int& y, unsigned& mo, unsigned& d) {
  std::size_t save = i;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ','))
    ++i;
  std::size_t mlen = 0;
  int month = month_from_name(s, i, mlen);
  int day = 0, year = 0;
  if (month > 0) {
    i += mlen;
    if (!parse_int_span(s, i, day)) {
      i = save;
      return false;
    }
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ','))
      ++i;
    if (!parse_int_span(s, i, year)) {
      i = save;
      return false;
    }
  } else {
    if (!parse_int_span(s, i, day)) {
      i = save;
      return false;
    }
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ','))
      ++i;
    month = month_from_name(s, i, mlen);
    if (month <= 0) {
      i = save;
      return false;
    }
    i += mlen;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ','))
      ++i;
    if (!parse_int_span(s, i, year)) {
      i = save;
      return false;
    }
  }
  if (year >= 0 && year < 100) year += 2000;
  if (month < 1 || month > 12 || day < 1 || day > 31) {
    i = save;
    return false;
  }
  auto ymd = year{year} / month{static_cast<unsigned>(month)} / day{static_cast<unsigned>(day)};
  if (!ymd.ok()) {
    i = save;
    return false;
  }
  y = year;
  mo = static_cast<unsigned>(month);
  d = static_cast<unsigned>(day);
  return true;
}

bool consume_word(const std::string& s, std::size_t& i, const char* word) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  auto w = std::string_view(word);
  if (i + w.size() > s.size()) return false;
  for (std::size_t k = 0; k < w.size(); ++k) {
    if (std::tolower(static_cast<unsigned char>(s[i + k])) !=
        static_cast<unsigned char>(w[k]))
      return false;
  }
  std::size_t after = i + w.size();
  if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) return false;
  i = after;
  return true;
}

enum class DateUnit { Seconds, Minutes, Hours, Days, Weeks, Months, Years };

bool parse_unit(const std::string& s, std::size_t& i, DateUnit& u) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  auto rest = to_lower_utf8(s.substr(i));
  auto take = [&](std::initializer_list<const char*> names, DateUnit unit) {
    for (auto n : names) {
      auto len = std::string_view(n).size();
      if (rest.rfind(n, 0) == 0) {
        std::size_t after = i + len;
        if (after < s.size() && std::isalpha(static_cast<unsigned char>(s[after]))) continue;
        i = after;
        u = unit;
        return true;
      }
    }
    return false;
  };
  if (take({"seconds", "second", "secs", "sec", "s"}, DateUnit::Seconds)) return true;
  if (take({"minutes", "minute", "mins", "min"}, DateUnit::Minutes)) return true;
  if (take({"hours", "hour", "hrs", "hr", "h"}, DateUnit::Hours)) return true;
  if (take({"days", "day", "d"}, DateUnit::Days)) return true;
  if (take({"weeks", "week", "wks", "wk", "w"}, DateUnit::Weeks)) return true;
  if (take({"months", "month", "mos", "mo"}, DateUnit::Months)) return true;
  if (take({"years", "year", "yrs", "yr", "y"}, DateUnit::Years)) return true;
  return false;
}

sys_days add_months(sys_days dp, int n) {
  year_month_day ymd{dp};
  auto ym = ymd.year() / ymd.month();
  ym += months{n};
  auto out = ym / ymd.day();
  if (!out.ok()) out = ym / last;
  return sys_days{out};
}

std::int64_t add_unit(std::int64_t unix_sec, int n, DateUnit u) {
  auto tp = unix_to_sys(unix_sec);
  switch (u) {
    case DateUnit::Seconds:
      return sys_to_unix(tp + seconds{n});
    case DateUnit::Minutes:
      return sys_to_unix(tp + minutes{n});
    case DateUnit::Hours:
      return sys_to_unix(tp + hours{n});
    case DateUnit::Days:
      return sys_to_unix(tp + days{n});
    case DateUnit::Weeks:
      return sys_to_unix(tp + weeks{n});
    case DateUnit::Months: {
      auto dp = floor<days>(tp);
      auto tod = tp - sys_seconds{dp};
      auto nd = add_months(dp, n);
      return sys_to_unix(sys_seconds{nd} + duration_cast<seconds>(tod));
    }
    case DateUnit::Years: {
      auto dp = floor<days>(tp);
      auto tod = tp - sys_seconds{dp};
      auto nd = add_months(dp, n * 12);
      return sys_to_unix(sys_seconds{nd} + duration_cast<seconds>(tod));
    }
  }
  return unix_sec;
}

bool skip_from_now(const std::string& s, std::size_t& i) {
  std::size_t save = i;
  if (consume_word(s, i, "from") && consume_word(s, i, "now")) return true;
  i = save;
  return consume_word(s, i, "later");
}

void fill_datetime(MathResult& out, std::int64_t utc, int offset_min, const char* zone_label,
                   bool date_only) {
  std::int64_t wall = utc + static_cast<std::int64_t>(offset_min) * 60;
  int y = 0, h = 0, mi = 0, se = 0;
  unsigned mo = 1, d = 1;
  civil_from_unix(wall, y, mo, d, h, mi, se);
  auto wd = weekday_from_unix(wall);
  out.ok = true;
  out.conversion = true;
  out.datetime = true;
  out.currency = false;
  out.color = false;
  out.value = static_cast<double>(utc);
  auto iso = format_iso_date(y, mo, d);
  auto date = format_date(y, mo, d, wd);
  if (date_only) {
    out.display = iso + "  ·  " + date;
  } else {
    auto clock = format_clock(h, mi, se, se != 0);
    std::string z = zone_label ? zone_label : "UTC";
    out.display = clock + "  " + z + "  ·  " + iso + "  UTC" + format_offset(offset_min);
  }
  (void)date;
  out.error.clear();
}

bool parse_base_time(const std::string& s, std::size_t& i, std::int64_t& utc, bool& date_only) {
  date_only = false;
  auto now = unix_seconds();
  if (consume_word(s, i, "now") || consume_word(s, i, "time")) {
    utc = now;
    return true;
  }
  if (consume_word(s, i, "noon")) {
    int y, h, mi, se;
    unsigned mo, d;
    civil_from_unix(now, y, mo, d, h, mi, se);
    utc = unix_from_civil(y, mo, d, 12, 0, 0);
    return true;
  }
  if (consume_word(s, i, "midnight")) {
    int y, h, mi, se;
    unsigned mo, d;
    civil_from_unix(now, y, mo, d, h, mi, se);
    utc = unix_from_civil(y, mo, d, 0, 0, 0);
    return true;
  }
  if (consume_word(s, i, "today")) {
    int y, h, mi, se;
    unsigned mo, d;
    civil_from_unix(now, y, mo, d, h, mi, se);
    utc = unix_from_civil(y, mo, d, 0, 0, 0);
    date_only = true;
    return true;
  }
  if (consume_word(s, i, "tomorrow")) {
    int y, h, mi, se;
    unsigned mo, d;
    civil_from_unix(now, y, mo, d, h, mi, se);
    utc = unix_from_civil(y, mo, d, 0, 0, 0) + 86400;
    date_only = true;
    return true;
  }
  if (consume_word(s, i, "yesterday")) {
    int y, h, mi, se;
    unsigned mo, d;
    civil_from_unix(now, y, mo, d, h, mi, se);
    utc = unix_from_civil(y, mo, d, 0, 0, 0) - 86400;
    date_only = true;
    return true;
  }
  int y = 0, h = 0, mi = 0, se = 0;
  unsigned mo = 1, d = 1;
  if (parse_iso_date(s, i, y, mo, d)) {
    h = 0;
    mi = 0;
    se = 0;
    date_only = true;
    while (i < s.size() && (s[i] == ' ' || s[i] == 'T' || s[i] == 't')) {
      if (s[i] == 'T' || s[i] == 't') {
        ++i;
        break;
      }
      std::size_t j = i + 1;
      int hh, mm, ss;
      if (parse_clock(s, j, hh, mm, ss)) {
        h = hh;
        mi = mm;
        se = ss;
        i = j;
        date_only = false;
        break;
      }
      break;
    }
    utc = unix_from_civil(y, mo, d, h, mi, se);
    return true;
  }
  {
    std::size_t k = i;
    int wy = 0;
    unsigned wmo = 1, wd = 1;
    if (parse_written_date(s, k, wy, wmo, wd)) {
      h = 0;
      mi = 0;
      se = 0;
      date_only = true;
      i = k;
      while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
      std::size_t j = i;
      int hh, mm, ss;
      if (parse_clock(s, j, hh, mm, ss)) {
        h = hh;
        mi = mm;
        se = ss;
        i = j;
        date_only = false;
      }
      utc = unix_from_civil(wy, wmo, wd, h, mi, se);
      return true;
    }
  }
  std::size_t j = i;
  int hh = 0, mm = 0, ss = 0;
  if (parse_clock(s, j, hh, mm, ss)) {
    int cy, ch, cmi, cse;
    unsigned cmo, cd;
    civil_from_unix(now, cy, cmo, cd, ch, cmi, cse);
    utc = unix_from_civil(cy, cmo, cd, hh, mm, ss);
    i = j;
    date_only = false;
    return true;
  }
  return false;
}

bool parse_zone_from_rest(const std::string& s, std::size_t i, Zone& z) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
    ++i;
  consume_word(s, i, "in");
  consume_word(s, i, "at");
  auto rest = trim_copy(s.substr(i));
  if (rest.empty()) return false;
  return find_zone(rest, z);
}

bool try_unix(const std::string& l, MathResult& out) {
  std::size_t i = 0;
  if (!(consume_word(l, i, "unix") || consume_word(l, i, "epoch") ||
        consume_word(l, i, "timestamp")))
    return false;
  while (i < l.size() && (l[i] == ' ' || l[i] == '\t'))
    ++i;
  if (i >= l.size()) return false;
  std::int64_t n = 0;
  try {
    n = std::stoll(l.substr(i));
  } catch (...) {
    return false;
  }
  if (std::llabs(n) > 100000000000ll) n /= 1000;
  Zone z{0, 0, DstRule::None, "UTC"};
  auto pos = l.find(" to ");
  if (pos != std::string::npos) find_zone(l.substr(pos + 4), z);
  fill_datetime(out, n, zone_offset_at(z, n), z.label, false);
  return true;
}

bool try_until_since(const std::string& l, MathResult& out) {
  std::size_t i = 0;
  DateUnit unit = DateUnit::Days;
  bool have_unit = parse_unit(l, i, unit);
  bool until = false, since = false;
  if (have_unit) {
    until = consume_word(l, i, "until") || consume_word(l, i, "to") || consume_word(l, i, "before");
    since = consume_word(l, i, "since") || consume_word(l, i, "after") || consume_word(l, i, "from");
    if (!until && !since) return false;
  } else {
    i = 0;
    until = consume_word(l, i, "until");
    since = consume_word(l, i, "since");
    if (!until && !since) return false;
    unit = DateUnit::Days;
  }
  std::int64_t target = 0;
  bool date_only = false;
  if (!parse_base_time(l, i, target, date_only)) return false;
  auto now = unix_seconds();
  double delta = static_cast<double>(until ? (target - now) : (now - target));
  double denom = 86400.0;
  const char* label = "days";
  switch (unit) {
    case DateUnit::Seconds:
      denom = 1;
      label = "seconds";
      break;
    case DateUnit::Minutes:
      denom = 60;
      label = "minutes";
      break;
    case DateUnit::Hours:
      denom = 3600;
      label = "hours";
      break;
    case DateUnit::Days:
      denom = 86400;
      label = "days";
      break;
    case DateUnit::Weeks:
      denom = 86400 * 7;
      label = "weeks";
      break;
    case DateUnit::Months:
      denom = 86400 * 30.4375;
      label = "months";
      break;
    case DateUnit::Years:
      denom = 86400 * 365.25;
      label = "years";
      break;
  }
  double n = delta / denom;
  out.ok = true;
  out.conversion = true;
  out.datetime = true;
  out.value = n;
  char buf[64];
  if (std::fabs(n - std::round(n)) < 1e-6 && denom >= 86400)
    std::snprintf(buf, sizeof(buf), "%.0f %s", std::round(n), label);
  else
    std::snprintf(buf, sizeof(buf), "%.2f %s", n, label);
  out.display = buf;
  out.error.clear();
  return true;
}

bool try_in_ago(const std::string& l, MathResult& out) {
  std::size_t i = 0;
  int sign = 1;
  bool prefixed = consume_word(l, i, "in");
  int n = 0;
  if (!parse_int_span(l, i, n)) return false;
  DateUnit u;
  if (!parse_unit(l, i, u)) return false;
  if (consume_word(l, i, "ago")) {
    if (prefixed) return false;
    sign = -1;
  } else {
    bool later = skip_from_now(l, i);
    if (!prefixed && !later) return false;
  }
  auto utc = add_unit(unix_seconds(), sign * n, u);
  bool date_only = (u == DateUnit::Days || u == DateUnit::Weeks || u == DateUnit::Months ||
                    u == DateUnit::Years);
  while (true) {
    std::size_t save = i;
    int extra = 0;
    DateUnit u2;
    if (!parse_int_span(l, i, extra) || !parse_unit(l, i, u2)) {
      i = save;
      break;
    }
    utc = add_unit(utc, sign * extra, u2);
    if (u2 == DateUnit::Hours || u2 == DateUnit::Minutes || u2 == DateUnit::Seconds)
      date_only = false;
  }
  skip_from_now(l, i);
  while (i < l.size() && (l[i] == ' ' || l[i] == '\t'))
    ++i;
  if (i < l.size()) return false;
  fill_datetime(out, utc, 0, "UTC", date_only);
  return true;
}

bool try_arith(const std::string& l, MathResult& out) {
  std::size_t i = 0;
  std::int64_t utc = 0;
  bool date_only = false;
  if (!parse_base_time(l, i, utc, date_only)) return false;
  while (i < l.size() && (l[i] == ' ' || l[i] == '\t'))
    ++i;
  if (i >= l.size()) {
    if (date_only || l.find('-') != std::string::npos) {
      fill_datetime(out, utc, 0, "UTC", date_only);
      return date_only || l.find("today") != std::string::npos ||
             l.find("tomorrow") != std::string::npos || l.find("yesterday") != std::string::npos;
    }
    return false;
  }
  int sign = 0;
  if (l[i] == '+')
    sign = 1;
  else if (l[i] == '-')
    sign = -1;
  else
    return false;
  ++i;
  int n = 0;
  if (!parse_int_span(l, i, n)) return false;
  DateUnit u;
  if (!parse_unit(l, i, u)) return false;
  utc = add_unit(utc, sign * n, u);
  if (u == DateUnit::Hours || u == DateUnit::Minutes || u == DateUnit::Seconds) date_only = false;
  while (true) {
    while (i < l.size() && (l[i] == ' ' || l[i] == '\t'))
      ++i;
    if (i >= l.size()) break;
    int s2 = 0;
    if (l[i] == '+')
      s2 = 1;
    else if (l[i] == '-')
      s2 = -1;
    else
      break;
    ++i;
    int n2 = 0;
    DateUnit u2;
    if (!parse_int_span(l, i, n2) || !parse_unit(l, i, u2)) return false;
    utc = add_unit(utc, s2 * n2, u2);
    if (u2 == DateUnit::Hours || u2 == DateUnit::Minutes || u2 == DateUnit::Seconds)
      date_only = false;
  }
  fill_datetime(out, utc, 0, "UTC", date_only);
  return true;
}

bool split_to(const std::string& l, std::string& left, std::string& right) {
  auto pos = l.find(" to ");
  if (pos == std::string::npos) pos = l.find(" in ");
  if (pos == std::string::npos) return false;
  left = trim_copy(l.substr(0, pos));
  right = trim_copy(l.substr(pos + 4));
  return !right.empty();
}

bool try_zone_convert(const std::string& l, MathResult& out) {
  std::string left, right;
  if (!split_to(l, left, right)) {
    // "now tokyo" / "time london"
    std::size_t i = 0;
    if (consume_word(l, i, "now") || consume_word(l, i, "time") || consume_word(l, i, "date")) {
      Zone z;
      if (!parse_zone_from_rest(l, i, z) && !find_zone(l.substr(i), z)) return false;
      auto now = unix_seconds();
      fill_datetime(out, now, zone_offset_at(z, now), z.label, false);
      return true;
    }
    Zone z;
    return false;
  }
  Zone dest;
  if (!find_zone(right, dest)) return false;
  std::size_t i = 0;
  std::int64_t utc = unix_seconds();
  bool date_only = false;
  Zone src;
  bool have_src = false;
  auto ll = to_lower_utf8(left);
  if (ll == "now" || ll == "time" || ll == "date" || ll.empty()) {
    utc = unix_seconds();
  } else {
    bool parsed_time = parse_base_time(ll, i, utc, date_only);
    while (i < ll.size() && (ll[i] == ' ' || ll[i] == '\t'))
      ++i;
    if (i < ll.size()) {
      have_src = find_zone(ll.substr(i), src);
    }
    if (!have_src) {
      // "3pm est" parsed clock then zone; if parse_base_time consumed the number only
      std::size_t j = 0;
      int h, mi, se;
      if (parse_clock(ll, j, h, mi, se)) {
        int y, ch, cmi, cse;
        unsigned mo, d;
        civil_from_unix(unix_seconds(), y, mo, d, ch, cmi, cse);
        utc = unix_from_civil(y, mo, d, h, mi, se);
        parsed_time = true;
        have_src = find_zone(ll.substr(j), src);
      }
    }
    if (!parsed_time && find_zone(left, src)) {
      have_src = true;
      utc = unix_seconds();
    }
    if (!parsed_time && !have_src) return false;
    if (have_src) {
      // `utc` is currently wall time encoded as if UTC.
      int off = src.std_min;
      auto as_utc = utc - static_cast<std::int64_t>(off) * 60;
      off = zone_offset_at(src, as_utc);
      utc = utc - static_cast<std::int64_t>(off) * 60;
    }
  }
  int doff = zone_offset_at(dest, utc);
  fill_datetime(out, utc, doff, dest.label, false);
  return true;
}

}  // namespace

bool convert_datetime(std::string_view expr, MathResult& out) {
  auto l = to_lower_utf8(trim_copy(std::string(expr)));
  if (l.rfind("convert ", 0) == 0) l = trim_copy(l.substr(8));
  if (l.rfind("tz ", 0) == 0) l = trim_copy(l.substr(3));
  if (l.rfind("timezone ", 0) == 0) l = trim_copy(l.substr(9));
  if (l.empty()) return false;
  if (try_unix(l, out)) return true;
  if (try_until_since(l, out)) return true;
  if (try_in_ago(l, out)) return true;
  if (try_arith(l, out)) return true;
  if (try_zone_convert(l, out)) return true;
  return false;
}

}  // namespace wilfred
