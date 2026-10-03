#!/usr/bin/env bash
# Build the Wilfred Android APK (Kotlin UI + C++ core via the NDK).
# Usage:
#   ./scripts/build-android.sh [--release] [--debug] [--apk] [--bundle] [--clean]
# Requires: JDK 17+, Android SDK (ANDROID_HOME), NDK r26+, Gradle wrapper or gradle.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MODE="debug"
BUILD_APK=1
BUILD_BUNDLE=0
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release) MODE="release"; shift ;;
    --debug) MODE="debug"; shift ;;
    --apk) BUILD_APK=1; shift ;;
    --bundle) BUILD_BUNDLE=1; shift ;;
    --clean) CLEAN=1; shift ;;
    -h|--help)
      echo "Usage: $(basename "$0") [--release|--debug] [--apk] [--bundle] [--clean]"
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

if [[ -z "${ANDROID_HOME:-}" && -z "${ANDROID_SDK_ROOT:-}" ]]; then
  echo "ANDROID_HOME (or ANDROID_SDK_ROOT) must point at the Android SDK." >&2
  exit 1
fi
if [[ -z "${ANDROID_NDK_HOME:-}" ]]; then
  echo "Note: ANDROID_NDK_HOME not set; Gradle will use the NDK from SDKManager." >&2
fi
if ! command -v java >/dev/null 2>&1; then
  echo "JDK 17+ is required." >&2
  exit 1
fi

cd android
if [[ "$CLEAN" -eq 1 ]]; then
  if [[ -x ./gradlew ]]; then
    ./gradlew clean
  else
    gradle clean || true
  fi
fi

TASK_APK="assemble${MODE^}"
TASK_BUNDLE="bundle${MODE^}"
if [[ -x ./gradlew ]]; then
  GRADLE=./gradlew
else
  GRADLE=gradle
fi

if [[ "$BUILD_APK" -eq 1 ]]; then
  echo "Building APK ($MODE): $TASK_APK"
  $GRADLE "$TASK_APK"
  echo
  echo "APK:"
  find app/build/outputs/apk -name '*.apk' 2>/dev/null || true
fi

if [[ "$BUILD_BUNDLE" -eq 1 ]]; then
  echo "Building App Bundle ($MODE): $TASK_BUNDLE"
  $GRADLE "$TASK_BUNDLE"
  echo
  echo "AAB:"
  find app/build/outputs/bundle -name '*.aab' 2>/dev/null || true
fi
