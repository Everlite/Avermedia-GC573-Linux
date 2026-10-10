# open_gc573 — Blob-Free Open Source Linux Driver for AVerMedia Live Gamer 4K (GC573)

A fully open-source, blob-free Linux kernel driver (V4L2 + ALSA) for the **AVerMedia Live Gamer 4K GC573** (`1461:0054`, Subsystem `1461:5730`) and **AVerMedia Live Gamer BOLT GC555** (`1461:0054`, Subsystem `1461:5550`).

Unlike the proprietary vendor driver package, `open_gc573` requires **no binary blobs** (`aver_xilinx.o` / `ite6805.o`) to build or load. It compiles directly against modern Linux kernels using pure C and standard kernel frameworks (`videobuf2-dma-sg`, `v4l2-dv-timings`, `sound/pcm`).

---

## Key Features

- **1440p @ 120 FPS Native Capture:** EDID and the V4L2 mode table advertise 2560×1440 @ 120 Hz CVT-RB. The ITE6805 under-read (~100 fps at ~413 MHz) is rescaled to 120, and `0x1080[31:16]` keeps that rate. Picture at 1440p120 is confirmed on the blob driver; this module still needs a capture run.
- **Dynamic FPGA Clocking:** Automatically calculates and programs internal VIP Frame Period (`0x103c` @ 148.5 MHz) to prevent hardware frame rate decimation.
- **High-Bandwidth DDR Dual-Pixel Mode:** Enables dual-pixel DDR mode (`0x3` in VIP Pixel Mode `0x1088`) for >400 MHz pixel clocks (1440p120, 4K60).
- **Format Flexibility:** Supports NV12, YUYV (YUY2), RGB32, and P010 (HDR10).
- **ALSA Stereo HDMI Audio:** Native ALSA capture device (`open_gc573` / `Live Gamer 4K Audio Capture`).
- **Subsystem Auto-Detection:** Automatically binds to both PCIe GC573 (`1461:5730`) and Thunderbolt GC555 (`1461:5550`).

---

## Architecture Overview

```
                      +-----------------------------+
                      |   HDMI In (PS5 / PC / etc)  |
                      +--------------+--------------+
                                     |
                                     v
                       ITE IT6805 Dual HDMI 2.0 Rx
                         (gc573-it6805-core.c)
                                     |
                  [LVDS / TTL DDR Dual-Pixel @ ~420 MHz]
                                     v
                       Xilinx Artix-7 FPGA (VIP)
                         (gc573-fpga.c)
                         - Reg 0x103c: Frame Period (1.2375M cycles @ 120Hz)
                         - Reg 0x1080: Target FPS (120 << 16)
                         - Reg 0x1088: Pixel Mode (0x3 = Dual-Px DDR)
                                     |
                                     v
                       PCIe Scatter-Gather DMA
                         (gc573-dma.c / gc573-video-dma.c)
                                     |
                     +---------------+---------------+
                     |                               |
                     v                               v
             V4L2 Video Node                 ALSA Audio Device
              /dev/video*                       hw:CARD=GC573
```

---

## Building and Testing

### Build
```bash
make LLVM=1 CC=clang
```

### Safe Load / Test
```bash
./load.sh
```
The script will unload any existing `cx511h` driver and load `open_gc573.ko`.

### Test Capture (1440p @ 120 FPS)
```bash
v4l2-ctl --stream-mmap --stream-count=240 -d /dev/videoX
```

### Unload
```bash
./unload.sh
```

---

## Licensing & Attribution

- **License:** GPL-2.0-only
- Based on the pioneering reverse-engineering and architecture by **ostrich** ([github.com/ostrich/gc555](https://github.com/ostrich/gc555)), released under GPL-2.0-only.
- Extended with:
  - PCI Subsystem `1461:5730` GC573 support and soft-failing LED detection.
  - Native 2560×1440 @ 120 Hz CVT-RB EDID injection and RGB Limited color pipeline validation.
  - FPGA `VIP_FRAME_PERIOD` (0x103c) / `VIP_PIXEL_MODE` (0x1088) tuning for unthrottled 120 FPS capture.
