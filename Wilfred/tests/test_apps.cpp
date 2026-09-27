#include "test.hpp"
#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/platform/platform.hpp"

void test_apps() {
  using namespace wilfred;
  auto apps = discover_applications();
  (void)apps;
  auto browsers = list_browsers();
  auto id = default_browser_id();
  auto exe = default_browser_executable();
  (void)id;
  (void)exe;
  auto url = web_search_url("https://example.com/q={query}", "hello world");
  CHECK(url.find("hello") != std::string::npos);
  CHECK(url.find("{query}") == std::string::npos);

  CHECK(!platform_name().empty());
  CHECK(is_windows() || is_macos() || is_linux());
  CHECK(!(is_windows() && is_linux()));
}
