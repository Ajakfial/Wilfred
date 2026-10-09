#include "test.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/platform/android_bridge.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

void test_ios() {
  using namespace wilfred;

#if defined(WILFRED_IOS)
  // On-device contract: iOS reports its own platform, never macOS/Linux.
  CHECK_EQ(platform_name(), "ios");
  CHECK(is_ios());
  CHECK(!is_macos());
  CHECK(!is_linux());
  CHECK(!is_android());
#else
  // On desktop hosts the iOS macro is off and platform detection is
  // unchanged (macOS stays macOS, Linux stays Linux, BSD stays BSD).
  CHECK(!is_ios());
  CHECK(platform_name() == "windows" || platform_name() == "macos" || platform_name() == "linux" ||
        platform_name() == "freebsd" || platform_name() == "openbsd" ||
        platform_name() == "netbsd" || platform_name() == "dragonfly");
#endif

  // The iOS Swift UI talks to the shared mobile core (MobileCore, aliased
  // as AndroidCore for the JNI layer) over the ObjC++ bridge. Exercise the
  // same surface the bridge uses, without any UI framework.
  {
    SearchResult r;
    r.title = "Notes";
    r.subtitle = "sub";
    r.path = "/Documents/notes.txt";
    r.payload = "https://example.com/?q=a\"b";
    r.score = 7;
    r.action = ResultAction::WebSearch;
    r.category = "file";
    r.kind_label = "document";
    ResultActionItem act;
    act.id = "open";
    act.label = "Open";
    r.actions.push_back(act);
    std::vector<SearchResult> v{r};
    std::string js = MobileCore::results_to_json(v, 10);
    CHECK(js.front() == '[');
    CHECK(js.back() == ']');
    // Quotes must be escaped for JSONSerialization on the Swift side.
    CHECK(js.find("\\\"") != std::string::npos);
    CHECK(js.find("\"actions\"") != std::string::npos);
    CHECK(MobileCore::results_to_json(v, 0) == "[]");
    CHECK(MobileCore::results_to_json({}, 10) == "[]");
    CHECK_EQ(MobileCore::action_to_string(3), "web");
  }

  // Full boot + search + assist + clipboard + execute + preview against a
  // temp dir: the exact calls WilfredCoreBridge.mm makes.
  {
    auto tmp = fs::temp_directory_path() / "wilfred_ios_bridge_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    std::string dir = tmp.string();

    MobileCore core;
    std::string error;
    CHECK(core.boot(dir, error));
    CHECK(error.empty());
    CHECK(core.search_json("", 5).front() == '[');
    CHECK(core.search_json("notes", 10).front() == '[');
    CHECK(core.status_json().find("\"ok\":true") != std::string::npos);

    // iOS has no app enumeration; register_app stays available but the
    // Swift layer never calls it.
    CHECK(core.register_app("Notes", "com.apple.Notes", "Notes"));

    std::string assist = core.assist_json("wea");
    CHECK(assist.find("candidates") != std::string::npos);

    core.set_clipboard("hello ios");
    CHECK_EQ(core.clipboard_text(), "hello ios");

    core.search_json("2+2", 10);
    CHECK(core.actions_json(0).front() == '[');
    CHECK_EQ(core.actions_json(9999), "[]");
    std::string exec_err;
    CHECK(core.execute_action(0, "copy_text", exec_err));
    CHECK(!core.execute_action(9999, "copy_text", exec_err));

    auto probe = tmp / "preview.txt";
    {
      std::ofstream f(probe);
      f << "hello preview";
    }
    std::string pv = core.preview_json(probe.string());
    CHECK(pv.find("\"exists\":true") != std::string::npos);
    CHECK(pv.find("hello preview") != std::string::npos);

    MobileCore bad;
    CHECK(!bad.boot("", error));
    CHECK(!error.empty());
    CHECK_EQ(bad.search_json("x", 5), "[]");

    fs::remove_all(tmp, ec);
  }
}
