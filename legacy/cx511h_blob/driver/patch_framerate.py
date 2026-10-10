#!/usr/bin/env python3
"""
patch_framerate.py — Patch aver_xilinx_vip_set_framerate to support up to 240 FPS.

In AverMediaLib_64.a (aver_xilinx.o), the vendor blob contains:
    cmp DWORD PTR [rbp-0x20], 0x3c   ; 83 7d e0 3c (compare with 60)
    jle 2b33                         ; 7e 07
    mov DWORD PTR [rbp-0x20], 0x3c   ; c7 45 e0 3c 00 00 00 (clamp to 60!)

This clamps the target framerate written to FPGA register 0x1080 [31:16] to 60 FPS,
causing the FPGA hardware to decimate 120/144/240 FPS streams down to 60 FPS.

This script patches the clamp from 0x3c (60) to 0xf0 (240 FPS).
"""

import sys

OLD_CLAMP = bytes([0x83, 0x7d, 0xe0, 0x3c, 0x7e, 0x07, 0xc7, 0x45, 0xe0, 0x3c, 0x00, 0x00, 0x00])
BUG_CLAMP = bytes([0x83, 0x7d, 0xe0, 0xf0, 0x7e, 0x07, 0xc7, 0x45, 0xe0, 0xf0, 0x00, 0x00, 0x00])
# Replace 7e 07 (jle 2b33) with eb 07 (jmp 2b33) to unconditionally skip the 60 FPS clamp
UNCLAMP   = bytes([0x83, 0x7d, 0xe0, 0x3c, 0xeb, 0x07, 0xc7, 0x45, 0xe0, 0x3c, 0x00, 0x00, 0x00])


def main():
    if len(sys.argv) < 2:
        print("Usage: patch_framerate.py <target_file>", file=sys.stderr)
        sys.exit(1)

    target_file = sys.argv[1]
    with open(target_file, "rb") as f:
        data = f.read()

    if data.count(UNCLAMP) > 0:
        print(f"[patch-framerate] Already un-clamped in {target_file}")
        sys.exit(0)

    if data.count(OLD_CLAMP) > 0:
        patched = data.replace(OLD_CLAMP, UNCLAMP)
        count = data.count(OLD_CLAMP)
    elif data.count(BUG_CLAMP) > 0:
        patched = data.replace(BUG_CLAMP, UNCLAMP)
        count = data.count(BUG_CLAMP)
    else:
        print(f"[patch-framerate] WARNING: Clamp pattern not found in {target_file}", file=sys.stderr)
        sys.exit(1)

    with open(target_file, "wb") as f:
        f.write(patched)

    print(f"[patch-framerate] Successfully un-clamped {count} occurrence(s) in {target_file} (no 60 FPS clamp)")


if __name__ == "__main__":
    main()
