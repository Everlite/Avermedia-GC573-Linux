# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted, heavily patched Linux driver for the AVerMedia GC573 (PCI `1461:0054`, subsystem `1461:5730`).
Modernized for recent kernels. **Experimental — development and testing only.**

**Last aligned with code:** 2026-06-11 · **Phase 3** (color / byte-order validation)

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
| **IRQ / Interrupts** | ✅ [OK] | MSI with INTx fallback (`pci_model.c`) |
| **System Stability** | 🟡 [WORKAROUND] | Stable when avoiding I2C **writes**; `hdmirxwr()` during stream can still freeze |
| **DMA Transfer** | ✅ [OK] | Continuous streaming: doorbell MMIO `0x304` + IRQ ACK MMIO `0x10` per buffer |
| **Capture Content** | 🟡 [BLOCKED] | DMA delivers frames; **green screen** from YUYV-on-wire vs UYVY V4L2 label (see Progress Report 2026-06-11) |
| **Driver Unload** | ✅ [OK] | `unload.sh`: rmmod → unbind → **PCI sysfs remove** + `rmmod -f` on refcnt −1 → **PCIe rescan**; audio restored |
| **Audio Capture** | ✅ [OK] | ALSA `AVerMedia CL511H`: S16_LE/S24_LE, 32–192 kHz, **2 channels** |
| **General Use** | ❌ [NO] | Not for daily use / production |

### Development Phases

| Phase | Status | Description |
|:---|:---:|:---|
| **Phase 1** (Reverse Engineering) | ✅ COMPLETE | Builds on modern kernels, probe, FPGA/ITE6805 bring-up |
| **Phase 2** (Continuous Streaming) | 🟡 COMPLETE* | DMA/IRQ/doorbell at 60 fps; **all I2C writes in `stream_on` skipped** |
| **Phase 3** (Color Correction) | 🟡 IN PROGRESS | YUYV↔UYVY mismatch confirmed; FPGA reg `0x1000` ineffective; CPU swap path blocked — **real buffer-done hook TBD** |
| **Phase 4** (Production Ready) | ⏳ PENDING | Robust PM, OBS/GStreamer, fixed test scripts, no I2C deadlock |

> \*Phase 2: End-to-end streaming uses the **FPGA MMIO path** only (`0x10`, `0x304`, `0x1040`). ITE6805 register writes that the Windows driver performs in `stream_on` are disabled.

---

## Progress Report — 2026-06-11

### 1. Infrastructure & tooling (resolved)

| Area | Change |
|:---|:---|
| **C scope / debug flags** | Frame-counter / hex-dump state (`cx511h_frame_counter`) at file scope in `board_v4l2.c` — reset in `stream_on`, dump in `video_buffer_done`. |
| **Module refcnt −1** | Hard-killing `ffplay` could leave `cx511h` with refcnt **−1**, blocking normal `rmmod`. |
| **`unload.sh` hardened** | Escalation path: PCI sysfs **`…/remove`** + **`rmmod -f`**, then **`echo 1 > /sys/bus/pci/rescan`** so the card can be re-probed; PipeWire/WirePlumber restart unchanged. |
| **Kernel 7.0+ DMA API** | `dma_sync_sgtable_for_cpu` / `_for_device` in `v4l2_model_videobuf2.c` now pass **`struct device *`** from `vb->vb2_queue->dev` (required on modern kernels / CachyOS). |

### 2. Test results & hardware diagnosis

**Initial hex dump (frame 0 / cleared buffer):** `80 10 80 10 …`

- In **YUYV** byte order: `Y=0x10` (16), `U/V=0x80` (128) → limited-range digital black.
- **Green-screen mechanism:** If the FPGA emits **YUYV** on the PCIe bus but V4L2/userspace treat the buffer as **UYVY**, luma and chroma lanes are misread — neutral black becomes a strong green tint in players.

**Approaches tried (failed or inactive):**

| Approach | Result |
|:---|:---|
| **V4L2 enum / FourCC swap to YUYV** | Kernel buffer validation failed; FFmpeg rejected frames as corrupted. Driver must keep advertising **`V4L2_PIX_FMT_UYVY`**. |
| **FPGA register `0x1000[15:8]`** (e.g. `0x9` ↔ `0x0`) | No visible change — blob/`set_out_colorspace` appears to ignore or reset lane-swap codes. |
| **CPU byte-pair swap in `cx511h_video_buffer_done`** | Implemented via `v4l2_model_swap_pending_plane_byte_pairs()` before `v4l2_model_buffer_done()`, but **`AVER_LIVE_HEX_DUMP` at frame 100 never appeared in dmesg** during live streaming → **`cx511h_video_buffer_done` is likely not called** on the active DMA path; frames may complete via another IRQ/callback in `v4l2_model_videobuf2.c`. |

### 3. Next session (focused)

1. **Find the real completion hook** — Trace `v4l2_model_videobuf2.c` (IRQ path, `v4l2_model_buffer_done`, scatter-gather handling) to locate where finished DMA buffers are handed to vb2 in live operation; move the YUYV→UYVY byte-pair swap there.
2. **Clean V4L2 format fix (optional)** — If software swap is insufficient, adjust format tables so Linux sees **`V4L2_PIX_FMT_YUYV`** while keeping **`bytesperline` and frame size** unchanged so vb2 validation still passes (FourCC-only lie, not size/stride changes).

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
| `copy_protetion_pic` | charp | NULL | Bitmap for copy-protected content. **Parameter name is misspelled in `board_config.c`** (vendor/original spelling) — you must pass `copy_protetion_pic=...` on `insmod` until the symbol is renamed in code. |
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
> - `iommu=pt` — passthrough mode for DMA.

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
8. **MMIO** — `pci_model_mmio_write(0x304, 0x01)` — initial doorbell

YUV422 from userspace is mapped to FPGA **UYVY** unless `debug_pixel_format` overrides.

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

### Per frame (intended: `cx511h_video_buffer_done`)

Registered via `aver_xilinx_active_current_desclist(..., cx511h_video_buffer_done, ...)`. **As of 2026-06-11 this callback may not run during live capture** (no frame-100 `AVER_LIVE_HEX_DUMP` in dmesg); doorbell/IRQ below might still occur on a different path.

When invoked, the handler:

1. Optional frame-100 hex dump (`v4l2_model_peek_pending_plane_vaddr`)
2. For `V4L2_PIX_FMT_UYVY`: `v4l2_model_swap_pending_plane_byte_pairs()` — adjacent byte swap (YUYV→UYVY) over `width×height×2`
3. `v4l2_model_buffer_done()`
4. `pci_model_mmio_write(0x304, 0x01)` — doorbell
5. `pci_model_mmio_write(0x10, 0x02)` — IRQ ACK
6. `wmb()`

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
| Utils | `driver/utils/pci/pci_model.c` | PCI, MMIO, IRQ, DMA |
| Utils | `driver/utils/v4l2/*.c` | V4L2, videobuf2, framegrabber |
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

### Audio

- Name: `AVerMedia CL511H` (subsystem `0x5730`)
- Formats: S16_LE, S24_LE
- Rates: 32 / 44.1 / 48 / 96 / 192 kHz
- **Channels: 2** (stereo PCM device; `alsa_model.c`)
- Buffer: `period_size = 7680×4` (30720 bytes), up to **128** periods (`board_alsa.c`)

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
- **Status:** workaround in `stream_on`
- **Issue:** `hdmirxwr()` (I2C **write**) to ITE6805 during/around streaming can **freeze** the system.
- **Impact:** TTL, unmute, streaming, CSC I2C sequences skipped; MMIO path used instead.
- **Reads** (`ite6805_get_*`) still run in `stream_on` and event handlers.

### 2. Byte-order / green screen
- **Status:** root cause identified; fix blocked on callback path
- **Issue:** Hardware outputs **YUYV** byte layout (`80 10 80 10 …` = limited black in YUYV); V4L2 advertises **UYVY** → green in players.
- **Failed fixes:** V4L2 FourCC-only swap to YUYV (vb2 corruption); FPGA `0x1000` lane codes (no effect); CPU swap in `cx511h_video_buffer_done` (callback not seen live).
- **Next:** Hook byte-pair swap at the **actual** buffer-done site in `v4l2_model_videobuf2.c`, or FourCC-only YUYV with unchanged stride/size.

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

### 14. `cx511h_video_buffer_done` not on live path
- **Status:** open (2026-06-11)
- **Issue:** `AVER_LIVE_HEX_DUMP` and CPU byte-swap in `cx511h_video_buffer_done` never trigger during active streaming despite visible video in userspace.
- **Impact:** Doorbell path and color fix must be retargeted after tracing `v4l2_model_videobuf2.c` / IRQ completion.

---

## Reverse Engineering Progress

### Working (MMIO)

| Register | Value | Role |
|:---:|:---:|:---|
| **0x10** | `0x02` | IRQ ACK per buffer |
| **0x304** | `0x01` | Doorbell (next buffer) |
| **0x1040** | dynamic | CSC (422 mode, RGB→YUV, matrix bits [10:8]) |

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
| `10 80 10 80...` | Black YUV — DMA ok, muted/no picture |
| `00 00 00 00...` | No DMA data |
| `80 10 80 10...` | **YUYV** limited black (Y=16, U/V=128) — typical FPGA wire format; V4L2 **UYVY** label → green |
| `10 80 10 80...` | **UYVY** limited black — what userspace expects for correct colors |
| Varying | Real pixels — tune format/CSC / swap hook |

```bash
# Kernel log
dmesg | grep -iE 'cx511h-csc|cx511h-phase2|cx511h-color|cx511h-dma|cx511h-pixfmt'

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
- V4L2 / videobuf2 callback tracing
- Iterative testing on real GC573 hardware
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
