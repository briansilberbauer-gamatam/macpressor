#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
CONFIG="${CONFIG:-Release}"

if [ -d "$ROOT/deps/sysroot/usr" ]; then
    export PKG_CONFIG_PATH="$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:$ROOT/deps/sysroot/usr/share/pkgconfig:${PKG_CONFIG_PATH:-}"
    export PKG_CONFIG_SYSROOT_DIR="$ROOT/deps/sysroot"
    CMAKE_EXTRA_ARGS=(
        "-DCMAKE_INCLUDE_PATH=$ROOT/deps/sysroot/usr/include"
        "-DCMAKE_LIBRARY_PATH=$ROOT/deps/sysroot/usr/lib/x86_64-linux-gnu"
    )
else
    CMAKE_EXTRA_ARGS=()
fi

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DMACPRESSOR_BUILD_TESTS=ON \
    -DMACPRESSOR_BUILD_SCREENSHOT=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

if [ -z "${JOBS:-}" ]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$JOBS"

VST3="$BUILD_DIR/MacPressor_artefacts/$CONFIG/VST3/MacPressor.vst3"
STANDALONE="$BUILD_DIR/MacPressor_artefacts/$CONFIG/Standalone/MacPressor"

mkdir -p "$ROOT/dist"
if [ -d "$VST3" ]; then
    rm -rf "$ROOT/dist/MacPressor.vst3"
    cp -a "$VST3" "$ROOT/dist/"
fi
if [ -x "$STANDALONE" ]; then
    cp -a "$STANDALONE" "$ROOT/dist/MacPressor"
fi

echo
echo "Built artefacts:"
[ -d "$ROOT/dist/MacPressor.vst3" ] && echo "  VST3        $ROOT/dist/MacPressor.vst3"
[ -x "$ROOT/dist/MacPressor" ] && echo "  Standalone  $ROOT/dist/MacPressor"
echo "  Tests       $BUILD_DIR/MacPressorTests_artefacts/$CONFIG/MacPressorTests"
if [ -x "$BUILD_DIR/MacPressorHostSmokeTest_artefacts/$CONFIG/MacPressorHostSmokeTest" ]; then
    echo "  Host test   $BUILD_DIR/MacPressorHostSmokeTest_artefacts/$CONFIG/MacPressorHostSmokeTest"
fi
if [ -x "$BUILD_DIR/MacPressorScreenshot_artefacts/$CONFIG/MacPressorScreenshot" ]; then
    echo "  Screenshot  $BUILD_DIR/MacPressorScreenshot_artefacts/$CONFIG/MacPressorScreenshot"
fi
