#include "test.hpp"

#include "wilfred/updater/updater.hpp"

void test_updater() {
  using namespace wilfred;

  auto v = parse_version("v1.2.3");
  CHECK(v.valid());
  CHECK_EQ(v.major, 1);
  CHECK_EQ(v.minor, 2);
  CHECK_EQ(v.patch, 3);
  CHECK_EQ(v.tweak, 0);

  // Multi-digit components of any width.
  auto w2 = parse_version("v10.2.3");
  CHECK(w2.valid());
  CHECK_EQ(w2.major, 10);
  auto w3 = parse_version("v123.45.67");
  CHECK(w3.valid());
  CHECK_EQ(w3.major, 123);
  CHECK_EQ(w3.minor, 45);
  CHECK_EQ(w3.patch, 67);

  // No prefix, uppercase prefix, short forms.
  auto bare = parse_version("1.2.3");
  CHECK(bare.valid());
  CHECK_EQ(bare.compare(v), 0);
  auto upper = parse_version("V1.2.3");
  CHECK(upper.valid());
  CHECK_EQ(upper.compare(v), 0);
  auto two = parse_version("v1.2");
  CHECK(two.valid());
  CHECK_EQ(two.major, 1);
  CHECK_EQ(two.minor, 2);
  CHECK_EQ(two.patch, 0);
  auto one = parse_version("v5");
  CHECK(one.valid());
  CHECK_EQ(one.major, 5);

  // Fourth component.
  auto four = parse_version("v1.2.3.4");
  CHECK(four.valid());
  CHECK_EQ(four.tweak, 4);
  CHECK(four.compare(v) > 0);
  CHECK_EQ(four.to_string(), "1.2.3.4");
  CHECK_EQ(v.to_string(), "1.2.3");

  // Whitespace and suffixes are tolerated.
  auto spaced = parse_version("  v1.2.3  ");
  CHECK(spaced.valid());
  CHECK_EQ(spaced.compare(v), 0);
  auto beta = parse_version("v1.2.3-beta");
  CHECK(beta.valid());
  CHECK_EQ(beta.compare(v), 0);
  auto build = parse_version("v1.2.3+build.1");
  CHECK(build.valid());
  CHECK_EQ(build.compare(v), 0);
  auto both = parse_version("v10.0.1-rc.1+build");
  CHECK(both.valid());
  CHECK_EQ(both.major, 10);
  CHECK_EQ(both.patch, 1);

  // Ordering.
  CHECK(parse_version("v1.2.4").compare(v) > 0);
  CHECK(parse_version("v1.2.3").compare(parse_version("v1.2.4")) < 0);
  CHECK(parse_version("v2.0.0").compare(parse_version("v10.0.0")) < 0);
  CHECK(parse_version("v1.10.0").compare(parse_version("v1.9.9")) > 0);

  // Invalid inputs.
  CHECK(!parse_version("").valid());
  CHECK(!parse_version("v").valid());
  CHECK(!parse_version("abc").valid());
  CHECK(!parse_version("v1.2.x").valid());
  CHECK(!parse_version("v1..3").valid());
  CHECK(!parse_version("v1.2.3.4.5").valid());
  auto trailing_dash = parse_version("v1.2.3-");
  CHECK(trailing_dash.valid());
  CHECK_EQ(trailing_dash.compare(v), 0);
  CHECK(!Version().valid());
}
