// Copies the plugin gallery (plugins/registry.json + dist zips) into
// www/public/plugins/ so the static site serves them. CI runs the same
// step in website.yml; `npm run prebuild` runs this automatically.
import { copyFileSync, cpSync, existsSync, mkdirSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const www = join(dirname(fileURLToPath(import.meta.url)), "..");
const plugins = join(www, "..", "plugins");
const dest = join(www, "public", "plugins");

mkdirSync(dest, { recursive: true });

const reg = join(plugins, "registry.json");
if (existsSync(reg)) copyFileSync(reg, join(dest, "registry.json"));

const dist = join(plugins, "dist");
if (existsSync(dist)) {
  for (const f of readdirSync(dist)) {
    if (f.endsWith(".zip")) copyFileSync(join(dist, f), join(dest, f));
  }
}

console.log(`staged plugin gallery into ${dest}`);
