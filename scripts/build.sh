#!/bin/sh
# build.sh - Build Emerald on Linux (Debian, Ubuntu, Red Hat, Fedora, ...) and macOS.
set -e
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
echo
echo "Built: ./build/emerald"
echo "Try:   ./build/emerald examples/hello.em"
echo "Test:  cd build && ctest"
