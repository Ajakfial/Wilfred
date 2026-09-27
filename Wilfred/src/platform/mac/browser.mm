#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include <filesystem>

namespace wilfred {
#ifdef __APPLE__
namespace fs = std::filesystem;

static std::string cf_to_std(CFStringRef s) {
  if (!s) return {};
  char buf[1024];
  if (CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) return buf;
  return {};
}

static bool bundle_exists(const char* p) {
  std::error_code ec;
  return fs::exists(fs::u8path(p), ec);
}

std::string native_default_browser_id() {
  CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, CFSTR("http://example.com"), nullptr);
  CFURLRef app = nullptr;
  LSGetApplicationForURL(url, kLSRolesAll, nullptr, &app);
  if (url) CFRelease(url);
  if (!app) return "safari";
  CFStringRef path = CFURLCopyFileSystemPath(app, kCFURLPOSIXPathStyle);
  auto p = cf_to_std(path);
  if (path) CFRelease(path);
  CFRelease(app);
  auto l = to_lower_utf8(p);
  if (l.find("chrome") != std::string::npos) return "chrome";
  if (l.find("firefox") != std::string::npos) return "firefox";
  if (l.find("safari") != std::string::npos) return "safari";
  if (l.find("edge") != std::string::npos) return "edge";
  if (l.find("brave") != std::string::npos) return "brave";
  if (l.find("chromium") != std::string::npos) return "chromium";
  return p.empty() ? "safari" : p;
}

std::string native_default_browser_executable() {
  CFURLRef url = CFURLCreateWithString(kCFAllocatorDefault, CFSTR("http://example.com"), nullptr);
  CFURLRef app = nullptr;
  LSGetApplicationForURL(url, kLSRolesAll, nullptr, &app);
  if (url) CFRelease(url);
  if (!app) return "/Applications/Safari.app";
  CFStringRef path = CFURLCopyFileSystemPath(app, kCFURLPOSIXPathStyle);
  auto p = cf_to_std(path);
  if (path) CFRelease(path);
  CFRelease(app);
  return p.empty() ? "/Applications/Safari.app" : p;
}

std::vector<BrowserInfo> native_list_browsers() {
  std::vector<BrowserInfo> out;
  struct Cand {
    const char* id;
    const char* name;
    const char* path;
  };
  Cand cands[] = {{"safari", "Safari", "/Applications/Safari.app"},
                  {"chrome", "Google Chrome", "/Applications/Google Chrome.app"},
                  {"firefox", "Firefox", "/Applications/Firefox.app"},
                  {"edge", "Microsoft Edge", "/Applications/Microsoft Edge.app"},
                  {"brave", "Brave Browser", "/Applications/Brave Browser.app"},
                  {nullptr, nullptr, nullptr}};
  auto def = to_lower_utf8(native_default_browser_executable());
  for (int i = 0; cands[i].id; ++i) {
    if (!bundle_exists(cands[i].path)) continue;
    BrowserInfo b;
    b.id = cands[i].id;
    b.name = cands[i].name;
    b.executable = cands[i].path;
    b.is_default = def.find(to_lower_utf8(cands[i].id)) != std::string::npos ||
                   def.find(to_lower_utf8(std::string(cands[i].path))) != std::string::npos;
    out.push_back(std::move(b));
  }
  if (out.empty()) {
    BrowserInfo b;
    b.id = native_default_browser_id();
    b.name = b.id;
    b.executable = native_default_browser_executable();
    b.is_default = true;
    out.push_back(b);
  }
  return out;
}

#endif
}  // namespace wilfred
