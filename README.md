# AVerMedia Live Gamer 4K (GC573) - Linux Driver (Kernel 6.19+ Development)

This repository contains a community-maintained, AI-Agent driven and heavily patched version of the AVerMedia GC573 Linux driver. It has been modernized for the latest kernels, but **is currently in an unstable state.**

### ⚠️ Essential Kernel Parameter (Intel Users)
Since the proprietary binary blob is not compatible with Intel's Indirect Branch Tracking (IBT), you **must** disable it in your bootloader, or the module will fail to load.

**Add this to your kernel command line:**
ibt=off

## 🚀 Status: ⚠️ UNSTABLE / BROKEN / WIP

*   **Kernel Compatibility:** Builds on **Kernel 6.19.6** (CachyOS / Arch).
*   **Current State:** 
    *   **Loads:** Yes (often auto-loads if previously installed).
    *   **Signal Detection:** Hardware appears to detect signal (ITE6805 fallback). Kernel logs confirm **4K/60Hz sync**, but V4L2 reporting remains stuck at "no power".
    *   **Capture:** **NOT WORKING.** Attempting to capture or preview results in no image or system instability.
    *   **Unloading:** **CRITICAL BUG.** Attempting to unload the module (`rmmod`) or using `rmmod -f` causes a **complete system freeze**.

## ✨ Recent Changes & Fixes (Under Investigation)

### 1. Modern Kernel Support (V4L2 & IBT)
- **API Modernization**: Updated to the new 2-argument `v4l2_fh_add/del` signatures.
- **Intel IBT (CET) Bypass**: Implemented `MODULE_INFO(ibt, "N")` and custom CFLAGS to allow the proprietary binary blob to run.

### 2. Stability Architecture (Workqueues)
- **Tasklet to Workqueue Conversion**: Replaced the legacy tasklet system with a workqueue to prevent "scheduling while atomic" panics. 
- **Atomic-Aware Locking**: Patched system wrappers to detect atomic context.

### 3. I2C & Signal Logic
- **Linker Fallback**: Manual `attach` for the ITE6805 chip.
- **Device Registration**: Manual registration to expose the chip to V4L2.

## 🛠️ How to Build & Install (FOR DEVELOPERS ONLY)

**WARNING: Running this driver may freeze your system.**

### Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/realEverlite/Avermedia-GC573-Linux.git
    cd Avermedia-GC573-Linux
    ```

2.  **Build:**
    ```bash
    ./build.sh LLVM=1
    ```

3.  **Load:**
    ```bash
    sudo insmod driver/cx511h.ko
    ```

## ⚠️ Known Blockers
1.  **System Freeze on Unload**: The module reference count management or hardware teardown is broken, leading to a hard lockup during `rmmod`.
2.  **No Image Data**: Although the signal is detected, the DMA transfer or V4L2 buffer management is not delivering actual video frames.

## ⚖️ Disclaimer

**This is an unofficial, experimental patch. Use at your own risk.**
This project is currently a work-in-progress to rescue the driver for modern Linux systems.

---
*Maintained by the community. Credits to derrod.*
