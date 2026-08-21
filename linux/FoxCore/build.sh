#!/bin/bash
# FoxVast core build script (Linux) — mirrors windows/FoxCore/build.bat
# Usage: bash build.sh   (or: chmod +x build.sh && ./build.sh)
set -e
cd "$(dirname "$0")"

mkdir -p ../build
cmake -S . -B ../build -DCMAKE_BUILD_TYPE=Release
cmake --build ../build --target fox -j

TARGETS="math random file util socket time"
if cmake --build ../build --target help | grep -q "^\.\.\. graphics"; then
    TARGETS="$TARGETS graphics"
fi
cmake --build ../build --target $TARGETS -j

echo ""
echo "Build OK. Run:"
echo "  ./build/core/fox -f your_script.fox"
echo "  ./build/core/fox -c your_script.fox && ./build/core/fox -fc your_script.fox"
