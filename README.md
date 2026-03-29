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
| **System Stability** | [OK] STABLE — Hard lockups resolved via MSI and STREAMON safety guards |
| **DMA Transfer** | [OK] WORKING — Valid data flowing to buffers (4,147,200 bytes/frame) |
| **Capture Content** | [WIP] — Transitioned from Black Screen to **Green Flickering Signal**. Raw data is reaching user-space, but Color Space/TTL alignment is still under investigation. |

---

##  Key Features & Recent Progress

### 1. Robust Streaming Initialization
- **V4L2 Callback Bridge:** Discovered and fixed a critical issue where the hardware initialization logic was disconnected from the active V4L2 `STREAMON` path. The driver now correctly triggers FPGA and HDMI-Receiver configuration when a capture starts.
- **MSI Interrupts:** Migrated from legacy INTx to proper **MSI (Message Signaled Interrupts)**, eliminating the "interrupt storm" system freezes.

### 2. Triple-EDID & 1080p Force
- **EDID Override:** Patched multiple locations to force the card to identify as a **1080p-max device**. This prevents signal handshake failures with modern consoles.
- **Timing Correction:** Fixed a **Hz vs. kHz mismatch** in the `pixel_clock` calculation, ensuring stable 1080p60 targeting.

### 3. ITE68051 Manual Register Overrides (Experimental)
The driver now includes the ability to manually override internal registers of the ITE68051 HDMI receiver to troubleshoot the Color Space Mismatch:
- **CSC (Color Space Converter):** Force Input Format (0x6b), Enable/Bypass CSC (0x6c), and Select Color Standard (0x6e - BT.709/BT.601).
- **TTL Output Interface:** Manually configure the TTL bus mode (0xc0/0xc1) and AVI InfoFrame Colorimetry (0x98).

### Tested CSC Register Combinations (ITE68051)

| Test | 0x6b | 0x6c | 0x6e | 0x98 | 0xc0 | 0xc1 | Result |
|------|------|------|------|------|------|------|--------|
| 1 | 0x02 | 0x00 | 0x01 | - | - | - | Green |
| 2 | 0x00 | 0x01 | 0x01 | - | - | - | Green |
| 3 | 0x03 | 0x01 | 0x01 | - | - | - | Green |
| 4 | 0x02 | 0x01 | 0x01 | 0x00 | 0x01 | 0x00 | Green |
| 5 | 0x02 | 0x00 | 0x01 | 0x02 | 0x00 | 0x00 | Green |

**Conclusion:** Data is flowing (green = YUV/RGB mismatch). Next: Investigate FPGA input format expectations or additional ITE68051 registers (0xb0, 0xb1).


### 4. Input Format Override (Module Parameter)
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

##  Debugging & Contributing

### Current Blocker: "Green Screen / Flickering"
We have successfully built the "DMA bridge" — data is now definitely arriving in user-space buffers. However, the exact alignment of color bits or the TTL output configuration compared to what the FPGA expects is still not perfect.

**How you can help:**
If you have access to a Windows machine with a working GC573, please help us by gathering **MMIO/PCIe register logs** using **RWEverything**. Specifically, we need logs of the writes performed during the moments a video preview starts in the official AVerMedia software.

- **Check logs:** `sudo dmesg | grep cx511h`
- **Verify data flow:** `v4l2-ctl --stream-mmap --stream-count=5 --stream-to=/tmp/test.raw`
- **Register target:** ITE68051 (I2C Slave 0x58/0x90).

---

##  Reverse Engineering Methods
- **Ghidra** for Windows driver analysis (`AVXGC573_x64.sys`)
- **ITE68051 register mapping** extracted from official driver sessions
- **V4L2 callback chain** traced to identify initialization gaps

---

##  Disclaimer
Unofficial community fork. Maintained by [Everlite](https://github.com/Everlite). Special thanks to original work by [derrod](https://github.com/derrod).
