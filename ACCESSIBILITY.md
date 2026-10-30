# Accessibility

Wilfred aims to be usable with keyboard, screen readers, and high-contrast
setups.

## Keyboard

- The overlay is fully keyboard-driven: arrow keys navigate, Enter opens,
  Tab completes, and Esc dismisses.
- `mod+k` opens the actions menu for the selected result.
- `F3` (or the configured preview key) opens the preview pane.

## Screen readers

- Result rows expose `role="option"` with `aria-selected` and `aria-label`
  where appropriate.
- The search input announces result counts via the live region
  (`aria-live="polite"`).
- Icon-only buttons carry `aria-label`.

## Visual

- Respects `prefers-color-scheme` and the configured accent color.
- Supports reduced motion and transparency settings via the overlay config.

## Reporting

Please file accessibility issues with the `accessibility` label.