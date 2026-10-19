# Dev Container

A reproducible Linux development environment for Wilfred: Ubuntu 24.04, GCC,
CMake, Ninja, `pkg-config`, X11 headers, and the lint tools. It mirrors the
Linux job in `.github/workflows/tests.yml`, so a build here matches CI.

## Use it

**VS Code / Cursor**

1. Install the **Dev Containers** extension.
2. Open the repository and run **Dev Containers: Reopen in Container**.
3. The container configures the `linux-debug` preset on creation.

**GitHub Codespaces**

Create a codespace from the repository; it picks up `.devcontainer/`.

**Dev Container CLI**

```bash
devcontainer up --workspace-folder .
devcontainer exec --workspace-folder . bash -lc './scripts/build-linux.sh --debug --tests'
```

## Build and test

```bash
# CMake preset (already configured by postCreateCommand)
cmake --build --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure -C Debug

# Or the platform script
./scripts/build-linux.sh --debug --tests
```

Binaries land in `build/linux-debug/` (preset) or `build/linux/` (script).

## What is included

| Tool | Purpose |
|------|---------|
| GCC + GDB | C++20 compiler and debugger |
| CMake 3.16+ | Build system and presets |
| Ninja | Fast generator used by the presets |
| `pkg-config` | Optional dependency detection |
| `libx11-dev` | X11 overlay build |
| `clang-format`, `clang-tidy` | Match the lint workflow |

## Limits

- The container is headless. Building and running `ctest` works, but the
  overlay and global hotkey need a desktop session (X11/XWayland with a
  display). Wayland anchoring additionally needs `libgtk-layer-shell-dev`.
- WebKitGTK is not installed, so CMake uses the X11 fallback overlay. Add
  `libwebkit2gtk-4.1-dev libgtk-3-dev` to the Dockerfile if you need the
  WebKit overlay.
- Android and iOS toolchains are not part of this container; use
  `scripts/build-android.sh` / `scripts/build-ios.sh` on a host with the
  respective SDKs.

## Rebuild

After editing `Dockerfile` or `devcontainer.json`, run **Dev Containers:
Rebuild Container** (or `devcontainer up --workspace-folder . --rebuild`).
