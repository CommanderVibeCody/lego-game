#!/usr/bin/env sh
# Configure, build and (optionally) run the game. Usage: ./build.sh [run]
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
echo "Built: build/bin/lego-game"
if [ "$1" = "run" ]; then ./build/bin/lego-game; fi
