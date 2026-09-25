#include "test.hpp"

#include "wilfred/platform/platform.hpp"

#include <iostream>

int main() {
  wilfred::platform_init();
  test_yaml();
  test_config();
  test_tokenizer();
  test_fuzzy();
  test_rank();
  test_filter();
  test_math();
  test_classify();
  test_index();
  test_paths();
  test_history();
  test_apps();
  test_protocol();
  test_search_extras();
  std::cout << "passed " << wilfred::test::g_passes << ", failed " << wilfred::test::g_fails
            << "\n";
  return wilfred::test::g_fails == 0 ? 0 : 1;
}
