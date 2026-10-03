#!/usr/bin/env bash
# Build a .deb from a staged Linux dir (see release workflow Package step).
# Usage: build-deb.sh --staging dist/wilfred-v1.2.3-linux-x64 --out dist/file.deb --version v1.2.3 [--arch amd64]
set -euo pipefail

STAGING=""
OUT=""
VERSION=""
ARCH="amd64"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --staging) STAGING="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --arch) ARCH="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $(basename "$0") --staging DIR --out FILE.deb --version v1.2.3 [--arch amd64]"
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
if ! command -v dpkg-deb >/dev/null 2>&1; then
  echo "dpkg-deb is required (Debian/Ubuntu)" >&2
  exit 1
fi

# Tag v1.2.3 -> Debian version 1.2.3 (hyphen separates the debian revision).
DEBVER="${VERSION#v}"
DEBVER="${DEBVER#V}"
ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

PKG="$ROOT/wilfred_${DEBVER}_${ARCH}"
mkdir -p "$PKG/DEBIAN" "$PKG/usr/bin" "$PKG/usr/share/applications" \
  "$PKG/usr/share/icons/hicolor/scalable/apps" "$PKG/usr/share/doc/wilfred"

cp "$STAGING/wilfred" "$PKG/usr/bin/wilfred"
chmod 755 "$PKG/usr/bin/wilfred"
if [[ -d "$STAGING/ui/overlay" ]]; then
  mkdir -p "$PKG/usr/share/wilfred"
  cp -R "$STAGING/ui/overlay" "$PKG/usr/share/wilfred/overlay"
fi
cp packaging/linux/wilfred.desktop "$PKG/usr/share/applications/wilfred.desktop"
cp www/public/favicon.svg "$PKG/usr/share/icons/hicolor/scalable/apps/wilfred.svg"
[[ -f README.md ]] && cp README.md "$PKG/usr/share/doc/wilfred/" || true
[[ -f config/wilfred.default.yml ]] && cp config/wilfred.default.yml "$PKG/usr/share/doc/wilfred/" || true

cat > "$PKG/DEBIAN/control" <<EOF
Package: wilfred
Version: $DEBVER
Architecture: $ARCH
Maintainer: Wilfred Open Contributors
Section: utils
Priority: optional
Depends: libc6, libstdc++6, libx11-6
Description: Fast keyboard launcher and automation hub
 Wilfred is a keyboard-first launcher, snippet expander and
 automation hub with a built-in HTML overlay UI.
EOF
printf 'Installed-Size: %s\n' "$(du -sk "$PKG/usr" | cut -f1)" >> "$PKG/DEBIAN/control"

cat > "$PKG/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
exit 0
EOF
chmod 755 "$PKG/DEBIAN/postinst"

mkdir -p "$(dirname "$OUT")"
dpkg-deb --build "$PKG" "$OUT"
echo "wrote $OUT"
