#include "wilfred/service/service.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/core/utf8.hpp"

#import <AppKit/AppKit.h>
#include <vector>

namespace wilfred {
#ifdef __APPLE__

static OverlayQuery g_query;
static OverlaySubmit g_submit;

@interface WilfredWindow : NSWindow
@end
@implementation WilfredWindow
- (BOOL)canBecomeKeyWindow {
  return YES;
}
@end

@interface WilfredOverlayController : NSObject <NSTextFieldDelegate>
@property(nonatomic) WilfredWindow* window;
@property(nonatomic) NSTextField* field;
@property(nonatomic) NSTableView* table;
@property(nonatomic) std::vector<SearchResult> results;
@property(nonatomic) int sel;
@property(nonatomic) BOOL vis;
- (void)rebuild;
- (void)accept;
@end

static WilfredOverlayController* g_ctl = nil;

@implementation WilfredOverlayController
- (void)rebuild {
  if (g_query) self.results = g_query(std::string([[self.field stringValue] UTF8String] ?: ""));
  self.sel = 0;
  [self.table reloadData];
}
- (void)accept {
  if (self.sel >= 0 && self.sel < static_cast<int>(self.results.size()) && g_submit)
    g_submit(self.results[static_cast<std::size_t>(self.sel)]);
}
- (void)controlTextDidChange:(NSNotification*)n {
  (void)n;
  [self rebuild];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv {
  (void)tv;
  return static_cast<NSInteger>(self.results.size());
}
- (id)tableView:(NSTableView*)tv objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
  (void)tv;
  (void)col;
  if (row < 0 || row >= static_cast<NSInteger>(self.results.size())) return @"";
  return [NSString stringWithUTF8String:self.results[static_cast<std::size_t>(row)].title.c_str()];
}
@end

class MacOverlay final : public OverlayUi {
public:
  OverlayQuery query;
  OverlaySubmit submit;
  bool create() override {
    @autoreleasepool {
      if (![NSApplication sharedApplication]) return false;
      [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
      g_ctl = [WilfredOverlayController new];
      NSRect frame = NSMakeRect(0, 0, 720, 480);
      WilfredWindow* w = [[WilfredWindow alloc]
          initWithContentRect:frame
                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                      backing:NSBackingStoreBuffered
                        defer:NO];
      [w setTitle:@"Wilfred"];
      [w setLevel:NSFloatingWindowLevel];
      NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 440, 688, 28)];
      [field setPlaceholderString:@"Search"];
      field.delegate = g_ctl;
      NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(8, 8, 704, 420)];
      NSTableView* table = [[NSTableView alloc] initWithFrame:scroll.bounds];
      NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"name"];
      col.width = 680;
      [table addTableColumn:col];
      table.dataSource = (id)g_ctl;
      table.delegate = (id)g_ctl;
      scroll.documentView = table;
      [w.contentView addSubview:field];
      [w.contentView addSubview:scroll];
      g_ctl.window = w;
      g_ctl.field = field;
      g_ctl.table = table;
      return true;
    }
  }
  void show() override {
    @autoreleasepool {
      if (!g_ctl) return;
      [g_ctl.field setStringValue:@""];
      g_ctl.results.clear();
      [g_ctl.window center];
      [g_ctl.window makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
      g_ctl.vis = YES;
    }
  }
  void hide() override {
    @autoreleasepool {
      if (g_ctl) [g_ctl.window orderOut:nil];
      if (g_ctl) g_ctl.vis = NO;
    }
  }
  void destroy() override {
    @autoreleasepool {
      if (g_ctl) [g_ctl.window close];
      g_ctl = nil;
    }
  }
  bool visible() const override { return g_ctl && g_ctl.vis; }
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

#endif
}  // namespace wilfred
