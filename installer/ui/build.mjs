// Bundles the setup wizard into a single self-contained dist/index.html.
//
// The C++ host (installer/host) embeds that file as a Win32 RCDATA resource
// and hands it to WebView2 with NavigateToString, so there must be no
// external references (no fonts, images, JS or CSS files).
//
// Run: npm run build   (needs devDependencies installed once via npm ci)
import { build } from "esbuild";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..", "..");
const licensePath = resolve(root, "packaging", "wix", "Wilfred-License.txt");
const license = readFileSync(licensePath, "utf8");
const version = process.env.WILFRED_SETUP_VERSION || "dev";

const result = await build({
  entryPoints: [resolve(here, "src", "main.ts")],
  bundle: true,
  minify: true,
  sourcemap: false,
  format: "iife",
  platform: "browser",
  target: ["chrome110"],
  write: false,
  define: {
    __LICENSE_TEXT__: JSON.stringify(license),
    __SETUP_VERSION__: JSON.stringify(version),
  },
  logLevel: "info",
});

const js = result.outputFiles[0].text;
const css = readFileSync(resolve(here, "src", "styles.css"), "utf8");
let html = readFileSync(resolve(here, "index.html"), "utf8");

if (!html.includes("/*__STYLE__*/") || !html.includes("/*__SCRIPT__*/")) {
  throw new Error("index.html is missing the style/script placeholders");
}
// Function replacers: bundle/CSS text may contain "$&" style sequences.
html = html.replace("/*__STYLE__*/", () => css);
html = html.replace("/*__SCRIPT__*/", () => js);

mkdirSync(resolve(here, "dist"), { recursive: true });
writeFileSync(resolve(here, "dist", "index.html"), html);
console.log(`dist/index.html (${(Buffer.byteLength(html) / 1024).toFixed(1)} KiB)`);
