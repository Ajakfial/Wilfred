#!/usr/bin/env bash
# Build an AppImage from a staged Linux dir (see release workflow).
# Usage: build-appimage.sh --staging dist/wilfred-v1.2.3-linux-x64 --out dist/file.AppImage --version v1.2.3
# Needs curl; downloads appimagetool (extracted, no FUSE needed).
set -euo pipefail

STAGING=""
OUT=""
VERSION=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --staging) STAGING="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $(basename "$0") --staging DIR --out FILE.AppImage --version v1.2.3"
      exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

if [[ -z "$STAGING" || -z "$OUT" || -z "$VERSION" ]]; then
  echo "--staging, --out and --version are required" >&2
  exit 2
fi
if [[ ! -x "$STAGING/wilfred" ]]; then
  echo "staging dir missing wilfred binary: $STAGING" >&2
  exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/Wilfred.AppDir"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/wilfred"

cp "$STAGING/wilfred" "$APPDIR/usr/bin/wilfred"
chmod 755 "$APPDIR/usr/bin/wilfred"
if [[ -d "$STAGING/ui/overlay" ]]; then
  cp -R "$STAGING/ui/overlay" "$APPDIR/usr/share/wilfred/overlay"
fi
cp packaging/linux/wilfred.desktop "$APPDIR/wilfred.desktop"
cp www/public/favicon.svg "$APPDIR/wilfred.svg"
cp www/public/favicon.svg "$APPDIR/.DirIcon"
cp packaging/linux/AppRun "$APPDIR/AppRun"
chmod 755 "$APPDIR/AppRun"

TOOL="$WORK/appimagetool"
if ! command -v appimagetool >/dev/null 2>&1; then
  curl -fsSL -o "$WORK/appimagetool.AppImage" \
    https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
  chmod +x "$WORK/appimagetool.AppImage"
  # No FUSE on CI runners: extract and run the inner binary instead.
  (cd "$WORK" && "$WORK/appimagetool.AppImage" --appimage-extract >/dev/null)
  TOOL="$WORK/squashfs-root/AppRun"
fi

mkdir -p "$(dirname "$OUT")"
ARCH=x86_64 "$TOOL" "$APPDIR" "$OUT"
echo "wrote $OUT"
