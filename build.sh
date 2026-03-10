#!/bin/sh
# Build script for Avermedia GC573 Driver
# Usage: ./build.sh [LLVM=1]

make -C driver clean
make -C driver $@ -j$(nproc)
cp driver/cx511h.ko ./
