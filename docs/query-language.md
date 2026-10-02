# Query Language

Every character typed into the overlay (or passed to `wilfred search`/
`wilfred launch`) goes through `classify_query()`
(`include/wilfred/query/classify.hpp`, implemented in
`src/query/classify.cpp`) first. Classification is a fast, ordered set of
heuristics — the first one that matches wins — followed by
`QueryInterpreter::interpret()` actually producing results. This document
describes each recognized form, in the order they're checked.

## 1. Web search prefixes

A query starting with any of these is sent straight to
`browser.search_template` (default Google) instead of the index:

| Prefix | Example |
|---|---|
| `?` | `?rust vs c++` |
| `g ` | `g rust vs c++` |
| `web ` | `web rust vs c++` |
| `search ` | `search rust vs c++` |

## 2. URLs

`looks_like_url()` recognizes:

* Explicit schemes: `http://`, `https://`, `file://`
* `www.`-prefixed hosts
* A bare host with a known TLD (`.com`, `.org`, `.net`, `.io`, `.dev`, `.app`,
  and a few dozen more — see `classify.cpp`) and no path characters that
  would make it look like a Windows path (a literal `\` disqualifies it)

Matched queries are opened directly in the default browser
(`treat_urls_as_open`, on by default) rather than fuzzy-matched against the
index.

## 3. Calculator expressions

`looks_like_math()` requires at least one digit, plus either an operator
(`+ - * / ^ % ( )`) or a recognized function name (`sin`, `cos`, `tan`,
`sqrt`, `log`, `ln`, `abs`, `pow`, `ceil`, `floor`, `exp`, `pi`), or a
`" to "` / `" in "` / `convert ` phrase for unit conversion — and the
expression must actually evaluate successfully
(`math::evaluate_math()` / `convert_metric()`, see below). Successful
dev-utility commands (`convert_devutil()`) also count, even with no
digits (`uuid`, `base64 hi`). A bare number like `42` is deliberately
**not** treated as math (it would just be a search).

```
25 * 42
sqrt(2)
(3 + 4) * 2
10 km in miles
convert 5 gb to mb
100 usd to eur
$50 to gbp
10 dollars in euros
#ff5500
rgb(255, 85, 0) to hsl
3pm est to pst
now in tokyo
today + 7 days
2024-01-31 + 1 month
in 3 weeks
unix 1700000000
uuid
guid
base64 hello
base64d aGVsbG8=
sha256 abc
lorem 12
json {"a":1,"b":[true,null]}
json minify {"a": 1}
```

Unit conversion covers SI and common US customary units (length, mass,
volume, area, temperature, speed, energy, pressure, time, data, force,
power, angle), including cooking abbreviations (`tbsp`/`tbs`/`tbl`,
`tsp`, `cup`, `floz`, `dl`, `cl`, `dash`, `pinch`, `g`/`gm`, `mg`,
`mcg`, `kg`, `oz`, `gr`/`grain`, `dr`/`dram`). Volume↔mass converts via
density, assuming water by default (`1 tbsp to g`, `1 cup to oz`), with an
optional trailing ingredient for common foods (`1 tbsp sugar to g`,
`1 cup flour to g`, `100 g honey to tbsp`). Currency pairs use ISO codes or names (`usd`/`dollar`,
`eur`/`euro`, `gbp`/`pound`, …) and optional leading symbols (`$`, `€`,
`£`, `¥`, `₹`). Rates are fetched from Frankfurter when the network is
reachable, with a built-in approximate USD table used offline. Enter copies
the converted amount.

Color conversion accepts `#RGB` / `#RRGGBB` / `#RRGGBBAA`, `rgb()`/`rgba()`,
`hsl()`/`hsla()`, `hsv()`, CSS color names with `to hex`/`rgb`/`hsl`, and
shows hex, RGB, and HSL (enter copies the selected form).

Date and time conversion covers ISO dates, `today`/`tomorrow`/`yesterday`,
`+`/`-` duration math (`days`, `weeks`, `months`, `hours`, …),
`in 3 days` / `5 days ago`, `days until 2026-12-25`, unix timestamps, and
timezone conversion (`3pm est to utc`, `now in tokyo`, `12:00 utc to utc+1`).
Offsets for named cities apply a simplified DST rule (US / EU / AU / NZ);
abbreviations like `EST`/`PDT` are fixed offsets. Enter copies the result.

Dev utilities live next to the calculator (`src/math/dev.cpp` /
`convert_devutil()`). `uuid`/`guid` generates a UUID v4. `base64` /
`base64d` encode or decode (clipboard is used if you type the command
with no payload). `sha256 <text>` hashes the rest of the query. `lorem`
/`lorem 40` emits placeholder words. `json {…}` pretty-prints; `json
minify {…}` compact-prints. Enter copies the result.

Number bases and bits: `hex 255`, `dec 0xff`, `bin 10`, `oct 8`,
`base 16 255`, bare `0xff` / `0b1010` / `0o17` (all show
`dec = hex = bin = oct`), `255 to hex`, `bit and/or/xor <a> <b>`,
`bit not <a>`, `bit shl/shr <a> <n>`. URL codec: `urlencode <text>`,
`urldecode <text>` (`url encode/decode` also work). JWT: `jwt <token>`
decodes header/payload without verifying the signature. Regex tester:
`regex <pattern> <text>` (`regexi` ignores case, `/pat/i` flags work,
quoted patterns allow spaces).

The calculator is a safe recursive-descent parser (`src/math/expr.cpp`) —
there is no `eval`/scripting backdoor.

## 4. Command prefix

`>`, `cmd `, or `run ` prefixes classify the remainder as a `Command`
query, offered for direct execution rather than fuzzy search.

```
> notepad
cmd ipconfig /all
```

## 5. Minis — system info cards

A single recognized keyword (optionally followed by an argument) returns a
synthetic result card instead of searching the index
(`include/wilfred/search/minis.hpp`, `src/search/minis.cpp`):

| Query | Aliases | Shows |
|---|---|---|
| `weather [place]` | `wttr`, `forecast` | Local or named-city forecast |
| `time` | `date`, `clock`, `now` | Current time and date |
| `disk` | `disks`, `storage`, `drives` | Per-volume free/used space |
| `disku` | `diskusage`, `disk-usage` | Aggregate disk usage summary |
| `ram` | `memory`, `mem` | Physical memory used/free |
| `cpu` | `processor` | Current CPU load |
| `process <name>` | `proc`, `ps`, `top`, `processes` | Matching live processes with CPU/RAM |
| `screenshot [mode]` | `screenshots`, `screencap`, `screencapture`, `printscreen`, `print screen`, `screen capture`, `screen shot`, `capture screen` | Capture the screen (`fullscreen`, `window`, or `region`); bare `screenshot` lists all three. Saves to `Pictures/Wilfred` — fullscreen is silent, window captures the active window (Windows) or picks one, region drag-selects (Windows opens the Snipping Tool). Enter captures + opens, `Tab` offers capture + reveal / copy path |
| `battery` | `power` | Battery percentage and charge state |
| `hostname` | `host` | Machine hostname |
| `ip` | `ipaddress` | Local IP address |
| `uptime` | | System uptime |
| `user` | `whoami` | Current OS user |
| `clip` | `clipboard` | Current clipboard contents |
| `clips [query]` | `cliphist`, `pasteboard` | Searchable clipboard history (Tab offers Pin/Unpin; `clips clear` empties it) |
| `bm [query]` | `bookmarks`, `tabs`, `hist`, `history` | Browser bookmarks, recent history, and open tabs (`bm tabs` lists tabs only) |
| `os` | `systeminfo`, `sysinfo` | OS name/version |
| `cores` | `nproc`, `threads` | CPU core/thread count |
| `screen` | `resolution`, `display` | Display resolution |
| `swap` | `pagefile`, `vmem` | Swap/page-file usage |
| `speedtest` | `speed-test`, `netspeed`, `bandwidth`, `internetspeed` | Live download, upload, and ping |
| `help` | `minis`, `cmds`, `commands` | List of available minis |
| `macros` | `bangs` | List of configured macros |
| `emoji [name]` | `emojis`, `emote`, `emotes` | Emoji picker (enter copies the character) |
| `symbol [name]` | `symbols`, `glyph`, `glyphs` | Punctuation, math, and currency signs |
| `fx [amount from to]` | `currency`, `forex`, `ccy` | Currency conversion (`fx 100 usd to eur` or `fx 25 gbp jpy`); bare `fx` lists spot rates |
| `tz [zone]` | `timezone`, `worldclock` | World clock (`tz tokyo`) or zone convert (`tz 3pm est to pst`) |
| `color [value]` | `colour` | Color convert (`color #ff5500`); enter copies hex/rgb/hsl |
| `uuid` | `uuid4`, `guid`, `uuidv4` | Generate a UUID v4 (enter copies) |
| `base64 [text]` | `b64`, `encode64` | Base64-encode the argument or clipboard |
| `base64d [text]` | `b64d`, `decode64` | Base64-decode the argument or clipboard |
| `sha256 [text]` | `sha`, `hash` (with text) | SHA-256 of the argument or clipboard |
| `lorem [n]` | `ipsum`, `loremipsum` | Placeholder text (`n` words, default 30) |
| `json [value]` | `prettyjson`, `pretty {…}` | Pretty-print JSON; `json minify {…}` compact-prints |
| `hex/dec/bin/oct [value]` | `base` | Number-base convert (`hex 255`, `dec 0xff`, `0b1010`, `base 16 255`) |
| `bit <op> …` | `bits`, `bitwise` | Bit tools (`bit and 12 10`, `bit not 5`, `bit shl 1 4`) |
| `regex <pat> <text>` | `regexp`, `re`, `regexi` | Regex tester (enter copies match/groups or `no match`) |
| `urlencode/urldecode` | `url`, `encodeurl` | URL percent-encode/decode |
| `jwt <token>` | `jwtdecode` | Decode JWT header/payload (signature not verified) |
| `timer [duration]` | `timers`, `alarm`, `countdown` | Countdown (`timer 10m`, `timer 25`, `timer stop`) |
| `pomodoro [preset]` | `pomo` | Pomodoro (`pomodoro`, `pomodoro break`, `pomodoro 5`) |
| `stopwatch [cmd]` | `stop-watch`, `sw` | Stopwatch (`stopwatch start/stop/lap/reset`) |
| `note [text]` | `notes`, `memo` | Quick notes (`note buy milk`, `notes`, `note clear`) |
| `todo [task]` | `todos`, `task` | Todos (`todo ship it`, `todos`, `todo done 1`, `todo clear`) |
| `kill <pid\|name>` | `killall`, `pkill` | Process killer (enter terminates; `process` lists) |
| `media [action]` | `player`, `music`, `play`, `pause`, `next`, `prev`, `mute`, `volume` | Native media controls, nothing to install — Win: system media keys; macOS: system HID keys + Music/Spotify/VLC fallback; Linux: native MPRIS D-Bus (Spotify/VLC/Chrome/…) with optional `playerctl`, volume via built-in `wpctl` → `pactl` → `amixer` → `pamixer` |
| `ping <host>` | | Ping summary via the OS ping CLI |
| `dns <host>` | `nslookup`, `resolve` | DNS lookup via getaddrinfo |
| `myip` | `publicip`, `ip public` | Public IP (via api.ipify.org) + local IP |
| `large [n] [dir]` | `largefiles`, `bigfiles` | Largest files from the index |
| `dupes [dir]` | `dups`, `duplicates`, `dedupe` | Duplicate candidates (same size + hash) |
| `transcribe <file>` | `stt`, `transcription` | Speech-to-text for mp3/wav/m4a/mp4 via whisper (enter transcribes) |
| `workflow [name]` | `workflows`, `flow`, `run` | Multi-step workflows (`workflows` lists; `run <name>` runs) |
| `ql <name> <args>` | `quicklink`, `link` | Parameterized quicklinks (`{query}` `{1}` `{*}` `{clipboard}`) |
| `lock` | `lockscreen` | Lock the session (enter runs) |
| `sleep` | `suspend` | Sleep / suspend the machine |
| `shutdown` | `poweroff`, `halt`, `power off` | Power off |
| `restart` | `reboot` | Reboot |
| `logout` | `logoff`, `signout`, `log out` | Sign out of this session |
| `empty trash` | `emptyrecycle`, `empty bin` | Empty the recycle bin / trash |

`clips` also filters by type: `clips url`, `clips email`, `clips path`,
`clips code`, `clips ip` (plus an optional text query after the type).

Minis can be disabled entirely with `search.minis: false`. `weather`, `speedtest`,
and live FX rates make outbound HTTPS requests (a short-timeout WinHTTP/`curl`
fetch). Currency still works offline from an approximate table. There's no
dedicated flag for those requests today, so disable `search.minis` if you
need to suppress the weather mini. `speedtest` downloads and uploads a few
megabytes against Cloudflare to measure live throughput; type `speedtest again`
to rerun.

## 6. Macros — query templates

A macro expands the rest of the query into a URL template
(`include/wilfred/search/macros.hpp`). Two invocation forms:

```
!yt lofi hip hop        # bang form
yt:lofi hip hop         # colon form
yt lofi hip hop         # bare form (works for most macros)
```

The bang (`!`) or slash (`/`) prefix, or a `name:argument` colon form, always
counts as an explicit macro invocation. A few very short/common macro names
that would otherwise collide with normal words or file searches
(`w`, `so`, `mail`, `g`, `x`) only trigger via the bang/colon form, not the
bare form, so typing `g` alone still lets you search for something literally
named "g" instead of always jumping to Google.

Built-in macros (all templates support `{query}`, `{query_enc}`,
`{clipboard}`, `{clipboard_enc}` placeholders):

| Name(s) | Destination |
|---|---|
| `yt`, `youtube` | YouTube search |
| `g`, `google` | Google search |
| `bing` | Bing search |
| `ddg` | DuckDuckGo search |
| `gh`, `github` | GitHub code/repo search |
| `gist` | GitHub Gist search |
| `wiki`, `w` | Wikipedia search |
| `maps`, `gmaps` | Google Maps search |
| `so` | Stack Overflow search |
| `imdb` | IMDb search |
| `npm` | npm registry search |
| `pypi` | PyPI search |
| `crates` | crates.io search |
| `mdn` | MDN search |
| `define` | Google "define" search |
| `translate` | Google Translate |
| `mail` | Gmail search |
| `amazon` | Amazon search |
| `reddit` | Reddit search |
| `tw`, `x` | X (Twitter) search |
| `images` | Google Images search |
| `wolfram` | WolframAlpha query |
| `arch` | Arch Wiki search |
| `hn` | Hacker News (Algolia) search |
| `gclip`, `clipsearch` | Google-search the current clipboard |
| `ytclip` | YouTube-search the current clipboard |

Add your own or override a built-in name in `config.macros` — see
[configuration.md](configuration.md#macros--query-templates). List them all
at any time with the `macros` mini.

### Quicklinks and workflows

Quicklinks (`config.quicklinks`) are macros with positional parameters:
`{1}` `{2}` ... plus `{*}` (all args) alongside the usual
`{query}`/`{clipboard}` forms. Invoke as `ql <name> <args>`,
`!name args`, or `name:args`:

```
ql ticket ABC-123
ticket:ABC-123
!docs some query
```

Workflows (`config.workflows`) are named multi-step result actions.
`workflows` lists them; `workflow <name>` / `run <name>` shows the
chain; picking `Run <name>` from a file's actions (`Ctrl`/`⌘`+`K`)
executes each step in order. Steps are the same ids as per-file
actions, joined with `+` ad-hoc (`copy_path+reveal`).

## 7. Filtered search — inline filter clauses

Any other query is scanned for filter clauses before falling back to plain
fuzzy search (`parse_filter_clauses()`, `include/wilfred/search/filter.hpp`).
Clauses are whitespace-separated tokens; anything left over after clauses
are stripped out is used as the plain-text fuzzy query. Quoted phrases
(`"like this"`) are kept together as one token.

**`key:value` clauses:**

| Clause | Meaning |
|---|---|
| `ext:cpp` / `extension:cpp` | Extension filter (leading `.` optional) |
| `type:image` / `kind:image` | Kind filter — see [FileKind values](#filekind-values) below; `type:app`/`type:apps` also sets apps-only |
| `in:Projects` / `path:Projects` / `dir:Projects` | Only under a directory whose path contains/matches this |
| `scope:home` | Only within a named directory group from `config.scopes` |
| `name:foo` | Name must contain `foo` |
| `content:widget` / `text:widget` / `intext:widget` | Only files whose indexed *content* contains this token (implies content-only) |
| `is:app` / `is:folder` / `is:file` | Shorthand kind filters |
| `size:>10mb` / `size:<1gb` / `size:500kb` | Size filter (`>`/`<`/exact; units `kb`/`mb`/`gb` or bare `k`/`m`/`g`) |
| `modified:7d` / `after:2w` | Modified-after filter (`d`=days, `h`=hours, `w`=weeks, `m`=months); bare `today`/`recent`/`recently` means "within the last 7 days" |
| `hidden:true` / `hidden:false` | Require or exclude hidden files |
| `system:true` / `system:false` | Require or exclude system files |

**Bare-word clauses:**

| Token | Meaning |
|---|---|
| `*.ext` | Extension filter (e.g. `*.cpp`) |
| `.ext` / bare recognized extension (`pdf`, `png`, `jpg`, `cpp`, `h`, `hpp`, `rs`, `py`, `mp4`, `mp3`, `zip`, `exe`, `docx`, `txt`, `md`, ...) | Extension filter without the colon form |
| `in <dir>` / `inside <dir>` | Same as `in:<dir>`, as two words |
| `containing <text>` | Same as `name:<text>` |
| `intext <text>` / `contents <text>` | Same as `content:<text>` |
| `applications` / `apps` | Applications only |
| `folders` / `directories` | Directories only |
| `images` / `photos` | Image kind |
| `videos` / `movies` | Video kind |

Clauses combine — everything must match:

```
*.cpp in Projects
type:image name:logo
size:>10mb modified:7d
content:TODO ext:py
scope:projects react
is:folder recent
```

### `FileKind` values

Usable after `type:`/`kind:`/`is:`: `file`, `directory`, `application`,
`executable`, `document`, `image`, `video`, `audio`, `archive`, `source`,
`config`, `shortcut`, `browserdata` (see `kind_from_name()` in
`include/wilfred/index/record.hpp`).

## 8. Plain fuzzy search (the default)

If nothing above matched, the whole query is a fuzzy name/path search over
the index, combined with any signals in [ranking.md](ranking.md):

* **Exact / prefix / substring** matches on the file or app name.
* **Fuzzy subsequence matching** (`score_fuzzy`) — letters of the query
  appear in order in the name, not necessarily contiguous (so `vsc` can
  match "Visual Studio Code").
* **Acronym matching** — the query matches the first letters of each token
  in the name.
* **Path component matches** — the query matches a folder name above the
  file, not just the file name itself.
* **Content matches** — if `search.content_indexing` and content was
  indexed for that file, matching indexed text also contributes (see
  `content_hit` in [ranking.md](ranking.md)), independent of the explicit
  `content:`/`intext:` filter clause above.

## Clipboard as a query source

If `search.clipboard` is enabled, clipboard text and clipboard-derived paths
are also considered — both as an extra source of results (e.g. a copied
file path shows up as a hit) and as a ranking signal
(`clipboard_overlap` — see [ranking.md](ranking.md)) when the query overlaps
with what's currently on the clipboard. The `clip`/`clips` minis above
surface the clipboard directly as a result rather than as a ranking
influence.

## Snippets

If `snippets.expansion` is enabled, typing the configured `snippets.prefix`
(default `;`) followed by a trigger matches a saved text snippet
(`include/wilfred/search/snippets.hpp`) for insertion/paste, independent of
the classification pipeline above — snippet matching runs alongside the main
search rather than being one of the `QueryKind` branches. Bodies support
`{date} {time} {datetime} {year} {month} {day} {clipboard} {query}`, snippets
carry an optional `folder:` (filter with `;folder/name`), and
`snippets.global_expansion` expands abbreviations typed in any app.

## AI assistant

`ai <question>` / `ask <question>` (also `gpt ...`) queries the optional
local assistant (`ai.enabled` + `ai.api_key`). Supports OpenAI, Anthropic,
Gemini, and Groq over HTTPS (WinHTTP on Windows, `curl` elsewhere). Answers
return as a single copyable card; failures are non-blocking hints.

## Calendar / contacts / notes

When `sources.calendar/contacts/notes` are on, `.ics` events, `.vcf`
contacts, and Markdown notes under the configured + platform-default roots
are searched as `calendar` / `contact` / `note` results alongside files.

## Plugin queries

Regardless of how a query classifies, if `search.plugins` is enabled every
loaded plugin also receives the query text and may contribute additional
results (tagged with `category: "plugin"` and that plugin's `plugin_id`) —
see [plugins.md](plugins.md).
