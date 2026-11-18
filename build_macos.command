#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "============================================================"
echo "  DuckFlow 2 - macOS Universal (Apple Silicon + Intel) Build"
echo "============================================================"

if ! command -v cmake >/dev/null 2>&1; then
    echo "[ERROR] CMake is not installed. Run: brew install cmake"
    exit 1
fi

echo "[1/2] Configuring Universal Binary (arm64 + x86_64) & fetching JUCE 8..."
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET="10.15"

echo "[2/2] Building DuckFlow 2.vst3 and DuckFlow 2.component..."
cmake --build build --config Release -j"$(sysctl -n hw.ncpu)"

VST3_PATH="build/DuckFlow2_artefacts/Release/VST3/DuckFlow 2.vst3"
codesign --force --deep --sign - "$VST3_PATH" || true

echo ""
echo "[SUCCESS] Build complete!"
echo "VST3 bundle: $VST3_PATH"
echo "Install with:"
echo "  cp -R \"$VST3_PATH\" ~/Library/Audio/Plug-Ins/VST3/"
