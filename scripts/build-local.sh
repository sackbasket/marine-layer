#!/usr/bin/env bash
# Linux check build used by Claude before pushing: compiles the plug-in,
# runs the engine tests and renders the UI to snapshot.png.
#   scripts/build-local.sh [/path/to/JUCE]
set -euo pipefail
cd "$(dirname "$0")/.."
JUCE="${1:-${JUCE_DIR:-$HOME/JUCE}}"
NAME=$(sed -nE 's/^set\(PLUGIN_NAME +"([^"]+)".*/\1/p' plugin.cmake)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="$JUCE" -DPLUGIN_BUILD_TESTS=ON > build-config.log
cmake --build build --target "${NAME}_VST3" EngineTests Snapshot
./build/EngineTests_artefacts/Release/EngineTests
xvfb-run -a ./build/Snapshot_artefacts/Release/Snapshot snapshot.png
echo "UI rendered to snapshot.png"
