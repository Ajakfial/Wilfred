#include "wilfred/platform/platform.hpp"
#include "wilfred/service/service.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// The overlay UI is implemented in Swift (ContentView). The native service
// therefore runs headless; null is a valid OverlayUi state throughout
// Service (every use is guarded by `if (ui_)`).

std::unique_ptr<OverlayUi> create_overlay() { return nullptr; }

// Headless (Swift owns the UI and its own runloop): pump is a no-op so
// pump_native_events() links on iOS like on every other backend.
void overlay_pump() {}

#endif
}  // namespace wilfred
