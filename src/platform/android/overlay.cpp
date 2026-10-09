#include "wilfred/service/service.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// The overlay UI is implemented in Kotlin (FloatingWService + MainActivity).
// The native service therefore runs headless; null is a valid OverlayUi
// state throughout Service (every use is guarded by `if (ui_)`).

std::unique_ptr<OverlayUi> create_overlay() { return nullptr; }

// Headless (Kotlin owns the UI and its own event loop): pump is a no-op so
// pump_native_events() links on Android like on every other backend.
void overlay_pump() {}

#endif
}  // namespace wilfred
