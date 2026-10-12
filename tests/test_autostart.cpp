#include "test.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/service/autostart.hpp"

#include <string>
#include <vector>

void test_autostart() {
  using namespace wilfred;

  // --- Content builders are deterministic (no machine state) ---
  {
    const std::string exe = "/opt/wilfred/wilfred";
    const std::string de = autostart_desktop_entry(exe);
    CHECK(de.compare(0, 16, "[Desktop Entry]") == 0);
    CHECK(de.find("Type=Application\n") != std::string::npos);
    CHECK(de.find("Exec=\"/opt/wilfred/wilfred\"\n") != std::string::npos);
    CHECK(de.find("X-GNOME-Autostart-enabled=true\n") != std::string::npos);
    const std::string pl = autostart_launchd_plist(exe);
    CHECK(pl.find("<string>com.wilfred.launcher</string>") != std::string::npos);
    CHECK(pl.find("<string>/opt/wilfred/wilfred</string>") != std::string::npos);
    CHECK(pl.find("<true/>") != std::string::npos);
    CHECK(pl.find("RunAtLoad") != std::string::npos);
    // XML-escaping keeps the plist well-formed.
    CHECK(autostart_launchd_plist("a&b<c>").find("a&amp;b&lt;c&gt;") != std::string::npos);
  }

  // --- exe_path resolves on desktop CI (read-only) ---
  if (is_windows() || is_macos() || is_linux() || is_bsd()) {
    CHECK(!exe_path().empty());
  }

  // --- status() is read-only and safe everywhere ---
  {
    bool enabled = false;
    std::string detail, error;
    if (is_android() || is_ios()) {
      CHECK(!native_autostart_status(enabled, detail, error));
      CHECK(!error.empty());
    } else {
      CHECK(native_autostart_status(enabled, detail, error));
    }
  }

  // --- CLI arg handling (no OS mutation: unknown sub only) ---
  {
    CHECK_EQ(run_autostart({"bogus-subcommand"}), 2);
  }
}
