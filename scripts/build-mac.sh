#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then echo 'This script needs macOS.'; exit 1; fi
if ! xcrun --find clang >/dev/null 2>&1; then
  echo 'Install Xcode Command Line Tools: xcode-select --install'; exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo 'Install CMake from https://cmake.org/download/ or run: brew install cmake'; exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
printf '\nBuilt: build/TapeBloom_artefacts/Release/Standalone/TapeBloom.app\n'
printf 'Built: build/TapeBloom_artefacts/Release/AU/TapeBloom.component\n'
printf 'Install with: bash scripts/install-mac.sh\n'
