# Overlay UI

React + TypeScript glass command palette (Raycast-grade) shared by Windows
(WebView2), macOS (WKWebView) and Linux (WebKitGTK). The X11 canvas overlay
is a separate native fallback in `src/ui/x11_overlay.cpp` with a matching
palette — no HTML needed there.

Glass is progressive enhancement: `style.css` paints solid opaque fallbacks
first, then an `@supports (backdrop-filter)` block upgrades to
`blur(32px) saturate(1.5)` glass. Unsupported engines stay fully readable.

## Layout

```
index.html          shell page, loads dist/app.bundle.js
style.css           hand-maintained solid theme (dark/light, reduced-motion)
src/                TypeScript + React sources
  main.tsx          createRoot entry
  App.tsx           state reducer, keyboard/mouse, native bridge, effects
  protocol.ts       message transport, kind/group maps, highlighting
  demo.ts           offline demo backend for browser-only `?preview` mode
  components/       ResultRow, PreviewPane, icons (Lucide-style inline SVGs)
dist/               committed esbuild bundle (see below)
```

The message protocol is unchanged from the vanilla-JS version, so no C++
changes are needed: `ready` / `query` / `submit` / `hidden` / `resize`
outbound, `show` / `hide` / `results` / `preview` inbound. Keyboard map:
`Esc` back/dismiss (menu, correction, details, then the panel), `Tab` fix/complete,
`Ctrl`/`⌘`+`K` actions popover (`↑/↓` move inside it), `Ctrl`+`U` clear query,
`↑/↓` move (`Home`/`End` jump, `PgUp`/`PgDn` page), `Ctrl`/`⌘`+`1–9` quick-open
(hold the modifier to see the numbers), `Enter` submit (`Shift`/`Alt` picks the
secondary action), `F3` split detail pane. Hover
selects (mouse-first, keyboard keeps explicit index).

## Resizing

By default the window hugs its content (the UI reports `.shell` size via
`resize`). The native windows are also drag-resizable from any edge/corner
(Windows: `WS_THICKFRAME` + `WM_NCHITTEST`; macOS: `NSWindowStyleMaskResizable`;
Linux: `gtk_window_begin_resize_drag` on an 8px edge band). When the viewport
changes without the UI having asked for it, `protocol.ts` flips on `html.fit`:
the panel fills the viewport, the list/detail flex and scroll inside it, the
split view collapses below 560px wide, and `resize` reporting stops so the host
never fights the user's size. Min size is 420x100 on every platform.

## Building

Requires Node 18+ (only for UI work — C++ builds use the committed bundle):

```bash
cd ui/overlay
npm install        # once
npm run typecheck  # tsc --noEmit
npm run build      # esbuild -> dist/app.bundle.js
```

**Always rebuild `dist/` and commit it alongside source changes** — CMake
copies the whole `ui/overlay` directory next to the binary, and the C++
build does not run npm.

## Demo mode

Open `index.html?preview` directly in a browser (no native host): the app
falls back to the `demo.ts` backend with sample results, including a
simulated live speedtest, so UI changes can be iterated without Wilfred.
