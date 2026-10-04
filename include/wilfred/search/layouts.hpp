#pragma once

// Saved window layouts: named sets of window placements (match text +
// geometry) persisted as JSON under a layouts directory, applied through
// the native window move/resize/maximize primitives.
//
// A layout entry matches the first still-unplaced open window whose title
// or owner contains `match` (case-insensitive). Layouts back the `layout`
// mini (`layout save <name>`, `layout <name>`, `layouts`) and the
// `layout:<name>` workflow step.

#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct LayoutEntry {
  std::string match;
  int x{0};
  int y{0};
  int w{0};
  int h{0};
  bool maximized{false};
};

struct Layout {
  std::string name;
  std::vector<LayoutEntry> entries;
};

class LayoutStore {
 public:
  static LayoutStore& instance();

  // Configure the layouts directory (e.g. `<data dir>/layouts`).
  // Empty means memory-only, which is what unit tests use.
  void configure(std::string dir);

  bool save_layout(const Layout& layout, std::string& error);
  bool load_layout(const std::string& name, Layout& out, std::string& error) const;
  bool delete_layout(const std::string& name);
  std::vector<std::string> list() const;

  // Serialize/parse helpers (pure, unit-testable).
  static std::string serialize(const Layout& layout);
  static bool parse(const std::string& blob, Layout& out);

  static bool valid_name(const std::string& name);

  // Tiling presets over the *current* open windows (no saved file needed):
  // halves | thirds | grid | columns [N] | rows [N] | stack.
  // Windows are sorted by title and tiled across the primary work area.
  static std::vector<std::string> tiling_preset_names();
  static bool apply_tiling_preset(const std::string& preset, std::string& error);

  // Shared apply core used by the overlay action, `wilfred exec`, the tile
  // CLI, and monitor-change auto-apply.
  static bool apply_layout_by_name(const std::string& name, std::string& error);

 private:
  LayoutStore() = default;
  mutable std::mutex mu_;
  std::string dir_;
};

}  // namespace wilfred
