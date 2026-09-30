# IPC and the Local HTTP API

Wilfred exposes two local-only control surfaces for talking to an already
running `wilfred daemon`. Both are designed for **local, trusted use** —
scripting, the CLI, or a local integration — not as a network service; see
[SECURITY.md](../SECURITY.md) for the threat model.

## The IPC socket (used by the CLI)

`wilfred search`, `wilfred launch`, `wilfred status`, `wilfred backup`, and
`wilfred restore` talk to a running daemon over a local, always-on control
channel (`include/wilfred/ipc/server.hpp` /
`include/wilfred/ipc/protocol.hpp`) rather than opening a second copy of the
index. If no daemon is running, these commands fall back to operating on
the index directly in-process.

### Endpoint

| Platform | Endpoint |
|---|---|
| Windows | Named pipe: `\\.\pipe\wilfred` |
| macOS / Linux | Unix domain socket: `<data dir>/wilfred.sock` (see [configuration.md](configuration.md#file-location) for the data directory) |

(`core::ipc_endpoint()`, `src/core/paths.cpp`)

### Wire format

One request per connection: write a single line, get a single line back,
connection closes. A request is either a compact shorthand command or a
JSON object; requests and responses are UTF-8 text.

**Shorthand forms** (`decode_request()`, `include/wilfred/ipc/protocol.hpp`):

```
SEARCH<TAB>firefox
LAUNCH<TAB>/path/to/thing
STATUS
BACKUP<TAB>/optional/dest/path
RESTORE<TAB>/optional/src/path
```

**JSON form** — any subset of these fields:

```json
{"cmd": "search", "q": "firefox", "limit": 40}
```

Recognized fields: `cmd`/`command`, `q`/`query`, `path`, `action`, `token`,
`n`/`limit` (defaults to 40 if omitted or unparsable). `cmd` is required;
everything else is optional and command-specific (e.g. `search` uses `q`
and `limit`; `launch` uses `q` and, optionally, `action` to pick a
non-default `ResultActionItem`).

### Response

Always JSON:

```json
{
  "ok": true,
  "error": "",
  "text": "",
  "results": [
    {
      "id": 123,
      "score": 1200,
      "title": "firefox",
      "path": "/usr/bin/firefox",
      "kind": "executable",
      "subtitle": "",
      "action": "open",
      "category": "",
      "payload": "/usr/bin/firefox",
      "plugin": "",
      "meter": 62,
      "actions": [
        {"id": "reveal", "label": "Show in file manager"}
      ]
    }
  ]
}
```

`action` is one of: `open`, `reveal`, `copy`, `web`, `calc`, `convert`,
`none`, `habit`, `mini`, `expand`, `plugin` (`ipc_action_name()` maps
`ResultAction` to these strings). Optional `meter` is an integer 0–100 for
live cards (`speedtest`, RAM, disk). `ok: false` responses carry a
human-readable `error` string and an empty `results` array.

## The local HTTP API

Optional, off by default (`api.enabled: false` — see
[configuration.md](configuration.md#api--optional-local-http-api)). When
enabled, binds to `api.bind:api.port` (default `127.0.0.1:17380`) and speaks
plain HTTP + JSON.

### Authentication

Send the token either as a custom header or as a standard bearer token:

```
X-Wilfred-Token: <api.token>
```
```
Authorization: Bearer <api.token>
```

If `api.token` is set, every non-`OPTIONS` request must present a matching
token or gets rejected. If `api.token` is **empty**, non-local (non-loopback)
requests are rejected outright with `{"ok":false,"error":"token required
for remote clients"}` — set a token before relying on the API from anything
other than `127.0.0.1`/`::1`. `OPTIONS` requests always succeed with an
empty body (CORS preflight support).

### Endpoints

All paths also accept an optional `/v1/` prefix (`/v1/search` and `/search`
are equivalent). Query parameters and a JSON request body are both
accepted for the same fields — query params win if both are present.

| Method | Path | Purpose |
|---|---|---|
| `GET`/`POST` | `/search` (alias `/query`) | Run a search |
| `GET` | `/status` | Index/plugin/snippet counts |
| `POST` | `/show` | Trigger the same action as pressing the global hotkey |
| `POST` | `/index` | Force a full index scan (like `wilfred index`) |
| `POST` | `/launch` (aliases `/open`, `/exec`) | Execute a result's action |
| `GET` | `/backup` | Download a fresh backup archive |
| `PUT` | `/backup` | Upload and restore a backup archive |
| `POST` | `/sync/push` | Push a backup to `sync.url` |
| `POST` | `/sync/pull` | Pull and restore a backup from `sync.url` |
| `POST` | `/snippets` | Create/update a snippet |
| `GET` | `/snippets` | List snippets |

#### `GET/POST /search`

Params/body fields: `q` (or `query`), `limit` (default 40).

```
GET /search?q=firefox&limit=10
```

Response body: the same `IpcResponse` JSON shape as the IPC protocol above
(`{"ok":true,"error":"","text":"","results":[...]}`).

#### `GET /status`

```json
{"ok": true, "files": 128034, "dirs": 9021, "apps": 214, "plugins": 2, "snippets": 5}
```

#### `POST /show`

No body needed. Toggles/shows the overlay the same way the global hotkey
does. `{"ok": true}`.

#### `POST /index`

No body needed. Starts a full rescan asynchronously (does not block until
the scan completes). `{"ok": true}`.

#### `POST /launch` (`/open`, `/exec`)

Two modes:

* **By query** — provide `q`; the top search result for that query is
  executed:
  ```json
  {"q": "firefox"}
  ```
* **By explicit result** — provide `path` (and optionally `payload`,
  `title`, `plugin`/`plugin_id`, `category`) to execute a specific result
  directly without re-running search — this is how a client that already
  has a `SearchResult` from `/search` re-executes it:
  ```json
  {"path": "/usr/bin/firefox", "action": "open"}
  ```

`action` (optional) selects a non-default `ResultActionItem` id (e.g. a
plugin result's secondary action). Response: `{"ok": true}` or, on failure,
HTTP 400/404 with `{"ok": false, "error": "..."}`.

#### `GET /backup`

Streams a freshly created backup archive as
`application/octet-stream` (see [sync-and-backup.md](sync-and-backup.md)
for the format). Errors return HTTP 500.

#### `PUT /backup`

Body is a backup archive blob (as produced by `GET /backup` or
`wilfred backup`); it's written to a temp file and restored immediately.
`{"ok": true}` on success, HTTP 400 with an error message on a corrupt or
unreadable archive.

#### `POST /sync/push` / `POST /sync/pull`

No body needed — uses `sync.url`/`sync.token` from the config, same as
`wilfred sync-push`/`wilfred sync-pull`. `{"ok": true}` or HTTP 400 with an
error.

#### `POST /snippets`

Body fields: `trigger` (or `name`) — required; `id` (defaults to `trigger`);
`title`; `body` (or `text`); `kind`.

```json
{"trigger": ";addr", "title": "Home address", "body": "123 Main St"}
```

`{"ok": true}`, or HTTP 400 with `{"ok": false, "error": "trigger
required"}` if `trigger` is missing.

#### `GET /snippets`

```json
{"ok": true, "items": [{"id": ";addr", "trigger": ";addr", "title": "Home address"}]}
```

### Unknown paths

Any other path/method returns HTTP 404 with
`{"ok": false, "error": "not found"}`.

## Choosing between them

* Use the **CLI** (which uses the IPC socket internally) for scripting on
  the same machine — it's already installed, handles the platform-specific
  endpoint for you, and needs no configuration.
* Use the **HTTP API** if you're building a separate tool/integration (a
  browser extension, a status-bar widget, a different language) that would
  rather speak HTTP+JSON than the CLI's process-spawn model, and you're
  comfortable managing a token. Leave it disabled if you don't need it —
  it's off by default for a reason.
