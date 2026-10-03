#include "wilfred/service/service.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// The overlay UI is implemented in Kotlin (FloatingWService + MainActivity).
// The native service therefore runs headless; null is a valid OverlayUi
// state throughout Service (every use is guarded by `if (ui_)`).

std::unique_ptr<OverlayUi> create_overlay() { return nullptr; }

#endif
}  // namespace wilfred
