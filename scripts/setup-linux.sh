#!/usr/bin/env bash
set -euo pipefail

# Installs Linux build dependencies and JUCE into workspace-local directories.
# This environment already provides a populated sibling cache; scripts/build.sh
# can be run directly when external/JUCE and deps/sysroot exist.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p external deps

if [ ! -e external/JUCE/CMakeLists.txt ]; then
    if [ -e "${ROOT}/../plugin_poc/external/JUCE/CMakeLists.txt" ]; then
        ln -sfn "$(cd "${ROOT}/../plugin_poc/external/JUCE" && pwd)" external/JUCE
    else
        git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git external/JUCE
    fi
fi

if [ ! -e deps/sysroot/usr ] && [ -e "${ROOT}/../plugin_poc/deps/sysroot/usr" ]; then
    ln -sfn "$(cd "${ROOT}/../plugin_poc/deps/sysroot" && pwd)" deps/sysroot
fi

if ! pkg-config --exists x11 freetype2 fontconfig alsa; then
    echo "System JUCE dependencies are missing. Install the -dev packages listed in README.md." >&2
fi

echo "Dependencies are ready. Run scripts/build.sh next."
