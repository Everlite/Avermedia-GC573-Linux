#!/bin/bash
# Build script for Avermedia GC573 Driver
# Usage: ./build.sh [LLVM=1]
set -euo pipefail

# Ensure we are in the project root (where this script lives)
cd "$(dirname "$0")"

echo "==> Cleaning previous build artifacts..."

# Linux kbuild splits M=<external-module-directory> on whitespace.  This
# project can therefore not be compiled in place when any parent directory
# contains a space (as is common for the default checkout path).  Build a
# temporary copy with a whitespace-free path and copy only the final module
# back to the checkout.
PROJECT_ROOT="$(pwd -P)"
BUILD_ROOT="$PROJECT_ROOT"
STAGED_BUILD=""
if [[ "$PROJECT_ROOT" == *[[:space:]]* ]]; then
    STAGED_BUILD="$(mktemp -d /tmp/cx511h-build.XXXXXX)"
    trap 'rm -rf -- "$STAGED_BUILD"' EXIT
    BUILD_ROOT="$STAGED_BUILD/repo"
    echo "==> Staging build outside whitespace-containing path..."
    cp -a "$PROJECT_ROOT/." "$BUILD_ROOT"
fi

make -C "$BUILD_ROOT/driver" clean

echo "==> Building the cx511h kernel module..."
# Pass any extra arguments (e.g. LLVM=1) through to make
make -C "$BUILD_ROOT/driver" "$@" -j"$(nproc)"

# Verify the module was built before attempting to copy
if [ ! -f "$BUILD_ROOT/driver/cx511h.ko" ]; then
    echo "ERROR: Build failed — driver/cx511h.ko was not produced." >&2
    exit 1
fi

echo "==> Copying cx511h.ko to project root..."
cp "$BUILD_ROOT/driver/cx511h.ko" "$PROJECT_ROOT/cx511h.ko"
echo "==> Build complete. cx511h.ko is ready in $(pwd)"
