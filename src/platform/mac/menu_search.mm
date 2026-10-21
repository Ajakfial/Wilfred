#include "wilfred/platform/native.hpp"

#if defined(__APPLE__) && !defined(IOS)

#import <ApplicationServices/ApplicationServices.h>

#include <string>
#include <utility>
#include <vector>

namespace wilfred {
namespace {

std::string cf_to_utf8(CFStringRef s) {
  if (!s) return {};
  CFIndex len = CFStringGetLength(s);
  CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
  std::string out(static_cast<std::size_t>(max), '\0');
  if (!CFStringGetCString(s, &out[0], max, kCFStringEncodingUTF8)) return {};
  out.resize(std::char_traits<char>::length(out.c_str()));
  return out;
}

std::string attr_string(AXUIElementRef el, CFStringRef attr) {
  CFTypeRef value = nullptr;
  if (AXUIElementCopyAttributeValue(el, attr, &value) != kAXErrorSuccess || !value) return {};
  std::string out;
  if (CFGetTypeID(value) == CFStringGetTypeID()) out = cf_to_utf8(static_cast<CFStringRef>(value));
  CFRelease(value);
  return out;
}

// The app that had focus when the launcher was summoned. The overlay is
// focused while searching, so a live lookup would always answer "Wilfred".
AXUIElementRef g_menu_app = nullptr;

struct RawItem {
  std::string title;
  std::string path;
  std::string shortcut;
  std::string app;
  AXUIElementRef element{nullptr};  // retained
};

void walk(AXUIElementRef el, const std::string& prefix, int depth, const std::string& app,
          std::vector<RawItem>& out, std::size_t& budget) {
  if (!el || depth > 4 || budget == 0) return;
  CFTypeRef kids_ref = nullptr;
  if (AXUIElementCopyAttributeValue(el, kAXChildrenAttribute, &kids_ref) != kAXErrorSuccess ||
      !kids_ref) {
    return;
  }
  CFArrayRef kids = static_cast<CFArrayRef>(kids_ref);
  const CFIndex n = CFArrayGetCount(kids);
  for (CFIndex i = 0; i < n && budget > 0; ++i) {
    --budget;
    AXUIElementRef child =
        static_cast<AXUIElementRef>(const_cast<void*>(CFArrayGetValueAtIndex(kids, i)));
    if (!child) continue;

    const std::string title = attr_string(child, kAXTitleAttribute);
    if (title.empty()) continue;
    const std::string path = prefix.empty() ? title : prefix + " > " + title;

    RawItem item;
    item.title = title;
    item.path = path;
    item.shortcut = attr_string(child, kAXMenuItemCmdCharAttribute);
    item.app = app;
    item.element = child;
    CFRetain(child);
    out.push_back(std::move(item));

    walk(child, path, depth + 1, app, out, budget);
  }
  CFRelease(kids_ref);
}

bool collect(std::vector<RawItem>& out, std::string& error) {
  AXUIElementRef app_el = g_menu_app;
  bool owns_app = false;
  if (!app_el) {
    AXUIElementRef sys = AXUIElementCreateSystemWide();
    if (!sys) {
      error = "could not reach the accessibility API";
      return false;
    }
    CFTypeRef app_ref = nullptr;
    if (AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, &app_ref) !=
            kAXErrorSuccess ||
        !app_ref) {
      CFRelease(sys);
      error = "Accessibility permission is required to read menus";
      return false;
    }
    app_el = static_cast<AXUIElementRef>(const_cast<void*>(app_ref));
    owns_app = true;
  }

  CFTypeRef bar_ref = nullptr;
  bool ok = false;
  if (AXUIElementCopyAttributeValue(app_el, kAXMenuBarAttribute, &bar_ref) != kAXErrorSuccess ||
      !bar_ref) {
    error = "this app does not expose a menu bar";
  } else {
    const std::string app = attr_string(app_el, kAXTitleAttribute);
    AXUIElementRef bar = static_cast<AXUIElementRef>(const_cast<void*>(bar_ref));
    std::size_t budget = 600;
    walk(bar, {}, 0, app, out, budget);
    CFRelease(bar_ref);
    ok = !out.empty();
    if (!ok) error = "the menu bar is empty";
  }
  if (owns_app) CFRelease(app_el);
  return ok;
}

void release_items(std::vector<RawItem>& items) {
  for (auto& it : items) {
    if (it.element) CFRelease(it.element);
  }
  items.clear();
}

}  // namespace

void native_menu_remember_foreground() {
  AXUIElementRef sys = AXUIElementCreateSystemWide();
  if (!sys) return;
  CFTypeRef app_ref = nullptr;
  if (AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, &app_ref) ==
          kAXErrorSuccess &&
      app_ref) {
    AXUIElementRef app = static_cast<AXUIElementRef>(const_cast<void*>(app_ref));
    CFRetain(app);
    if (g_menu_app) CFRelease(g_menu_app);
    g_menu_app = app;
    CFRelease(app_ref);
  }
  CFRelease(sys);
}

bool native_active_menu_items(std::vector<NativeMenuItem>& out, std::string& error) {
  error.clear();
  std::vector<RawItem> raw;
  if (!collect(raw, error)) {
    release_items(raw);
    return false;
  }
  for (const auto& r : raw) {
    NativeMenuItem item;
    item.id = r.path;
    item.title = r.title;
    item.path = r.path;
    item.shortcut = r.shortcut;
    item.app = r.app;
    out.push_back(std::move(item));
  }
  release_items(raw);
  return true;
}

bool native_invoke_menu_item(const std::string& id, std::string& error) {
  error.clear();
  std::vector<RawItem> raw;
  if (!collect(raw, error)) {
    release_items(raw);
    return false;
  }
  bool ok = false;
  for (auto& r : raw) {
    if (r.path != id || !r.element) continue;
    ok = AXUIElementPerformAction(r.element, kAXPressAction) == kAXErrorSuccess;
    break;
  }
  if (!ok && error.empty()) error = "menu item is gone (did the app change?)";
  release_items(raw);
  return ok;
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string&);
bool native_invoke_menu_item(const std::string&, std::string&);
void native_menu_remember_foreground();
}  // namespace wilfred
#endif
