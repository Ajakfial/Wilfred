# Backup and Sync

Wilfred can package its own state into a single portable archive, for local
backup/restore or for syncing between machines via a remote endpoint you
control. Implemented in `include/wilfred/sync/backup.hpp` /
`src/sync/backup.cpp`.

## What's included

A backup archive bundles:

| Entry name | Source | Always included? |
|---|---|---|
| `wilfred.yml` | The active config file | Yes |
| `snippets.yml` | Saved text-expansion snippets | Yes |
| `history.bin` | Search/selection history | Yes |
| `index/...` | The full index directory (snapshot + WAL), recursively | Only if `include_index` |

`include_index` defaults to `true` for `wilfred backup` unless `--no-index`
is passed, and is controlled by `sync.include_index` (default `true`) for
`sync-push`/`sync-pull`. Excluding the index makes for a much smaller
archive at the cost of needing a fresh scan (`wilfred index`) after a
restore.

## Archive format

A small custom binary container (magic `WILFBK1`), not a zip/tar — chosen to
keep the implementation dependency-free:

```
"WILFBK" (7 bytes, no trailing NUL)
uint32   entry_count
for each entry:
  uint32   name_length
  bytes    name (UTF-8, e.g. "wilfred.yml" or "index/snapshot.wilf")
  uint64   data_length
  uint32   crc32(data)
  bytes    data
```

All integers are little-endian. `unpack_backup_archive()` validates the
magic, then each entry's CRC32 individually — a single corrupted entry fails
the whole restore with a specific error (`"checksum mismatch for <name>"`)
rather than silently restoring partial/corrupt data. An entry count above
10,000 is rejected outright as a corrupt header (a sanity bound, not a real
limit any normal backup approaches).

## Local backup and restore

```bash
wilfred backup                  # write to the default backup path
wilfred backup ~/wilfred.wilfbk # write to an explicit path
wilfred backup --no-index       # skip the (large) index directory

wilfred restore ~/wilfred.wilfbk
```

Default path: `<data dir>/backups/wilfred.wilfbk` (see
[configuration.md](configuration.md#file-location) for the data directory;
`sync::default_backup_path()` creates the `backups/` folder if needed).

**Restoring overwrites your current config, snippets, history, and (if
present in the archive) index in place** — there's no merge step. Take a
fresh backup of your current state first if you might want it back.

## Remote sync

For syncing the same setup across machines, set:

```yaml
sync:
  enabled: true
  url: "https://your-server.example/wilfred-backup"
  token: "a-shared-secret"
  interval_seconds: 3600   # optional: auto-sync hourly from the daemon
  include_index: true
```

Then:

```bash
wilfred sync-push   # PUT the archive to sync.url
wilfred sync-pull    # GET the archive from sync.url and restore it
```

or via the HTTP API's `/sync/push` / `/sync/pull` (see
[ipc-and-api.md](ipc-and-api.md)), or automatically every
`interval_seconds` from within a running daemon if non-zero.

### Transport

`sync_push()`/`sync_pull()` use plain `HTTP PUT`/`GET` against `sync.url`
(`include/wilfred/ipc/http.hpp`'s `http_put()`/`http_get()`), sending
`sync.token` as an `X-Wilfred-Token` header. **Wilfred does not implement
its own transport encryption** — if `sync.url` is `http://`, the archive
(which contains your file index and history) travels in the clear. Use an
`https://` endpoint for anything beyond a trusted local network, and treat
`sync.token` like any other bearer credential.

You are responsible for running whatever server sits at `sync.url` — it
just needs to accept a `PUT` with the raw archive bytes as the body and
serve the same bytes back on `GET` (with the token, if you choose to check
it server-side too). A directory index served over WebDAV, a small custom
endpoint, or object storage with presigned URLs would all work.

## Choosing what to sync

If you're syncing between machines that will each maintain their *own*
local file index (the common case — machine A's files aren't the same as
machine B's), set `include_index: false`: sync config, snippets, and
history (the parts that represent *your preferences and habits*, which are
meaningfully shared across machines), and let each machine build its own
index locally with `wilfred index`. Only sync the index itself if you
specifically want one machine's index available on another (e.g. restoring
onto the same machine after a reinstall).
