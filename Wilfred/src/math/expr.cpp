#include "wilfred/math/expr.hpp"

#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace wilfred {
namespace {

struct Lexer {
  std::string s;
  std::size_t i{0};
  char peek() const { return i < s.size() ? s[i] : 0; }
  char get() { return i < s.size() ? s[i++] : 0; }
  void skip() {
    while (peek() == ' ' || peek() == '\t') get();
  }
};

enum class Tk { End, Num, Id, Plus, Minus, Star, Slash, Caret, Percent, LParen, RParen, Comma };

struct Tok {
  Tk k{Tk::End};
  double num{0};
  std::string id;
};

struct Parser {
  Lexer lx;
  Tok cur;
  std::string err;
  bool failed{false};

  explicit Parser(std::string e) {
    std::string n;
    n.reserve(e.size());
    int depth = 0;
    for (std::size_t i = 0; i < e.size(); ++i) {
      if (e[i] == '(') ++depth;
      else if (e[i] == ')') {
        if (depth > 0) --depth;
      }
      if (e[i] == ',' && depth == 0 && i > 0 && i + 1 < e.size() &&
          std::isdigit(static_cast<unsigned char>(e[i - 1])) &&
          std::isdigit(static_cast<unsigned char>(e[i + 1])))
        n.push_back('.');
      else
        n.push_back(e[i]);
    }
    lx.s = std::move(n);
    next();
  }

  void fail(const std::string& m) {
    if (!failed) {
      failed = true;
      err = m;
    }
  }

  void next() {
    lx.skip();
    char c = lx.peek();
    if (!c) {
      cur = {Tk::End};
      return;
    }
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
      std::string n;
      bool exp = false;
      while (std::isdigit(static_cast<unsigned char>(lx.peek())) || lx.peek() == '.' ||
             lx.peek() == 'e' || lx.peek() == 'E' ||
             ((lx.peek() == '+' || lx.peek() == '-') && exp == false && !n.empty() &&
              (n.back() == 'e' || n.back() == 'E'))) {
        if (lx.peek() == 'e' || lx.peek() == 'E') exp = true;
        n.push_back(lx.get());
      }
      Tok t;
      t.k = Tk::Num;
      try {
        t.num = std::stod(n);
      } catch (...) {
        fail("invalid number");
        t.num = 0;
      }
      cur = t;
      return;
    }
    if (std::isalpha(static_cast<unsigned char>(c))) {
      std::string id;
      while (std::isalnum(static_cast<unsigned char>(lx.peek())) || lx.peek() == '_')
        id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(lx.get()))));
      cur = {Tk::Id, 0, id};
      return;
    }
    lx.get();
    switch (c) {
      case '+':
        cur = {Tk::Plus};
        break;
      case '-':
        cur = {Tk::Minus};
        break;
      case '*':
      case 'x':
      case 'X':
        cur = {Tk::Star};
        break;
      case '/':
        cur = {Tk::Slash};
        break;
      case '^':
        cur = {Tk::Caret};
        break;
      case '%':
        cur = {Tk::Percent};
        break;
      case '(':
        cur = {Tk::LParen};
        break;
      case ')':
        cur = {Tk::RParen};
        break;
      case ',':
        cur = {Tk::Comma};
        break;
      default:
        fail(std::string("unexpected character"));
        cur = {Tk::End};
        break;
    }
  }

  bool eat(Tk k) {
    if (cur.k == k) {
      next();
      return true;
    }
    return false;
  }

  double parse() {
    double v = expr();
    if (cur.k != Tk::End && !failed) fail("unexpected trailing input");
    return v;
  }

  double expr() { return term_rest(term()); }

  double term_rest(double l) {
    if (eat(Tk::Plus)) return term_rest(l + term());
    if (eat(Tk::Minus)) return term_rest(l - term());
    return l;
  }

  double term() { return factor_rest(factor()); }

  double factor_rest(double l) {
    if (eat(Tk::Star)) return factor_rest(l * factor());
    if (eat(Tk::Slash)) {
      auto r = factor();
      if (r == 0) {
        fail("division by zero");
        return 0;
      }
      return factor_rest(l / r);
    }
    if (eat(Tk::Percent)) {
      auto r = factor();
      if (r == 0) {
        fail("modulo by zero");
        return 0;
      }
      return factor_rest(std::fmod(l, r));
    }
    return l;
  }

  double factor() { return power(); }

  double power() {
    double b = unary();
    if (eat(Tk::Caret)) return std::pow(b, unary());
    return b;
  }

  double unary() {
    if (eat(Tk::Plus)) return unary();
    if (eat(Tk::Minus)) return -unary();
    return primary();
  }

  double primary() {
    if (cur.k == Tk::Num) {
      double n = cur.num;
      next();
      return n;
    }
    if (cur.k == Tk::Id) {
      auto id = cur.id;
      next();
      static const std::unordered_map<std::string, double> k = {
          {"pi", 3.14159265358979323846}, {"e", 2.71828182845904523536}, {"tau", 6.283185307179586}};
      auto it = k.find(id);
      if (it != k.end()) return it->second;
      if (!eat(Tk::LParen)) {
        fail("unknown identifier: " + id);
        return 0;
      }
      double a = expr();
      double b = 0;
      bool two = false;
      if (eat(Tk::Comma)) {
        b = expr();
        two = true;
      }
      if (!eat(Tk::RParen)) fail("expected ')'");
      if (id == "sqrt") return std::sqrt(a);
      if (id == "cbrt") return std::cbrt(a);
      if (id == "abs") return std::fabs(a);
      if (id == "sin") return std::sin(a);
      if (id == "cos") return std::cos(a);
      if (id == "tan") return std::tan(a);
      if (id == "asin") return std::asin(a);
      if (id == "acos") return std::acos(a);
      if (id == "atan") return std::atan(a);
      if (id == "atan2") return std::atan2(a, two ? b : 0);
      if (id == "ln" || id == "log") return std::log(a);
      if (id == "log10") return std::log10(a);
      if (id == "log2") return std::log2(a);
      if (id == "exp") return std::exp(a);
      if (id == "floor") return std::floor(a);
      if (id == "ceil") return std::ceil(a);
      if (id == "round") return std::round(a);
      if (id == "min") return two ? std::min(a, b) : a;
      if (id == "max") return two ? std::max(a, b) : a;
      if (id == "pow") return std::pow(a, two ? b : 2);
  if (id == "frac" || id == "div") {
    if (!two || b == 0) {
      fail("invalid fraction");
      return 0;
    }
    return a / b;
  }
      if (id == "deg") return a * 3.14159265358979323846 / 180.0;
      if (id == "rad") return a * 180.0 / 3.14159265358979323846;
      fail("unknown function: " + id);
      return 0;
    }
    if (eat(Tk::LParen)) {
      double v = expr();
      if (!eat(Tk::RParen)) fail("expected ')'");
      return v;
    }
    fail("expected number or '('");
    return 0;
  }
};

}  // namespace

MathResult evaluate_math(std::string_view expr) {
  MathResult r;
  std::string s(expr);
  if (convert_metric(s, r)) return r;
  Parser p(s);
  double v = p.parse();
  if (p.failed || !std::isfinite(v)) {
    r.ok = false;
    r.error = p.err.empty() ? "invalid expression" : p.err;
    return r;
  }
  r.ok = true;
  r.value = v;
  std::ostringstream os;
  os.precision(12);
  os << v;
  r.display = os.str();
  return r;
}

}  // namespace wilfred
