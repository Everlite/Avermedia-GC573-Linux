# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19+ Development)

This repository contains a **community-maintained, AI-assisted**, and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for the latest kernels.

---

## 🚀 Status: ⚠️ EXPERIMENTAL / BETA

**Kernel Compatibility:** Successfully builds and runs on **Kernel 6.19.6+** (CachyOS / Arch / Gentoo).

| Feature | Status |
|---|---|
| **Module Loading** | ✅ STABLE — No longer crashes the kernel upon loading |
| **Signal Detection** | ✅ FUNCTIONAL — Hardware syncs via forced 1080p EDID handshake |
| **IRQ / Interrupts** | ✅ WORKING — MSI interrupt allocation with INTx fallback |
| **System Stability** | ✅ STABLE — Hard lockup on STREAMON resolved via DMA safety guards |
| **DMA Transfer** | ✅ WORKING — Valid YUYV buffer data (4,147,200 bytes per frame) |
| **Capture** | 🔧 WIP — Signal is "Locked", DMA delivers frames, but pixel content is black (CSC mismatch under investigation) |

---

## ✨ Key Features & Fixes

### 1. Interrupt Infrastructure (MSI Support)

- **IRQ Allocation Fix:** Implemented proper PCIe interrupt request logic via `pci_alloc_irq_vectors`. The driver now correctly switches to **MSI (Message Signaled Interrupts)**, preventing the interrupt-storm that previously caused total system freezes.
- **Resource Management:** Fixed the legacy IRQ mapping to ensure the card can actually "talk" to the CPU without colliding with other hardware.

### 2. Triple-EDID & 1080p Force

- **EDID Override:** Patched three separate locations (`ite6805.h`, `ite6805_EDID.h`, `include/ite6805.h`) to force the card to identify as a **1080p-max device**. This prevents consoles (like PS5) from forcing an unstable 4K signal.
- **Timing Correction:** Fixed a critical **Hz vs. kHz mismatch** in the `pixel_clock` calculation. The driver now correctly targets **148.5 MHz** for 1080p60.
- **Single-Pixel Mode:** Forced the deactivation of `Dual-Pixel-Mode` and `DDR-Mode` to simplify the data path for modern V4L2 compatibility.

### 3. DMA Stability & STREAMON Safety

- **Hard Lockup Fix:** Added safety sequence before enabling video streaming: `streaming disable → 50ms settle → config → 200ms settle → streaming enable`.
- **DMA Address Logging:** Full debug output of scatter-gather descriptor addresses for every buffer prepare call.
- **Makefile Modernization:** Proper kbuild rule for linking the pre-compiled `AverMediaLib_64.a` blob, fixing "no rule to make target" errors.

### 4. Input Format Override (Module Parameter)

Runtime-configurable module parameter (`force_input_mode`) to manually set the HDMI input colorspace, bypassing ITE6805 auto-detection:

| Value | Mode | Description |
|---|---|---|
| `0` | Auto | ITE6805 detection (default) |
| `1` | YUV 4:2:2 | BT.709 Limited Range |
| `2` | YUV 4:4:4 | BT.709 Limited Range |
| `3` | RGB Full | 0-255, BT.709 |
| `4` | RGB Limited | 16-235, BT.709 |

```bash
# Set at load time:
sudo insmod cx511h.ko force_input_mode=3

# Change at runtime (no recompile needed):
echo 3 | sudo tee /sys/module/cx511h/parameters/force_input_mode
```

---

## 🛠️ How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel Parameters:** You **must** add these to your boot command line (GRUB / systemd-boot):
> ```
> ibt=off iommu=pt
> ```
> - `ibt=off` — Disables Intel Indirect Branch Tracking (required for proprietary blob compatibility)
> - `iommu=pt` — Sets IOMMU passthrough mode (required for DMA access)

- **Build Tools:** `base-devel`, `cmake`, `llvm`, `clang`

### Kernel Compatibility

The driver includes automatic compatibility shims for different kernel versions:

| Kernel Version | Status | Notes |
|---|---|---|
| **6.19+** | ✅ Tested | Primary development target (CachyOS) |
| **6.18** | ✅ Supported | `v4l2_fh_add/del` 2-arg API auto-detected |
| **6.8 – 6.17** | ✅ Supported | `PCI_IRQ_INTX` auto-detected |
| **5.x – 6.7** | ⚠️ Should work | Falls back to `PCI_IRQ_LEGACY` automatically |
| **< 5.0** | ❌ Untested | Not recommended |

### Installation

**1. Load Required Kernel Modules:**

```bash
sudo modprobe videobuf2-v4l2
sudo modprobe videobuf2-dma-sg
sudo modprobe videobuf2-dma-contig
sudo modprobe videobuf2-vmalloc
```

> [!NOTE]
> On **CachyOS**, these must be loaded **after every reboot** before loading the driver.
> Other distributions may load them automatically. To make this persistent:
> ```bash
> echo -e "videobuf2-v4l2\nvideobuf2-dma-sg\nvideobuf2-dma-contig\nvideobuf2-vmalloc" | sudo tee /etc/modules-load.d/avermedia.conf
> ```

**2. Clone & Build:**

```bash
git clone https://github.com/realEverlite/Avermedia-GC573-Linux.git
cd Avermedia-GC573-Linux
./build.sh LLVM=1
```

Or manually:
```bash
cd driver
make clean && make LLVM=1 CC=clang
```

**3. Load the Module:**

```bash
sudo insmod driver/cx511h.ko
# With input format override:
sudo insmod driver/cx511h.ko force_input_mode=3
```

**4. Verify via Logs:**

```bash
sudo dmesg | grep -i "cx511h"
```

Look for:
- `[cx511h-debug] FORCING 1080p mode now` — EDID override active
- `[cx511h-color] MANUAL OVERRIDE: Mode X` — Input format override active
- `[cx511h-dma] stream_on: AFTER enable_video_streaming — survived!` — Streaming works

---

## ⚠️ Known Blockers

- **Black Frame in OBS / V4L2:** DMA delivers valid YUYV data (correct frame sizes), but pixel content is `Y=16 U=128 V=128` (black). The FPGA's Color Space Converter (CSC) is suspected to discard pixel data. All `force_input_mode` values (0–4) still produce black frames.
- **VB2 Module Dependencies:** On CachyOS, `videobuf2-*` modules must be loaded manually before the driver (see installation steps above).

---

## 🔧 Debugging

```bash
# Check colorspace detection:
sudo dmesg | grep cx511h-color

# Check DMA addresses:
sudo dmesg | grep cx511h-dma

# Capture test frame:
v4l2-ctl -d /dev/video0 \
  --set-fmt-video=width=1920,height=1080,pixelformat=YUYV \
  --stream-mmap --stream-count=5 \
  --stream-to=/tmp/testframe.raw

# Check if frame is black (10 80 = black YUYV):
hexdump -C /tmp/testframe.raw | head -5

# Check / change input mode at runtime:
cat /sys/module/cx511h/parameters/force_input_mode
echo 3 | sudo tee /sys/module/cx511h/parameters/force_input_mode
```

---

## 🤝 Seeking Contributors

**The DMA bridge is built — now we just need the pixels to cross it!**

We need help with:

- **FPGA CSC Configuration:** Understanding the correct `vip_cfg` parameters for the Xilinx FPGA's Color Space Converter.
- **AverMediaLib_64.a Reverse Engineering:** The proprietary blob handles the FPGA register configuration. Understanding its internal logic would help fix the colorspace issue.
- **Kernel / V4L2 / PCIe / DMA expertise** is highly welcome.

If you're interested, open an issue or submit a PR!

---

## ⚖️ Disclaimer

This is an **unofficial, experimental patch**. Use at your own risk.

Maintained by [realEverlite](https://github.com/realEverlite). Special thanks to the community and original work by [derrod](https://github.com/derrod).
