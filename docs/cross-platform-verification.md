# Cross-Platform Verification Guide

This document provides a comprehensive checklist for verifying Wilfred functionality
across all supported platforms: Windows, macOS, Linux, FreeBSD, OpenBSD, NetBSD,
DragonFly BSD, Android, and iOS.

## Platform Support Matrix

| Feature | Windows | macOS | Linux | FreeBSD | OpenBSD | NetBSD | DragonFly | Android | iOS |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| **Core** | | | | | | | | | |
| Daemon (indexer + watcher + hotkey) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CLI (search, launch, index, status) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Overlay UI | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Config (YAML + validation) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Indexing** | | | | | | | | | |
| Filesystem watcher | ✅ (RDCW) | ✅ (FSEvents) | ✅ (inotify) | ✅ (kqueue) | ✅ (kqueue) | ✅ (kqueue) | ✅ (kqueue) | ✅ (polling) | ✅ (polling) |
| USN Journal (Windows) | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Volume enumeration | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| **Search** | | | | | | | | | |
| Fuzzy/ranking/filter | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| OS search federation | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ |
| Semantic/vector search | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ⚠️ |
| **Actions** | | | | | | | | | |
| File operations (copy/move/rename) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ⚠️ |
| Open in file manager | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Open with specific app | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Terminal/editor integration | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| **System Integration** | | | | | | | | | |
| Global hotkey | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ (floating btn) | ✅ (app icon) |
| Autostart/login item | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| System toggles (WiFi/BT/Vol/Brightness) | ✅ | ✅ | ✅ | ⚠️ | ❌ | ❌ | ⚠️ | ❌ | ❌ |
| Settings deep-links | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Power actions (lock/sleep/shutdown) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Window management | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Media keys | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ |
| **Extras** | | | | | | | | | |
| Calculator/conversions | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Package manager search | ✅ (winget) | ✅ (brew) | ✅ (apt/dnf/pacman) | ✅ (pkg) | ✅ (pkg_add) | ✅ (pkgin) | ✅ (pkg) | ❌ | ❌ |
| Browser integration | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| Clipboard history | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ⚠️ |
| Plugins (native + stdio) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| **Accessibility** | | | | | | | | | |
| Screen reader support | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Keyboard navigation | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| High contrast | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Reduced motion | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

Legend: ✅ Full support | ⚠️ Partial/needs tools | ❌ Not applicable/unsupported

## Verification Checklist

### Pre-Release Verification (All Platforms)

#### Build Verification
- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` succeeds
- [ ] `cmake --build build --config Release --parallel` succeeds
- [ ] `cmake --build build --config Release --target wilfred_tests` succeeds
- [ ] `cmake --build build --config Release --target wilfred_bench` succeeds
- [ ] All unit tests pass (`ctest --test-dir build --output-on-failure`)
- [ ] Binary runs without missing dependencies (`ldd` / `otool -L` / `dumpbin /dependents`)

#### Functional Verification

##### Core Daemon
- [ ] `wilfred` starts daemon successfully
- [ ] `wilfred search <query>` returns results
- [ ] `wilfred launch <query>` opens top result
- [ ] `wilfred index` performs full reindex
- [ ] `wilfred status` shows healthy index
- [ ] Daemon handles SIGTERM/SIGINT gracefully
- [ ] Config validation works (`wilfred config-validate`)

##### Overlay UI
- [ ] Global hotkey summons overlay (Ctrl+Alt+W / ⌘+Alt+W)
- [ ] Search input accepts text and shows results
- [ ] Keyboard navigation (↑/↓/Enter/Escape/Tab)
- [ ] Action menu opens (Tab/Ctrl+K) and navigates
- [ ] Preview pane opens (F3) and shows content
- [ ] Settings panel opens (Ctrl+,) and saves changes
- [ ] Overlay dismisses on Escape/focus loss
- [ ] Glass/opaque/blur appearance config works

##### File Operations
- [ ] `open` launches files with default handler
- [ ] `reveal` / `open_parent` opens containing folder in file manager
- [ ] `open_with` shows available applications
- [ ] `copy_path`, `copy_name`, `copy_file_uri` work
- [ ] `compress_zip` creates archive
- [ ] `move_to`, `bulk_rename`, `new_from_template` work
- [ ] `open_terminal`, `open_editor` open in file's directory
- [ ] `new_file`, `new_folder` create entries

##### System Integration
- [ ] `lock`, `sleep`, `shutdown`, `restart`, `logout` work
- [ ] `wifi`, `bluetooth`, `volume`, `brightness` show status
- [ ] `volume 50`, `brightness 70`, `wifi off` apply changes
- [ ] `settings wifi`, `settings sound` open OS settings
- [ ] `native_kill_process` terminates processes
- [ ] `native_list_windows` shows open windows
- [ ] Window actions (minimize/maximize/snap/close) work

##### Search Features
- [ ] Fuzzy matching with typo tolerance
- [ ] Filters (`*.cpp`, `type:image`, `size:>10mb`)
- [ ] Scopes (`scope:home`, `scope:work`)
- [ ] Calculator (`25 * 42`, `100 usd to eur`)
- [ ] Mini commands (`weather`, `time`, `disk`, `ram`, `cpu`, `process`)
- [ ] Web search (`? query`, `g query`)
- [ ] Bookmarks/history/tabs (`bm`, `hist`, `tabs`)
- [ ] Snippets (`snip`, `;keyword`)
- [ ] Notes/todos (`note`, `todo`, `notes`, `todos`)
- [ ] Timers (`timer`, `pomodoro`, `stopwatch`)

##### Advanced Features
- [ ] Workflows (`workflow save`, `workflow run`)
- [ ] Quicklinks (`ql name arg1 arg2`)
- [ ] Macros (`!yt cats`, `gh repo`)
- [ ] Pins (`pin`, `pins`, `unpin`)
- [ ] Large/dupes finder (`large`, `dupes`)
- [ ] File conversion (`convert`, `bgremove`)
- [ ] Transcription (`transcribe`, `dictate`)
- [ ] AI assistant (`ai`, `ask`, `ai see`)

#### Platform-Specific Verification

##### Windows
- [ ] USN journal indexing works (`wilfred index` on NTFS)
- [ ] Windows Search federation (`os query`)
- [ ] WebView2 overlay renders correctly
- [ ] Chocolatey package (`choco install wilfred`) installs and runs
- [ ] MSI installer works (per-user, no UAC)
- [ ] Portable ZIP runs from any location
- [ ] Autostart via registry/Task Scheduler
- [ ] VERSIONINFO shows "Wilfred Open Contributors"

##### macOS
- [ ] FSEvents watcher picks up changes instantly
- [ ] Spotlight federation (`os query`)
- [ ] WKWebView overlay with transparency/blur
- [ ] LaunchAgent autostart works
- [ ] Homebrew formula installs correctly
- [ ] Code signing / notarization passes
- [ ] Native media keys (HID) control Music/Spotify/VLC
- [ ] AppleScript fallbacks for media/volume work

##### Linux
- [ ] inotify watcher works on ext4/btrfs/xfs
- [ ] X11 overlay works without compositor
- [ ] WebKitGTK overlay with compositor
- [ ] Wayland layer-shell on Sway/KDE (gtk-layer-shell)
- [ ] X11 fallback on GNOME/other Wayland
- [ ] XDG autostart entry created
- [ ] MPRIS media control via D-Bus
- [ ] AppImage/Flatpak/package manager builds work

##### FreeBSD
- [ ] kqueue watcher picks up changes
- [ ] getmntinfo volumes enumerated
- [ ] sysctl RAM/CPU/uptime works
- [ ] KERN_PROC_PROC process listing
- [ ] OSS mixer volume control
- [ ] acpiconf sleep
- [ ] pkg package search
- [ ] X11/WebKitGTK overlay
- [ ] Wayland layer-shell on sway/wlroots

##### OpenBSD
- [ ] kqueue watcher works
- [ ] getmntinfo volumes
- [ ] sysctl hw.physmem/kern.cp_time
- [ ] KERN_PROC_ALL process listing
- [ ] swapctl swap stats
- [ ] sndio/mixerctl volume
- [ ] zzz sleep
- [ ] pkg_add package search
- [ ] pledge/unveil compatibility (no violations)

##### NetBSD
- [ ] kqueue watcher works
- [ ] getvfsstat volumes
- [ ] sysctl hw.physmem/kern.cp_time
- [ ] KERN_PROC2 process listing
- [ ] swapctl swap stats
- [ ] hw.acpi.sleep.state sleep
- [ ] pkgin package search
- [ ] curses/ncurses compatibility

##### DragonFly BSD
- [ ] kqueue watcher works
- [ ] getmntinfo volumes
- [ ] sysctl vm.stats.vm.v_free_count
- [ ] KERN_PROC_ALL process listing
- [ ] swapinfo swap stats
- [ ] OSS mixer volume
- [ ] acpiconf sleep
- [ ] pkg package search

##### Android
- [ ] APK builds and installs
- [ ] Floating W button appears
- [ ] Search bar works
- [ ] Index builds in app sandbox
- [ ] Results open via Intents
- [ ] NDK core links without errors
- [ ] Kotlin/C++ JNI bridge works

##### iOS
- [ ] Simulator .app builds
- [ ] SwiftUI search UI works
- [ ] Sandbox-only index builds
- [ ] Results open via UIApplication
- [ ] CocoaPods/SPM integration works

#### Accessibility Verification

##### Overlay UI
- [ ] Screen reader announces search results count
- [ ] Screen reader announces selected item
- [ ] Screen reader announces corrections/completions
- [ ] Screen reader announces action menu open/close
- [ ] Screen reader announces preview open/close
- [ ] Focus trapped in modal dialogs (settings, action menu)
- [ ] Tab order logical through all elements
- [ ] All interactive elements have accessible names
- [ ] Live regions announce dynamic content changes
- [ ] High contrast mode (`prefers-contrast: more`) works
- [ ] Reduced motion (`prefers-reduced-motion: reduce`) disables animations
- [ ] Keyboard-only operation fully functional

#### Performance Verification
- [ ] `wilfred_bench` runs without errors
- [ ] 1M record index insert < 2s (modern hardware)
- [ ] 1M record fuzzy search < 500ms
- [ ] 1M record ranking < 100ms
- [ ] Memory usage < 50MB for 1M records
- [ ] Daemon idle CPU < 0.5%
- [ ] Daemon idle RAM < 30MB base + index
- [ ] Overlay opens < 100ms (cold) / < 20ms (warm)
- [ ] Search latency < 50ms p95 for 100K index

#### Packaging Verification
- [ ] Windows: Portable ZIP, MSI, Chocolatey
- [ ] macOS: .tar.gz, Homebrew formula
- [ ] Linux: .tar.gz, AppImage, Flatpak, distro packages
- [ ] FreeBSD: .tar.gz, ports submission ready
- [ ] OpenBSD: .tar.gz, ports submission ready
- [ ] NetBSD: .tar.gz, pkgsrc submission ready
- [ ] Android: .apk, .aab (signed/unsigned)
- [ ] iOS: Simulator .zip

## CI/CD Verification

### GitHub Actions Workflows

#### `.github/workflows/ci.yml`
- [ ] Runs on: ubuntu-latest, windows-latest, macos-latest
- [ ] Builds Debug + Release
- [ ] Runs unit tests
- [ ] Runs benchmarks (quick mode)
- [ ] Checks formatting (clang-format)
- [ ] Checks static analysis (clang-tidy)

#### `.github/workflows/nightly.yml`
- [ ] Runs on: ubuntu-latest, freebsd-vm, openbsd-vm, netbsd-vm
- [ ] Builds from main branch nightly
- [ ] Runs full test suite
- [ ] Runs full benchmarks
- [ ] Publishes nightly artifacts

#### `.github/workflows/release.yml`
- [ ] Triggers on `v*.*.*` tags
- [ ] Builds all platforms
- [ ] Creates release artifacts
- [ ] Signs Windows binaries (if secrets)
- [ ] Publishes to GitHub Releases
- [ ] Updates Homebrew tap
- [ ] Updates Chocolatey package

## Regression Testing

### Automated Regression Suite
```bash
# Run full test suite
ctest --test-dir build --output-on-failure -j$(nproc)

# Run specific platform tests
ctest --test-dir build -R "test_bsd|test_platform|test_paths" --output-on-failure

# Run benchmarks and compare with baseline
python3 scripts/run_benchmarks.py --build-dir build --format json --output baseline.json
# ... after changes ...
python3 scripts/run_benchmarks.py --build-dir build --format json --output current.json
python3 scripts/compare_benchmarks.py baseline.json current.json
```

### Manual Regression Scenarios
1. **Fresh install**: Clean config/data dirs, run setup, verify defaults
2. **Upgrade**: Old config + new binary, verify migration
3. **Large index**: 500K+ files, verify search performance
4. **Network disruption**: WiFi toggle during search, verify recovery
5. **Power events**: Sleep/wake, verify watcher reconnects
6. **Locale changes**: Switch system language, verify overlay strings

## Troubleshooting Common Cross-Platform Issues

### Build Failures
| Platform | Issue | Resolution |
|---|---|---|
| OpenBSD | `pledge`/`unveil` violations | Add promises to `main.cpp` |
| NetBSD | Missing `std::filesystem` features | Use polyfill or conditional compile |
| Android | NDK version mismatch | Pin NDK version in `gradle.properties` |
| iOS | Bitcode/signing errors | Disable bitcode, configure signing |

### Runtime Failures
| Platform | Issue | Resolution |
|---|---|---|
| Linux | WebKitGTK not found | Install `webkit2gtk-4.1` + `gtk3` |
| Linux | Wayland layer-shell missing | Install `gtk-layer-shell` or use X11 fallback |
| BSD | WiFi/Bluetooth not working | Install `wpa_supplicant`, `bluez`; check permissions |
| Windows | WebView2 not found | Bundle WebView2 loader or install runtime |
| macOS | Hardened runtime blocks | Add entitlements for JIT/file access |

## Release Criteria

A release is ready when:
- [ ] All CI workflows pass on all platforms
- [ ] All verification checklist items pass on at least 3 platforms
- [ ] No critical regressions vs previous release
- [ ] Benchmarks within 5% of baseline
- [ ] All packaging artifacts generated and tested
- [ ] Documentation updated for new features
- [ ] Changelog written
- [ ] Version bumped in `CMakeLists.txt` and `cmake/version.hpp.in`

## Platform-Specific Test Commands

```bash
# Linux
./build/wilfred_tests
./build/wilfred_bench
./build/wilfred --version

# Windows (PowerShell)
.\build\Release\wilfred_tests.exe
.\build\Release\wilfred_bench.exe
.\build\Release\wilfred.exe --version

# macOS
./build/wilfred_tests
./build/wilfred_bench
./build/wilfred --version

# FreeBSD
./build/wilfred_tests
./build/wilfred_bench
./build/wilfred --version

# OpenBSD
./build/wilfred_tests
./build/wilfred_bench
./build/wilfred --version

# NetBSD
./build/wilfred_tests
./build/wilfred_bench
./build/wilfred --version

# Android (requires device/emulator)
adb install -r build/wilfred.apk
adb shell am start -n com.wilfred/.MainActivity

# iOS (requires Xcode/simulator)
xcodebuild -scheme Wilfred -destination 'platform=iOS Simulator,name=iPhone 15' build
```

## Contact

For platform-specific issues:
- Windows: @windows-maintainers
- macOS: @macos-maintainers
- Linux: @linux-maintainers
- BSD: @bsd-maintainers
- Mobile: @mobile-maintainers