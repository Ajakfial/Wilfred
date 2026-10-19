# Wilfred Implementation Plan

## 1. Explorer/Files App Integration
- [ ] Add `open_parent` action to file/folder results (opens containing folder in native file manager)
- [ ] Add `open_with_file_manager` action for directories
- [ ] Implement native_reveal for all platforms (Windows, macOS, Linux, BSD)
- [ ] Add file manager detection and preferred app selection

## 2. Overlay Accessibility Pass
- [ ] Add comprehensive ARIA labels and roles
- [ ] Improve keyboard navigation (focus management, roving tabindex)
- [ ] Add screen reader announcements for dynamic content
- [ ] Add high contrast mode support
- [ ] Add reduced motion support
- [ ] Add focus trapping in modals/popovers
- [ ] Add live regions for search results and status updates

## 3. BSD Promotion
- [ ] Add BSD-specific file manager integration (Thunar, PCManFM, Dolphin, Nautilus)
- [ ] Improve BSD system toggle support (WiFi, Bluetooth, volume, brightness)
- [ ] Add BSD package manager integration (pkg, pkgin, pkg_add)
- [ ] Update BSD documentation with feature parity matrix
- [ ] Add BSD CI testing improvements
- [ ] Add BSD-specific benchmarks

## 4. 1M+ Benchmark Numbers
- [ ] Extend bench_main.cpp to support 1M record benchmarks
- [ ] Add memory-efficient benchmark modes
- [ ] Add benchmark runner script with multiple dataset sizes
- [ ] Add benchmark result publishing script
- [ ] Add CI benchmark job for regular performance tracking

## 5. Cross-Platform Verification
- [ ] Verify all native functions implemented on all platforms
- [ ] Add platform feature matrix documentation
- [ ] Add cross-platform test suite
- [ ] Fix any platform-specific bugs
- [ ] Ensure consistent behavior across platforms