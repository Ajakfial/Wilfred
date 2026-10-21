#include "test.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/platform/platform.hpp"

#include <iostream>

int main(int argc, char** argv) {
  if (argc > 0 && argv && argv[0]) wilfred::set_program_invocation(argv[0]);
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
  test_updater();
  test_file_actions();
  test_power();
  test_search_extras();
  test_extensibility();
  test_new_features();
  test_assist();
  test_powertools();
  test_convert();
  test_import();
  test_android();
  test_ios();
  test_wayland();
  test_bsd();
  test_toggles_remote_setup();
  test_packages();
  test_os_search();
  test_async_providers();
  test_new_batch();
  test_locale();
  test_plugins();
  test_settings();
  test_autostart();
  test_agent_extras();
  std::cout << "passed " << wilfred::test::g_passes << ", failed " << wilfred::test::g_fails
            << "\n";
  return wilfred::test::g_fails == 0 ? 0 : 1;
}
