# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted, heavily patched Linux driver for the AVerMedia GC573 (PCI `1461:0054`, subsystem `1461:5730`).
Modernized for recent kernels. **Experimental — development and testing only.**

**Last aligned with code:** 2026-06-11 · **Phase 3 breakthrough** (DMA live — IOMMU / vb2 device binding fixed)

> [!NOTE]
> **Vendor blob:** The driver links against `AverMediaLib_64.a` in the **repository root** (~565 KB, tracked in git). The Makefile copies it into `driver/AverMediaLib_64.o` at build time. See Legal — redistribution of this precompiled archive may be restricted.

> [!NOTE]
> After every kernel upgrade, rebuild the module. `vermagic` must match `uname -r` (`modinfo cx511h`).

---

## Status: [EXPERIMENTAL] / ALPHA

**Kernel compatibility:** Builds and runs on **6.19.x–7.x** with matching headers (tested on CachyOS / Arch-style Clang kernels). There is no portable prebuilt `.ko` in the repo — always run `./build.sh LLVM=1 CC=clang` on the machine (and kernel) you use.

| Feature | Status | Description |
|:---|:---:|:---|
| **Build / Toolchain** | ⚠️ [CLANG] | Requires `LLVM=1 CC=clang`; GCC may fail on Clang-built kernels; uses `AverMediaLib_64.a` from repo root |
| **Module Loading** | ✅ [OK] | `insmod.sh` loads deps + `driver/cx511h.ko`, restores audio if PipeWire blocked |
| **Signal Detection** | 🟡 [PARTIAL] | HDMI lock via ITE6805 events; **4K inputs forced to 1080p** in software (`ITE6805_LOCK` handler) |
| **IRQ / Interrupts** | ✅ [OK] | MSI/INTx; **V-DESC hook** on `0x10` bit `0x2`; opt-in I2C ACK on bit `0x800` (`pci_model.c`) |
| **System Stability** | 🟡 [IMPROVED] | **`insmod` no longer freezes** (I2C IRQ opt-in ACK); **hard-freeze** if `0x304` slot-armed before valid descriptors |
| **DMA Transfer** | ✅ [OK] | V-DESC IRQs fire; FPGA walks SG descriptor chain; **DMA writes reach host RAM** (IOMMU passthrough + `q->dev` fix) |
| **Capture Content** | 🟡 [IMPROVED] | **Non-zero payload confirmed** (`10 80 10 80…` = YUV422 limited black); ffplay may report **corrupted data** — buffer-done timing / byte-order polish in progress |
| **Driver Unload** | ✅ [OK] | `unload.sh`: rmmod → unbind → **PCI sysfs remove** + `rmmod -f` on refcnt −1 → **PCIe rescan**; audio restored |
| **Audio Capture** | ❌ [STUB] | ALSA device `AVerMedia CL511H` **registers** (skeleton in `board_alsa.c`) but **no audio DMA** — capture is silent; no real data delivery yet |
| **General Use** | ❌ [NO] | **Not for daily use** — DMA reaches RAM, but userspace still shows **green/corrupt output** or **drops after the first frame**; OBS/GStreamer untested (Phase 4) |

### Development Phases

| Phase | Status | Description |
|:---|:---:|:---|
| **Phase 1** (Reverse Engineering) | ✅ COMPLETE | Builds on modern kernels, probe, FPGA/ITE6805 bring-up |
| **Phase 2** (Continuous Streaming) | 🟡 COMPLETE* | DMA/IRQ/doorbell at 60 fps; **all I2C writes in `stream_on` skipped** |
| **Phase 3** (Color / DMA) | 🟡 **BREAKTHROUGH** | Root cause of green screen fixed (`q->dev`); DMA payload live; register semantics clarified; tuning for stable dequeue |
| **Phase 4** (Production Ready) | ⏳ NEXT | Stable ffplay/OBS output, YUYV↔UYVY verification, buffer-done timing, 4K path, fixed test scripts |

> \*Phase 2: End-to-end streaming uses the **FPGA MMIO path** only (`0x10`, `0x304`, `0x1040`). ITE6805 register writes that the Windows driver performs in `stream_on` are disabled.

---

## Progress Report

### 2026-06-11 — Phase 3 breakthrough: DMA is alive

After months of all-zero (green) frames, we identified and fixed the **architectural root cause** and confirmed **live video payload** in host memory.

#### 1. Architectural core fix — vb2 device binding (`q->dev = dev`)

| Item | Detail |
|:---|:---|
| **File** | `driver/utils/v4l2/v4l2_model_videobuf2.c` → `v4l2_model_vb2_init()` |
| **Bug** | The vb2 queue was initialized with `mem_ops` and `alloc_ctx`, but **`q->dev` was never set** (kernel ≥ 4.8). |
| **Effect** | vb2-dma-sg mapped buffers against a **NULL device** → dma-direct path, **no IOMMU domain binding**, and all `dma_sync_sgtable_for_cpu` / `dma_sync_single_for_cpu` calls in the swap/sync path were **silently skipped** (`if (dma_dev)` guard). CPU saw stale zero cache lines even when the FPGA wrote RAM. |
| **Fix** | `q->dev = dev;` where `dev` is the PCI `struct device *` from `cxt_manager_get_dev()`. |
| **Companion** | `v4l2_model_sync_pending_plane_for_cpu()` called from `cx511h_video_buffer_done()` **before** `v4l2_model_buffer_done()` so completed frames are cache-coherent for the CPU. |

This was the **primary cause** of the infamous green screen (all zeros), not a wrong FPGA register value.

#### 2. Hardware register semantics — corrected (`aver_xilinx.o` disassembly)

Earlier diagnostics misread the descriptor ring. The blob behaviour is **by design**:

| Register | Actual meaning | Common misread |
|:---|:---|:---|
| **`0x308 + n·0xc`** | Bus address **low** of the **descriptor chain** (blob-allocated ring buffer) | Frame buffer address |
| **`0x30c + n·0xc`** | Bus address **high** of the descriptor chain | Frame buffer address high |
| **`0x310 + n·0xc`** | **Descriptor count** — number of 16-byte HW descriptors in the chain (`node[0x20]`) | Byte transfer size (`0x3f4800`) |

**Inside each 16-byte chain entry** (written by `aver_xilinx_add_to_cur_desclist`):

| Offset | Content |
|:---|:---|
| `+0x0` / `+0x4` | **Video target** bus address (our V4L2 `sg_dma_address`) |
| `+0x8` | Transfer length in **dwords** (`size >> 2`) |
| `+0xc` | Control flags (`0x80006000`) |

A value **`0x310 ≈ 0x7`** means **7 SG fragments** in the chain (typical for vb2-dma-sg), **not** a 7-byte transfer. Comparing `0x308` to V4L2 `Desc0` was a **category error** — the chain pointer (`0x583e0000`-class addresses) will never equal the frame address.

Use **`[gc573-chain]`** logs (read entry `[0]` after `active_current_desclist`) to verify the chain **target** matches our buffer.

#### 3. Forensic proof of life — payload `10 80 10 80`

With `q->dev` fixed, userspace and kernel diagnostics show **non-zero DMA**:

| Signal | Meaning |
|:---|:---|
| **ffplay** | `Dequeued v4l2 buffer contains corrupted data (4147200 bytes)` — buffer is **full-sized and non-empty** (corruption flag ≠ zero fill). |
| **`[gc573-payload]`** | First 16 bytes after `dma_sync_*`: e.g. **`10 80 10 80 10 80 …`** (see excerpt below) |
| **Interpretation** | Valid **UYVY limited-range black**: **Y = 0x10 (16)**, **U/V = 0x80 (128)** per macropixel. The FPGA DMA path works; remaining work is **V4L2 handoff quality** (timing, byte order, CSC). |

Excerpt from `cx511h_video_buffer_done()` in `board_v4l2.c` (documentation — not abbreviated):

```c
printk(KERN_ERR
    "[gc573-payload] First 16 bytes of frame: "
    "%02x %02x %02x %02x %02x %02x %02x %02x "
    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
    p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
```

#### 4. Diagnostic tags (current session)

```bash
dmesg | grep -iE 'gc573-payload|gc573-chain|gc573-debug|gc573-intercept|cx511h-diag|cx511h-swap|cx511h-desc|cx511h-dma'
```

| Tag | Purpose |
|:---|:---|
| `[gc573-debug]` | SG chain total size vs `0x3f4800`, `Desc0`/`Desc1` bus addresses |
| `[gc573-chain]` | Map chain at `0x308`/`0x30c`, compare entry0 target to V4L2 Desc0 |
| `[gc573-intercept]` | V-DESC IRQ: live chain pointer + descriptor count per slot |
| `[gc573-payload]` | First 16 frame bytes (raw FPGA order, pre-swap) |
| `[cx511h-diag]` | Pre/post YUYV→UYVY swap at frame 10 in `v4l2_model_buffer_done()` |

#### 5. Phase 4 — next steps

1. **Stable dequeue** — eliminate ffplay “corrupted data” by hardening `cx511h_video_buffer_done` ↔ `v4l2_model_buffer_done` ordering and ensuring swap + sync run exactly once per frame.
2. **Byte order** — confirm whether hardware delivers **YUYV** or **UYVY** (`10 80` = UYVY black; `80 10` = YUYV black); tune swap in `v4l2_model_buffer_done()` accordingly.
3. **Picture unlock** — with valid DMA, re-enable safe ITE6805/TTL paths where possible; verify CSC (`0x1040`) for real content (not just black).
4. **4K / high refresh** — ITE6805 downscale path (`iTE68051_Video_Output_Setting`) vs native FPGA modes (Phase 4+).

---

### 2026-06-11 — Earlier diagnostic session (still relevant)

#### Infrastructure & tooling (resolved)

| Area | Change |
|:---|:---|
| **C scope / debug flags** | Frame-counter / hex-dump state (`cx511h_frame_counter`) at file scope in `board_v4l2.c` — reset in `stream_on`, dump in `video_buffer_done`. |
| **Module refcnt −1** | Hard-killing `ffplay` could leave `cx511h` with refcnt **−1**, blocking normal `rmmod`. |
| **`unload.sh` hardened** | Escalation path: PCI sysfs **`…/remove`** + **`rmmod -f`**, then **`echo 1 > /sys/bus/pci/rescan`** so the card can be re-probed; PipeWire/WirePlumber restart unchanged. |
| **Kernel 7.0+ DMA API** | `dma_sync_sgtable_for_cpu` / `_for_device` in `v4l2_model_videobuf2.c` now pass **`struct device *`** from `vb->vb2_queue->dev` (required on modern kernels / CachyOS). |

#### I2C deadlock fixed (`pci_model.c`)

- Hardware continuously asserted IRQ bit **`0x800`** [I2C complete] during probe/init.
- **Fix:** asynchronous `struct completion` + **opt-in ISR ACK** — bit `0x800` is cleared only when `i2c_waiters > 0`. Unconditional ACK starved the vendor blob’s synchronous I2C poll loop and **froze `insmod`**.
- **`pci_model_wait_i2c_done()`** uses a 50 ms cap and falls back to `mdelay()` polling when IRQs are not yet live (probe phase).

#### Hardware intercept (`pci_model.c` + `board_v4l2.c`)

- Custom **Hard-IRQ hook** on bit **`0x2`** [V-DESC complete], registered via `pci_model_register_vdesc_hook()`.
- Reads MMIO register **`0x300 & 7`** for the active descriptor slot index.
- **Confirmed:** FPGA ping-pong DMA cycles **Slot 1 ↔ Slot 2** (`[cx511h-bypass] V-DESC INTERCEPTED! Slot Index: N`).

#### Superseded hypothesis (do not repeat)

| Old belief | Correction (2026-06-11) |
|:---|:---|
| **`0x310` = byte size; blob writes `0x7` instead of `0x3f4800`** | **`0x310` = descriptor count** (7 SG entries). Per-fragment byte size is in chain entry `+0x8` (dwords). |
| **Green screen = wrong descriptor size in blob** | **Green screen = missing `q->dev`** → no DMA sync → CPU read stale zeros. |
| **Override `0x308` with V4L2 frame address** | **Dangerous and wrong** — `0x308` points at the **chain**, not the frame. Patching chain **entries** is the correct lever if needed. |

**Descriptor programming path (unchanged):**

`v4l2_model_qops_buf_prepare` → `cx511h_v4l2_buffer_prepare` → `aver_xilinx_add_to_cur_desclist(phys, size)` → `aver_xilinx_active_current_desclist()` (blob programs ring + arms slots).

#### Approaches tried (summary)

| Approach | Result |
|:---|:---|
| **V4L2 FourCC swap to YUYV** | vb2 validation failed; keep **`V4L2_PIX_FMT_UYVY`**. |
| **FPGA `0x1000[15:8]` lane codes** | No effect — blob resets/ignores. |
| **CPU swap in Hard-IRQ hook** | PCIe timing stalls → userspace frame drops. Moved to `v4l2_model_buffer_done()` (soft path). |
| **`0x304 ← 0x07` at `stream_on`** | Hard-freeze — slots armed before ring programmed. |
| **`0x304 ← 0x07` per frame** | FPGA state-machine collision during active DMA → hard-freeze. |
| **Force V4L2 addr into `0x308/0x30c`** | **Do not** — misinterprets register semantics; causes garbage DMA / lockup. |

---

## Module Parameters

Load from `driver/` (or pass parameters to `insmod`):

```bash
cd driver
sudo insmod cx511h.ko force_input_mode=1
# Runtime: echo 1 | sudo tee /sys/module/cx511h/parameters/force_input_mode
```

### From `board_v4l2.c`

| Parameter | Type | Default | Description |
|:---|:---:|:---:|:---|
| `force_input_mode` | int | 0 | 0=Auto, 1=YUV422 BT.709, 2=YUV444 BT.709, 3=RGB Full, 4=RGB Limited |
| `debug_pixel_format` | int | -1 | -1=Auto; 0–3=YUYV/UYVY/YVYU/VYUY; 4–11=RGB variants (see `aver_xilinx.h`) |
| `auto_test_byteorder` | int | 0 | 1=On `stream_on`, cycle YUV byte orders 0–3 and log first pixels (see Known Issue #11) |

### From `board_config.c`

| Parameter | Type | Default | Description |
|:---|:---:|:---:|:---|
| `no_signal_pic` | charp | NULL | Bitmap when no input signal |
| `copy_protetion_pic` | charp | NULL | Bitmap for copy-protected content. **Typo preserved from vendor** (`board_config.c`) — the **insmod/sysfs name is exactly `copy_protetion_pic`**, not `copy_protection_pic`. |
| `led_pin_r` | int | 3 | Red LED GPIO (-1=disabled) |
| `led_pin_g` | int | 4 | Green LED GPIO (-1=disabled) |
| `led_pin_b` | int | 5 | Blue LED GPIO (-1=disabled) |

---

## How to Build & Install

### Prerequisites

> [!IMPORTANT]
> **Kernel command line (GRUB / systemd-boot):**
> ```bash
> ibt=off iommu=pt
> ```
> - `ibt=off` — `AverMediaLib_64.a` has no ENDBR64; Makefile uses `-fcf-protection=none` and `MODULE_INFO(ibt, "N")` as extra mitigation.
> - `iommu=pt` — **passthrough mode** for DMA (confirmed on test hardware: bus addresses match host physical addresses; IOMMU still required for correct vb2 **`q->dev`** mapping and `dma_sync_*`).

- Kernel headers for **running** kernel (`/lib/modules/$(uname -r)/build`)
- `base-devel`, `llvm`, `clang`
- **`AverMediaLib_64.a`** in repository root (included in clone)

### Build & Load

```bash
# Build (output: driver/cx511h.ko, copy to project root)
./build.sh LLVM=1 CC=clang

# Verify vermagic
modinfo cx511h.ko | grep vermagic

# Load
sudo ./insmod.sh
```

`build.sh` copies `cx511h.ko` to the project root; `insmod.sh` loads **`driver/cx511h.ko`** (always the freshest build after `build.sh`).

### System Installation (persistent)

```bash
sudo ./install.sh   # requires cx511h.ko in project root
sudo ./insmod.sh    # or: modprobe cx511h (after depmod)
```

Install path: `/lib/modules/$(uname -r)/kernel/drivers/media/avermedia/`

---

## Quick Start

**Recommended:** use `ffplay` / `ffmpeg` with explicit UYVY — avoid `v4l2-ctl` for streaming tests (see Known Issues).

> [!NOTE]
> The V4L2 device index is **not fixed** — it may be `/dev/video0`, `/dev/video2`, or another number depending on webcams and other capture devices. After `insmod`, find the AVerMedia node with `v4l2-ctl --list-devices` (listing only; do **not** use `--stream-mmap` on this card). Substitute **`/dev/videoX`** below with your actual node.

```bash
sudo ./insmod.sh

# UYVY = FPGA native output in auto mode
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/videoX

sudo ./unload.sh
```

### GStreamer test scripts (legacy / experimental)

```bash
./gst_1.0_raw_video.sh 0
./gst_1.0_raw_video_audio.sh 0
```

> [!WARNING]
> **Do not run these scripts casually.** Both call **`v4l2-ctl`**, which can trigger **I2C traffic on the ITE6805** and cause a **full system freeze / I2C bus deadlock** — the same failure mode the driver deliberately avoids in `stream_on` (Known Issue #1). They also derive **height from the wrong `v4l2-ctl` field** and use caps (YV12/YUY2) that do not match the card’s **UYVY** capture path. Prefer **`ffplay`** with explicit **UYVY** until the scripts are fixed.

---

## What Actually Happens in `stream_on`

Source: `driver/board/cx511h/board_v4l2.c` → `cx511h_stream_on()`.

### Executed (summary order)

1. **I2C reads** — `ite6805_get_frameinfo()`, `get_workingmode()`, `get_colorspace()`, `get_sampingmode()` (reads only; safe today)
2. **CPU** — fill `vip_cfg` (resolution, framerate, colorspace, bypass, pixel format; optional `force_input_mode`)
3. **MMIO** — `aver_xilinx_enable_video_streaming(FALSE)`; `msleep(50)`
4. **MMIO** — `aver_xilinx_config_video_process(&vip_cfg)`
5. **MMIO** — `pci_model_mmio_write(0x1040, csc_value)` — FPGA CSC
6. **MMIO** — `msleep(200)`; optional pixel-format debug (`debug_pixel_format` / `auto_test_byteorder`)
7. **MMIO** — `aver_xilinx_enable_video_streaming(TRUE)`
8. **MMIO** — `pci_model_mmio_write(0x304, 0x01)` — initial doorbell (run bit only; **no slot arming** until descriptor ring verified)
9. **Read-only** — `cx511h_dma_verify_slots()` + `[gc573-chain]` descriptor entry audit (first buffers only)

YUV422 from userspace is mapped to FPGA **UYVY** unless `debug_pixel_format` overrides.

> [!CAUTION]
> Do **not** write `0x304 ← 0x07` (run + arm slots) at `stream_on` until the descriptor **chain** at `0x308`/`0x30c` is programmed and verified via `[gc573-chain]`. Arming empty slots → **PCIe hard-freeze**. Do **not** write the V4L2 frame address into `0x308` — that register holds the **chain pointer**, not the pixel buffer.

### Skipped (I2C deadlock workaround)

| Operation | Registers | Mechanism |
|:---|:---|:---|
| TTL pixel mode | 0xc0, 0xc1, 0xbd, 0xbe, 0xc4 | `goto skip_ttl_config` at start |
| HDMI video unmute | 0xb0, 0xa0, 0x02 | empty SKIPPING block |
| ITE68051 streaming | 0x20, 0x86, 0x90, 0xA0–A2, 0xA4, 0xB0 | empty SKIPPING block |
| RX deskew | vendor | empty SKIPPING block |
| ITE6805 CSC sync | 0x6b, 0x6c, 0x6e, 0x2a | empty SKIPPING block |
| HDCP / freerun | — | commented out |

**Reason:** `hdmirxwr()` during streaming can **deadlock** the ITE6805 I2C bus. CSC and stream control use MMIO instead.

### Per frame — completion paths (2026-06-11)

**Path A — PCIe ISR (active):** `pci_model_irq` in `pci_model.c` intercepts bit **`0x2`** [V-DESC complete] **before** the vendor blob handler. Hook `cx511h_vdesc_irq_hook()` logs slot index from **`0x300 & 7`**, optional `[gc573-intercept]` chain pointer/count, and frame-100 hex dump.

**Path B — blob callback (active when streaming):** `cx511h_video_buffer_done` via `aver_xilinx_active_current_desclist(...)`. Sequence:

1. **`v4l2_model_sync_pending_plane_for_cpu()`** — invalidate CPU cache for the pending vb2 plane (`DMA_FROM_DEVICE`).
2. **`[gc573-payload]`** — first 16 bytes (raw FPGA byte order, budgeted).
3. **`v4l2_model_buffer_done()`** — YUYV→UYVY swap (if UYVY FourCC) + `vb2_buffer_done()`.
4. Doorbell **`0x304 ← 0x01`** → IRQ ACK **`0x10 ← 0x02`**. **Do not** re-arm slot bits on `0x304` per frame.

**Path C — V4L2 handoff detail:** `v4l2_model_buffer_done()` in `v4l2_model_videobuf2.c` includes one-shot `[cx511h-diag] FIRST 16 BYTES` pre/post swap at frame 10.

### `stream_off`

`aver_xilinx_enable_video_streaming(FALSE)` only — no I2C teardown.

---

## Architecture

### Source layout

| Layer | Files | Purpose |
|:---|:---|:---|
| Entry | `driver/entry.c` | `module_init`, PCI ID table, softdeps, IBT disable |
| Context | `driver/cxt_mgr.c` | Reference-counted handles |
| Board | `driver/board/cx511h/board_config.c` | Probe/remove, init order |
| Board | `driver/board/cx511h/board_v4l2.c` | V4L2, streaming, CSC, doorbell |
| Board | `driver/board/cx511h/board_i2c.c` | I2C, ITE6805 @ 0x58 |
| Board | `driver/board/cx511h/board_gpio.c` | GPIO (reset pin 0, HPD pin 2) |
| Board | `driver/board/cx511h/board_alsa.c` | ALSA PCM |
| Utils | `driver/utils/pci/pci_model.c` | PCI, MMIO, IRQ (V-DESC hook, opt-in I2C ACK), DMA verify |
| Utils | `driver/utils/v4l2/*.c` | V4L2, videobuf2 (**`q->dev` binding**), framegrabber, cache sync |
| Blob | `AverMediaLib_64.a` (repo root) | Precompiled vendor FPGA / ITE6805 logic |

### Build (`driver/Makefile`)

- `LLVM=1 CC=clang` on Clang-kernel distros
- `-fcf-protection=none`, `-fno-stack-protector`, `MODULE_INFO(ibt, "N")`
- Blob: `cp AverMediaLib_64.a` → `AverMediaLib_64.o` via custom kbuild rule

### `board_probe` init sequence

1. PCI handle · 2. I2C manager · 3. GPIO manager · 4. Memory manager · 5. Task manager  
6. `aver_xilinx_init` + `aver_xilinx_init_registers` · 7. I2C bus `I2C_BUS_COM` · 8. Board GPIO  
9. `board_i2c_init` · 10. ITE6805 attach (with linker-section fallback) · 11. Bitmap overlay  
12. ALSA · 13. V4L2  

### Audio (stub — not functional)

- **Status:** PCM device visible in the system (`AVerMedia CL511H`, subsystem `0x5730`) because the ALSA registration path exists — **actual HDMI audio capture is unimplemented**.
- **No audio DMA stream** — buffers stay silent; `board_alsa.c` / `alsa_model.c` are scaffolding only.
- **Advertised caps** (if probed): S16_LE / S24_LE, 32–192 kHz, 2 channels; `period_size = 7680×4` (30720 bytes), up to 128 periods — not validated on live audio.

### Suspend / resume

Legacy `board_suspend` / `board_resume` wired from PCI setup — not migrated to `dev_pm_ops` (see Known Issue #10).

---

## Scripts

| Script | Purpose |
|:---|:---|
| `build.sh` | `make -C driver`, copy `cx511h.ko` to root |
| `insmod.sh` | Modprobe vb2/snd deps, `insmod driver/cx511h.ko`, audio restore |
| `unload.sh` | Release V4L2/ALSA clients → rmmod → unbind → **PCI `remove` + `rmmod -f`** if refcnt −1 → **PCIe rescan**; restore PipeWire |
| `install.sh` | Install `.ko` under `/lib/modules/.../avermedia/`, `depmod -a` |
| `gst_1.0_raw_video.sh` | Legacy GStreamer test — **see warnings above** |
| `gst_1.0_raw_video_audio.sh` | Legacy A/V test — **see warnings above** |

---

## Tools (`tools/`)

| Tool | Purpose |
|:---|:---|
| `check_patterns.py` | Stack-canary pattern count in binaries |
| `patch_library.py` | NOP stack-canary prologue/epilogue |
| `scan_epilogues.py` | List `xor %gs:0x28` epilogue variants |

---

## Known Issues

### 1. I2C bus deadlock (critical)
- **Status:** **`insmod` freeze mitigated** (2026-06-11); streaming I2C writes still skipped
- **Issue:** Hardware spams IRQ bit **`0x800`** [I2C complete]. Unconditional ISR ACK cleared the bit before the vendor blob’s poll loop could see it → **module load freeze**.
- **Fix:** Opt-in ACK in `pci_model_irq` when `i2c_waiters > 0`; `pci_model_wait_i2c_done()` with 50 ms timeout + probe-phase poll fallback.
- **Remaining:** `hdmirxwr()` during streaming can still freeze; TTL/unmute/CSC I2C sequences skipped in `stream_on`.

### 2. Green screen / zero-filled buffers
- **Status:** **resolved** (2026-06-11) — missing **`q->dev`** in vb2 init
- **Issue:** V4L2 buffers appeared **`0x00` throughout** (pure green in YUV). Root cause was **not** FPGA register `0x310` or byte order.
- **Fix:** `q->dev = dev` in `v4l2_model_vb2_init()` + explicit `v4l2_model_sync_pending_plane_for_cpu()` before buffer handoff.
- **Proof:** `[gc573-payload]` shows **`10 80 10 80…`**; ffplay reports full 4147200-byte frames (non-zero).
- **Remaining:** ffplay may still flag **corrupted data** — tune buffer-done timing and YUYV↔UYVY swap (Phase 4).

### 3. V4L2 vs FPGA format
- Driver advertises many pixel formats; hardware path uses **UYVY** for YUV422 in auto mode.
- Always set capture format explicitly in applications.

### 4. Module “in use” / refcnt −1
- **Status:** mitigated by hardened `unload.sh`
- **Issue:** Abrupt client exit (e.g. killed `ffplay`) can leave refcnt **−1**; normal `rmmod` fails.
- **Fix:** `unload.sh` escalates to PCI sysfs **remove** + **`rmmod -f`**, then **PCIe rescan**; also kills PipeWire/WirePlumber holders when needed.

### 5. `v4l2-ctl` risk
- **Status:** known
- **Issue:** `v4l2-ctl` (especially `--stream-mmap`, format probing, or scripts that call it) can trigger **I2C traffic** and hang.
- **Prefer:** `ffplay` / `ffmpeg` with explicit formats; raw `dd` only while streaming.

### 6. Toolchain
- **Status:** unchanged — use `LLVM=1 CC=clang` on Clang-built kernels.

### 7. `vactive` / `hactive` in `vip_cfg`
- **Status:** under investigation
- **Location:** `board_v4l2.c` ~506–507
- **Issue:** `vactive = width`, `hactive = height` (swapped vs usual timing names). Bypass tables use the same convention — test before “fixing”.

### 8. Kernel upgrades
- **Status:** operational note (not a code bug)
- **Issue:** Module must be **rebuilt** after each kernel update; vermagic mismatch prevents load.
- **Fix:** `./build.sh LLVM=1 CC=clang` then reload.

### 9. V4L2 exposes all pixel formats
- Hardware realistically: YUV422 (UYVY). OBS/GStreamer may pick an unsupported FourCC.

### 10. Legacy suspend/resume
- PCI driver uses legacy suspend hooks; should move to `dev_pm_ops` for long-term 7.x maintenance.

### 11. `auto_test_byteorder` debug path
- **Status:** misleading on its own
- **Issue:** `dump_first_pixels()` reads the DMA physical address via **PCI MMIO offset**, not CPU-mapped buffer memory — dmesg output may not reflect real frame bytes. Prefer **userspace** `dd` + `xxd` (below).

### 12. GStreamer helper scripts
- **Status:** broken / risky
- **Issue:** wrong height parsing; calls `v4l2-ctl`; video caps use YV12/YUY2 without matching capture format.

### 13. 4K HDMI input
- **Status:** software limited
- On lock, widths **> 1920** are forced to **1920×1080@60** in `cx511h_ite6805_event()` — true 4K capture not implemented.

### 14. Descriptor ring register semantics (clarified)
- **Status:** documented (2026-06-11) — **not** a size-mismatch bug
- **`0x308` / `0x30c`:** Pointer to the blob’s **descriptor chain** in RAM (not the video frame).
- **`0x310`:** **Descriptor count** (e.g. `0x7` = seven SG fragments). Per-fragment byte length is in chain entry dword at offset `+0x8` (`bytes >> 2`).
- **Verification:** Use `[gc573-chain]` to compare chain entry0 **target address** to V4L2 `Desc0`, not `0x308` itself.
- **Do not:** Write frame addresses into `0x308` or force `0x310 ← 0x3f4800`.

### 15. `0x304` slot arming hazards
- **Status:** documented safe baseline
- **Safe:** `0x304 ← 0x01` at `stream_on` and per-frame doorbell only.
- **Unsafe:** `0x304 |= 0x07` at `stream_on` (before ring programmed) or per-frame re-arm during active DMA → **system hard-freeze**.

### 16. ffplay “corrupted data” / unstable dequeue (Phase 4)
- **Status:** open (2026-06-11)
- **Issue:** After the DMA fix, ffplay may log **`Dequeued v4l2 buffer contains corrupted data (4147200 bytes)`** while `[gc573-payload]` shows valid YUV (`10 80…`). Payload reaches RAM; userspace validation or wrapper timing/byte-order still needs hardening.
- **Next:** Verify YUYV↔UYVY swap once per frame; audit `cx511h_video_buffer_done` vs vb2 queue state; test with `ffmpeg -f v4l2` and raw `dd`/`xxd`.

---

## Reverse Engineering Progress

### Working (MMIO) — Windows driver audit + Linux validation (2026-06-11)

| Register | Bits / value | Role |
|:---:|:---:|:---|
| **0x10** | `0x1fff` mask | IRQ status/ACK: `0x2` V-DESC complete, `0x20`/`0x200` audio, `0x800` I2C engine |
| **0x300** | `& 7` | Active V-DESC slot index (ping-pong: slots 1↔2 observed) |
| **0x304** | bit `0` | Global run/enable — **safe doorbell value `0x01`** |
| **0x304** | bits `1..4` | Descriptor slot arm (`1 << (slot+1)`) — **only after chain programmed** |
| **0x308+n·0xc** | addr lo | **Descriptor chain** bus address (low) — blob-allocated ring buffer |
| **0x30c+n·0xc** | addr hi | **Descriptor chain** bus address (high) |
| **0x310+n·0xc** | count | **Number of HW descriptors** in chain (SG fragment count, e.g. `0x7`) |
| **0x1040** | dynamic | CSC (422 mode, RGB→YUV, matrix bits [10:8]) |

**Chain entry layout** (16 bytes at address from `0x308`/`0x30c`): `[0]`/`[1]` = video target addr, `[2]` = size in dwords, `[3]` = `0x80006000` control.

### Identified but skipped (I2C writes)

| Reg | Device | Notes |
|:---:|:---:|:---|
| 0x20, 0x86, 0x90, 0xA0–A2, 0xA4, 0xB0 | ITE68051 | Streaming enable chain |
| 0x6b, 0x6c, 0x6e, 0x2a | ITE6805 | CSC sync |
| 0xc0, 0xc1, … | ITE6805 | TTL / SDR pixel mode |

---

## Debugging

**First step for color problems:** dump one frame while HDMI source is active and compare hex patterns.

```bash
sudo ./insmod.sh
# Start capture in another terminal, e.g. ffplay ... &
sleep 2
sudo dd if=/dev/videoX of=frame_raw.bin bs=4147200 count=1
xxd frame_raw.bin | head -4
```

| Hex pattern | Likely meaning |
|:---|:---|
| `00 00 00 00...` | **Stale cache / pre-fix** — DMA may have written but CPU saw zeros; check **`q->dev`** and `[gc573-payload]` after sync |
| `10 80 10 80...` | **UYVY limited black** — DMA OK (Y=0x10, U/V=0x80). **Confirmed live payload (2026-06-11).** |
| `80 10 80 10...` | **YUYV limited black** — DMA OK; enable or disable byte-pair swap accordingly |
| Varying non-zero | Real picture — tune CSC (`0x1040`) and colours only if needed |

```bash
# Kernel log — Phase 3 breakthrough tags
dmesg | grep -iE 'gc573-payload|gc573-chain|gc573-debug|gc573-intercept|cx511h-bypass|cx511h-desc|cx511h-diag|cx511h-swap|cx511h-irq|cx511h-i2c|cx511h-phase2|cx511h-csc|cx511h-dma'

# Module parameters (from driver/)
cd driver && sudo rmmod cx511h 2>/dev/null; sudo insmod cx511h.ko debug_pixel_format=1

# Force input colorspace interpretation (from driver/)
cd driver && sudo rmmod cx511h 2>/dev/null; sudo insmod cx511h.ko force_input_mode=1

# ffplay format sweep (safe)
for fmt in uyvy422 yuyv422 yvyu422 vyuy422; do
  ffplay -f v4l2 -input_format $fmt -video_size 1920x1080 -framerate 60 /dev/videoX
done
```

> [!CAUTION]
> Do not rely on `auto_test_byteorder=1` alone — confirm with userspace `dd`/`xxd`.

---

## Reverse Engineering Methods

- Register probing and Windows-driver comparison
- **`AverMediaLib_64.a` object disassembly** (`aver_xilinx.o`, `ite6805_sys.o`, …) — descriptor chain semantics, ITE6805 downscale path
- V4L2 / videobuf2 callback tracing (`[gc573-*]` forensic tags)
- Iterative testing on real GC573 hardware (CachyOS / kernel 7.x)
- AI-assisted analysis (clearly experimental)

---

## Legal / Compliance

Interoperability-focused community project (EU Directive 2009/24/EC Art. 6). AVerMedia trademarks and the vendor blob belong to their respective owners.

> [!CAUTION]
> `AverMediaLib_64.a` is a precompiled vendor archive — check license/redistribution before publishing binaries or forks.

---

## Disclaimer

Community project, not supported by AVerMedia. Use at your own risk.

---

**Repository:** [github.com/Everlite/Avermedia-GC573-Linux](https://github.com/Everlite/Avermedia-GC573-Linux)  
**Maintained by [Everlite](https://github.com/Everlite)** · Thanks to [derrod](https://github.com/derrod) for earlier work.
