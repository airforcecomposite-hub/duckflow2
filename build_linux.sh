#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

echo "============================================================"
echo "  DuckFlow 2 - Linux x86_64 VST3 Build Script"
echo "============================================================"

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j"$(nproc)"

echo ""
echo "[SUCCESS] VST3 bundle built at:"
echo "  build/DuckFlow2_artefacts/Release/VST3/DuckFlow 2.vst3"
echo "Install with:"
echo "  mkdir -p ~/.vst3 && cp -R \"build/DuckFlow2_artefacts/Release/VST3/DuckFlow 2.vst3\" ~/.vst3/"
