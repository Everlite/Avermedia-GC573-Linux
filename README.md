# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19+ Development)

This repository contains a **community-maintained, AI-assisted**, and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for the latest kernels and stabilized for production-like testing.

---

##  Status: [EXPERIMENTAL] / BETA

**Kernel Compatibility:** Successfully builds and runs on **Kernel 6.19.10+** (CachyOS / Arch / Gentoo).

| Feature | Status |
|---|---|
| **Module Loading** | [OK] STABLE — No longer crashes the kernel upon loading |
| **Signal Detection** | [OK] FUNCTIONAL — Hardware syncs via forced 1080p EDID handshake |
| **IRQ / Interrupts** | [OK] WORKING — MSI interrupt allocation with INTx fallback |
| **System Stability** | [OK] STABLE — I2C read-locks during streaming resolved |
| **DMA Transfer** | [OK] FUNCTIONAL — Stable data flow confirmed via ffplay/xxd |
| **Capture Content** | [BETA] — Raw YUV 4:2:2 flowing. Window opens and shows flickering signal. |

---

##  Key Features & Recent Progress

### 1. Robust Streaming Initialization
- **V4L2 Callback Bridge:** Discovered and fixed a critical issue where the hardware initialization logic was disconnected from the active V4L2 `STREAMON` path. The driver now correctly triggers FPGA and HDMI-Receiver configuration when a capture starts.
- **MSI Interrupts:** Migrated from legacy INTx to proper **MSI (Message Signaled Interrupts)**, eliminating the "interrupt storm" system freezes.

### 2. Triple-EDID & 1080p Force
- **EDID Override:** Patched multiple locations to force the card to identify as a **1080p-max device**. This prevents signal handshake failures with modern consoles.
- **Timing Correction:** Fixed a **Hz vs. kHz mismatch** in the `pixel_clock` calculation, ensuring stable 1080p60 targeting.

### 3. YUV Data Path Forcing (Color Correction)
We have confirmed through buffer analysis (`xxd`) that the hardware delivers **YUV 4:2:2** data (`10 80`). The driver now forces a consistent YUV path across all layers:
- **V4L2 Layer**: Restricted to `YUYV` only to prevent color interpretation errors.
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
|------|------|------|------|------|------|------|------|--------|
| D    | 0x02 | 0x02 | 0x01 | -    | 0x00 | 0x00 | 0x3a | **LOCKED YUV** |
| Unmute | 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x00 | 0x00 | 0x3a | **Success** |
| TTL | 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x02 | 0x00 | 0x3a | **FLICKERING DATA** |
| Phase| 0x02 | 0x02 | 0x01 | 0xb0-a0-02 | 0x03 | 0x20 | 0x3a | **FREEZE / BLACK** |

**Conclusion:** The configuration `0xc0=0x02` and `0xc1=0x00` is the current stable baseline for data flow.


### 6. Input Format Override (Module Parameter)
Runtime-configurable module parameter (`force_input_mode`) to manually set the HDMI input colorspace:

| Value | Mode | Description |
|---|---|---|
| `0` | Auto | ITE6805 detection (default) |
| `1` | YUV 4:2:2 | BT.709 Limited Range |
| `2` | YUV 4:4:4 | BT.709 Limited Range |
| `3` | RGB Full | 0-255, BT.709 |
| `4` | RGB Limited | 16-235, BT.709 |

---

##  How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel Parameters:** You **must** add these to your boot command line (GRUB / systemd-boot):
> ```bash
> ibt=off iommu=pt
> ```
> - `ibt=off` — Disables Intel Indirect Branch Tracking (required for proprietary blob compatibility)
> - `iommu=pt` — Sets IOMMU passthrough mode (required for DMA access)

- **Build Tools:** `base-devel`, `cmake`, `llvm`, `clang`

### Kernel Compatibility

| Kernel Version | Status | Notes |
|---|---|---|
| **6.18 – 6.19+** | [OK] Tested | Primary development target (CachyOS) |
| **6.8 – 6.17** | [OK] Supported | `PCI_IRQ_INTX` compat defines included |
| **5.x – 6.7** | [EXPERIMENTAL] Should work | Falls back to `PCI_IRQ_LEGACY` |

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

---

## Debugging & Contributing

### Quick Debug Commands
```bash
# Check if device is blocked by another process
sudo lsof /dev/video0

# Kill any blocking processes
sudo fuser -k /dev/video0

# Test with ffplay (1080p60 YUYV)
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0

# Analyze raw buffer content
v4l2-ctl --stream-mmap --stream-count=1 --stream-to=/tmp/test.raw
xxd /tmp/test.raw | head -30

# Check handshake and color logs
sudo dmesg | grep -iE "cx511h-hdmi|cx511h-v4l2|cx511h-color"
```

### Expected Buffer Patterns (YUV 4:2:2)
| State | Hex Pattern | Meaning |
|---|---|---|
| **Black Screen** | `10 80 10 80...` | Valid YUV stream, but Y=16 (black). Handshake ok. |
| **Color Signal** | `85 7a 90 85...` | Varying luma/chroma. Active video flowing. |
| **Green Tint** | `80 10 80 10...` | UYVY/YUYV swap. Byte alignment issue. |

---

---

## Known Issues & Stability Notes

### 1. Driver Unloading (Device Busy)
The driver often fails to unload cleanly.
- **Symptom**: `sudo rmmod cx511h` returns `Device or resource busy`.
- **Cause**: V4L2 device file references or DMA memory buffers are not fully released in some states.
- **WARNING**: Do **NOT** use `rmmod -f` (forced removal). This triggers a **Kernel Freeze**, requiring a hard reset.

### 2. I2C Bus Collision (Streaming Freeze)
Accessing the I2C bus (reading registers) while the HDMI pixel stream is active triggers system-wide freezes on many systems.
- **Solution**: Avoid `hdmirxrd()` calls in `stream_on`/`stream_off`. The driver is currently configured for write-only control during these phases to prevent lockups.

### 3. Flickering / Sync Issues
While the DMA path is active (`ffplay` window opens), the signal may still flicker or show green distortions. This is a synchronization issue between the receiver TTL phase and the FPGA internal clock.
- **PS5 HDCP**: Ensure HDCP is disabled in PS5 settings.

---

### 7. Critical Register Discovery (March 29, 2026)

**Registers to AVOID (NOT TTL Output Control):**

| Register | Actual Purpose | Why NOT to touch |
|----------|---------------|------------------|
| **0x51/0x52** | DFE Channel Tuning (RX Calibration) | Not TTL! Causes instability |
| **0x53/0x54** | DFE Calibration Path | Same as above |

**Correct TTL Registers:**

| Register | Purpose | Value for 1080p60 |
|----------|---------|------------------|
| **0xc0** | TTL Base Mode | 0x02 (Single/SDR) |
| **0xc1** | Lane/DDR Config | 0x00 (No DDR) |
| **0xbd** | Pixel Mode Enum | 0x00 |
| **0xbe** | Counter Reset | 0x00 |
| **0xc4** | DDR Flag | 0x00 |

### 8. v4l2-ctl Incompatibility Warning
- ❌ **Do NOT use `v4l2-ctl --stream-mmap` for testing**
- **Reason:** Triggers unexpected I2C status reads during stream_off sequence, which causes a total **System Freeze** (Kernel Lockup) due to bus contention during DMA.
- **Alternative:** Use `ffplay` for safe testing:
  ```bash
  ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0
  ```

---

## Reverse Engineering Methods
- **Ghidra** for Windows driver analysis (`AVXGC573_x64.sys`)
- **ITE68051 register mapping** extracted from official driver sessions
- **V4L2 callback chain** traced to identify initialization gaps

---

## Disclaimer
Unofficial community fork. Maintained by [Everlite](https://github.com/Everlite). Special thanks to original work by [derrod](https://github.com/derrod).
