# BSD Support

Wilfred builds and runs on FreeBSD, OpenBSD, NetBSD, and DragonFly.
FreeBSD is the primary target (full feature set); the others share the
same backend with a few documented gaps below.

## Building

```bash
# FreeBSD / OpenBSD / NetBSD / DragonFly
./scripts/build-bsd.sh --release --tests
```

Base dependencies (a C++20 compiler is included in the base system on all
four; only `sh` is guaranteed as a shell, so `bash` is needed for the
build scripts): `cmake`, `ninja`, `pkgconf`, `bash`/`shells/bash`.

| OS | Base packages |
|---|---|
| FreeBSD | `pkg install cmake ninja pkgconf shells/bash` (`./scripts/build-bsd.sh --deps`) |
| OpenBSD | `pkg_add cmake ninja pkgconf bash` (as root) |
| NetBSD | `pkgin install cmake ninja pkgconf shells/bash` |
| DragonFly | `pkg install cmake ninja pkgconf shells/bash` |

Optional GUI packages (same graceful degradation as Linux — the daemon
builds and runs headless without them):

| Package | Enables |
|---|---|
| `libX11` | Overlay window, global hotkey, window listing |
| `webkit2-gtk3` + `gtk3` | Full HTML overlay UI (else X11 canvas fallback) |
| `gtk-layer-shell` | Native Wayland anchoring (same as Linux) |

Manual CMake works too — no preset required, though `freebsd-release` /
`freebsd-debug` presets exist in `CMakePresets.json`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure
```

## How it maps

BSD desktops reuse the portable Unix files from `src/platform/linux/`
(XDG app discovery, `xdg-open` launching, X11 hotkey/windows/screenshots)
with BSD-native replacements for the Linux-only pieces:

| Piece | Linux | BSD |
|---|---|---|
| Filesystem watcher | inotify (`linux/fs_watch.cpp`) | kqueue (`bsd/fs_watch.cpp`) |
| Volume list | `/proc/mounts` | `getmntinfo` (`bsd/volumes.cpp`) |
| RAM / CPU / uptime | `/proc/meminfo`, `/proc/stat`, `/proc/uptime` | `hw.physmem`, `kern.cp_time`, `kern.boottime` via sysctl |
| Process list | `/proc` scan | sysctl `KERN_PROC_PROC` (FreeBSD) |
| Swap | `/proc/meminfo` | `swapinfo -k` (FreeBSD) |
| Ping timeout | `-W` seconds | `-W` milliseconds (FreeBSD/DragonFly), `-w` seconds (NetBSD) |
| exe path | `/proc/self/exe` | `KERN_PROC_PATHNAME` (FreeBSD/DragonFly) |

Config and data live in the same XDG locations as on Linux
(`~/.config/wilfred/wilfred.yml`, `~/.local/share/wilfred/`), and the
default index roots (home, `/usr/share/applications`,
`/usr/local/share/applications`, `/opt`) already cover BSD layouts —
ports put `.desktop` files in `/usr/local/share/applications`.

## Known gaps (by OS)

* **Process list / swap stats**: implemented via sysctl/`swapinfo` on
  FreeBSD only. OpenBSD, NetBSD, and DragonFly show an empty process list
  and a "stats unavailable" swap card — everything else works.
* **Ping timeout**: OpenBSD `ping` has no wait-timeout flag, so an
  unreachable host takes the full default wait to fail there (FreeBSD and
  DragonFly use `-W` in milliseconds, NetBSD `-w` in seconds).
* **`logout` system action**: unsupported on BSD (no logind session to
  terminate); `shutdown` / `restart` use `shutdown(8)`, `sleep` uses
  `acpiconf` (FreeBSD) / `zzz` (OpenBSD).
* **MPRIS media control**: works over D-Bus where a session bus with
  players exists; the "Linux-only" fallback message is shown otherwise.
* **Packages**: no ports/packages yet — build from source, grab a tagged
  FreeBSD release tarball, or use the nightly FreeBSD CI job.
  `pkg`/`ports` submissions welcome; see `docs/installer.md` for how the
  other artifacts are staged.

## CI

`.github/workflows/nightly.yml` has a `bsd` job that builds and runs the
full test suite on FreeBSD (`vmactions/freebsd-vm`). Tagged releases also
ship a `wilfred-<tag>-freebsd-x64.tar.gz`, built by the `bsd` job in
`.github/workflows/release.yml` with the same staging as the Linux tarball
(binary + `ui/overlay/` + README + default config).
