// Bundles src/main.tsx into dist/app.bundle.js for the overlay WebViews.
// Run: npm run build   (needs devDependencies installed once via npm install)
import { build } from "esbuild";
import { mkdirSync } from "node:fs";

mkdirSync(new URL("./dist/", import.meta.url), { recursive: true });

await build({
  entryPoints: ["src/main.tsx"],
  bundle: true,
  minify: true,
  sourcemap: false,
  format: "iife",
  platform: "browser",
  target: ["chrome90", "safari14"],
  define: { "process.env.NODE_ENV": '"production"' },
  outfile: "dist/app.bundle.js",
  logLevel: "info",
});
