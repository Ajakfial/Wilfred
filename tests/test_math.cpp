#include "test.hpp"
#include "wilfred/math/expr.hpp"

void test_math() {
  using namespace wilfred;
  auto a = evaluate_math("25 * 42");
  CHECK(a.ok);
  CHECK_NEAR(a.value, 1050.0, 1e-9);

  auto b = evaluate_math("(2 + 3) * 4 - 1");
  CHECK(b.ok);
  CHECK_NEAR(b.value, 19.0, 1e-9);

  auto c = evaluate_math("2^10");
  CHECK(c.ok);
  CHECK_NEAR(c.value, 1024.0, 1e-9);

  auto d = evaluate_math("sqrt(144) + abs(-6)");
  CHECK(d.ok);
  CHECK_NEAR(d.value, 18.0, 1e-9);

  auto e = evaluate_math("sin(0) + cos(0)");
  CHECK(e.ok);
  CHECK_NEAR(e.value, 1.0, 1e-9);

  auto f = evaluate_math("log10(1000)");
  CHECK(f.ok);
  CHECK_NEAR(f.value, 3.0, 1e-9);

  auto g = evaluate_math("1e3 + 2.5");
  CHECK(g.ok);
  CHECK_NEAR(g.value, 1002.5, 1e-9);

  auto h = evaluate_math("10 % 3");
  CHECK(h.ok);
  CHECK_NEAR(h.value, 1.0, 1e-9);

  auto i = evaluate_math("10 km to mi");
  CHECK(i.ok);
  CHECK(i.value > 6.0 && i.value < 7.0);

  auto j = evaluate_math("32 f to c");
  CHECK(j.ok);
  CHECK_NEAR(j.value, 0.0, 1e-9);

  CHECK(!evaluate_math("1 / 0").ok);
  CHECK(!evaluate_math("not math at all").ok);
  CHECK(!evaluate_math("").ok);

  auto frac = evaluate_math("1/2 + 1/4");
  CHECK(frac.ok);
  CHECK_NEAR(frac.value, 0.75, 1e-9);

  auto neg = evaluate_math("-3 * -4");
  CHECK(neg.ok);
  CHECK_NEAR(neg.value, 12.0, 1e-9);

  auto pi = evaluate_math("pi * 0");
  CHECK(pi.ok);
  CHECK_NEAR(pi.value, 0.0, 1e-9);

  auto fr = evaluate_math("frac(3,4)");
  CHECK(fr.ok);
  CHECK_NEAR(fr.value, 0.75, 1e-9);

  auto mb = evaluate_math("2048 kb to mb");
  CHECK(mb.ok);
  CHECK_NEAR(mb.value, 2.0, 1e-9);

  auto conv = evaluate_math("100 cm to m");
  CHECK(conv.ok);
  CHECK(conv.conversion);
  CHECK_NEAR(conv.value, 1.0, 1e-9);

  auto vol = evaluate_math("1 liter to milliliters");
  CHECK(vol.ok);
  CHECK_NEAR(vol.value, 1000.0, 1e-6);

  auto k = evaluate_math("0 c to k");
  CHECK(k.ok);
  CHECK_NEAR(k.value, 273.15, 1e-9);

  auto spd = evaluate_math("36 km/h to m/s");
  CHECK(spd.ok);
  CHECK_NEAR(spd.value, 10.0, 1e-9);

  CHECK(!evaluate_math("10 km to kg").ok);
}
