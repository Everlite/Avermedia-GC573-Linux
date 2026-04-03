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
| **Capture Content** | ⚠️ [EARLY] | First frames visible, but streaming lifecycle and buffer completion are still incomplete |
| **Driver Unload** | ✅ [OK] | Fixed lifecycle cleanup prevents kernel freezes; `rmmod` now works in current testing without kernel freezes |
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

- **Robust Streaming Initialization** – V4L2 `STREAMON` now triggers FPGA and HDMI‑receiver configuration. MSI interrupts eliminate interrupt storms.
- **Triple‑EDID & 1080p Force** – EDID overrides force the card to identify as a 1080p‑max device; pixel‑clock mismatch fixed.
- **YUV Data Path Forcing** – Hardware delivers YUV 4:2:2 (`10 80`). Driver forces `YUYV` across FPGA, ITE68051, and V4L2 layers.
- **V4L2 Color Space Metadata** – Explicit BT.709, limited‑range (16‑235) flags; register `0x2a` synchronized with V4L2 state.
- **HDMI Handshake & Stability** – Windows‑driver 3‑stage unmute sequence (`0xb0→0xa0→0x02`). Write‑only I2C path during streaming avoids freezes.
- **TTL Pixel Mode** – Single‑pixel/SDR output via registers `0xc0‑0xc4`.
- **Input Format Override** – Module parameter `force_input_mode` (0=auto, 1=YUV422, 2=YUV444, 3=RGB‑Full, 4=RGB‑Limited).
- **Critical Cleanup Fixes** – Module cleanup lifecycle fixed; `rmmod` no longer freezes the kernel in current testing: proper `board_remove()` sequence, PCI‑context use‑before‑release, negative error‑code returns.

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

### 1. I2C Bus Collision (Streaming Freeze)
Accessing the I2C bus (reading registers) while the HDMI pixel stream is active triggers system‑wide freezes.
- **Solution:** Avoid `hdmirxrd()` calls in `stream_on`/`stream_off`. The driver uses a write‑only control path during these phases.

### 2. v4l2‑ctl Incompatibility Warning
- ❌ **Do NOT use `v4l2-ctl --stream-mmap` for testing.**
- **Reason:** Triggers unexpected I2C status reads during stream‑off sequence, causing a total **System Freeze** due to bus contention.
- **Alternative:** Use `ffplay` for safe testing.

### 3. Format Negotiation Mismatch
- The active hardware capture path is forced to `YUYV`.
- Some userspace‑visible V4L2 format negotiation paths may still expose additional formats while the hardware path remains fixed.
- Treat non‑`YUYV` capture modes as experimental until the negotiation path is fully aligned.

### 4. Streaming Lifecycle Still In Progress
- `STREAMON` is wired into the real hardware init path, but continuous buffer re‑queue/IRQ completion is still under reverse engineering.
- Userspace start/stop behavior, OBS, GStreamer, and continuous streaming lifecycle should still be considered unstable.

### 5. Toolchain Sensitivity
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

### Critical Missing Registers (Identified)

| Register | Type | Value | Purpose | Priority |
|:---:|:---:|:---:|:---|:---:|
| **MMIO 0x10** | FPGA | `0x02` | IRQ ACK after EACH buffer | **CRITICAL** |
| **MMIO 0x304** | FPGA | Bit set | Doorbell re‑queue (next buffer ready) | **CRITICAL** |
| **I2C 0x20** | ITE68051 | `0x40` | Video Output Enable (Bit 6) | **CRITICAL** |
| **I2C 0x86** | ITE68051 | `0x01` | Global Enable | **HIGH** |
| **I2C 0x90** | ITE68051 | `0x8f` | IRQ Enable | **HIGH** |
| **I2C 0xA0‑A2** | ITE68051 | `0x80` | DMA Channel Enable | **HIGH** |
| **I2C 0xA4** | ITE68051 | `0x08` | DMA Enable | **HIGH** |
| **I2C 0xB0** | ITE68051 | `0x01` | Buffer Enable | **HIGH** |

### UPCOMING Testing Plan
All missing registers have been identified. Testing will focus on implementation of the re‑queue (MMIO 0x304) and interrupt acknowledgment (MMIO 0x10) mechanisms to achieve continuous data flow.

### Known Technical Risks

1. **Register Discovery ≠ Problem Solved**
   - Missing registers identified, but timing/sequence may vary.
   - Edge‑triggered vs level‑triggered bits unknown.
   - Read‑modify‑write behavior not fully documented.

2. **I2C Freeze May Be Deeper**
   - Could be locking issues between ISR/workqueue/stream thread.
   - Power‑state dependencies not fully understood.
   - Race conditions with reset/mute/unmute possible.

3. **ffplay Success ≠ Full Driver Success**
   - v4l2‑ctl, OBS, GStreamer may still fail.
   - V4L2 state machine lifecycle needs work.
   - Buffer queue management incomplete.

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