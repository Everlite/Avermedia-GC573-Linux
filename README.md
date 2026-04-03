# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19+ Development)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19%2B%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)
[![Issues](https://img.shields.io/github/issues/Everlite/Avermedia-GC573-Linux.svg)](https://github.com/Everlite/Avermedia-GC573-Linux/issues)

This repository contains a **community-maintained, AI-assisted**, and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for recent kernels and is currently aimed at reverse engineering, hardware bring-up, and experimental capture testing.

> [!NOTE]
> This tree currently links against the precompiled vendor archive `AverMediaLib_64.a` for low-level hardware logic.

---

##  Status: [EXPERIMENTAL] / ALPHA

**Kernel Compatibility:** Successfully builds and runs on **Kernel 6.19.10+** (CachyOS / Arch / Gentoo).

| Feature | Status | Description |
|:---|:---:|:---|
| **Build / Toolchain** | ⚠️ [CLANG] | Tested build path is `LLVM=1 CC=clang`; plain GCC builds may fail on Clang-built kernels |
| **Module Loading** | ✅ [OK] | STABLE — No longer crashes the kernel upon loading |
| **Signal Detection** | ✅ [OK] | FUNCTIONAL — Hardware syncs via forced 1080p EDID handshake |
| **IRQ / Interrupts** | ✅ [OK] | WORKING — MSI interrupt allocation with INTx fallback |
| **System Stability** | ⚠️ [FRAGILE] | I2C read-locks during streaming cause system freezes |
| **DMA Transfer** | ✅ [OK] | FUNCTIONAL — Data flow confirmed via ffplay/xxd |
| **Capture Content** | ⚠️ [EARLY] | First frames visible, but streaming lifecycle and buffer completion are still incomplete |
| **Driver Unload** | ✅ [OK] | Fixed lifecycle cleanup prevents kernel freezes; `rmmod` now works reliably |
| **General Use** | ❌ [NOT REC] | For development/testing only — NOT FOR DAILY USE |

### Development Phase Status

| Phase | Status | Description |
|:---|:---:|:---|
| **Phase 1** | ✅ DONE | Builds on modern kernels, module loads stable |
| **Phase 2** | ✅ DONE | HDMI lock, first DMA data, format recognized |
| **Phase 3** | 🔄 IN PROGRESS | Continuous streaming, buffer lifecycle, IRQ completion |
| **Phase 4** | ⏳ PENDING | Robust start/stop, suspend/resume, OBS/GStreamer support |

---

##  Key Features & Recent Progress

### 1. Robust Streaming Initialization
- **V4L2 Callback Bridge:** Discovered and fixed a critical issue where the hardware initialization logic was disconnected from the active V4L2 `STREAMON` path. The driver now correctly triggers FPGA and HDMI-Receiver configuration when a capture starts.
- **MSI Interrupts:** Migrated from legacy INTx to proper **MSI (Message Signaled Interrupts)**, eliminating the "interrupt storm" system freezes.
- **Lifecycle Caveat:** The `STREAMON` path is now connected, but continuous streaming, re-queue/IRQ completion, and teardown are still under active work.

### 2. Triple-EDID & 1080p Force
- **EDID Override:** Patched multiple locations to force the card to identify as a **1080p-max device**. This prevents signal handshake failures with modern consoles.
- **Timing Correction:** Fixed a **Hz vs. kHz mismatch** in the `pixel_clock` calculation, ensuring stable 1080p60 targeting.

### 3. YUV Data Path Forcing (Color Correction)
We have confirmed through buffer analysis (`xxd`) that the hardware delivers **YUV 4:2:2** data (`10 80`). The driver now forces a consistent YUV path across all layers:
- **Hardware Path**: The active capture path is forced to `YUYV` to prevent color interpretation errors.
- **Userspace Format Negotiation**: Still being tightened. Some V4L2 paths may still advertise formats beyond the currently forced hardware path.
- **FPGA Layer**: Forced `AVER_XILINX_FMT_YUYV` in the video process pipeline.
- **ITE68051 Layer**: Synchronized inputs and TTL output to YUV mode.

### 4. V4L2 Color Space Metadata
Fixed issues where players like `ffplay` reported "unknown range/csp" or washed-out colors:
- **Explicit Metadata**: The driver now explicitly reports **BT.709**, **Limited Range (16-235)**, and correctly encoded YCbCr flags to the OS.
- **AVI InfoFrame Sync**: Register `0x2a` is now synchronized with the V4L2 state to ensure the HDMI receiver and OS agree on the signal type.

### 5. HDMI Handshake & Stability Fixes
- **Video Unmute Sequence**: Implemented the official Windows driver 3-stage sequence (`0xb0 -> 0xa0 -> 0x02`) to "wake up" the video output buffers on the HDMI receiver.
- **I2C Safety (Critical)**: Discovered that reading ITE68051 registers (I2C) while DMA is active causes instant system freezes. The driver now uses a **Write-Only** initialization path during streaming to maintain kernel stability.
- **TTL Pixel Mode**: Identified and implemented true TTL bus configuration (Registers `0xc0-0xc4`) for Single-Pixel/SDR output.

### Tested Register Combinations (ITE68051 Final Status)

| Test | 0x6b | 0x6c | 0x6e | 0x23 | 0xc0 | 0xc1 | 0x2a | Result |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **D** | 0x02 | 0x02 | 0x01 | - | 0x00 | 0x00 | 0x3a | **LOCKED YUV** |
| **Unmute** | 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x00 | 0x00 | 0x3a | **Success** |
| **TTL** | 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x02 | 0x00 | 0x3a | **FLICKERING DATA** |
| **Phase** | 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x03 | 0x20 | 0x3a | **FREEZE / BLACK** |

> [!NOTE]
> **Conclusion:** The configuration `0xc0=0x02` and `0xc1=0x00` is the current stable baseline for data flow.

### 6. Input Format Override (Module Parameter)
Runtime-configurable module parameter (`force_input_mode`) to manually set the HDMI input colorspace:

| Value | Mode | Description |
|:---:|:---|:---|
| `0` | Auto | ITE6805 detection (default) |
| `1` | YUV 4:2:2 | BT.709 Limited Range |
| `2` | YUV 4:4:4 | BT.709 Limited Range |
| `3` | RGB Full | 0-255, BT.709 |
| `4` | RGB Limited | 16-235, BT.709 |

---

##  Critical Stability Fixes (April 2025)

### 1. Fixed `rmmod` Kernel Freeze (Use-After-Free)
**Problem:** Module removal (`rmmod`) caused kernel freezes due to incorrect cleanup order and use-after-free in PCI context.
**Solution:** 
- **Proper `board_remove()` sequence:** Implemented hardware streaming stop, IRQ free, context release before PCI teardown.
- **Fixed `pci_model_remove()`:** Reordered operations to use PCI context before `cxt_manager_release()`.
- **New helper functions:** Added `board_v4l2_stop()` and `board_alsa_stop()` for clean hardware shutdown.

### 2. Fixed Probe Error Paths
**Problem:** Error paths in `board_probe()` returned positive numbers instead of negative kernel error codes.
**Solution:** Converted all error returns to proper negative kernel error codes (`-ENODEV`, `-ENOMEM`, `-EIO`).

### 3. Enhanced Streaming Lifecycle
**Problem:** Incomplete streaming stop during module removal left hardware active.
**Solution:**
- `board_v4l2_stop()`: Stops video streaming, powers off ITE6805 chip, sets LED to off state.
- `board_alsa_stop()`: Stops audio streaming before PCI resource release.
- `v4l2_model_streamoff()`: Ensures V4L2 model properly stops streaming.

### Cleanup Order (Prevents Use-After-Free):
1. Stop hardware streaming (video & audio)
2. Free IRQs while PCI context is still valid
3. Unmap MMIO BARs
4. Release context manager (frees PCI context memory)
5. Disable PCI device and release regions

---

##  Testing Module Cleanup

Verify that the driver can be cleanly loaded and unloaded without kernel freezes:

```bash
# 1. Load the driver
sudo insmod driver/cx511h.ko force_input_mode=1

# 2. Test streaming (optional but recommended)
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0 &
FFPLAY_PID=$!
sleep 5
kill $FFPLAY_PID 2>/dev/null || true

# 3. Unload the driver (should complete without freeze)
sudo rmmod cx511h

# 4. Verify clean removal
echo "Driver removal exit code: $?"
dmesg | tail -5 | grep -i "cx511h\|board_remove\|pci_model_remove"
```

**Expected Results:**
- `rmmod` completes with exit code 0 (success)
- No "Device or resource busy" errors
- No kernel warnings or panics in `dmesg`
- Driver can be immediately reloaded without reboot

**Debugging Tips:**
- If `rmmod` hangs, check for active userspace processes with `lsof /dev/video0`
- Monitor kernel logs in real-time: `sudo dmesg -w`
- The fixes ensure: hardware streaming stops before IRQ free, PCI context used before release, proper error code returns

---

##  How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel Parameters:** The currently tested setup uses these boot parameters (GRUB / systemd-boot):
> ```bash
> ibt=off iommu=pt
> ```
> - `ibt=off` — May still be needed on some systems because this tree links against a precompiled vendor blob.
> - `iommu=pt` — Sets IOMMU passthrough mode (required for DMA access).

- **Kernel Headers:** Matching headers for the currently running kernel
- **Build Tools:** `base-devel`, `llvm`, `clang`
- **Compiler Note:** Use `LLVM=1 CC=clang`. A plain `make` with GCC may fail on Clang-built kernels (for example CachyOS).

***

### Installation

**1. Load Required Kernel Modules:**
```bash
sudo modprobe videobuf2-v4l2 videobuf2-dma-sg videobuf2-dma-contig videobuf2-vmalloc
```

**2. Build & Load:**
```bash
cd driver
make clean && make LLVM=1 CC=clang
sudo rmmod cx511h || true
sudo insmod cx511h.ko force_input_mode=1
```

**Helper Script:**
```bash
./build.sh LLVM=1 CC=clang
```
`build.sh` forwards its arguments to `make`, so pass the Clang flags explicitly.

> [!NOTE]
> **Recent Stability Fixes Applied:** The driver now includes critical cleanup fixes that prevent kernel freezes during `rmmod`. The fixes ensure proper hardware shutdown sequence, correct PCI context management, and proper error code returns. Module loading/unloading should now be stable.


---

##  Debugging & Contributing

### Validation Status
Validation is currently hardware-driven. There is no automated CI or test suite in this repository yet.

### Quick Debug Commands
```bash
# Check if device is blocked by another process
sudo lsof /dev/video0

# Kill any blocking processes
sudo fuser -k /dev/video0

# Test with ffplay (1080p60 YUYV)
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0

# Check handshake and color logs
sudo dmesg | grep -iE "cx511h-hdmi|cx511h-v4l2|cx511h-color"
```

### Expected Buffer Patterns (YUV 4:2:2)
| State | Hex Pattern | Meaning |
|:---|:---:|:---|
| **Black Screen** | `10 80 10 80...` | Valid YUV stream, but Y=16 (black). |
| **Color Signal** | `85 7a 90 85...` | Active video flowing. |
| **Green Tint** | `80 10 80 10...` | UYVY/YUYV swap. Byte alignment issue. |

---

## ⚠️ Known Issues & Stability Notes

### 1. Driver Unloading (Device Busy) - FIXED
The driver unloading issue has been resolved with proper cleanup lifecycle.
- **Previous Symptom:** `sudo rmmod cx511h` returned `Device or resource busy` or caused kernel freezes.
- **Fix:** Implemented proper cleanup sequence: stop hardware streaming → free IRQs → unmap MMIO → release contexts → disable PCI device.
- **Current Status:** `rmmod` now works reliably without kernel freezes.

### 2. I2C Bus Collision (Streaming Freeze)
Accessing the I2C bus (reading registers) while the HDMI pixel stream is active triggers system-wide freezes.
- **Solution:** Avoid `hdmirxrd()` calls in `stream_on`/`stream_off`. The driver is currently configured for write-only control during these phases.

### 3. v4l2-ctl Incompatibility Warning
- ❌ **Do NOT use `v4l2-ctl --stream-mmap` for testing.**
- **Reason:** Triggers unexpected I2C status reads during stream_off sequence, which causes a total **System Freeze** due to bus contention.
- **Alternative:** Use `ffplay` for safe testing.

### 4. Format Negotiation Mismatch
- The active hardware capture path is currently forced to `YUYV`.
- Some userspace-visible V4L2 format negotiation paths may still expose additional formats while the hardware path remains fixed.
- Treat non-`YUYV` capture modes as experimental until the negotiation path is fully aligned with the forced data path.

### 5. Streaming Lifecycle Still In Progress
- `STREAMON` is wired into the real hardware init path, but continuous buffer re-queue/IRQ completion is still under reverse engineering.
- Start/stop, OBS, GStreamer, and clean teardown should still be considered unstable.

### 6. Toolchain Sensitivity
- The repository builds on the tested kernel with `LLVM=1 CC=clang`.
- A plain GCC build may fail immediately on Clang-built kernels because the kernel build system passes Clang-oriented flags through to external modules.

---

##  Reverse Engineering Progress (April 2025)

### Community Analysis
This driver is based on community reverse engineering efforts to enable Linux support for hardware without official vendor drivers.

> [!CAUTION]
> **Legal Notice:**
> - This project currently includes the precompiled vendor archive `AverMediaLib_64.a`.
> - The handwritten glue code and reverse-engineering notes are community-maintained.
> - Review redistribution/licensing status carefully before republishing binaries or forks.
> - **Purpose:** Interoperability and Linux hardware support.
> - Non-commercial, community-driven project.

### Critical Missing Registers (Identified)

| Register | Type | Value | Purpose | Priority |
|:---:|:---:|:---:|:---|:---:|
| **MMIO 0x10** | FPGA | `0x02` | IRQ ACK after EACH buffer |  **CRITICAL** |
| **MMIO 0x304** | FPGA | Bit set | Doorbell re-queue (next buffer ready) |  **CRITICAL** |
| **I2C 0x20** | ITE68051 | `0x40` | Video Output Enable (Bit 6) |  **CRITICAL** |
| **I2C 0x86** | ITE68051 | `0x01` | Global Enable |  **HIGH** |
| **I2C 0x90** | ITE68051 | `0x8f` | IRQ Enable |  **HIGH** |
| **I2C 0xA0-A2** | ITE68051 | `0x80` | DMA Channel Enable |  **HIGH** |
| **I2C 0xA4** | ITE68051 | `0x08` | DMA Enable |  **HIGH** |
| **I2C 0xB0** | ITE68051 | `0x01` | Buffer Enable |  **HIGH** |

### UPCOMING Testing Plan
All missing registers have been identified. Testing will focus on implementation of the re-queue (MMIO 0x304) and interrupt acknowledgment (MMIO 0x10) mechanisms to achieve continuous data flow.

### Known Technical Risks

1. **Register Discovery ≠ Problem Solved**
   - Missing registers identified, but timing/sequence may vary.
   - Edge-triggered vs level-triggered bits unknown.
   - Read-modify-write behavior not fully documented.

2. **I2C Freeze May Be Deeper**
   - Could be locking issues between ISR/workqueue/stream thread.
   - Power-state dependencies not fully understood.
   - Race conditions with reset/mute/unmute possible.

3. **ffplay Success ≠ Full Driver Success**
   - v4l2-ctl, OBS, GStreamer may still fail.
   - V4L2 state machine lifecycle needs work.
   - Buffer queue management incomplete.

---

##  Reverse Engineering Methods
- Community-driven register probing and analysis.
- V4L2 callback chain tracing.
- Iterative testing with actual hardware.

---

##  Disclaimer
This is an unofficial community project for educational and interoperability purposes only. AVerMedia does not endorse or support this driver. **Use at your own risk.**

---

**Maintained by [Everlite](https://github.com/Everlite).**
Special thanks to original work by [derrod](https://github.com/derrod).
