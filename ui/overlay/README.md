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
`Esc` dismiss, `Tab` fix-or-actions, `←/→` menu, `↑/↓` move (`Home`/`End`
jump, `PgUp`/`PgDn` page), `Ctrl`/`⌘`+`1–9` quick-open, `Enter` submit
(`Shift`/`Alt` picks the secondary action), `F3` preview pane. Hover
selects (mouse-first, keyboard keeps explicit index).

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
