# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted, heavily patched Linux driver for the AVerMedia GC573.
Modernized for recent kernels. Experimental — for development and testing only.

> [!NOTE]
> Links against the precompiled vendor blob `AverMediaLib_64.a` for low-level FPGA/ITE6805 logic.

---

## Status: [EXPERIMENTAL] / ALPHA

**Kernel Compatibility:** Builds and runs on **Kernel 6.19.10+** (CachyOS / Arch / Gentoo).
Kernel 7.0.1 requires rebuild — current `.ko` has `vermagic: 6.19.10`.

| Feature | Status | Description |
|:---|:---:|:---|
| **Build / Toolchain** | ⚠️ [CLANG] | Requires `LLVM=1 CC=clang`; GCC may fail on Clang-built kernels |
| **Module Loading** | ✅ [OK] | insmod.sh with audio service restoration |
| **Signal Detection** | ✅ [OK] | Forced 1080p EDID handshake |
| **IRQ / Interrupts** | ✅ [OK] | MSI with INTx fallback |
| **System Stability** | ✅ [OK] | I2C deadlock workaround, PCI handle fallback, smart unload.sh |
| **DMA Transfer** | ✅ [OK] | 60fps continuous streaming via doorbell (MMIO 0x304) + IRQ ACK (MMIO 0x10) |
| **Capture Content** | 🟡 [COLOR] | Streaming works, FPGA CSC active (MMIO 0x1040), I2C path disabled, byte-order testing needed |
| **Driver Unload** | ✅ [OK] | 3-stage unload.sh (rmmod → PCI unbind → force), audio restored |
| **Audio Capture** | ✅ [OK] | HDMI audio via ALSA (S16_LE/S24_LE, 32–192 kHz, 1 channel) |
| **General Use** | ❌ [NO] | Development/testing only — NOT FOR DAILY USE |

### Development Phases

| Phase | Status | Description |
|:---|:---:|:---|
| **Phase 1** (Reverse Engineering) | ✅ COMPLETE | Builds on modern kernels, module loads, hardware bring-up |
| **Phase 2** (Continuous Streaming) | 🟡 COMPLETE* | DMA/IRQ/Doorbell working at 60fps, but I2C path disabled (deadlock workaround) |
| **Phase 3** (Color Correction) | 🟡 IN PROGRESS | FPGA CSC active, byte-order testing pending |
| **Phase 4** (Production Ready) | ⏳ PENDING | Robust start/stop, suspend/resume, OBS/GStreamer support |

> \*Phase 2: Streaming works end-to-end, but **all I2C write operations are skipped** due to bus deadlock.
> The FPGA MMIO path (registers 0x10, 0x304, 0x1040) handles streaming control instead.

---

## Module Parameters

### From `board_v4l2.c`

| Parameter | Type | Default | Description |
|:---|:---:|:---:|:---|
| `force_input_mode` | int | 0 | 0=Auto, 1=YUV422 BT.709, 2=YUV444 BT.709, 3=RGB Full, 4=RGB Limited |
| `debug_pixel_format` | int | -1 | -1=Auto, 0=YUYV, 1=UYVY, 2=YVYU, 3=VYUY, 4–11=RGB variants |
| `auto_test_byteorder` | int | 0 | 1=Cycle through formats 0-3 on stream_on, dump first 64 bytes each |

### From `board_config.c`

| Parameter | Type | Default | Description |
|:---|:---:|:---:|:---|
| `no_signal_pic` | charp | NULL | Bitmap shown when no input signal |
| `copy_protetion_pic` | charp | NULL | Bitmap shown for copy-protected content |
| `led_pin_r` | int | 3 | GPIO pin for red LED (-1=disabled) |
| `led_pin_g` | int | 4 | GPIO pin for green LED (-1=disabled) |
| `led_pin_b` | int | 5 | GPIO pin for blue LED (-1=disabled) |

---

## How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel Parameters (GRUB/systemd-boot):**
> ```bash
> ibt=off iommu=pt
> ```
> - `ibt=off` — Required because `AverMediaLib_64.a` is a precompiled blob without ENDBR64 instructions.
>   The Makefile sets `-fcf-protection=none` and `MODULE_INFO(ibt, "N")` as additional mitigation.
> - `iommu=pt` — IOMMU passthrough mode (required for DMA).

- **Kernel Headers** matching running kernel
- **Build Tools:** `base-devel`, `llvm`, `clang`

### Build & Load

```bash
# Build (compiles in driver/, copies .ko to project root)
./build.sh LLVM=1 CC=clang

# Load with dependency handling and audio restoration
sudo ./insmod.sh
```

> **Note:** `build.sh` copies `cx511h.ko` to the project root, but `insmod.sh` loads from
> `driver/cx511h.ko` (it `cd`s into `driver/` internally). Both paths work; the `driver/` copy
> is always the freshest build.

### System Installation (persistent)

```bash
sudo ./install.sh
```
Installs to `/lib/modules/$(uname -r)/kernel/drivers/media/avermedia/` and runs `depmod -a`.

---

## Quick Start

```bash
sudo ./insmod.sh

# Test capture (UYVY = FPGA native format)
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/video0

sudo ./unload.sh
```

### GStreamer Test Scripts

```bash
# Video only (YV12 format, X11 output)
./gst_1.0_raw_video.sh 0

# Video + HDMI audio (YUY2 format, 48kHz stereo)
./gst_1.0_raw_video_audio.sh 0
```

---

## What Actually Happens in `stream_on`

### Executed (in order):

1. **I2C Reads** — `ite6805_get_frameinfo()`, `ite6805_get_workingmode()`, `ite6805_get_colorspace()`, `ite6805_get_sampingmode()` read HDMI input state from ITE6805
2. **CPU** — Populates `vip_cfg` struct (resolution, framerate, colorspace, bypass, pixel format)
3. **MMIO** — `aver_xilinx_enable_video_streaming(FALSE)` — stops any running stream
4. **MMIO** — `aver_xilinx_config_video_process(&vip_cfg)` — configures FPGA scaler/CSC/DMA
5. **MMIO** — `pci_model_mmio_write(0x1040, csc_value)` — writes FPGA CSC control register
6. **MMIO** — `aver_xilinx_enable_video_streaming(TRUE)` — starts streaming
7. **MMIO** — `pci_model_mmio_write(0x304, 0x01)` — initial doorbell

### Skipped (I2C deadlock workaround):

| Operation | Registers | Mechanism |
|:---|:---|:---|
| TTL Pixel Mode config | 0xc0, 0xc1, 0xbd, 0xbe, 0xc4 | `goto skip_ttl_config` |
| HDMI Video Unmute | 0xb0, 0xa0, 0x02 | SKIPPING block (empty) |
| ITE68051 Streaming Regs | 0x20, 0x86, 0x90, 0xA0–A2, 0xA4, 0xB0 | SKIPPING block (empty) |
| RX DeSkew | (vendor regs) | SKIPPING block (empty) |
| ITE6805 CSC Sync | 0x6b, 0x6c, 0x6e, 0x2a | SKIPPING block (empty) |
| HDCP State | — | Commented out |
| Freerun Screen | — | Commented out |

**Reason:** Any `hdmirxwr()` I2C write during streaming deadlocks on the ITE6805 bus.
The FPGA MMIO path handles CSC and streaming control instead.

### Per-Frame (in `cx511h_video_buffer_done`, 60×/second):

1. `v4l2_model_buffer_done()` — hands filled buffer to V4L2/userspace
2. `pci_model_mmio_write(0x304, 0x01)` — doorbell: next buffer ready
3. `pci_model_mmio_write(0x10, 0x02)` — IRQ ACK
4. `wmb()` — write memory barrier
5. Logs frame count once per minute (every 3600 frames)

### `stream_off`:

Single call: `aver_xilinx_enable_video_streaming(FALSE)` — stops DMA.

---

## Architecture

### Source Files

| Layer | Files | Purpose |
|:---|:---|:---|
| Entry | `entry.c` | module_init/exit, PCI ID table, MODULE_SOFTDEP, IBT disable |
| Context | `cxt_mgr.c` | Reference-counted context manager for driver handles |
| Board | `board/cx511h/board_config.c` | PCI probe/remove, FPGA/ITE6805/GPIO/ALSA/V4L2 init |
| Board | `board/cx511h/board_v4l2.c` | V4L2 streaming, CSC, doorbell, IRQ ACK, pixel format |
| Board | `board/cx511h/board_i2c.c` | I2C bus setup, ITE6805 at address 0x58 |
| Board | `board/cx511h/board_gpio.c` | GPIO init (Reset=Pin0, HPD=Pin2) |
| Board | `board/cx511h/board_alsa.c` | ALSA capture device (PCM, 32–192kHz, S16_LE/S24_LE) |
| Utils | `utils/pci/pci_model.c` | PCI MMIO read/write, DMA, IRQ handling |
| Utils | `utils/v4l2/*.c` | V4L2 device, ioctl, videobuf2, framegrabber abstraction |
| Utils | `utils/alsa/alsa_model.c` | ALSA model wrapper |
| Utils | `utils/i2c/i2c_model.c` | I2C model wrapper |
| Blob | `AverMediaLib_64.a` | Precompiled vendor binary (FPGA/ITE6805 low-level logic) |

### Build Configuration (`driver/Makefile`)

- Compiler: `LLVM=1 CC=clang` required
- Flags: `-Wno-maybe-uninitialized`, `-Wno-implicit-fallthrough`, `-fcf-protection=none`, `-fno-stack-protector`
- Clang-specific: `-Wno-implicit-enum-enum-cast`, `-Wno-missing-prototypes`, `-Wno-unused-variable`
- Blob integration: `AverMediaLib_64.a` is copied to `.o` via custom kbuild rule (kbuild doesn't support `.a` in `-objs`)

### Hardware Initialization (`board_probe`, 14 steps)

1. Get PCI handle
2. Init I2C manager
3. Init GPIO manager
4. Init memory manager
5. Init task/thread manager
6. Init Xilinx FPGA (GPIO mask, I2C speed 400kHz, audio buffer 11520×4)
7. Configure FPGA registers
8. Setup I2C bus `I2C_BUS_COM`
9. Init board GPIOs (Reset=Pin0, HPD=Pin2)
10. Attach ITE6805 HDMI receiver (I2C addr 0x58) with fallback for broken linker section mechanism
11. Init bitmap overlay (no-signal / copy-protection)
12. Init ALSA audio subsystem
13. Init V4L2 video subsystem

### Audio

HDMI audio capture via ITE6805 → FPGA DMA → ALSA:
- Device name: `AVerMedia CL511H`
- Formats: S16_LE, S24_LE
- Rates: 32kHz, 44.1kHz, 48kHz, 96kHz, 192kHz
- Channels: 1 (capture)
- Buffer: 128 periods × 30720 bytes

### LED Control

RGB LED via FPGA GPIO pins (configurable via module parameters `led_pin_r/g/b`).
Function `cx511h_set_led_color()` in `board_v4l2.c`.

### Suspend/Resume

- `board_suspend()` → `board_v4l2_suspend()`
- `board_resume()` → re-inits FPGA registers via `aver_xilinx_init_registers()` → `board_v4l2_resume()`

---

## Scripts Reference

| Script | Purpose |
|:---|:---|
| `build.sh` | Builds module in `driver/`, copies `cx511h.ko` to root. Passes args to `make` (e.g. `LLVM=1`). |
| `insmod.sh` | Loads vb2/snd dependencies, inserts `driver/cx511h.ko`, kills+restores PipeWire/WirePlumber if blocking. |
| `unload.sh` | 3-stage unload: `rmmod` → PCI unbind → `rmmod -f`. Restores audio services. |
| `install.sh` | Installs to `/lib/modules/.../kernel/drivers/media/avermedia/`, runs `depmod -a`. |
| `gst_1.0_raw_video.sh` | GStreamer: v4l2src → videoconvert → YV12 → xvimagesink. |
| `gst_1.0_raw_video_audio.sh` | GStreamer: v4l2src (YUY2) + alsasrc (48kHz/16bit/stereo) → autovideosink + alsasink. |

---

## Tools (`tools/`)

| Tool | Purpose |
|:---|:---|
| `check_patterns.py` | Counts `mov %gs:0x28` / `xor %gs:0x28` stack-canary patterns in a binary |
| `patch_library.py` | Patches stack-canary prologue/epilogue in a binary to NOPs (neutralizes `-fstack-protector`) |
| `scan_epilogues.py` | Scans binary for all `xor %gs:0x28, %reg` variants (inventory before patching) |

---

## Known Issues

### 1. I2C Bus Deadlock (Critical)
- **Status:** WORKAROUND APPLIED
- **Issue:** Any I2C write (`hdmirxwr()`) to the ITE6805 during streaming freezes the system.
- **Impact:** All I2C write operations in `stream_on` are skipped (TTL config, HDMI unmute, streaming registers, CSC sync). Streaming works via FPGA MMIO path only.
- **I2C reads** (`ite6805_get_*`) work and are used for input format detection.

### 2. Byte-Order / Green Screen
- **Status:** TESTING REQUIRED
- **Issue:** Green tint indicates YUYV/UYVY byte swap between HDMI source and FPGA.
- **Solution:** Use `debug_pixel_format` (0–3) or `auto_test_byteorder=1` to identify correct mapping.
- **Workaround:** Always request `uyvy422` in userspace (FPGA native format).

### 3. V4L2/FPGA Format Mismatch
- **Status:** KNOWN
- **Issue:** FPGA always outputs UYVY in auto-mode, but V4L2 may negotiate a different format.
- **Impact:** Green tint if userspace requests YUYV but FPGA sends UYVY.
- **Workaround:** Always use `-input_format uyvy422` with ffplay/ffmpeg.

### 4. Module "In Use" Errors
- **Status:** RESOLVED
- **Issue:** PipeWire/WirePlumber hold ALSA devices open.
- **Solution:** `unload.sh` kills blocking processes, restores audio after unload.

### 5. v4l2-ctl Incompatibility
- **Status:** KNOWN
- **Issue:** `v4l2-ctl --stream-mmap` triggers I2C reads during stream-off → freeze.
- **Solution:** Use `ffplay` instead.

### 6. Toolchain Sensitivity
- **Status:** UNCHANGED
- **Issue:** Requires `LLVM=1 CC=clang` on Clang-built kernels (e.g. CachyOS).

### 7. Width/Height Swap in `vip_cfg`
- **Status:** UNDER INVESTIGATION
- **Location:** `driver/board/cx511h/board_v4l2.c` line ~364
- **Issue:** `vactive` receives `width` and `hactive` receives `height` — possibly vendor convention, possibly a bug affecting scaler/DMA config.

### 8. Kernel 7.0.1 Migration
- **Status:** PENDING
- **Issue:** Current `.ko` has `vermagic: 6.19.10`. Won't load on 7.x without rebuild.
- **Solution:** `./build.sh LLVM=1 CC=clang` after kernel upgrade.

### 9. V4L2 Exposes All Pixel Formats
- **Status:** KNOWN
- **Issue:** Driver exposes `FRAMEGRABBER_PIXFMT_BITMSK` (all formats), but hardware only supports YUV422 (UYVY). OBS/GStreamer may negotiate unsupported formats.

### 10. Legacy suspend/resume
- **Status:** DEPRECATED
- **Issue:** Uses legacy `.suspend`/`.resume` in `struct pci_driver`. Should migrate to `dev_pm_ops` for Kernel 7.x.

---

## Reverse Engineering Progress

### Working (MMIO Path)

| Register | Type | Value | Purpose |
|:---:|:---:|:---:|:---|
| **MMIO 0x10** | FPGA | `0x02` | IRQ ACK after each buffer |
| **MMIO 0x304** | FPGA | `0x01` | Doorbell: next buffer ready |
| **MMIO 0x1040** | FPGA | dynamic | CSC control (bit0=422 mode, bit1=RGB→YUV, bits[10:8]=matrix) |

### Identified but Skipped (I2C Deadlock)

| Register | Device | Value | Purpose |
|:---:|:---:|:---:|:---|
| **I2C 0x20** | ITE68051 | `0x40` | Video Output Enable (Bit 6) |
| **I2C 0x86** | ITE68051 | `0x01` | Global Enable |
| **I2C 0x90** | ITE68051 | `0x8f` | IRQ Enable |
| **I2C 0xA0–A2** | ITE68051 | `0x80` | DMA Channel Enable |
| **I2C 0xA4** | ITE68051 | `0x08` | DMA Enable |
| **I2C 0xB0** | ITE68051 | `0x01` | Buffer Enable |
| **I2C 0x6b,0x6c,0x6e** | ITE6805 | — | CSC matrix |
| **I2C 0x2a** | ITE6805 | `0x3a` | Colorspace select |
| **I2C 0xc0,0xc1** | ITE6805 | `0x02,0x00` | TTL pixel mode (Single/SDR) |

---

## Debugging

```bash
# Load/unload
sudo ./insmod.sh
sudo ./unload.sh

# Streaming test (UYVY = FPGA native)
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/video0

# Byte-order testing
sudo insmod cx511h.ko debug_pixel_format=0  # YUYV
sudo insmod cx511h.ko debug_pixel_format=1  # UYVY (FPGA native)
sudo insmod cx511h.ko debug_pixel_format=2  # YVYU
sudo insmod cx511h.ko debug_pixel_format=3  # VYUY

# Auto-test all formats on stream_on
sudo insmod cx511h.ko auto_test_byteorder=1

# Kernel logs
dmesg | grep -iE "cx511h-csc|cx511h-phase2|cx511h-color|cx511h-dma"

# Raw frame dump (check hex pattern)
sudo dd if=/dev/video0 of=frame_raw.bin bs=$((1920*1080*2)) count=1
xxd frame_raw.bin | head -4
```

### Expected Buffer Patterns (YUV 4:2:2)

| Hex Pattern | Meaning |
|:---|:---|
| `10 80 10 80...` | Black YUV (Y=16, Cb/Cr=128) — DMA working, source muted |
| `00 00 00 00...` | No data — DMA not delivering |
| `80 10 80 10...` | Byte swap (UYVY vs YUYV) |
| Varying values | Real video — color issue is format/CSC only |

---

## Reverse Engineering Methods
- Community-driven register probing and analysis
- V4L2 callback chain tracing
- AI-assisted hardware behavior analysis for interoperability testing
- Iterative testing with actual hardware

---

## Legal/Compliance

**EU Directive 2009/24/EC Art. 6 (Interoperability):** This project constitutes lawful reverse engineering for achieving interoperability with the AVerMedia GC573 hardware.

> [!CAUTION]
> This project includes the precompiled vendor archive `AverMediaLib_64.a`.
> Review redistribution/licensing status before republishing binaries or forks.
> Non-commercial, community-driven project.

---

## Disclaimer
Unofficial community project for educational and interoperability purposes.
AVerMedia does not endorse or support this driver. **Use at your own risk.**

---

**Maintained by [Everlite](https://github.com/Everlite).**
Special thanks to original work by [derrod](https://github.com/derrod).
