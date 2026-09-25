#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/ui/web_ui.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>
#include <vector>

namespace wilfred {
#ifdef __APPLE__

static OverlayQuery g_query;
static OverlaySubmit g_submit;
static std::vector<SearchResult> g_results;
static bool g_visible = false;

@interface WilfredPanel : NSWindow
@end
@implementation WilfredPanel
- (BOOL)canBecomeKeyWindow {
  return YES;
}
- (BOOL)canBecomeMainWindow {
  return YES;
}
@end

@interface WilfredCtl : NSObject <WKScriptMessageHandler, WKNavigationDelegate>
@property(nonatomic, strong) WilfredPanel* window;
@property(nonatomic, strong) WKWebView* web;
@property(nonatomic, assign) BOOL ready;
@property(nonatomic, assign) BOOL wantShow;
- (void)sendJson:(const std::string&)json;
- (void)handleJson:(const std::string&)json;
- (void)placeWidth:(int)w height:(int)h;
@end

static WilfredCtl* g_ctl = nil;

@implementation WilfredCtl
- (void)sendJson:(const std::string&)json {
  if (!self.web) return;
  NSString* js =
      [NSString stringWithFormat:@"window.__wilfredNative && window.__wilfredNative(%s);", json.c_str()];
  [self.web evaluateJavaScript:js completionHandler:nil];
}
- (void)placeWidth:(int)w height:(int)h {
  NSRect screen = [[NSScreen mainScreen] visibleFrame];
  if (w < 420) w = 420;
  if (h < 100) h = 100;
  CGFloat x = NSMidX(screen) - w / 2.0;
  CGFloat y = NSMaxY(screen) - screen.size.height / 5.0 - h;
  [self.window setFrame:NSMakeRect(x, y, w, h) display:YES];
}
- (void)userContentController:(WKUserContentController*)ucc didReceiveScriptMessage:(WKScriptMessage*)msg {
  (void)ucc;
  std::string json;
  if ([msg.body isKindOfClass:[NSString class]]) {
    json = [msg.body UTF8String] ?: "";
  } else if ([NSJSONSerialization isValidJSONObject:msg.body]) {
    NSData* data = [NSJSONSerialization dataWithJSONObject:msg.body options:0 error:nil];
    if (data) json.assign(reinterpret_cast<const char*>(data.bytes), data.length);
  }
  [self handleJson:json];
}
- (void)handleJson:(const std::string&)json {
  std::string type;
  overlay_json_field(json, "type", type);
  if (type == "ready") {
    self.ready = YES;
    if (self.wantShow) {
      self.wantShow = NO;
      [self.window makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
      g_visible = true;
      [self sendJson:"{\"type\":\"show\"}"];
    }
    return;
  }
  if (type == "query") {
    std::string q;
    overlay_json_field(json, "q", q);
    if (g_query) g_results = g_query(q);
    [self sendJson:overlay_results_json(g_results)];
    return;
  }
  if (type == "submit") {
    std::string idxs;
    overlay_json_field(json, "index", idxs);
    int idx = 0;
    try {
      idx = std::stoi(idxs);
    } catch (...) {
      idx = 0;
    }
    if (idx >= 0 && idx < static_cast<int>(g_results.size()) && g_submit)
      g_submit(g_results[static_cast<std::size_t>(idx)]);
    [self.window orderOut:nil];
    g_visible = false;
    return;
  }
  if (type == "hidden") {
    [self.window orderOut:nil];
    g_visible = false;
    return;
  }
  if (type == "resize") {
    std::string ws, hs;
    overlay_json_field(json, "width", ws);
    overlay_json_field(json, "height", hs);
    int w = 704, h = 140;
    try {
      if (!ws.empty()) w = std::stoi(ws);
      if (!hs.empty()) h = std::stoi(hs);
    } catch (...) {
    }
    [self placeWidth:w height:h];
  }
}
- (void)webView:(WKWebView*)web didFinishNavigation:(WKNavigation*)nav {
  (void)web;
  (void)nav;
}
@end

class MacOverlay final : public OverlayUi {
public:
  OverlayQuery query;
  OverlaySubmit submit;
  bool create() override {
    @autoreleasepool {
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
      g_ctl = [WilfredCtl new];
      NSRect frame = NSMakeRect(0, 0, 704, 140);
      WilfredPanel* w =
          [[WilfredPanel alloc] initWithContentRect:frame
                                          styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
      [w setOpaque:NO];
      [w setHasShadow:NO];
      [w setBackgroundColor:[NSColor clearColor]];
      [w setLevel:NSFloatingWindowLevel];
      [w setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                               NSWindowCollectionBehaviorFullScreenAuxiliary];
      WKWebViewConfiguration* cfg = [WKWebViewConfiguration new];
      [cfg.userContentController addScriptMessageHandler:g_ctl name:@"wilfred"];
      WKWebView* web = [[WKWebView alloc] initWithFrame:frame configuration:cfg];
      web.navigationDelegate = g_ctl;
      [web setValue:@NO forKey:@"drawsBackground"];
      if (@available(macOS 12.0, *)) {
        web.underPageBackgroundColor = [NSColor clearColor];
      }
      NSView* content = [w contentView];
      content.wantsLayer = YES;
      content.layer.backgroundColor = [NSColor clearColor].CGColor;
      web.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      [content addSubview:web];
      auto dir = overlay_ui_dir();
      NSString* path = [NSString stringWithUTF8String:path_join(dir, "index.html").c_str()];
      NSURL* url = [NSURL fileURLWithPath:path];
      NSURL* root = [url URLByDeletingLastPathComponent];
      [web loadFileURL:url allowingReadAccessToURL:root];
      g_ctl.window = w;
      g_ctl.web = web;
      g_query = query;
      g_submit = submit;
      return true;
    }
  }
  void show() override {
    @autoreleasepool {
      if (!g_ctl) return;
      g_ctl.wantShow = YES;
      if (!g_ctl.ready) return;
      [g_ctl.window makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
      g_visible = true;
      [g_ctl sendJson:"{\"type\":\"show\"}"];
    }
  }
  void hide() override {
    @autoreleasepool {
      if (g_ctl) [g_ctl.window orderOut:nil];
      g_visible = false;
    }
  }
  void destroy() override {
    @autoreleasepool {
      if (g_ctl) [g_ctl.window close];
      g_ctl = nil;
    }
  }
  bool visible() const override { return g_visible; }
};

static MacOverlay* g_overlay = nullptr;

std::unique_ptr<OverlayUi> create_overlay() {
  auto p = std::make_unique<MacOverlay>();
  g_overlay = p.get();
  return p;
}

void overlay_bind(OverlayQuery q, OverlaySubmit s) {
  if (g_overlay) {
    g_overlay->query = std::move(q);
    g_overlay->submit = std::move(s);
    g_query = g_overlay->query;
    g_submit = g_overlay->submit;
  }
}

void overlay_set_quit(std::function<void()> fn) { (void)fn; }

void overlay_pump() {}

#endif
}  // namespace wilfred
