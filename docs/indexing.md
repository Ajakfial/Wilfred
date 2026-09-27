# Indexing Internals

Wilfred keeps a persistent, incrementally-updated index of every file,
folder, and application it's configured to search. This document covers the
on-disk format, how it stays in sync with the filesystem, and how recovery
works after an unclean shutdown.

## On-disk layout

The index lives in the platform data directory (see
[configuration.md](configuration.md#file-location)) as two files:

| File | Purpose |
|---|---|
| `snapshot.wilf` | A full, compact serialization of the current index (`IndexStore::save`/`load`) |
| `journal.wal` | A write-ahead log of changes made since the last snapshot |

This is a classic snapshot + WAL design: mutations are cheap (append one
small record to the WAL) and durable immediately, while the expensive full
serialization only happens periodically.

### `IndexRecord` — what's stored per entry

(`include/wilfred/index/record.hpp`)

```cpp
struct IndexRecord {
  uint32_t id, name_id, path_id, parent_id, ext_id;  // interned string IDs
  uint64_t size;
  int64_t  ctime, mtime, atime;
  RecordFlags flags;   // Hidden | System | Symlink | Executable | Directory
                       // | Application | ReadOnly | MountedVolume (bitmask)
  uint16_t volume_id;
  FileKind kind;        // File, Directory, Application, Executable, Document,
                       // Image, Video, Audio, Archive, Source, Config,
                       // Shortcut, BrowserData, Unknown
  uint32_t mode;
};
```

Names, paths, parent paths, and extensions are stored once in a shared
`StringPool` (`include/wilfred/index/string_pool.hpp`) and referenced by
integer ID — this is what keeps memory/disk usage low even with hundreds of
thousands of records, since long, repeated path prefixes aren't duplicated
per file.

### `IndexStore` — the in-memory structure

`include/wilfred/index/store.hpp` holds:

* `records_` — the flat vector of `IndexRecord`s, plus a `live_` bitmap
  (deleted records are tombstoned, not immediately compacted out of the
  vector — see [Compaction](#compaction-and-checkpoints)).
* `path_to_id_` — path → record ID lookup.
* `token_postings_` — inverted index from name token → record IDs, for
  fast fuzzy/substring candidate lookup.
* `tri_postings_` — a trigram index (`trigrams()`, `index/tokenizer.hpp`)
  used for fuzzy-matching candidate generation on partial/misspelled input.
* `ext_postings_` — extension → record IDs, for fast `ext:`/`*.ext` filter
  lookups.
* `sorted_by_name_` — a name-sorted ordering used for prefix search.
* `extra_tokens_` — content tokens per record ID, populated when content
  indexing is enabled (see [Content indexing](#content-indexing) below).

All of this is guarded by a single `std::recursive_mutex` per `IndexStore`,
since search queries and the walker/watcher can run on different threads
concurrently.

## Building the index

### The walker

`fs::walk_tree()` (`include/wilfred/fs/walker.hpp`) recursively visits
`index.paths` (or platform defaults when empty), calling `stat_path()` on
each entry (`fs::classify.hpp`) and invoking a callback with a `WalkEntry`
(path + `FileStat`). Along the way it:

* Skips names in `index.exclude` and paths matching `index.exclude_globs`.
* Skips hidden entries unless `index.index_hidden`, and system entries
  unless `index.index_system`.
* Skips symlinks unless `index.follow_symlinks` (default off, to avoid
  cycles).
* Applies `index.extensions.include`/`exclude`.
* Respects `index.max_file_size_bytes` (`0` = unlimited).
* Can be cancelled mid-walk via the `std::atomic<bool>*` passed in (used for
  clean shutdown and for `IndexEngine::cancel()`).
* Reports `WalkStats` (visited/skipped/error counts).

`path_is_excluded()` and `extension_allowed()` in `config/config.hpp` are the
shared predicates used both by the walker and (for excludes) by the
watcher's event filter, so a manual `wilfred index` and a live filesystem
event are excluded consistently.

### Application discovery

Applications are indexed separately from the general file walk:
`apps::index_applications()` (`include/wilfred/apps/discovery.hpp`) calls
`discover_applications()`, which is platform-specific — Start Menu shortcuts
and registered `.exe`s on Windows, `.app` bundles on macOS, `.desktop`
entries on Linux — and inserts each as an `IndexRecord` with
`kind = FileKind::Application` and the `Application` flag set, plus any
`keywords` from the platform metadata folded into the searchable tokens.

### Content indexing

If `index.content_indexing` is enabled, eligible files (text/source/config,
decided by `content_indexable()` and `looks_like_text_extension()` in
`include/wilfred/search/content.hpp`) have up to `content_max_bytes` read
and tokenized into up to `content_max_tokens` tokens
(`extract_content_tokens`), stored via `IndexStore::add_content_tokens()`.
This is what powers the `content:`/`intext:` filter clause and the
`content_hit` ranking signal (see [query-language.md](query-language.md) and
[ranking.md](ranking.md)) — binary files are detected and skipped
(`looks_binary()`).

### Writes always go through the WAL

Every mutation — `IndexEngine::upsert_file()`, `remove_path()`,
`rename_path()` — appends a `WalEntry` (`WalOp::Upsert` / `Delete` /
`Rename`) to `journal.wal` via `WriteAheadLog::append_*()` *before* (or as
part of) updating the in-memory `IndexStore`, so a crash between the two
never loses a change that was already durably logged.

## Staying in sync: the filesystem watcher

`FsWatcher` (`include/wilfred/fs/watcher.hpp`) uses the native OS
notification API per platform:

| Platform | Mechanism |
|---|---|
| Windows | `ReadDirectoryChangesW` |
| macOS | `FSEvents` |
| Linux | `inotify` |

Events (`FsEventKind::Created/Deleted/Modified/Renamed/Overflow`) are
debounced by `index.debounce_fs_ms` (default 80 ms) before being applied, so
a burst of writes to the same file (e.g. a build system rewriting an object
file repeatedly) doesn't cause redundant re-indexing. An `Overflow` event
(the OS notification queue dropped events — this can happen under very
heavy filesystem activity) triggers a full rescan of the affected root as a
correctness fallback.

`index.rescan_interval_seconds`, if non-zero, additionally forces a full
rescan on a timer regardless of watcher activity — useful as a
belt-and-suspenders safety net, or on filesystems/mounts where native
watching isn't reliable (e.g. some network shares).

## Compaction and checkpoints

* **Checkpoint** (`IndexEngine::checkpoint()`): if there are pending changes
  (`dirty_ != 0`) or a non-empty WAL, writes a fresh `snapshot.wilf` via
  `persist_snapshot()`, then appends a `WalOp::Checkpoint` marker and
  truncates the WAL. Happens automatically after `index.persist_every_records`
  changed records, and always on clean shutdown (`IndexEngine::close()`).
* **Compact** (`IndexEngine::compact()`): additionally calls
  `store_.rebuild_secondary()` to rebuild the derived indexes (postings,
  trigrams, sorted-by-name) from scratch — this is where tombstoned
  (deleted) records are actually dropped rather than just marked dead.
  Triggered when the WAL grows past `index.wal_compact_bytes` (default 8
  MiB), or manually via `wilfred index` performing a full rescan.

## Recovery after an unclean shutdown

On `IndexEngine::open()`:

1. If `snapshot.wilf` exists, it's loaded (`IndexStore::load`). A corrupt or
   unreadable snapshot is treated as "start from empty" rather than a fatal
   error — recovery then relies entirely on the WAL.
2. `WriteAheadLog::replay()` reads every entry from `journal.wal`.
3. If a snapshot was loaded successfully, only WAL entries **after the last
   `Checkpoint` marker** are replayed (`recover()`) — everything before that
   marker is already reflected in the snapshot. If the snapshot failed to
   load, every WAL entry is replayed from the beginning.
4. Each replayed entry is applied to `IndexStore` (`upsert`/`remove_path`/
   `rename_path`); a malformed individual entry is logged and skipped rather
   than aborting the whole recovery.

This means the worst case for data loss is the interval between the last
completed WAL `append_*()` call and a crash — a single in-flight write, not
the whole index.

## Statistics

`IndexEngine::stats()` returns an `IndexStats` snapshot — `files`, `dirs`,
`apps`, `errors`, `bytes`, whether a scan is `scanning`, and
`last_scan_seconds` — which is what `wilfred status` prints, and can be
subscribed to live via `IndexEngine::set_progress()` for UI progress
reporting during a scan.

## Volumes

`fs::list_volumes()` (`include/wilfred/fs/volumes.hpp`) enumerates mounted
drives/volumes (with `removable`/`network`/`ready` flags), used both to
decide platform default index roots and to power the `disk`/`disku` minis
(see [query-language.md](query-language.md)).
