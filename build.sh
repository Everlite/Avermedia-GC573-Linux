#!/bin/bash
# Build script for Avermedia GC573 Driver
# Usage: ./build.sh [LLVM=1]
set -euo pipefail

# Ensure we are in the project root (where this script lives)
cd "$(dirname "$0")"

echo "==> Cleaning previous build artifacts..."
make -C driver clean

echo "==> Building the cx511h kernel module..."
# Pass any extra arguments (e.g. LLVM=1) through to make
make -C driver "$@" -j"$(nproc)"

# Verify the module was built before attempting to copy
if [ ! -f driver/cx511h.ko ]; then
    echo "ERROR: Build failed — driver/cx511h.ko was not produced." >&2
    exit 1
fi

echo "==> Copying cx511h.ko to project root..."
cp driver/cx511h.ko ./
echo "==> Build complete. cx511h.ko is ready in $(pwd)"
