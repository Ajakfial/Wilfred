# Overlay UI

React + TypeScript rewrite of the Wilfred search overlay (Windows WebView2
and macOS WKWebView; the Linux/X11 overlay is a separate native window in
`src/ui/x11_overlay.cpp` and is unaffected).

## Layout

```
index.html          shell page, loads dist/app.bundle.js
style.css           hand-maintained theme (dark/light, reduced-motion)
vendor/             bootstrap-icons CSS (shipped as-is)
src/                TypeScript + React sources
  main.tsx          createRoot entry
  App.tsx           state reducer, keyboard/mouse, native bridge, effects
  protocol.ts       message transport, badges/groups, kindOf, highlighting
  demo.ts           offline demo backend for browser-only `?preview` mode
  components/       ResultRow, PreviewPane
dist/               committed esbuild bundle (see below)
```

The message protocol is unchanged from the vanilla-JS version, so no C++
changes are needed: `ready` / `query` / `submit` / `hidden` / `resize`
outbound, `show` / `hide` / `results` / `preview` inbound. Keyboard map:
`Esc` dismiss, `Tab` actions, `←/→` menu, `↑/↓` move, `Enter` submit
(`Shift`/`Alt` picks the secondary action), `F3` preview pane.

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
