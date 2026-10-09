#include "test.hpp"

#include "wilfred/fs/volumes.hpp"
#include "wilfred/fs/watcher.hpp"
#include "wilfred/platform/platform.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace fs = std::filesystem;

void test_bsd() {
  using namespace wilfred;

#if defined(WILFRED_BSD)
  // On-device contract: BSD reports its own platform, never Linux/macOS.
#if defined(__FreeBSD__)
  CHECK_EQ(platform_name(), "freebsd");
#elif defined(__OpenBSD__)
  CHECK_EQ(platform_name(), "openbsd");
#elif defined(__NetBSD__)
  CHECK_EQ(platform_name(), "netbsd");
#elif defined(__DragonFly__)
  CHECK_EQ(platform_name(), "dragonfly");
#endif
  CHECK(is_bsd());
  CHECK(!is_linux());
  CHECK(!is_macos());
  CHECK(!is_windows());
  CHECK(!is_android());
  CHECK(!is_ios());
#else
  // On other hosts the BSD macro is off and detection is unchanged.
  CHECK(!is_bsd());
  CHECK(platform_name() == "windows" || platform_name() == "macos" || platform_name() == "linux" ||
        platform_name() == "android" || platform_name() == "ios");
#endif

  // Volume enumeration must never crash and every entry needs a path.
  // (Content varies by machine: containers may only report root.)
  {
    auto vols = list_volumes();
    for (auto& v : vols)
      CHECK(!v.path.empty());
  }

  // Watcher contract shared by every backend (inotify/kqueue/FSEvents):
  // create → modify → delete on a temp dir surfaces events naming the file.
  // Generous 10s budget keeps this stable on loaded CI runners.
  {
    auto tmp = fs::temp_directory_path() / "wilfred_watcher_test";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    const std::string file = (tmp / "note.txt").string();

    struct Hit {
      FsEventKind kind;
      std::string path;
    };
    std::mutex mu;
    std::vector<Hit> hits;
    FsWatcher watcher;
    CHECK(watcher.start({tmp.string()}, 50, [&](const FsEvent& ev) {
      std::lock_guard<std::mutex> lock(mu);
      hits.push_back({ev.kind, ev.path});
    }));

    auto saw = [&](const std::string& name) {
      auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
      for (;;) {
        {
          std::lock_guard<std::mutex> lock(mu);
          for (auto& h : hits)
            if (h.path.size() >= name.size() &&
                h.path.compare(h.path.size() - name.size(), name.size(), name) == 0)
              return true;
        }
        if (std::chrono::steady_clock::now() >= until) return false;
#if defined(__APPLE__)
        // FSEvents delivers on the main run loop, which a test binary never
        // pumps otherwise — run it briefly each poll. Other backends push
        // from their own threads and need no pumping.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
      }
    };
    auto count = [&] {
      std::lock_guard<std::mutex> lock(mu);
      return hits.size();
    };

    {
      std::FILE* f = std::fopen(file.c_str(), "w");
      CHECK(f != nullptr);
      if (f) {
        std::fputs("one", f);
        std::fclose(f);
      }
    }
    CHECK(saw("note.txt"));

    // Drain so the modify below is observed as a fresh event, not coalesced.
    {
      auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
      while (std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    {
      std::lock_guard<std::mutex> lock(mu);
      hits.clear();
    }
    // Sleep past the 50ms debounce window so the modify is observed as a
    // fresh event even on fast CI runners.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    {
      std::FILE* f = std::fopen(file.c_str(), "a");
      CHECK(f != nullptr);
      if (f) {
        std::fputs("two", f);
        std::fclose(f);
      }
    }
    CHECK(saw("note.txt"));
    CHECK(count() >= 1);

    {
      std::lock_guard<std::mutex> lock(mu);
      hits.clear();
    }
    // Same guard before the delete: without it a Modified emitted just
    // above can swallow the Deleted on path-keyed backends.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    fs::remove(file, ec);
    CHECK(saw("note.txt"));

    watcher.stop();
    fs::remove_all(tmp, ec);
  }
}
