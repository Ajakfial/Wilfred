# BSD Support

Wilfred builds and runs on FreeBSD, OpenBSD, NetBSD, and DragonFly.
FreeBSD is the primary target (full feature set); the others share the
same backend with a few documented gaps below.

## Feature Parity Matrix

| Feature | FreeBSD | OpenBSD | NetBSD | DragonFly | Linux |
|---|---|---|---|---|---|
| Core search/index | ✅ | ✅ | ✅ | ✅ | ✅ |
| Filesystem watcher (kqueue) | ✅ | ✅ | ✅ | ✅ | ❌ (inotify) |
| Overlay UI (X11/WebKitGTK) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Overlay UI (Wayland layer-shell) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Global hotkey (X11) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Application discovery (.desktop) | ✅ | ✅ | ✅ | ✅ | ✅ |
| File manager integration | ✅ | ✅ | ✅ | ✅ | ✅ |
| System toggles (WiFi) | ⚠️ | ❌ | ❌ | ⚠️ | ✅ |
| System toggles (Bluetooth) | ⚠️ | ❌ | ❌ | ⚠️ | ✅ |
| System toggles (Volume) | ✅ (OSS) | ✅ (sndio) | ✅ (sysctl) | ✅ (OSS) | ✅ (PipeWire/Pulse) |
| System toggles (Brightness) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Power actions (sleep/shutdown) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Process listing | ✅ | ✅ | ✅ | ✅ | ✅ |
| Window management (X11) | ✅ | ✅ | ✅ | ✅ | ✅ |
| Package manager search | ✅ (pkg) | ✅ (pkg_add) | ✅ (pkgin) | ✅ (pkg) | ✅ (apt/dnf/pacman) |
| Browser integration | ✅ | ✅ | ✅ | ✅ | ✅ |
| Calculator/minis | ✅ | ✅ | ✅ | ✅ | ✅ |
| Benchmarks | ✅ | ✅ | ✅ | ✅ | ✅ |

Legend: ✅ Full support | ⚠️ Partial/needs tools | ❌ Not available

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

Optional GUI packages (same graceful degradation as Linux -- the daemon
builds and runs headless without them):

| Package | Enables |
|---|---|
| `libX11` | Overlay window, global hotkey, window listing |
| `webkit2-gtk3` + `gtk3` | Full HTML overlay UI (else X11 canvas fallback) |
| `gtk-layer-shell` | Native Wayland anchoring (same as Linux) |

Manual CMake works too -- no preset required, though `freebsd-release` /
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
| Process list | `/proc` scan | per-OS sysctl (`minis.cpp`): `KERN_PROC_PROC` (FreeBSD), `KERN_PROC_ALL` (OpenBSD/DragonFly), `KERN_PROC2` (NetBSD) |
| Swap | `/proc/meminfo` | `swapinfo -k` (FreeBSD, best-effort DragonFly), `swapctl(2)` (OpenBSD/NetBSD) |
| Ping timeout | `-W` seconds | `-W` milliseconds (FreeBSD/DragonFly), `-w` seconds (NetBSD) |
| exe path | `/proc/self/exe` | `KERN_PROC_PATHNAME` (FreeBSD/DragonFly), `KERN_PROC_ARGS` + `KERN_PROC_PATHNAME` (NetBSD), `getexecpath(3)` on 8.0+ with argv[0] fallback (OpenBSD) |
| File manager open | `xdg-open` + FM detection | `xdg-open` + FM detection (Thunar, Dolphin, Nautilus, PCManFM, Caja, SpaceFM, Worker, lf, ranger, nnn, vifm, Krusader, Double Commander) |

Config and data live in the same XDG locations as on Linux
(`~/.config/wilfred/wilfred.yml`, `~/.local/share/wilfred/`), and the
default index roots (home, `/usr/share/applications`,
`/usr/local/share/applications`, `/opt`) already cover BSD layouts --
ports put `.desktop` files in `/usr/local/share/applications`.

## BSD-Specific Features

### File Manager Integration
The `open_parent` action (bound to `Ctrl+Enter` on files) opens the
containing folder in your preferred file manager. Wilfred detects and
prefers: Thunar, Dolphin, Nautilus, Nemo, PCManFM, Caja, SpaceFM, Worker,
lf, ranger, nnn, vifm, Krusader, Double Commander -- then falls back to
`xdg-open`.

### Package Manager Integration
The `pkg` mini (`pkg search`, `pkg install`, `pkg info`) works on FreeBSD
and DragonFly. OpenBSD uses `pkg_add`/`pkg_info`, NetBSD uses `pkgin`.
Run `packages` to see detected managers.

### System Toggles
- **Volume**: Uses OSS `mixer` on FreeBSD/DragonFly, `sndio`/`mixerctl` on
  OpenBSD, `sysctl hw.snd.vpc_*` on NetBSD.
- **Brightness**: Uses `backlight`/`intel_backlight`/`acpi_video` via
  `sysctl hw.acpi.video.*` or `/dev/backlight`.
- **WiFi**: FreeBSD/DragonFly support `ifconfig wlan0` scanning via
  `wlanctl`/`ifconfig`; OpenBSD/NetBSD use `ifconfig`/`iwconfig` (no
  NetworkManager equivalent).
- **Bluetooth**: Not yet implemented on BSD (patches welcome).

### Benchmarks
Run `wilfred_bench` on BSD to measure index/search performance. The
benchmark suite is identical across platforms and produces comparable
numbers. See `docs/benchmarks.md` for 1M+ record results.

## Known gaps (by OS)

* **DragonFly swap stats**: parsed from `swapinfo -k` best-effort (its
  column layout is not guaranteed); anything unparseable shows an honest
  "unavailable" card instead of wrong numbers.
* **DragonFly RAM used**: best-effort `vm.stats.vm.v_free_count`
  (FreeBSD-derived tree); falls back to total-only when absent.
* **OpenBSD exe path**: resolved via `getexecpath(3)` on 8.0+ with an
  argv[0] fallback (saved at startup) on older releases, so no procfs mount
  is needed; self-update staging and overlay dir resolution work out of the
  box.
* **Ping timeout**: OpenBSD `ping` has no wait-timeout flag, so an
  unreachable host takes the full default wait to fail there (FreeBSD and
  DragonFly use `-W` in milliseconds, NetBSD `-w` in seconds).
* **`logout` system action**: unsupported on BSD (no logind session to
  terminate); `shutdown` / `restart` use `shutdown(8)`, `sleep` uses
  `acpiconf` (FreeBSD/DragonFly), `zzz` (OpenBSD), `hw.acpi.sleep.state`
  (NetBSD).
* **MPRIS media control**: works over D-Bus where a session bus with
  players exists; the "Linux-only" fallback message is shown otherwise.
* **Packages**: no ports/packages yet -- build from source, grab a tagged
  FreeBSD/OpenBSD/NetBSD release tarball, or use the nightly CI jobs.
  `pkg`/`ports` submissions welcome; see `docs/installer.md` for how the
  other artifacts are staged.

## CI

`.github/workflows/nightly.yml` has `bsd`, `openbsd`, and `netbsd` jobs
that build and run the full test suite in VMs (`vmactions/freebsd-vm`,
`vmactions/openbsd-vm`, `vmactions/netbsd-vm`). Tagged releases ship
`wilfred-<tag>-{freebsd,openbsd,netbsd}-x64.tar.gz`, built by the matching
jobs in `.github/workflows/release.yml` with the same staging as the Linux
tarball (binary + `ui/overlay/` + README + default config). DragonFly has
no VM runner available, so it stays manual: `./scripts/build-bsd.sh
--release --tests` on a DragonFly host.

## Getting Help

- Report BSD-specific issues at https://github.com/ajakfial/wilfred/issues
- Join the discussion: FreeBSD forums, OpenBSD misc@, NetBSD pkgsrc-users
- BSD-specific config examples in `config/wilfred.bsd.yml.example`