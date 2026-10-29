#include "wilfred/platform/native.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <string>
#include <vector>
#include <functional>

#if defined(WILFRED_HAS_ATSPI)
#include <atspi/atspi.h>
#include <glib.h>
#endif

namespace wilfred {

#if defined(WILFRED_HAS_ATSPI)

namespace {

AtspiAccessible* g_menu_app = nullptr;
std::vector<AtspiAccessible*> g_items;
std::vector<std::string> g_item_ids;

void clear_items() {
  for (auto* a : g_items) if (a) g_object_unref(a);
  g_items.clear();
  g_item_ids.clear();
}

std::string atspi_name(AtspiAccessible* a) {
  if (!a) return {};
  GError* err = nullptr;
  gchar* name = atspi_accessible_get_name(a, &err);
  std::string out;
  if (name) { out = name; g_free(name); }
  if (err) g_error_free(err);
  return out;
}

AtspiRole atspi_role(AtspiAccessible* a) {
  if (!a) return ATSPI_ROLE_INVALID;
  GError* err = nullptr;
  AtspiRole r = atspi_accessible_get_role(a, &err);
  if (err) g_error_free(err);
  return r;
}

bool has_state(AtspiAccessible* a, AtspiStateType st) {
  if (!a) return false;
  GError* err = nullptr;
  AtspiStateSet* ss = atspi_accessible_get_state_set(a, &err);
  bool ok = false;
  if (ss) { ok = atspi_state_set_contains(ss, st); g_object_unref(ss); }
  if (err) g_error_free(err);
  return ok;
}

void find_menu_bar(AtspiAccessible* node, int depth, AtspiAccessible*& out) {
  if (!node || depth > 6 || out) return;
  if (atspi_role(node) == ATSPI_ROLE_MENU_BAR) { out = node; g_object_ref(out); return; }
  GError* err = nullptr;
  gint n = atspi_accessible_get_child_count(node, &err);
  if (err) { g_error_free(err); err = nullptr; }
  for (gint i = 0; i < n && !out; ++i) {
    AtspiAccessible* c = atspi_accessible_get_child_at_index(node, i, &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (c) { find_menu_bar(c, depth + 1, out); g_object_unref(c); }
  }
}

AtspiAccessible* focused_app() {
  if (g_menu_app) { g_object_ref(g_menu_app); return g_menu_app; }
  AtspiAccessible* desktop = atspi_get_desktop(0);
  if (!desktop) return nullptr;
  GError* err = nullptr;
  gint n = atspi_accessible_get_child_count(desktop, &err);
  if (err) { g_error_free(err); err = nullptr; }
  AtspiAccessible* found = nullptr;
  for (gint i = 0; i < n && !found; ++i) {
    AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!app) continue;
    if (has_state(app, ATSPI_STATE_ACTIVE)) found = app;
    else g_object_unref(app);
  }
  g_object_unref(desktop);
  return found;
}

}  // namespace

bool native_active_menu_items(std::vector<NativeMenuItem>& out, std::string& error) {
  error.clear();
  clear_items();
  AtspiAccessible* app = focused_app();
  if (!app) { error = "No focused application (accessibility permission?)"; return false; }
  AtspiAccessible* bar = nullptr;
  find_menu_bar(app, 0, bar);
  if (!bar) { g_object_unref(app); error = "this app does not expose a menu bar"; return false; }
  std::string appname = atspi_name(app);
  int budget = 600;
  std::function<void(AtspiAccessible*, const std::string&, int)> rec = [&](AtspiAccessible* node, const std::string& prefix, int depth) {
    if (!node || depth > 5 || budget <= 0) return;
    GError* err = nullptr;
    gint n = atspi_accessible_get_child_count(node, &err);
    if (err) { g_error_free(err); err = nullptr; }
    for (gint i = 0; i < n && budget > 0; ++i) {
      --budget;
      AtspiAccessible* c = atspi_accessible_get_child_at_index(node, i, &err);
      if (err) { g_error_free(err); err = nullptr; }
      if (!c) continue;
      std::string title = atspi_name(c);
      AtspiRole r = atspi_role(c);
      std::string path = title.empty() ? prefix : (prefix.empty() ? title : prefix + " > " + title);
      if ((r == ATSPI_ROLE_MENU_ITEM || r == ATSPI_ROLE_CHECK_MENU_ITEM || r == ATSPI_ROLE_RADIO_MENU_ITEM) && !title.empty()) {
        NativeMenuItem item;
        item.id = path;
        item.title = title;
        item.path = path;
        item.app = appname;
        out.push_back(std::move(item));
        g_items.push_back(c);
        g_item_ids.push_back(path);
        g_object_ref(c);
      }
      rec(c, title.empty() ? prefix : path, depth + 1);
      g_object_unref(c);
    }
  };
  rec(bar, {}, 0);
  g_object_unref(bar);
  g_object_unref(app);
  return !out.empty();
}

bool native_invoke_menu_item(const std::string& id, std::string& error) {
  error.clear();
  for (std::size_t i = 0; i < g_item_ids.size() && i < g_items.size(); ++i) {
    if (g_item_ids[i] != id) continue;
    AtspiAccessible* acc = g_items[i];
    if (!acc) break;
    GError* err = nullptr;
    gboolean ok = atspi_action_do_action(acc, 0, &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (ok) return true;
    break;
  }
  error = "menu item is gone (did the app change?)";
  return false;
}

void native_menu_remember_foreground() {
  if (g_menu_app) { g_object_unref(g_menu_app); g_menu_app = nullptr; }
  AtspiAccessible* desktop = atspi_get_desktop(0);
  if (!desktop) return;
  GError* err = nullptr;
  gint n = atspi_accessible_get_child_count(desktop, &err);
  if (err) { g_error_free(err); err = nullptr; }
  for (gint i = 0; i < n; ++i) {
    AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!app) continue;
    if (has_state(app, ATSPI_STATE_ACTIVE)) { g_menu_app = app; break; }
    else g_object_unref(app);
  }
  g_object_unref(desktop);
}

#else

bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string& error) {
  error = "Menu search is not implemented on Linux (needs AT-SPI)";
  return false;
}
bool native_invoke_menu_item(const std::string&, std::string& error) {
  error = "Menu search is not implemented on Linux (needs AT-SPI)";
  return false;
}
void native_menu_remember_foreground() {}

#endif

}  // namespace wilfred

#else
namespace wilfred {
bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string& error) {
  error = "Menu search is not implemented on this platform";
  return false;
}
bool native_invoke_menu_item(const std::string&, std::string& error) {
  error = "Menu search is not implemented on this platform";
  return false;
}
void native_menu_remember_foreground() {}
}  // namespace wilfred
#endif
