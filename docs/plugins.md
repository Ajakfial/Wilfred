# Writing Plugins

Wilfred supports two kinds of end-user-installable plugins, hosted by
`wilfred::PluginHost` (`include/wilfred/plugin/host.hpp`,
`src/plugin/host.cpp`):

* **Native** — a `.dll` (Windows), `.dylib`/`.so` (macOS), or `.so` (Linux)
  loaded directly into the Wilfred process via `LoadLibrary`/`dlopen`,
  implementing a small C ABI (`include/wilfred/plugin/abi.hpp`).
* **stdio** — an external executable, run as a subprocess once per query
  (and once per action execution), communicating over a single JSON
  request/response on stdin/stdout.

Both kinds respond to search queries with `SearchResult`-shaped JSON and can
optionally handle a follow-up "execute this action" call.

This is a separate extension mechanism from `providers::SearchProvider`
(`include/wilfred/providers/provider.hpp`), which is a compile-time,
in-process C++ interface for embedding another search backend directly into
a Wilfred fork — see [architecture.md](architecture.md#extensibility-plugins-and-providers).
Use a plugin if you want end users to install your extension without
recompiling Wilfred; use a `SearchProvider` if you're maintaining your own
build of Wilfred.

## Discovery and the manifest

Plugins are discovered from `plugins.directories` in the config (default:
`<config dir>/plugins` and `<data dir>/plugins` — see
[configuration.md](configuration.md#file-location)). `plugins.enabled` must
be `true` (the default) and `search.plugins` must be `true` for queries to
reach them.

Under a plugin directory, Wilfred recognizes:

* A **subfolder** containing `plugin.yml` or `plugin.json` — the manifest
  described below, with `path`/`command` resolved relative to that folder
  if not absolute.
* A **bare manifest file** (`plugin.yml` or `plugin.json`) directly in the
  plugin directory.
* A **bare native library** (`.dll`/`.so`/`.dylib` matching the current
  platform) directly in the plugin directory — no manifest needed; its
  filename (without extension) becomes the plugin ID, unless the library
  itself reports a different ID (see [Native plugins](#native-plugins)
  below).

### Manifest fields

```yaml
# plugin.yml
id: my-plugin        # defaults to the filename stem if omitted
kind: stdio           # "native" or "stdio" — inferred from whether
                     # `command` is set if omitted
path: ./my-plugin.so  # for kind: native — path to the library
command: ./run.sh     # for kind: stdio — the executable to spawn
args: ["--flag"]      # for kind: stdio — extra argv entries
timeout_ms: 400        # per-query timeout; falls back to plugins.timeout_ms
enabled: true
version: 1.0.0        # registry/approval display; changing sha256/permissions re-pends
sha256: <hex>         # pinned artifact hash (registry installs write this)
permissions: [network] # free-form labels shown on the approval card
origin: https://example.com/my-plugin  # where it came from
```

JSON manifests use the same field names. A manifest file that fails to
parse as YAML is also given a lenient JSON-ish fallback parse for `id`,
`kind`, `path`, `command`, and `timeout_ms`.

## Native plugins

Export these C functions (see `include/wilfred/plugin/abi.hpp`):

```c
#define WILFRED_PLUGIN_ABI 1

// Required: return WILFRED_PLUGIN_ABI (1). If present and it doesn't
// match, the plugin is rejected and unloaded.
int wilfred_plugin_abi(void);

// Required: handle a query, return a JSON response (see below).
// The returned pointer must remain valid until the next call into the
// plugin (Wilfred does not take ownership or free it).
const char* wilfred_plugin_query(const char* json_request);

// Optional: handle an action execution (see "Execute" below).
const char* wilfred_plugin_exec(const char* json_request);

// Optional: override the plugin's ID (otherwise the manifest's `id`,
// or the library's filename stem, is used).
const char* wilfred_plugin_id(void);
```

If `wilfred_plugin_abi` is present and returns something other than `1`, or
`wilfred_plugin_query` is missing entirely, the library is unloaded and
skipped with a warning — both are logged, so check Wilfred's log
(`logging.file`) if a native plugin isn't showing up.

## stdio plugins

`command` (or `path`, if `command` is empty) is resolved to an executable —
absolute, relative to the plugin's directory, or (as a last resort) looked
up on `PATH` — and spawned fresh for **each** query and **each** action
execution (`run_stdio_once`, not a persistent long-running process). The
request JSON is written to the child's stdin; the full stdout is read back
as the response, bounded by `timeout_ms` (the manifest's own value, or
`plugins.timeout_ms` if unset) — a process that doesn't respond in time is
killed and the query/action is skipped for that plugin.

This makes stdio plugins simple to write in any language, at the cost of
one process spawn per keystroke-triggered search — keep stdio plugins fast
to start, or prefer a native plugin for something performance-sensitive.

## Request/response protocol (both kinds)

### Query

Request (sent to `wilfred_plugin_query` or a stdio plugin's stdin):

```json
{"op": "query", "q": "<query text>", "limit": 40}
```

Response — a JSON object with a `results` array:

```json
{
  "results": [
    {
      "title": "Result title",
      "subtitle": "Optional subtitle",
      "path": "optional/path/or/identifier",
      "payload": "optional extra data (defaults to path if omitted)",
      "score": 500,
      "kind": "optional label shown as the result's kind (defaults to \"plugin\")",
      "action": "open | copy | web | reveal | none | (anything else = generic plugin action)",
      "actions": [
        {"id": "custom-action-id", "label": "Shown in the UI"}
      ]
    }
  ]
}
```

Notes:

* At least one of `title` or `path` must be present, or the result is
  dropped; an empty `title` falls back to the filename portion of `path`.
* `score` defaults to `500` if omitted — compare against the built-in
  ranking scale in [ranking.md](ranking.md) if you want your results to
  land relative to file-search results (built-in exact-name matches score
  ~1200).
* `action` maps to `ResultAction`: `"copy"` → `Copy`, `"web"`/`"websearch"`
  → `WebSearch`, `"reveal"` → `Reveal`, `"none"` → `None`; anything else
  (including omitted) → `Plugin`, meaning selecting it calls back into your
  plugin's exec handler rather than a built-in action.
* `actions` lets a single result expose multiple named actions (e.g. an
  "Open" primary action plus a secondary "Copy path") — each needs an `id`
  (or `action` as a fallback key) and a `label`.
* Every result from a plugin is tagged `category: "plugin"` and
  `plugin_id: <your manifest's id>` automatically — you don't set these
  yourself.
* Results from every enabled plugin are concatenated (not individually
  ranked against file-search results by the main `rank_record()` pipeline —
  your `score` field is what controls relative ordering among all results),
  then the combined list is truncated to the requested `limit`.
* Any exception/crash from a native call, or an empty/timed-out stdio
  response, is caught and logged; that plugin simply contributes no results
  for that query rather than failing the whole search.

### Execute

Sent when the user selects a result whose `action` was `Plugin` (i.e. not
one of the built-in action strings above), routed to whichever plugin
produced it (`SearchResult::plugin_id`):

```json
{
  "op": "exec",
  "action": "<the action id the user chose, or empty for the default>",
  "path": "<the result's path>",
  "payload": "<the result's payload>",
  "title": "<the result's title>"
}
```

Sent to `wilfred_plugin_exec` (native) or spawned as a fresh process
(stdio) — the response body is not currently interpreted by Wilfred beyond
confirming the call didn't throw/fail; do any user-visible work (opening a
window, printing something) as a side effect of handling this call.

## Timeouts and safety

* Each plugin gets its own `timeout_ms` — a slow plugin only delays how long
  *its own* results take to appear, not the whole search (though the
  overlay does wait for all sources before rendering — see
  [architecture.md](architecture.md#the-life-of-a-query)).
* Trust-on-first-use (default `plugins.require_approval: true`): a plugin id
  unseen in `<data>/plugins/trust.json` — or seen with a different sha256 or
  permission set — shows as a `plugins` approval card and contributes no
  results until `wilfred plugin approve <id>` (or the overlay Approve action)
  records its fingerprint. `wilfred plugin pending` lists them;
  `wilfred plugin revoke <id>` un-approves. Set `require_approval: false` to
  restore load-everything behavior.
* Native plugins run **in-process** with full access to the same address
  space as Wilfred — there is no sandboxing. Only install native plugins you
  trust; see [SECURITY.md](../SECURITY.md) for how this is scoped in
  Wilfred's security policy. Prefer the registry (`plugins.registry` +
  `wilfred plugin install`, sha256-verified) over hand-dropped binaries.
* stdio plugins run as a separate OS process per call, which is a real (if
  modest) isolation boundary compared to native plugins, but still run with
  the same user privileges as Wilfred itself.

## Registry and trust CLI

With `plugins.registry` set to a JSON index (`{"plugins": [{id, version,
kind, url, sha256, description, permissions}]}`):

```
wilfred plugin list              # registry entries (or installed plugins)
wilfred plugin pending           # ids awaiting trust approval
wilfred plugin install <id>      # download, sha256-verify, write plugin.yml
wilfred plugin approve <id>      # trust this fingerprint (--all for all)
wilfred plugin revoke <id>       # un-trust; the plugin is skipped again
```

### Per-platform artifacts (native plugins)

A single `url` can't serve three OSes, so entries may carry an `artifacts`
map instead (or in addition, as a fallback):

```json
{"plugins": [{
  "id": "dice", "version": "1.0.0", "kind": "native",
  "description": "Dice roller",
  "permissions": [],
  "artifacts": {
    "windows": {"url": "https://…/dice-1.0.0-windows.zip", "sha256": "<hex>"},
    "macos":   {"url": "https://…/dice-1.0.0-macos.zip", "sha256": "<hex>"},
    "linux":   {"url": "https://…/dice-1.0.0-linux.zip", "sha256": "<hex>"}
  }
}]}
```

`wilfred plugin install` picks the current OS (`windows`/`macos`/`linux`)
and falls back to the plain `url`/`sha256` when no artifact matches;
`wilfred plugin list` marks available platforms (`*` = this machine).

### Official gallery

The `plugins/` directory in the repo is the official index source:
flagship sources plus community submissions via PR (one `plugins/<id>/`
directory per PR — see [plugins/README.md](../plugins/README.md) for the
exact format). Merging to `main` runs `.github/workflows/plugins.yml`,
which compiles, smoke-tests, and publishes per-platform zips +
`plugins/registry.json` automatically; the website's Plugins page serves
that index at `https://ajakfial.github.io/Wilfred/plugins/registry.json`.
Set it once and install by id:

```
wilfred config-set plugins.registry https://ajakfial.github.io/Wilfred/plugins/registry.json
wilfred plugin install dice
wilfred plugin approve dice
```

Installs support single-file artifacts and `.zip` archives (extracted
with the updater's extractor). The overlay `plugins` mini mirrors all of
this: pending approvals appear as Approve cards, and `plugins approve
<id>` approves one id. See [cli.md](cli.md) for exit codes.

## Example: a minimal stdio plugin (Python)

```python
#!/usr/bin/env python3
import sys, json

req = json.loads(sys.stdin.read())
if req.get("op") == "query":
    q = req.get("q", "")
    results = []
    if q:
        results.append({
            "title": f"Echo: {q}",
            "subtitle": "from my-plugin",
            "path": q,
            "score": 300,
            "action": "copy",
        })
    print(json.dumps({"results": results}))
elif req.get("op") == "exec":
    pass  # do something with req["path"] / req["action"]
```

```yaml
# plugins/my-plugin/plugin.yml
id: my-plugin
kind: stdio
command: python3
args: ["my_plugin.py"]
timeout_ms: 500
```
