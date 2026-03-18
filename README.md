# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19+ Development)

This repository contains a **community-maintained, AI-assisted**, and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for the latest kernels.

---

## 🚀 Status: ⚠️ EXPERIMENTAL / BETA

**Kernel Compatibility:** Successfully builds and runs on **Kernel 6.19.6** (CachyOS / Arch / Gentoo).

| Feature | Status |
|---|---|
| **Module Loading** | ✅ STABLE — No longer crashes the kernel upon loading |
| **Signal Detection** | ✅ FUNCTIONAL — Hardware syncs via forced 1080p EDID handshake |
| **System Stability** | ✅ MAJOR IMPROVEMENT — "Hard Lockup" resolved through proper IRQ allocation (MSI) |
| **Capture** | 🔧 WIP — Signal is "Locked", but V4L2 buffers output a black frame |

---

## ✨ Recent Major Breakthroughs (March 2026)

### 1. Interrupt Infrastructure (MSI Support)

- **IRQ Allocation Fix:** Implemented proper PCIe interrupt request logic via `pci_alloc_irq_vectors`. The driver now correctly switches to **MSI (Message Signaled Interrupts)**, preventing the interrupt-storm that previously caused total system freezes.
- **Resource Management:** Fixed the legacy IRQ mapping to ensure the card can actually "talk" to the CPU without colliding with other hardware.

### 2. Triple-EDID & 1080p Force

- **EDID Override:** Patched three separate locations (`ite6805.h`, `ite6805_EDID.h`, `include/ite6805.h`) to force the card to identify as a **1080p-max device**. This prevents consoles (like PS5) from forcing an unstable 4K signal.
- **Timing Correction:** Fixed a critical **Hz vs. kHz mismatch** in the `pixel_clock` calculation. The driver now correctly targets **148.5 MHz** for 1080p60.

### 3. Hardware "Signal Sanitization" (Logic Override)

- **Hardcoded 1080p Mode:** Implemented a debug-level override that intercepts 4K signals and re-interprets them as stable 1080p streams.
- **Single-Pixel Mode:** Forced the deactivation of `Dual-Pixel-Mode` and `DDR-Mode` to simplify the data path for modern V4L2 compatibility.

---

## 🛠️ How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Intel Users:** You **must** disable Indirect Branch Tracking (IBT).
> Add `ibt=off` to your kernel command line (e.g. in GRUB or systemd-boot).

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



**1. Clone & Build:**

```bash
git clone https://github.com/realEverlite/Avermedia-GC573-Linux.git
cd Avermedia-GC573-Linux
./build.sh LLVM=1
```

**2. Load the Module:**

```bash
sudo insmod driver/cx511h.ko
```

**3. Verify via Logs:**

```bash
sudo dmesg | grep -i "cx511h"
```

Look for: `[cx511h-debug] FORCING 1080p mode now`

---

## ⚠️ Known Blockers

- **Black Frame in OBS / V4L2:** While the signal is "Locked" at 60 FPS, the actual pixel data is not yet reaching the V4L2 user-space buffers.
- **DMA Buffer Handover:** Investigation needed in `board_v4l2.c` regarding the `vb2_buffer` filling process.

---

## 🤝 Seeking Contributors

**The bridge is built — now we just need the pixels to cross it!**

We need help with:

- **V4L2 Videobuf2 (VB2):** Mapping hardware DMA memory to V4L2 buffers.
- **DMA Debugging:** Tracing why the proprietary blob doesn't fill the allocated buffers.
- **Kernel / V4L2 / PCIe / DMA expertise** is highly welcome.

If you're interested, open an issue or submit a PR!

---

## ⚖️ Disclaimer

This is an **unofficial, experimental patch**. Use at your own risk.

Maintained by [realEverlite](https://github.com/realEverlite). Special thanks to the community and original work by [derrod](https://github.com/derrod).
