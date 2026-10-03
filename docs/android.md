# Android Build

Wilfred on Android is a Kotlin UI over the same C++ core (`wilfred_core`)
used on desktop. The APK contains:

- **Kotlin UI** (`android/app/src/main/java/com/wilfred/launcher/`):
  `MainActivity` (search bar + assist strip + results), `FloatingWService`
  (floating "W" SVG button pinned to the **bottom-left** corner; tap pops up
  the search bar overlay without leaving the current app), `ResultAdapter`,
  `WilfredActions` (shared Intent/clipboard/menu logic), `WilfredBridge` (JNI).
- **C++ core via the NDK** (`libwilfred_jni.so`): `AndroidCore`
  (`include/wilfred/platform/android_bridge.hpp`) boots config/index/history/
  snippets/clips/notes/todos/layouts/plugins/providers from the app-private
  files dir and serves `search_json` / `assist_json` / `actions_json` /
  `execute_action` / `preview_json` / `status_json`, plus Kotlin-enumerated
  app indexing. All launching (app Intents, `ACTION_VIEW` URLs,
  `FileProvider` files) happens in Kotlin from the result payload.
- **Android platform backend** (`src/platform/android/`): polling file
  watcher, `PackageManager`-fed app discovery, intent-delegated
  launch/browser stubs, `/sdcard` volumes, null overlay (Kotlin owns it),
  no-op hotkey (the floating W replaces it), Android clipboard override
  (Kotlin pushes/pulls the OS clipboard; C++ never forks `wl-copy`).

## Requirements

- JDK 17+, Android SDK (`ANDROID_HOME`), NDK r26+, Gradle
- `ANDROID_HOME` (or `ANDROID_SDK_ROOT`) must be set; `ANDROID_NDK_HOME`
  optional when the NDK comes from sdkmanager

## Build the APK

```bash
./scripts/build-android.sh --debug              # app/build/outputs/apk/debug/*.apk
./scripts/build-android.sh --release            # release APK (unsigned until signed)
./scripts/build-android.sh --release --bundle   # plus release AAB for Play
```

Local version override (release CI sets these from the tag):

```bash
WILFRED_VERSION_NAME=1.2.3 WILFRED_VERSION_CODE=1002003 \
  ./scripts/build-android.sh --release --apk --bundle
# or: gradle -PversionName=1.2.3 -PversionCode=1002003 assembleRelease
```

Local signing (release `signingConfig` reads env; without it Gradle emits
`app-release-unsigned.apk`):

```bash
export ANDROID_KEYSTORE_PATH=/path/to/wilfred.jks
export ANDROID_KEYSTORE_PASSWORD=...
export ANDROID_KEY_ALIAS=wilfred
export ANDROID_KEY_PASSWORD=...  # defaults to the store password
```

The Gradle module (`android/`) builds `wilfred_jni` from the repo-root
`CMakeLists.txt` (`ANDROID` branch, `log`/`android` system libs only, no
X11/WebKit/desktop daemons) for `arm64-v8a` + `x86_64`, then packages it
with the Kotlin sources. First launch asks for overlay permission
(`SYSTEM_ALERT_WINDOW`) for the floating W and media/storage permissions
for indexing shared storage; the core itself only needs the app-private
files dir.

## Behavior notes

- Search, fuzzy/rank/filters, calculator/convert, macros, quicklinks,
  snippets (+`{clipboard}`/`{date}` expansion), notes/todos, timers,
  clipboard history, content/OCR/semantic providers, and ranking all run in
  C++; results cross JNI as JSON
  (`{title,subtitle,path,payload,score,action,category,kind,actions:[{id,label}]}`).
- Assist crosses as `{"correction":"","ghost":"","candidates":[]}` for
  "did you mean", ghost completion and the candidate strip.
- `package:<id>` paths launch via `PackageManager`; `http(s)` payloads via
  `ACTION_VIEW`; file paths via `FileProvider` (`${applicationId}.provider`);
  calc/convert/copy/snippet cards copy to the Android clipboard.
- Long-press shows the full C++ action list (`actions_json`) and runs
  side-effect actions via `execute_action` (timer stop, note/todo delete,
  clip pin/clear, copy flavors, hash, workflows/layouts).
- File peek uses `preview_json` (exists/is_dir/size/text head).
- Window management, global hotkeys, screenshots, and process/media control
  are desktop-only and report explicit errors on Android.

## Release builds (CI)

`.github/workflows/release.yml` has an `android` job (JDK 17, SDK 34,
NDK r26, Gradle 8.7) that runs on every `v*.*.*` tag, in parallel with the
desktop matrix:

- Derives `versionName`/`versionCode` from the tag (`v1.2.3` →
  `1.2.3` / `1002003`) and builds `assembleRelease` + `bundleRelease`
  (APK + AAB) via `scripts/build-android.sh --release --apk --bundle`.
- Verifies the APK with `apksigner verify --print-certs`.
- Stages `dist/wilfred-<tag>-android.apk` and `dist/wilfred-<tag>-android.aab`
  when signed, or `*-unsigned.apk` / `*-unsigned.aab` when built without
  secrets, and uploads them as the `wilfred-<tag>-android` artifact; the
  `release` job (`needs: [build, android]`) attaches `*.apk`/`*.aab`
  alongside the desktop archives.

Secrets are optional: with none configured every step still exits 0 (the
job stays green) and the unsigned artifacts are clearly suffixed. Only a
missing build output (no APK produced at all) fails the job.

Signing secrets (all optional; without them the workflow warns and ships
unsigned so forks still build):

| Secret | Meaning |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | base64 of the release `.jks` (decoded to `android/wilfred-release.jks`) |
| `ANDROID_KEYSTORE_PASSWORD` | keystore password |
| `ANDROID_KEY_ALIAS` | key alias (default `wilfred`) |
| `ANDROID_KEY_PASSWORD` | key password (defaults to the store password) |

Create a keystore once with:

```bash
keytool -genkeypair -keystore wilfred-release.jks -alias wilfred \
  -keyalg RSA -keysize 2048 -validity 9125
base64 -w0 wilfred-release.jks  # -> ANDROID_KEYSTORE_BASE64
```
