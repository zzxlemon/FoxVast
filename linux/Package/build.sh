#!/bin/bash
# foxpkg (package manager) build script (Linux) — mirrors windows/Package/build.bat
# Usage: bash build.sh   (or: chmod +x build.sh && ./build.sh)
# The top-level build entry is FoxCore/CMakeLists.txt (it pulls in Package/),
# so configure from there and build only the foxpkg target.
set -e
cd "$(dirname "$0")/../FoxCore"

mkdir -p ../build
cmake -S . -B ../build -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --target foxpkg -j

echo ""
echo "Build OK. Run:"
echo "  ./build/Package/foxpkg"
