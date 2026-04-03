# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19+ Development)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19%2B%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)
[![Issues](https://img.shields.io/github/issues/Everlite/Avermedia-GC573-Linux.svg)](https://github.com/Everlite/Avermedia-GC573-Linux/issues)

This repository contains a **community-maintained, AI-assisted**, and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for recent kernels and is currently aimed at reverse engineering, hardware bring‑up, and experimental capture testing.

> [!NOTE]
> This tree currently links against the precompiled vendor archive `AverMediaLib_64.a` for low‑level hardware logic.

---

##  Status: [EXPERIMENTAL] / ALPHA


**Kernel Compatibility:** Successfully builds and runs on **Kernel 6.19.10+** (CachyOS / Arch / Gentoo).

| Feature | Status | Description |
|:---|:---:|:---|
| **Build / Toolchain** | ⚠️ [CLANG] | Tested build path is `LLVM=1 CC=clang`; plain GCC builds may fail on Clang‑built kernels |
| **Module Loading** | ✅ [OK] | STABLE — No longer crashes the kernel upon loading |
| **Signal Detection** | ✅ [OK] | FUNCTIONAL — Hardware syncs via forced 1080p EDID handshake |
| **IRQ / Interrupts** | ✅ [OK] | WORKING — MSI interrupt allocation with INTx fallback |
| **System Stability** | ⚠️ [FRAGILE] | I2C read‑locks during streaming cause system freezes |
| **DMA Transfer** | ✅ [OK] | FUNCTIONAL — Data flow confirmed via ffplay/xxd |
| **Capture Content** | 🟡 [ALMOST] | Continuous streaming works, color fix pending (byte‑order mapping active, FPGA value fine‑tuning via debug_pixel_format) |
| **Driver Unload** | ✅ [OK] | Fixed lifecycle cleanup prevents kernel freezes; `rmmod` now works in current testing without kernel freezes |
| **General Use** | ❌ [NOT REC] | For development/testing only — NOT FOR DAILY USE |
| **Dependencies** | ✅ [OK] | MODULE_SOFTDEP + insmod.sh resolve symbol errors for videobuf2‑common, memops, v4l2, vmalloc, dma‑contig, dma‑sg, snd, snd‑pcm |

### Development Phase Status

| Phase | Status | Description |
|:---|:---:|:---|
| **Phase 1** | ✅ DONE | Builds on modern kernels, module loads stable |
| **Phase 2** | ✅ DONE | HDMI lock, first DMA data, format recognized |
| **Phase 3** | 🟡 [MOSTLY DONE] | Continuous streaming, buffer lifecycle, IRQ completion |
| **Phase 4** | ⏳ PENDING | Robust start/stop, suspend/resume, OBS/GStreamer support |

---

##  Key Features & Recent Progress

- **Robust Streaming Initialization** – V4L2 `STREAMON` now triggers FPGA and HDMI‑receiver configuration. MSI interrupts eliminate interrupt storms.
- **Triple‑EDID & 1080p Force** – EDID overrides force the card to identify as a 1080p‑max device; pixel‑clock mismatch fixed.
- **YUV Data Path Forcing** – Hardware delivers YUV 4:2:2 (`10 80`). Driver forces `YUYV` across FPGA, ITE68051, and V4L2 layers.
- **V4L2 Color Space Metadata** – Explicit BT.709, limited‑range (16‑235) flags; register `0x2a` synchronized with V4L2 state.
- **HDMI Handshake & Stability** – Windows‑driver 3‑stage unmute sequence (`0xb0→0xa0→0x02`). Write‑only I2C path during streaming avoids freezes.
- **TTL Pixel Mode** – Single‑pixel/SDR output via registers `0xc0‑0xc4`.
- **Input Format Override** – Module parameter `force_input_mode` (0=auto, 1=YUV422, 2=YUV444, 3=RGB‑Full, 4=RGB‑Limited).
- **Critical Cleanup Fixes** – Module cleanup lifecycle fixed; `rmmod` no longer freezes the kernel in current testing: proper `board_remove()` sequence, PCI‑context use‑before‑release, negative error‑code returns.
- **Module Dependencies** – MODULE_SOFTDEP ensures videobuf2‑common, memops, v4l2, vmalloc, dma‑contig, dma‑sg, snd, snd‑pcm load before cx511h.
- **Debug Pixel Format** – Module parameter `debug_pixel_format` (-1=auto, 0‑11=force FPGA format) for byte‑order troubleshooting.

<!-- --- PHASE2-UPDATE --- -->
### Continuous Streaming (Phase 2) – NEW
- **IRQ ACK (MMIO 0x10)** – Implemented in buffer completion path.
- **Doorbell Re‑Queue (MMIO 0x304)** – Triggers next buffer after each frame.
- **I2C Enable Sequence** – 8 streaming‑control registers (0x20, 0x86, 0x90, 0xA0‑A2, 0xA4, 0xB0) enabled in `stream_on`.
- **Result** – Streaming no longer stops after 1‑2 frames ("corrupted data" eliminated).
- **Remaining** – Green screen (byte‑order mapping active, FPGA value needs fine‑tuning via `debug_pixel_format` parameter).

### Tested Register Combinations (ITE68051)

| Test | 0x6b | 0x6c | 0x6e | 0x23 | 0xc0 | 0xc1 | 0x2a | Result |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **D** | 0x02 | 0x02 | 0x01 | – | 0x00 | 0x00 | 0x3a | **LOCKED YUV** |
| **Unmute** | 0x02 | 0x02 | 0x01 | 0xb0‑a0‑02 | 0x00 | 0x00 | 0x3a | **Success** |
| **TTL** | 0x02 | 0x02 | 0x01 | 0xb0‑a0‑02 | 0x02 | 0x00 | 0x3a | **FLICKERING DATA** |
| **Phase** | 0x02 | 0x02 | 0x01 | 0xb0‑a0‑02 | 0x03 | 0x20 | 0x3a | **FREEZE / BLACK** |

> [!NOTE]
> **Conclusion:** `0xc0=0x02` and `0xc1=0x00` is the current stable baseline for data flow.

### Testing Module Cleanup

```bash
# Load the driver
sudo insmod driver/cx511h.ko force_input_mode=1

# Optional streaming test
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0 &
FFPLAY_PID=$!
sleep 5
kill $FFPLAY_PID 2>/dev/null || true

# Unload (should complete without freeze)
sudo rmmod cx511h

# Verify clean removal
echo "Driver removal exit code: $?"
dmesg | tail -5 | grep -i "cx511h\|board_remove\|pci_model_remove"
```

**Expected:** `rmmod` exits 0, no “Device or resource busy” errors, and no kernel warnings in the tested unload path.

---

##  How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel Parameters:** The currently tested setup uses these boot parameters (GRUB / systemd‑boot):
> ```bash
> ibt=off iommu=pt
> ```
> - `ibt=off` — May still be needed because this tree links against a precompiled vendor blob.
> - `iommu=pt` — Sets IOMMU passthrough mode (required for DMA access).

- **Kernel Headers:** Matching headers for the currently running kernel.
- **Build Tools:** `base‑devel`, `llvm`, `clang`.
- **Compiler Note:** Use `LLVM=1 CC=clang`. A plain `make` with GCC may fail on Clang‑built kernels (e.g., CachyOS).

***

### Installation

**Option A – Automated Build & Load (Recommended):**
```bash
# Build the module and copy it to the project root
./build.sh LLVM=1 CC=clang

# Load all dependencies and insert the module
./insmod.sh
```

**Option B – Manual Steps:**
```bash
# Build the module
cd driver
make clean && make LLVM=1 CC=clang

# Load dependencies in correct order
sudo modprobe snd snd-pcm
sudo modprobe videobuf2-common videobuf2-memops videobuf2-v4l2
sudo modprobe videobuf2-vmalloc videobuf2-dma-contig videobuf2-dma-sg
sudo modprobe videodev media

# Insert the module (optional parameters)
sudo rmmod cx511h 2>/dev/null || true
sudo insmod cx511h.ko force_input_mode=1 debug_pixel_format=-1
```

Both methods now handle dependencies correctly via `MODULE_SOFTDEP` declarations in the driver.

> [!NOTE]
> **Recent Stability Fixes Applied:** The driver now includes critical cleanup fixes that prevent kernel freezes during `rmmod`. Module loading/unloading should now be stable.

---

##  Debugging & Contributing

Validation is currently hardware‑driven; no automated CI or test suite exists yet.

### Quick Debug Commands
```bash
# Check if device is blocked by another process
sudo lsof /dev/video0

# Kill any blocking processes
sudo fuser -k /dev/video0

# Test continuous streaming
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/video0

# Debug byte‑order (if green screen persists)
sudo insmod cx511h.ko debug_pixel_format=0  # Try FPGA YUYV
sudo insmod cx511h.ko debug_pixel_format=1  # Try FPGA UYVY

# Check handshake and streaming logs
sudo dmesg | grep -iE "cx511h-hdmi|cx511h-v4l2|cx511h-color|cx511h-stream"
```

### Expected Buffer Patterns (YUV 4:2:2)
| State | Hex Pattern | Meaning |
|:---|:---:|:---|
| **Black Screen** | `10 80 10 80...` | Valid YUV stream, but Y=16 (black). |
| **Color Signal** | `85 7a 90 85...` | Active video flowing. |
| **Green Tint** | `80 10 80 10...` | UYVY/YUYV swap. Byte alignment issue. |

---

## ⚠️ Known Issues & Stability Notes

### 1. I2C Bus Collision (Streaming Freeze)
Accessing the I2C bus (reading registers) while the HDMI pixel stream is active triggers system‑wide freezes.
- **Solution:** Avoid `hdmirxrd()` calls in `stream_on`/`stream_off`. The driver uses a write‑only control path during these phases.

### 2. v4l2‑ctl Incompatibility Warning
- ❌ **Do NOT use `v4l2-ctl --stream-mmap` for testing.**
- **Reason:** Triggers unexpected I2C status reads during stream‑off sequence, causing a total **System Freeze** due to bus contention.
- **Alternative:** Use `ffplay` for safe testing.

### 3. Green Screen (Byte‑Order)
- **Mapping implemented** – FPGA byte‑order mapping active.
- **Fine‑tuning needed** – Use `debug_pixel_format` parameter to test different FPGA pixel‑format values.
- **Workaround** – Try `sudo insmod cx511h.ko debug_pixel_format=0` (YUYV) or `debug_pixel_format=1` (UYVY).

### 4. Format Negotiation Mismatch
- The active hardware capture path is forced to `YUYV`.
- Some userspace‑visible V4L2 format negotiation paths may still expose additional formats while the hardware path remains fixed.
- Treat non‑`YUYV` capture modes as experimental until the negotiation path is fully aligned.

### 5. Streaming Lifecycle – IMPROVED
- **Continuous streaming** – IRQ ACK + doorbell re‑queue implemented, data flows without "corrupted data" stops.
- **Buffer lifecycle** – Hardware re‑triggers after each buffer completion.
- **Remaining** – Color correction via FPGA format tuning (`debug_pixel_format`).

### 6. Toolchain Sensitivity
- The repository builds on the tested kernel with `LLVM=1 CC=clang`.
- A plain GCC build may fail immediately on Clang‑built kernels because the kernel build system passes Clang‑oriented flags to external modules.

---

##  Reverse Engineering Progress (April 2026)

### Community Analysis
This driver is based on community reverse engineering efforts to enable Linux support for hardware without official vendor drivers.

> [!CAUTION]
> **Legal Notice:**
> - This project currently includes the precompiled vendor archive `AverMediaLib_64.a`.
> - The handwritten glue code and reverse‑engineering notes are community‑maintained.
> - Review redistribution/licensing status carefully before republishing binaries or forks.
> - **Purpose:** Interoperability and Linux hardware support.
> - Non‑commercial, community‑driven project.

<!-- --- PHASE2-UPDATE --- -->
### Critical Missing Registers (Identified)

| Register | Type | Value | Purpose | Priority | Status |
|:---:|:---:|:---:|:---|:---:|:---:|
| **MMIO 0x10** | FPGA | `0x02` | IRQ ACK after EACH buffer | **CRITICAL** | ✅ IMPLEMENTED |
| **MMIO 0x304** | FPGA | Bit set | Doorbell re‑queue (next buffer ready) | **CRITICAL** | ✅ IMPLEMENTED |
| **I2C 0x20** | ITE68051 | `0x40` | Video Output Enable (Bit 6) | **CRITICAL** | ✅ IMPLEMENTED |
| **I2C 0x86** | ITE68051 | `0x01` | Global Enable | **HIGH** | ✅ IMPLEMENTED |
| **I2C 0x90** | ITE68051 | `0x8f` | IRQ Enable | **HIGH** | ✅ IMPLEMENTED |
| **I2C 0xA0‑A2** | ITE68051 | `0x80` | DMA Channel Enable | **HIGH** | ✅ IMPLEMENTED |
| **I2C 0xA4** | ITE68051 | `0x08` | DMA Enable | **HIGH** | ✅ IMPLEMENTED |
| **I2C 0xB0** | ITE68051 | `0x01` | Buffer Enable | **HIGH** | ✅ IMPLEMENTED |

### Current Focus: Color Correction & FPGA Format Tuning
- **Continuous streaming achieved** – IRQ ACK (MMIO 0x10) and doorbell re‑queue (MMIO 0x304) implemented.
- **I2C enable sequence complete** – 8 streaming‑control registers enabled in `stream_on`.
- **Remaining work** – Green screen indicates byte‑order mismatch; tuning via `debug_pixel_format` parameter (-1=auto, 0‑11=force FPGA format).
- **Next steps** – Fine‑tune FPGA pixel‑format value to match hardware output, validate color correctness.

### Current Technical Status

1. **Register Implementation = Major Progress**
   - Missing registers identified and implemented in Phase 2.
   - Timing/sequence validated through continuous streaming.
   - Edge‑triggered behavior confirmed for doorbell re‑queue.

2. **I2C Freeze Mitigated**
   - Write‑only control path during streaming avoids bus contention.
   - Read operations restricted to non‑streaming states.
   - Power‑state sequencing follows Windows‑driver unmute pattern.

3. **Streaming Lifecycle = Functional**
   - Continuous data flow without "corrupted data" stops.
   - Hardware re‑triggers after each buffer completion.
   - IRQ ACK + doorbell mechanism proven stable.

4. **Remaining Color Challenge**
   - FPGA pixel‑format mapping active but requires fine‑tuning.
   - Byte‑order mismatch causes green screen (YUYV/UYVY swap).
   - `debug_pixel_format` parameter allows systematic testing.

---

##  Reverse Engineering Methods
- Community‑driven register probing and analysis.
- V4L2 callback chain tracing.
- Iterative testing with actual hardware.

---

##  Disclaimer
This is an unofficial community project for educational and interoperability purposes only. AVerMedia does not endorse or support this driver. **Use at your own risk.**

---

**Maintained by [Everlite](https://github.com/Everlite).**  
Special thanks to original work by [derrod](https://github.com/derrod).