# Accessibility

Wilfred aims to be usable with keyboard, screen readers, and high-contrast
setups on every supported platform. The cross-platform verification matrix
tracks screen-reader, keyboard, high-contrast, and reduced-motion support as
fully supported on all desktop and mobile targets.

## Keyboard

- The overlay is fully keyboard-driven: arrow keys navigate, Enter opens,
  Tab completes, and Esc dismisses.
- `mod+k` opens the actions menu for the selected result.
- `F3` (or the configured preview key) opens the preview pane.
- All interactive elements are reachable without a pointer, and tab order
  follows the visual layout.
- Modal dialogs (settings, action menu) trap focus until dismissed.

## Screen readers

- Result rows expose `role="option"` with `aria-selected` and `aria-label`
  where appropriate.
- The search input announces result counts via the live region
  (`aria-live="polite"`).
- Icon-only buttons carry `aria-label`.
- Dynamic content changes (corrections, completions, preview open/close,
  action menu open/close) are announced through live regions.
- On Linux, AT-SPI2 is used for menu search and screen-reader integration
  when `WILFRED_HAS_ATSPI` is enabled at build time.

## Visual

- Respects `prefers-color-scheme` and the configured accent color.
- Supports high contrast via `prefers-contrast: more`.
- Supports reduced motion via `prefers-reduced-motion: reduce`, which
  disables overlay animations.
- Supports transparency/blur settings via the overlay config, with an
  opaque fallback.

## Platform notes

- Windows: WebView2 overlay exposes the same ARIA contract as the web UI.
- macOS: WKWebView overlay with VoiceOver support.
- Linux: WebKitGTK overlay; on GNOME Wayland the `wilfred-layer-shell`
  GNOME Shell extension keeps the window on top and out of the taskbar.
- Mobile (Android/iOS): native search UIs follow platform accessibility
  guidelines.

## Reporting

Please file accessibility issues with the `accessibility` label.