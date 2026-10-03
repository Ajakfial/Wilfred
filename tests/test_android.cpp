#include "test.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/platform/android_bridge.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

void test_android() {
  using namespace wilfred;

  // Action mapping covers every ResultAction used by the Kotlin UI.
  CHECK_EQ(AndroidCore::action_to_string(0), "open");
  CHECK_EQ(AndroidCore::action_to_string(3), "web");
  CHECK_EQ(AndroidCore::action_to_string(12), "system");
  CHECK_EQ(AndroidCore::action_to_string(999), "open");

  // Result JSON encoding: structure the Kotlin parser depends on.
  {
    SearchResult r;
    r.title = "Fire\"fox";
    r.subtitle = "sub";
    r.path = "package:org.mozilla.firefox";
    r.payload = "https://example.com/?q=a\"b";
    r.score = 42;
    r.action = ResultAction::WebSearch;
    r.category = "app";
    r.kind_label = "application";
    ResultActionItem act;
    act.id = "open";
    act.label = "Open";
    r.actions.push_back(act);
    std::vector<SearchResult> v{r};
    std::string js = AndroidCore::results_to_json(v, 10);
    CHECK(js.front() == '[');
    CHECK(js.back() == ']');
    // Quotes must be escaped for org.json on the Kotlin side.
    CHECK(js.find("\\\"") != std::string::npos);
    CHECK(js.find("package:org.mozilla.firefox") != std::string::npos);
    // Per-result actions ride along for the Kotlin long-press menu.
    CHECK(js.find("\"actions\"") != std::string::npos);
    CHECK(js.find("\"open\"") != std::string::npos);
    CHECK(AndroidCore::results_to_json(v, 0) == "[]");
    CHECK(AndroidCore::results_to_json({}, 10) == "[]");
  }

  // Kotlin bridge JSON parsing round-trips through the same helpers.
  {
    std::string obj = "{\"name\":\"Firefox\"}";
    CHECK_EQ(json_get_string(obj, "name"), "Firefox");
  }

  // Full boot + search + registerApp against a temp files dir.
  // Exercises the real C++ core (config, index, interpreter) without JNI.
  {
    auto tmp = fs::temp_directory_path() / "wilfred_android_bridge_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    std::string dir = tmp.string();

    AndroidCore core;
    std::string error;
    CHECK(core.boot(dir, error));
    CHECK(error.empty());
    CHECK(core.search_json("", 5).front() == '[');
    CHECK(core.search_json("firefox", 10).front() == '[');
    CHECK(core.status_json().find("\"ok\":true") != std::string::npos);
    CHECK(core.register_app("Firefox", "org.mozilla.firefox", "Firefox"));
    CHECK(!core.register_app("", "org.mozilla.firefox", ""));
    CHECK(!core.register_app("Firefox", "", ""));
    CHECK(core.record_choice("firefox", "package:org.mozilla.firefox"));
    CHECK(!core.record_choice("firefox", ""));

    // App names are searchable via content tokens after registration.
    std::string hits = core.search_json("firefox", 10);
    CHECK(hits.find("package:org.mozilla.firefox") != std::string::npos);

    // Assist payload for ghost completion / did-you-mean / candidates.
    {
      std::string assist = core.assist_json("wea");
      CHECK(!assist.empty());
      CHECK(assist.find("candidates") != std::string::npos);
      CHECK(core.assist_json("").find("candidates") != std::string::npos);
    }

    // Clipboard sync (Kotlin pushes Android clipboard into C++).
    {
      core.set_clipboard("hello android");
      CHECK_EQ(core.clipboard_text(), "hello android");
    }

    // Actions + execute against the last search snapshot.
    {
      core.search_json("2+2", 10);
      CHECK(core.actions_json(0).front() == '[');
      CHECK_EQ(core.actions_json(9999), "[]");
      std::string exec_err;
      // Calculator copy action must succeed headless on all platforms.
      CHECK(core.execute_action(0, "copy_text", exec_err));
      CHECK(!core.execute_action(9999, "copy_text", exec_err));
    }

    // File preview JSON for the Kotlin peek UI.
    {
      auto probe = tmp / "preview.txt";
      {
        std::ofstream f(probe);
        f << "hello preview";
      }
      std::string pv = core.preview_json(probe.string());
      CHECK(pv.find("\"exists\":true") != std::string::npos);
      CHECK(pv.find("hello preview") != std::string::npos);
      CHECK(core.preview_json("package:org.mozilla.firefox").find("\"exists\":false") !=
            std::string::npos);
      CHECK(core.preview_json("").find("\"exists\":false") != std::string::npos);
    }

    // Boot rejects empty dirs.
    AndroidCore bad;
    CHECK(!bad.boot("", error));
    CHECK(!error.empty());
    CHECK_EQ(bad.search_json("x", 5), "[]");
    CHECK(bad.status_json().find("\"ok\":false") != std::string::npos);
    CHECK(bad.assist_json("x").find("candidates") != std::string::npos);
    CHECK_EQ(bad.actions_json(0), "[]");

    fs::remove_all(tmp, ec);
  }

  // Platform reports android build only under the NDK; on desktop this
  // documents the contract without asserting the NDK macro.
  (void)platform_name();
}
