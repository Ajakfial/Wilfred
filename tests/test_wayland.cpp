#include "test.hpp"
#include "wilfred/ui/wayland.hpp"

#include <cstdlib>
#include <string>

#ifdef _WIN32
// Portable set/unset for the session-hint tests below.
static void test_setenv(const char* name, const char* value) {
  std::string kv = std::string(name) + "=" + (value ? value : "");
  _putenv(kv.c_str());
}
#else
static void test_setenv(const char* name, const char* value) {
  if (value) {
    setenv(name, value, 1);
  } else {
    unsetenv(name);
  }
}
#endif

void test_wayland() {
  using namespace wilfred;
  // Snapshot the outer environment so the test never leaks state, even when
  // the suite itself runs inside a Wayland session.
  const char* outer_display = std::getenv("WAYLAND_DISPLAY");
  const char* outer_session = std::getenv("XDG_SESSION_TYPE");
  const std::string saved_display = outer_display ? outer_display : "";
  const std::string saved_session = outer_session ? outer_session : "";
  const bool had_display = outer_display != nullptr;
  const bool had_session = outer_session != nullptr;

  test_setenv("WAYLAND_DISPLAY", nullptr);
  test_setenv("XDG_SESSION_TYPE", nullptr);
  CHECK(!wayland_session_hint());

  // Empty display is the same as unset (some compositors export it empty).
  test_setenv("WAYLAND_DISPLAY", "");
  CHECK(!wayland_session_hint());

  test_setenv("WAYLAND_DISPLAY", "wayland-0");
  CHECK(wayland_session_hint());

  test_setenv("WAYLAND_DISPLAY", nullptr);
  CHECK(!wayland_session_hint());
  test_setenv("XDG_SESSION_TYPE", "wayland");
  CHECK(wayland_session_hint());
  test_setenv("XDG_SESSION_TYPE", "x11");
  CHECK(!wayland_session_hint());

  // A Wayland socket wins over a non-Wayland session type.
  test_setenv("WAYLAND_DISPLAY", "wayland-1");
  test_setenv("XDG_SESSION_TYPE", "x11");
  CHECK(wayland_session_hint());

  test_setenv("WAYLAND_DISPLAY", had_display ? saved_display.c_str() : nullptr);
  test_setenv("XDG_SESSION_TYPE", had_session ? saved_session.c_str() : nullptr);
}
