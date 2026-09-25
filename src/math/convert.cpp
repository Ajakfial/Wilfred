#include "wilfred/math/expr.hpp"

#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cmath>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unordered_map>

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
  while (!u.empty() && u.front() == ' ') u.erase(u.begin());
  while (!u.empty() && u.back() == ' ') u.pop_back();
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
  for (auto n : names) m.emplace(n, d);
}

const std::unordered_map<std::string, UnitDef>& units() {
  static const auto m = [] {
    std::unordered_map<std::string, UnitDef> u;
    // Length — SI metre
    add_unit(u, Qty::Length, 1e-9, 0, {"nm", "nanometer", "nanometers", "nanometre", "nanometres"});
    add_unit(u, Qty::Length, 1e-6, 0, {"um", "µm", "micrometer", "micrometers", "micrometre", "micrometres"});
    add_unit(u, Qty::Length, 1e-3, 0, {"mm", "millimeter", "millimeters", "millimetre", "millimetres"});
    add_unit(u, Qty::Length, 0.01, 0, {"cm", "centimeter", "centimeters", "centimetre", "centimetres"});
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
    add_unit(u, Qty::Mass, 1e-9, 0, {"ug", "µg", "microgram", "micrograms"});
    add_unit(u, Qty::Mass, 1e-6, 0, {"mg", "milligram", "milligrams"});
    add_unit(u, Qty::Mass, 0.001, 0, {"g", "gram", "grams", "gramme", "grammes"});
    add_unit(u, Qty::Mass, 1, 0, {"kg", "kilogram", "kilograms", "kilo", "kilos"});
    add_unit(u, Qty::Mass, 1000, 0, {"t", "tonne", "tonnes", "metricton", "metrictons"});
    add_unit(u, Qty::Mass, 0.45359237, 0, {"lb", "lbs", "pound", "pounds"});
    add_unit(u, Qty::Mass, 0.028349523125, 0, {"oz", "ounce", "ounces"});
    add_unit(u, Qty::Mass, 6.35029318, 0, {"st", "stone", "stones"});

    // Volume — SI cubic metre
    add_unit(u, Qty::Volume, 1e-9, 0, {"mm3", "cubicmillimeter", "cubicmillimeters"});
    add_unit(u, Qty::Volume, 1e-6, 0, {"ml", "milliliter", "milliliters", "millilitre", "millilitres", "cc",
                                       "cm3", "cubiccentimeter", "cubiccentimeters"});
    add_unit(u, Qty::Volume, 0.001, 0, {"l", "liter", "liters", "litre", "litres"});
    add_unit(u, Qty::Volume, 1, 0, {"m3", "cubicmeter", "cubicmeters", "cubicmetre", "cubicmetres"});
    add_unit(u, Qty::Volume, 1e-3, 0, {"dm3", "cubicdecimeter"});
    add_unit(u, Qty::Volume, 0.00454609, 0, {"impgal", "imperialgallon", "imperialgallons", "ukgal"});
    add_unit(u, Qty::Volume, 0.003785411784, 0, {"gal", "gallon", "gallons", "usgal"});
    add_unit(u, Qty::Volume, 0.000946352946, 0, {"qt", "quart", "quarts"});
    add_unit(u, Qty::Volume, 0.000473176473, 0, {"pt", "pint", "pints"});
    add_unit(u, Qty::Volume, 0.0002365882365, 0, {"cup", "cups"});
    add_unit(u, Qty::Volume, 2.95735295625e-5, 0, {"floz", "fluidounce", "fluidounces"});
    add_unit(u, Qty::Volume, 1.478676478125e-5, 0, {"tbsp", "tablespoon", "tablespoons"});
    add_unit(u, Qty::Volume, 4.92892159375e-6, 0, {"tsp", "teaspoon", "teaspoons"});

    // Area — SI square metre
    add_unit(u, Qty::Area, 1e-6, 0, {"mm2", "sqmm", "squaremillimeter", "squaremillimeters"});
    add_unit(u, Qty::Area, 1e-4, 0, {"cm2", "sqcm", "squarecentimeter", "squarecentimeters"});
    add_unit(u, Qty::Area, 1, 0, {"m2", "sqm", "sqmeter", "squaremeter", "squaremeters", "squaremetre",
                                 "squaremetres"});
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
    add_unit(u, Qty::Speed, 1000.0 / 3600.0, 0, {"kmh", "km/h", "kph", "kilometerperhour", "kilometersperhour"});
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

bool parse_conversion(std::string s, double& value, std::string& from, std::string& to) {
  auto lower = to_lower_utf8(s);
  while (!lower.empty() && (lower.front() == ' ' || lower.front() == '\t')) lower.erase(lower.begin());
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
  while (!left.empty() && left.back() == ' ') left.pop_back();
  while (!right.empty() && (right.front() == ' ' || right.front() == '\t')) right.erase(right.begin());
  while (!right.empty() && right.back() == ' ') right.pop_back();
  std::string num;
  std::size_t i = 0;
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
  while (i < left.size() && (left[i] == ' ' || left[i] == '\t')) ++i;
  from = left.substr(i);
  to = right;
  if (num.empty() || from.empty() || to.empty()) return false;
  try {
    value = std::stod(num);
  } catch (...) {
    return false;
  }
  return true;
}

}  // namespace

bool convert_metric(std::string_view expr, MathResult& out) {
  double v = 0;
  std::string from_raw, to_raw;
  if (!parse_conversion(std::string(expr), v, from_raw, to_raw)) return false;
  auto from_n = norm_unit(from_raw);
  auto to_n = norm_unit(to_raw);
  auto& tab = units();
  auto fa = tab.find(from_n);
  auto ta = tab.find(to_n);
  if (fa == tab.end() || ta == tab.end()) return false;
  if (fa->second.qty != ta->second.qty) return false;
  double si = v * fa->second.mul + fa->second.add;
  double dest = (si - ta->second.add) / ta->second.mul;
  if (!std::isfinite(dest)) return false;
  out.ok = true;
  out.conversion = true;
  out.value = dest;
  out.display = format_value(dest) + " " + to_raw;
  out.error.clear();
  return true;
}

}  // namespace wilfred
