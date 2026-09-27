#include "test.hpp"
#include "wilfred/config/yaml.hpp"

void test_yaml() {
  wilfred::YamlValue v;
  wilfred::YamlError err;
  CHECK(parse_yaml("a: 1\nb: hello\n", v, err));
  CHECK(v.is_map());
  CHECK_EQ(v.str("a"), "1");
  CHECK_EQ(v.str("b"), "hello");
  CHECK_EQ(v.integer("a", 0), 1);

  err = {};
  CHECK(parse_yaml("list:\n  - one\n  - two\n", v, err));
  auto* l = v.get("list");
  CHECK(l && l->is_list());
  CHECK_EQ(l->as_list().size(), 2u);

  CHECK(parse_yaml("flag: true\n", v, err));
  CHECK(v.boolean("flag", false));

  CHECK(parse_yaml("q: \"quoted value\"\n", v, err));
  CHECK_EQ(v.str("q"), "quoted value");

  CHECK(parse_yaml("nested:\n  x: 3\n  y: 4\n", v, err));
  auto* n = v.get("nested");
  CHECK(n && n->is_map());
  CHECK_EQ(n->integer("x", 0), 3);

  CHECK(parse_yaml("# comment only\nkey: value # trailing\n", v, err));
  CHECK_EQ(v.str("key"), "value");
}
