#include "wilfred/math/expr.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <cstdio>
#endif

namespace wilfred {
namespace {

enum class Qty {
  Length,
  Mass,
  Volume,
  Area,
  Temperature,
  Speed,
  Energy,
  Pressure,
  Time,
  Data,
  Force,
  Power,
  Angle
};

struct UnitDef {
  Qty qty;
  double mul;
  double add;
};

std::string norm_unit(std::string u) {
  while (!u.empty() && u.front() == ' ')
    u.erase(u.begin());
  while (!u.empty() && u.back() == ' ')
    u.pop_back();
  std::string o;
  o.reserve(u.size());
  for (std::size_t i = 0; i < u.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(u[i]);
    if (c == ' ' || c == '.' || c == '-') continue;
    // UTF-8 encoding of U+00B2 (²) is 0xC2 0xB2, and U+00B3 (³) is 0xC2 0xB3.
    if (c == 0xC2 && i + 1 < u.size()) {
      unsigned char c2 = static_cast<unsigned char>(u[i + 1]);
      if (c2 == 0xB2) {
        o += "2";
        ++i;
        continue;
      }
      if (c2 == 0xB3) {
        o += "3";
        ++i;
        continue;
      }
    }
    // Also tolerate a raw single-byte 0xB2/0xB3 (e.g. Latin-1 input).
    if (c == 0xB2) {
      o += "2";
      continue;
    }
    if (c == 0xB3) {
      o += "3";
      continue;
    }
    o.push_back(static_cast<char>(std::tolower(c)));
  }
  return o;
}

void add_unit(std::unordered_map<std::string, UnitDef>& m, Qty q, double mul, double add,
              std::initializer_list<const char*> names) {
  UnitDef d{q, mul, add};
  for (auto n : names)
    m.emplace(n, d);
}

const std::unordered_map<std::string, UnitDef>& units() {
  static const auto m = [] {
    std::unordered_map<std::string, UnitDef> u;
    // Length — SI metre
    add_unit(u, Qty::Length, 1e-9, 0, {"nm", "nanometer", "nanometers", "nanometre", "nanometres"});
    add_unit(u, Qty::Length, 1e-6, 0,
             {"um", "µm", "micrometer", "micrometers", "micrometre", "micrometres"});
    add_unit(u, Qty::Length, 1e-3, 0,
             {"mm", "millimeter", "millimeters", "millimetre", "millimetres"});
    add_unit(u, Qty::Length, 0.01, 0,
             {"cm", "centimeter", "centimeters", "centimetre", "centimetres"});
    add_unit(u, Qty::Length, 0.1, 0, {"dm", "decimeter", "decimeters", "decimetre", "decimetres"});
    add_unit(u, Qty::Length, 1, 0, {"m", "meter", "meters", "metre", "metres"});
    add_unit(u, Qty::Length, 10, 0, {"dam", "decameter", "decameters"});
    add_unit(u, Qty::Length, 100, 0, {"hm", "hectometer", "hectometers"});
    add_unit(u, Qty::Length, 1000, 0, {"km", "kilometer", "kilometers", "kilometre", "kilometres"});
    add_unit(u, Qty::Length, 0.0254, 0, {"in", "inch", "inches", "\""});
    add_unit(u, Qty::Length, 0.3048, 0, {"ft", "foot", "feet"});
    add_unit(u, Qty::Length, 0.9144, 0, {"yd", "yard", "yards"});
    add_unit(u, Qty::Length, 1609.344, 0, {"mi", "mile", "miles"});
    add_unit(u, Qty::Length, 1852, 0, {"nmi", "nauticalmile", "nauticalmiles"});

    // Mass — SI kilogram
    add_unit(u, Qty::Mass, 1e-9, 0, {"ug", "µg", "mcg", "microgram", "micrograms"});
    add_unit(u, Qty::Mass, 1e-6, 0, {"mg", "mgs", "milligram", "milligrams"});
    add_unit(u, Qty::Mass, 0.001, 0, {"g", "gm", "gms", "gram", "grams", "gramme", "grammes"});
    add_unit(u, Qty::Mass, 1, 0, {"kg", "kgs", "kilogram", "kilograms", "kilo", "kilos"});
    add_unit(u, Qty::Mass, 1000, 0, {"t", "tonne", "tonnes", "metricton", "metrictons"});
    add_unit(u, Qty::Mass, 0.45359237, 0, {"lb", "lbs", "pound", "pounds"});
    add_unit(u, Qty::Mass, 0.028349523125, 0, {"oz", "ozs", "ounce", "ounces"});
    add_unit(u, Qty::Mass, 6.35029318, 0, {"st", "stone", "stones"});
    add_unit(u, Qty::Mass, 6.479891e-5, 0, {"gr", "grain", "grains"});
    add_unit(u, Qty::Mass, 0.0017718451953125, 0, {"dr", "dram", "drams"});

    // Volume — SI cubic metre
    add_unit(u, Qty::Volume, 1e-9, 0, {"mm3", "cubicmillimeter", "cubicmillimeters"});
    add_unit(u, Qty::Volume, 1e-6, 0,
             {"ml", "milliliter", "milliliters", "millilitre", "millilitres", "cc", "cm3",
              "cubiccentimeter", "cubiccentimeters"});
    add_unit(u, Qty::Volume, 1e-5, 0,
             {"cl", "centiliter", "centiliters", "centilitre", "centilitres"});
    add_unit(u, Qty::Volume, 1e-4, 0, {"dl", "deciliter", "deciliters", "decilitre", "decilitres"});
    add_unit(u, Qty::Volume, 0.001, 0, {"l", "lt", "ltr", "liter", "liters", "litre", "litres"});
    add_unit(u, Qty::Volume, 1, 0,
             {"m3", "cubicmeter", "cubicmeters", "cubicmetre", "cubicmetres"});
    add_unit(u, Qty::Volume, 1e-3, 0, {"dm3", "cubicdecimeter"});
    add_unit(u, Qty::Volume, 0.00454609, 0,
             {"impgal", "imperialgallon", "imperialgallons", "ukgal"});
    add_unit(u, Qty::Volume, 0.003785411784, 0, {"gal", "gallon", "gallons", "usgal"});
    add_unit(u, Qty::Volume, 0.000946352946, 0, {"qt", "quart", "quarts"});
    add_unit(u, Qty::Volume, 0.000473176473, 0, {"pt", "pint", "pints"});
    add_unit(u, Qty::Volume, 0.0002365882365, 0, {"cup", "cups"});
    add_unit(u, Qty::Volume, 0.00025, 0, {"metriccup", "metriccups"});
    add_unit(u, Qty::Volume, 2.95735295625e-5, 0, {"floz", "flozs", "fluidounce", "fluidounces"});
    add_unit(u, Qty::Volume, 1.478676478125e-5, 0,
             {"tbsp", "tbs", "tbl", "tbls", "tblsp", "tbspn", "tablespoon", "tablespoons",
              "tablespoonful", "tablespoonfuls"});
    add_unit(u, Qty::Volume, 4.92892159375e-6, 0,
             {"tsp", "tspn", "teaspoon", "teaspoons", "teaspoonful", "teaspoonfuls"});
    add_unit(u, Qty::Volume, 9.8578431875e-6, 0, {"dsp", "dessertspoon", "dessertspoons"});
    add_unit(u, Qty::Volume, 6.1611519921875e-7, 0, {"dash", "dashes"});
    add_unit(u, Qty::Volume, 3.08057599609375e-7, 0, {"pinch", "pinches"});

    // Area — SI square metre
    add_unit(u, Qty::Area, 1e-6, 0, {"mm2", "sqmm", "squaremillimeter", "squaremillimeters"});
    add_unit(u, Qty::Area, 1e-4, 0, {"cm2", "sqcm", "squarecentimeter", "squarecentimeters"});
    add_unit(
        u, Qty::Area, 1, 0,
        {"m2", "sqm", "sqmeter", "squaremeter", "squaremeters", "squaremetre", "squaremetres"});
    add_unit(u, Qty::Area, 1e6, 0, {"km2", "sqkm", "squarekilometer", "squarekilometers"});
    add_unit(u, Qty::Area, 1e4, 0, {"ha", "hectare", "hectares"});
    add_unit(u, Qty::Area, 4046.8564224, 0, {"acre", "acres", "ac"});
    add_unit(u, Qty::Area, 0.09290304, 0, {"sqft", "ft2", "squarefoot", "squarefeet"});
    add_unit(u, Qty::Area, 0.00064516, 0, {"sqin", "in2", "squareinch", "squareinches"});
    add_unit(u, Qty::Area, 0.83612736, 0, {"sqyd", "yd2", "squareyard", "squareyards"});
    add_unit(u, Qty::Area, 2.589988110336e6, 0, {"sqmi", "mi2", "squaremile", "squaremiles"});

    // Temperature — SI kelvin (affine)
    add_unit(u, Qty::Temperature, 1, 273.15, {"c", "celsius", "centigrade", "°c", "degc"});
    add_unit(u, Qty::Temperature, 5.0 / 9.0, 273.15 - 32.0 * 5.0 / 9.0,
             {"f", "fahrenheit", "°f", "degf"});
    add_unit(u, Qty::Temperature, 1, 0, {"k", "kelvin", "kelvins"});

    // Speed — SI m/s
    add_unit(u, Qty::Speed, 1, 0, {"mps", "m/s", "meterpersecond", "meterspersecond"});
    add_unit(u, Qty::Speed, 1000.0 / 3600.0, 0,
             {"kmh", "km/h", "kph", "kilometerperhour", "kilometersperhour"});
    add_unit(u, Qty::Speed, 1609.344 / 3600.0, 0, {"mph", "mi/h", "mileperhour", "milesperhour"});
    add_unit(u, Qty::Speed, 1852.0 / 3600.0, 0, {"kn", "kt", "knot", "knots"});
    add_unit(u, Qty::Speed, 0.3048, 0, {"fps", "ft/s", "feetpersecond"});

    // Energy — SI joule
    add_unit(u, Qty::Energy, 1, 0, {"j", "joule", "joules"});
    add_unit(u, Qty::Energy, 1000, 0, {"kj", "kilojoule", "kilojoules"});
    add_unit(u, Qty::Energy, 4.184, 0, {"cal", "calorie", "calories"});
    add_unit(u, Qty::Energy, 4184, 0, {"kcal", "kilocalorie", "kilocalories"});
    add_unit(u, Qty::Energy, 3600, 0, {"wh", "watthour", "watthours"});
    add_unit(u, Qty::Energy, 3.6e6, 0, {"kwh", "kilowatthour", "kilowatthours"});
    add_unit(u, Qty::Energy, 1055.05585262, 0, {"btu", "btus"});

    // Pressure — SI pascal
    add_unit(u, Qty::Pressure, 1, 0, {"pa", "pascal", "pascals"});
    add_unit(u, Qty::Pressure, 1000, 0, {"kpa", "kilopascal", "kilopascals"});
    add_unit(u, Qty::Pressure, 1e6, 0, {"mpa", "megapascal", "megapascals"});
    add_unit(u, Qty::Pressure, 1e5, 0, {"bar", "bars"});
    add_unit(u, Qty::Pressure, 101325, 0, {"atm", "atmosphere", "atmospheres"});
    add_unit(u, Qty::Pressure, 6894.757293168, 0, {"psi"});
    add_unit(u, Qty::Pressure, 133.322368421, 0, {"torr", "mmhg"});

    // Time — SI second
    add_unit(u, Qty::Time, 1e-9, 0, {"ns", "nanosecond", "nanoseconds"});
    add_unit(u, Qty::Time, 1e-6, 0, {"us", "µs", "microsecond", "microseconds"});
    add_unit(u, Qty::Time, 1e-3, 0, {"ms", "millisecond", "milliseconds"});
    add_unit(u, Qty::Time, 1, 0, {"s", "sec", "secs", "second", "seconds"});
    add_unit(u, Qty::Time, 60, 0, {"min", "mins", "minute", "minutes"});
    add_unit(u, Qty::Time, 3600, 0, {"h", "hr", "hrs", "hour", "hours"});
    add_unit(u, Qty::Time, 86400, 0, {"d", "day", "days"});
    add_unit(u, Qty::Time, 604800, 0, {"wk", "week", "weeks"});
    add_unit(u, Qty::Time, 31557600, 0, {"yr", "year", "years"});

    // Data — keep kb/mb as 1024 to match existing launcher behaviour
    add_unit(u, Qty::Data, 1, 0, {"b", "byte", "bytes"});
    add_unit(u, Qty::Data, 1024, 0, {"kb", "kib", "kilobyte", "kilobytes"});
    add_unit(u, Qty::Data, 1024.0 * 1024, 0, {"mb", "mib", "megabyte", "megabytes"});
    add_unit(u, Qty::Data, 1024.0 * 1024 * 1024, 0, {"gb", "gib", "gigabyte", "gigabytes"});
    add_unit(u, Qty::Data, 1024.0 * 1024 * 1024 * 1024, 0, {"tb", "tib", "terabyte", "terabytes"});
    add_unit(u, Qty::Data, 0.125, 0, {"bit", "bits"});

    // Force — SI newton
    add_unit(u, Qty::Force, 1, 0, {"n", "newton", "newtons"});
    add_unit(u, Qty::Force, 1000, 0, {"kilonewton", "kilonewtons"});
    add_unit(u, Qty::Force, 4.4482216152605, 0, {"lbf", "poundforce"});

    // Power — SI watt
    add_unit(u, Qty::Power, 1, 0, {"w", "watt", "watts"});
    add_unit(u, Qty::Power, 1000, 0, {"kw", "kilowatt", "kilowatts"});
    add_unit(u, Qty::Power, 1e6, 0, {"mwatt", "megawatt", "megawatts"});
    add_unit(u, Qty::Power, 745.6998715822702, 0, {"hp", "horsepower"});

    // Angle — SI radian
    add_unit(u, Qty::Angle, 1, 0, {"rad", "radian", "radians"});
    add_unit(u, Qty::Angle, 3.14159265358979323846 / 180.0, 0, {"deg", "degree", "degrees"});

    return u;
  }();
  return m;
}

std::string format_value(double v) {
  std::ostringstream os;
  os.precision(12);
  os << v;
  return os.str();
}

bool starts_utf8(const std::string& s, std::size_t i, std::string_view seq) {
  if (i + seq.size() > s.size()) return false;
  return s.compare(i, seq.size(), seq.data(), seq.size()) == 0;
}

bool consume_money_sym(const std::string& s, std::size_t& i, std::string& sym) {
  if (i >= s.size()) return false;
  if (s[i] == '$') {
    sym = "$";
    ++i;
    return true;
  }
  if (starts_utf8(s, i, "\xE2\x82\xAC")) {  // €
    sym = "eur";
    i += 3;
    return true;
  }
  if (starts_utf8(s, i, "\xC2\xA3")) {  // £
    sym = "gbp";
    i += 2;
    return true;
  }
  if (starts_utf8(s, i, "\xC2\xA5")) {  // ¥
    sym = "jpy";
    i += 2;
    return true;
  }
  if (starts_utf8(s, i, "\xE2\x82\xB9")) {  // ₹
    sym = "inr";
    i += 3;
    return true;
  }
  return false;
}

bool parse_conversion(std::string s, double& value, std::string& from, std::string& to) {
  auto lower = to_lower_utf8(s);
  while (!lower.empty() && (lower.front() == ' ' || lower.front() == '\t'))
    lower.erase(lower.begin());
  if (lower.rfind("convert ", 0) == 0) lower = lower.substr(8);
  auto topos = lower.find(" to ");
  std::size_t seplen = 4;
  if (topos == std::string::npos) {
    topos = lower.find(" in ");
    seplen = 4;
  }
  if (topos == std::string::npos) return false;
  auto left = lower.substr(0, topos);
  auto right = lower.substr(topos + seplen);
  while (!left.empty() && left.back() == ' ')
    left.pop_back();
  while (!right.empty() && (right.front() == ' ' || right.front() == '\t'))
    right.erase(right.begin());
  while (!right.empty() && right.back() == ' ')
    right.pop_back();
  std::string lead;
  std::size_t i = 0;
  consume_money_sym(left, i, lead);
  std::string num;
  if (i < left.size() && (left[i] == '-' || left[i] == '+')) num.push_back(left[i++]);
  bool dot = false;
  while (i < left.size()) {
    unsigned char c = static_cast<unsigned char>(left[i]);
    if (std::isdigit(c)) {
      num.push_back(left[i++]);
      continue;
    }
    if (left[i] == '.' && !dot) {
      dot = true;
      num.push_back(left[i++]);
      continue;
    }
    break;
  }
  while (i < left.size() && (left[i] == ' ' || left[i] == '\t'))
    ++i;
  std::string trail;
  consume_money_sym(left, i, trail);
  while (i < left.size() && (left[i] == ' ' || left[i] == '\t'))
    ++i;
  from = left.substr(i);
  if (from.empty()) from = !lead.empty() ? lead : trail;
  std::string to_sym;
  std::size_t ti = 0;
  if (consume_money_sym(right, ti, to_sym) && ti >= right.size())
    to = to_sym;
  else
    to = right;
  if (from.empty() || to.empty()) return false;
  if (num.empty()) {
    value = 1;
  } else {
    try {
      value = std::stod(num);
    } catch (...) {
      return false;
    }
  }
  return true;
}

bool g_fx_net = true;
std::mutex fx_mu;
std::unordered_map<std::string, double> fx_usd;
std::string fx_date;
std::int64_t fx_cache_at{0};

const std::unordered_map<std::string, std::string>& ccy_alias() {
  static const auto m = [] {
    std::unordered_map<std::string, std::string> a;
    auto add = [&](const char* code, std::initializer_list<const char*> names) {
      for (auto n : names)
        a.emplace(n, code);
    };
    add("usd", {"usd", "dollar", "dollars", "buck", "bucks", "$", "usdollar", "usdollars"});
    add("eur", {"eur", "euro", "euros"});
    add("gbp", {"gbp", "pound", "pounds", "sterling", "quid"});
    add("jpy", {"jpy", "yen"});
    add("cny", {"cny", "yuan", "rmb", "renminbi"});
    add("cad", {"cad", "loonie"});
    add("aud", {"aud", "aussie"});
    add("chf", {"chf", "franc", "francs", "swissfranc"});
    add("inr", {"inr", "rupee", "rupees"});
    add("krw", {"krw", "won"});
    add("mxn", {"mxn"});
    add("brl", {"brl", "real", "reais"});
    add("zar", {"zar", "rand"});
    add("nzd", {"nzd", "kiwi"});
    add("sek", {"sek", "krona", "kronor"});
    add("nok", {"nok", "krone", "kroner"});
    add("dkk", {"dkk"});
    add("pln", {"pln", "zloty", "zlotys"});
    add("hkd", {"hkd"});
    add("sgd", {"sgd"});
    add("try", {"try", "lira", "liras"});
    add("thb", {"thb", "baht"});
    add("czk", {"czk", "koruna"});
    add("huf", {"huf", "forint"});
    add("ron", {"ron", "leu"});
    add("bgn", {"bgn", "lev"});
    add("ils", {"ils", "shekel", "shekels"});
    add("php", {"php"});
    add("myr", {"myr", "ringgit"});
    add("idr", {"idr", "rupiah"});
    add("isk", {"isk"});
    return a;
  }();
  return m;
}

void seed_fx_fallback(std::unordered_map<std::string, double>& m) {
  // Approximate USD cross rates used offline until a live table is fetched.
  m["usd"] = 1;
  m["eur"] = 0.92;
  m["gbp"] = 0.79;
  m["jpy"] = 149.0;
  m["cny"] = 7.24;
  m["cad"] = 1.36;
  m["aud"] = 1.53;
  m["chf"] = 0.88;
  m["inr"] = 83.5;
  m["krw"] = 1335.0;
  m["mxn"] = 17.1;
  m["brl"] = 5.05;
  m["zar"] = 18.4;
  m["nzd"] = 1.66;
  m["sek"] = 10.5;
  m["nok"] = 10.7;
  m["dkk"] = 6.86;
  m["pln"] = 3.98;
  m["hkd"] = 7.82;
  m["sgd"] = 1.34;
  m["try"] = 32.3;
  m["thb"] = 36.1;
  m["czk"] = 23.2;
  m["huf"] = 360.0;
  m["ron"] = 4.57;
  m["bgn"] = 1.80;
  m["ils"] = 3.72;
  m["php"] = 56.2;
  m["myr"] = 4.72;
  m["idr"] = 15400.0;
  m["isk"] = 138.0;
}

#ifdef _WIN32
std::string fx_http_get(const wchar_t* host, const wchar_t* path) {
  std::string body;
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return body;
  WinHttpSetTimeouts(ses, 1200, 1200, 1200, 1800);
  HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return body;
  }
  HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return body;
  }
  BOOL ok =
      WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  if (ok) {
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req, &avail) && avail) {
      std::string chunk(avail, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(req, chunk.data(), avail, &read)) break;
      chunk.resize(read);
      body += chunk;
      if (body.size() > 16384) break;
    }
  }
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return body;
}
#else
std::string fx_http_get(const char* url) {
  std::string cmd =
      std::string("curl -fsS --max-time 2 -A Wilfred/1.0 \"") + url + "\" 2>/dev/null";
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
  std::string body;
  char buf[512];
  while (fgets(buf, sizeof(buf), f)) {
    body += buf;
    if (body.size() > 16384) break;
  }
  pclose(f);
  return body;
}
#endif

bool parse_fx_rates(const std::string& body, std::unordered_map<std::string, double>& out,
                    std::string& date) {
  date = json_get_string(body, "date");
  auto pos = body.find("\"rates\"");
  if (pos == std::string::npos) return false;
  auto brace = body.find('{', pos);
  if (brace == std::string::npos) return false;
  auto end = body.find('}', brace);
  if (end == std::string::npos) return false;
  auto blob = body.substr(brace + 1, end - brace - 1);
  std::size_t i = 0;
  while (i < blob.size()) {
    auto q1 = blob.find('"', i);
    if (q1 == std::string::npos) break;
    auto q2 = blob.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    auto code = to_lower_utf8(blob.substr(q1 + 1, q2 - q1 - 1));
    auto colon = blob.find(':', q2);
    if (colon == std::string::npos) break;
    i = colon + 1;
    while (i < blob.size() && (blob[i] == ' ' || blob[i] == '\t'))
      ++i;
    std::string num;
    if (i < blob.size() && blob[i] == '-') num.push_back(blob[i++]);
    while (i < blob.size()) {
      char c = blob[i];
      if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+') {
        num.push_back(c);
        ++i;
        continue;
      }
      break;
    }
    try {
      if (!code.empty() && !num.empty()) out[code] = std::stod(num);
    } catch (...) {
    }
    auto comma = blob.find(',', i);
    if (comma == std::string::npos) break;
    i = comma + 1;
  }
  return !out.empty();
}

void ensure_fx() {
  std::lock_guard<std::mutex> lock(fx_mu);
  if (fx_usd.empty()) {
    seed_fx_fallback(fx_usd);
    fx_date = "approx";
  }
  if (!g_fx_net) return;
  auto now = unix_seconds();
  if (fx_cache_at && now - fx_cache_at < 3600) return;
#ifdef _WIN32
  auto body = fx_http_get(L"api.frankfurter.app", L"/latest?from=USD");
#else
  auto body = fx_http_get("https://api.frankfurter.app/latest?from=USD");
#endif
  std::unordered_map<std::string, double> parsed;
  std::string date;
  if (!parse_fx_rates(body, parsed, date)) return;
  parsed["usd"] = 1;
  fx_usd = std::move(parsed);
  fx_date = date.empty() ? "live" : date;
  fx_cache_at = now;
}

std::string iso_ccy(std::string_view raw) {
  auto n = norm_unit(std::string(raw));
  auto& al = ccy_alias();
  auto it = al.find(n);
  if (it != al.end()) return it->second;
  if (n.size() == 3) {
    bool letters = true;
    for (char c : n)
      if (!std::isalpha(static_cast<unsigned char>(c))) letters = false;
    if (letters) return n;
  }
  return {};
}

std::string format_ccy(double v, const std::string& iso, const std::string& raw) {
  bool zero_dec = iso == "jpy" || iso == "krw" || iso == "isk" || iso == "idr";
  char buf[80];
  if (zero_dec)
    std::snprintf(buf, sizeof(buf), "%.0f", v);
  else if (std::fabs(v) >= 100)
    std::snprintf(buf, sizeof(buf), "%.2f", v);
  else if (std::fabs(v) >= 1)
    std::snprintf(buf, sizeof(buf), "%.4f", v);
  else
    std::snprintf(buf, sizeof(buf), "%.6f", v);
  std::string label = raw;
  if (label.size() == 3) {
    for (char& c : label)
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  return std::string(buf) + " " + label;
}

// Cooking volume<->mass bridge. Densities in g/ml; bare conversions assume
// water (1 g/ml). Ingredient may trail the unit: "1 tbsp sugar to g".
double cooking_density_g_per_ml(const std::string& ingredient) {
  if (ingredient.empty()) return 1.0;
  static const auto m = [] {
    std::unordered_map<std::string, double> d;
    auto add = [&](double rho, std::initializer_list<const char*> names) {
      for (auto n : names)
        d.emplace(n, rho);
    };
    add(1.0, {"water"});
    add(1.03, {"milk", "whole milk"});
    add(0.53, {"flour", "all purpose flour", "plain flour", "ap flour"});
    add(0.55, {"bread flour"});
    add(0.85, {"sugar", "white sugar", "granulated sugar"});
    add(0.9, {"brown sugar", "packed brown sugar"});
    add(0.51, {"powdered sugar", "icing sugar", "confectioners sugar"});
    add(0.96, {"butter"});
    add(0.92, {"oil", "vegetable oil", "canola oil", "cooking oil"});
    add(0.91, {"olive oil"});
    add(1.42, {"honey"});
    add(1.22, {"salt", "table salt"});
    add(1.0, {"sea salt", "kosher salt"});
    add(0.8, {"rice", "white rice"});
    add(0.38, {"oats", "rolled oats", "oatmeal"});
    add(0.51, {"cocoa", "cocoa powder"});
    add(0.51, {"cornstarch", "corn starch", "cornflour"});
    return d;
  }();
  auto it = m.find(ingredient);
  return it == m.end() ? 0.0 : it->second;
}

struct SplitUnit {
  bool found{false};
  UnitDef def{Qty::Length, 0, 0};
  std::string unit_text;
  std::string ingredient;
};

// Split "tbsp sugar" / "fluid ounce of flour" into unit + ingredient by
// matching the longest leading token run against the unit table.
SplitUnit split_unit_ingredient(const std::string& raw,
                                const std::unordered_map<std::string, UnitDef>& tab) {
  SplitUnit out;
  std::vector<std::string> toks;
  std::string cur;
  for (char c : raw) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        toks.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) toks.push_back(cur);
  if (toks.empty()) return out;
  for (std::size_t k = toks.size(); k >= 1; --k) {
    std::string joined;
    for (std::size_t i = 0; i < k; ++i) {
      if (i) joined.push_back(' ');
      joined += toks[i];
    }
    auto n = norm_unit(joined);
    auto it = tab.find(n);
    if (it != tab.end()) {
      out.found = true;
      out.def = it->second;
      out.unit_text = joined;
      std::string ing;
      for (std::size_t i = k; i < toks.size(); ++i) {
        if (!ing.empty()) ing.push_back(' ');
        ing += toks[i];
      }
      if (ing.rfind("of ", 0) == 0) ing = ing.substr(3);
      out.ingredient = ing;
      return out;
    }
    if (k == 1) break;
  }
  return out;
}

bool apply_currency(double amount, std::string_view from_raw, std::string_view to_raw,
                    MathResult& out) {
  auto from = iso_ccy(from_raw);
  auto to = iso_ccy(to_raw);
  if (from.empty() || to.empty() || from == to) return false;
  ensure_fx();
  std::lock_guard<std::mutex> lock(fx_mu);
  auto fa = fx_usd.find(from);
  auto ta = fx_usd.find(to);
  if (fa == fx_usd.end() || ta == fx_usd.end()) return false;
  if (fa->second == 0) return false;
  double dest = amount * (ta->second / fa->second);
  if (!std::isfinite(dest)) return false;
  out.ok = true;
  out.conversion = true;
  out.currency = true;
  out.value = dest;
  out.display = format_ccy(dest, to, std::string(to_raw));
  out.error.clear();
  return true;
}

}  // namespace

void set_currency_network_enabled(bool enabled) {
  g_fx_net = enabled;
}

bool convert_currency(double amount, std::string_view from, std::string_view to, MathResult& out) {
  return apply_currency(amount, from, to, out);
}

bool convert_metric(std::string_view expr, MathResult& out) {
  double v = 0;
  std::string from_raw, to_raw;
  if (!parse_conversion(std::string(expr), v, from_raw, to_raw)) return false;
  auto from_n = norm_unit(from_raw);
  auto to_n = norm_unit(to_raw);
  auto& tab = units();
  auto fa = tab.find(from_n);
  auto ta = tab.find(to_n);
  if (fa != tab.end() && ta != tab.end()) {
    if (fa->second.qty == ta->second.qty) {
      double si = v * fa->second.mul + fa->second.add;
      double dest = (si - ta->second.add) / ta->second.mul;
      if (!std::isfinite(dest)) return false;
      out.ok = true;
      out.conversion = true;
      out.currency = false;
      out.value = dest;
      out.display = format_value(dest) + " " + to_raw;
      out.error.clear();
      return true;
    }
    // Volume<->mass falls through to the cooking bridge below; other
    // mismatched quantities (e.g. length to mass) are invalid.
    bool vol_mass = (fa->second.qty == Qty::Volume && ta->second.qty == Qty::Mass) ||
                    (fa->second.qty == Qty::Mass && ta->second.qty == Qty::Volume);
    if (!vol_mass) return false;
  }
  // Cooking bridge: allow volume<->mass via density (water by default) and
  // tolerate a trailing ingredient ("1 tbsp sugar to g"). Same-quantity
  // pairs with an ingredient ("1 cup sugar to ml") convert directly.
  {
    auto fs = split_unit_ingredient(from_raw, tab);
    auto ts = split_unit_ingredient(to_raw, tab);
    if (fs.found && ts.found) {
      bool fvol = fs.def.qty == Qty::Volume;
      bool fmass = fs.def.qty == Qty::Mass;
      bool tvol = ts.def.qty == Qty::Volume;
      bool tmass = ts.def.qty == Qty::Mass;
      if (fs.def.qty == ts.def.qty && (fvol || fmass)) {
        double si = v * fs.def.mul + fs.def.add;
        double dest = (si - ts.def.add) / ts.def.mul;
        if (!std::isfinite(dest)) return false;
        out.ok = true;
        out.conversion = true;
        out.currency = false;
        out.value = dest;
        out.display = format_value(dest) + " " + ts.unit_text;
        out.error.clear();
        return true;
      }
      if ((fvol && tmass) || (fmass && tvol)) {
        std::string ing = !fs.ingredient.empty() ? fs.ingredient : ts.ingredient;
        double rho = cooking_density_g_per_ml(ing);
        if (rho > 0) {
          double dest = 0;
          if (fvol && tmass) {
            double ml = v * fs.def.mul * 1e6;
            double grams = ml * rho;
            double kg = grams / 1000.0;
            dest = (kg - ts.def.add) / ts.def.mul;
          } else {
            double kg = v * fs.def.mul + fs.def.add;
            double grams = kg * 1000.0;
            double ml = grams / rho;
            double m3 = ml / 1e6;
            dest = (m3 - ts.def.add) / ts.def.mul;
          }
          if (!std::isfinite(dest)) return false;
          out.ok = true;
          out.conversion = true;
          out.currency = false;
          out.value = dest;
          out.display = format_value(dest) + " " + ts.unit_text;
          out.error.clear();
          return true;
        }
      }
    }
  }
  return apply_currency(v, from_raw, to_raw, out);
}

}  // namespace wilfred
