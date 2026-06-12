# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted Linux driver for the AVerMedia GC573 (PCI `1461:0054`, subsystem `1461:5730`).
Modernized for recent kernels. **Experimental — development and testing only.**

**Last aligned with code:** 2026-06-12 · **Phase 4** (lockup-free streaming path; picture quality still open)

> [!NOTE]
> **Vendor blob:** Links against `AverMediaLib_64.a` in the **repository root** (~565 KB). The Makefile copies it to `driver/AverMediaLib_64.o` at build time. Redistribution of the blob may be restricted — see [Legal](#legal--compliance).

> [!NOTE]
> Rebuild after every kernel upgrade. `vermagic` must match `uname -r` (`modinfo cx511h`).

---

## Status

**Kernel:** Builds on **6.19.x–7.x** with matching headers (tested on CachyOS / Clang kernels). No portable prebuilt `.ko` — run `./build.sh LLVM=1 CC=clang` on the target machine.

| Area | Status | Notes |
|:---|:---:|:---|
| **Build / load** | ✅ | `LLVM=1 CC=clang`; `insmod.sh` / `unload.sh` |
| **Probe / insmod** | ✅ | No hard-freeze when I2C IRQ opt-in ACK is active |
| **HDMI lock** | 🟡 | ITE6805 events; 4K sources detected; framegrabber metadata may show forced 1080p |
| **Phase 4 pipeline** | ✅ | Boot-time `iTE6805_Hardware_Init()`; MMIO-only `stream_on`; blob owns scaler/CSC |
| **V-DESC / DMA IRQ** | ✅ | Hook on `0x10` bit `0x2`; descriptor chain + handoff guards |
| **DMA to host RAM** | ✅ | `q->dev` binding + `dma_sync_*` on buffer done |
| **Userspace picture** | 🟡 | May still be green, `00` bytes, or ffplay “corrupted data” — **not production-ready** |
| **Audio** | ❌ | ALSA device registers; no audio DMA |
| **Daily use** | ❌ | Capture testing only |

### Development phases

| Phase | Status | Summary |
|:---|:---:|:---|
| **1** — RE & bring-up | ✅ | Kernel port, probe, FPGA / ITE6805 attach |
| **2** — DMA / IRQ | ✅ | V-DESC hook, doorbell `0x304 ← 0x01`, descriptor chain understood |
| **3** — DMA coherency | ✅ | `q->dev = dev` in vb2; cache sync before handoff |
| **4** — Stable streaming path | 🟡 **BREAKTHROUGH** | No I2C writes at stream time; boot bootstrap; blob-native 4K→1080p scaler |
| **4b** — Picture quality | ⏳ | Valid pixels in userspace; HDCP / byte order / handoff timing |

---

## Phase 4 — What changed (2026-06)

### Boot-time HDMI bootstrap (not `stream_on`)

**Problem:** `iTE6805_Hardware_Init()` or any `hdmirxwr()` during `stream_on` collided with the blob’s background HDMI negotiation → **PCIe / kernel hard-freeze** (registers `0xc0`, `0x23`, streaming chain, etc.).

**Fix:** One-shot init at end of `board_probe()` in `board_config.c`:

```
[cx511h-phase4] Bootstrapping ITE6805/IT6664 hardware pipeline once at probe...
iTE6805_Hardware_Init(ite6805_handle_1);
```

**IT6664 splitter:** Brought up inside the blob via `ite6805_attach()` → `ite6664_attach()` during `board_i2c_init()`. Background `ite6664_task` runs on a **500 ms** timer (blob `task_model`).

**When is HDMI ready?** Wait for this line in `dmesg` — that is the user-facing “go” signal (green LED also turns on):

```
cx511h_ite6805_event locked fe 3840x2160p (raw)
```

`(raw)` is the physical timing from ITE6805 **before** the framegrabber 1080p override. On 4K sources it may appear a few seconds after `insmod`; only then open `/dev/videoX`.

### Sterile `stream_on` (MMIO / FPGA only)

**Policy:** **No ITE6805 register writes** in `cx511h_stream_on()`. Log line:

```
[cx511h-dma] === STREAM ON (MMIO/FPGA only — no I2C) ===
```

**4K → 1080p:** Physical timing comes from `ite6805_get_frameinfo()` (e.g. 3840×2160), not framegrabber metadata (which `ITE6805_LOCK` may force to 1920×1080 for V4L2 caps). When input exceeds output:

- `vip_cfg.in_videoformat.vactive/hactive` = physical HDMI size  
- `vip_cfg.out_videoformat.width/height` = V4L2 output (e.g. 1920×1080)  
- `vip_cfg.dual_pixel = 1`, `vip_cfg.video_bypass = 0`  
- `valid_mask` includes `SCALER_CFG_SHRINK_MASK`  
- **Single call:** `aver_xilinx_config_video_process()` — blob programs XV scaler, **`0x1088`**, and **`0x1040`** (CSC + dual-pixel)

**Do not** manually write `0x1040` or `0x1088` after the blob call — post-patching CSC broke ingest and produced **`00`** frames.

### I2C IRQ deadlock (probe)

Hardware asserts IRQ bit **`0x800`** [I2C complete] continuously. **Fix in `pci_model.c`:** opt-in ACK only when `i2c_waiters > 0`. Unconditional ACK starved the blob’s I2C poll loop and froze `insmod`.

---

## Phase 3 — DMA coherency (still required)

| Fix | File |
|:---|:---|
| `q->dev = dev` on vb2 queue | `v4l2_model_videobuf2.c` |
| `v4l2_model_sync_pending_plane_for_cpu()` before handoff | called from `cx511h_video_buffer_done()` |
| No CPU byte-pair swap on active DMA buffers | swap removed from `v4l2_model_buffer_done()` |

Missing `q->dev` caused **silent skip** of `dma_sync_*` → CPU read stale zeros (classic green screen). This fix is **necessary but not sufficient** for a correct picture on all sources.

---

## Hard rules (do not break)

| Action | Result |
|:---|:---|
| `hdmirxwr()` / ITE6805 **writes** during `stream_on` | **Hard-freeze** |
| Manual `pci_model_mmio_write(0x1040, …)` after blob config | **Corrupt / zero payload** |
| `0x304 ← 0x07` at `stream_on` (arm slots before ring ready) | **Hard-freeze** |
| Write V4L2 frame address into `0x308` | Wrong semantics — `0x308` is **chain pointer**, not frame |
| `v4l2-ctl --stream-mmap` / GStreamer helper scripts | **I2C traffic → freeze risk** |

**Safe doorbell:** `0x304 ← 0x01` only (run bit, no slot arm at stream start).

---

## `stream_on` flow (current code)

Source: `driver/board/cx511h/board_v4l2.c` → `cx511h_stream_on()`.

1. **Read-only blob APIs** — `ite6805_get_frameinfo()`, `get_workingmode()`, `get_colorspace()`, `get_sampingmode()` (no register writes from our side)
2. Build **`vip_cfg`** — physical input dims, V4L2 output, colorspace (`force_input_mode` optional)
3. **`aver_xilinx_enable_video_streaming(FALSE)`** + `msleep(50)`
4. Re-seal **`vip_cfg`** for downscale path if `fe_frameinfo` > output resolution
5. **`aver_xilinx_config_video_process(&vip_cfg)`** — blob only; no manual `0x1040`
6. `msleep(200)`; optional pixel-format debug (`debug_pixel_format`, `auto_test_byteorder`)
7. **`aver_xilinx_enable_video_streaming(TRUE)`**
8. Doorbell **`0x304 ← 0x01`**

**Per frame:** V-DESC IRQ → `cx511h_video_buffer_done()` → handoff guard → `dma_sync_*` → `v4l2_model_buffer_done()` → doorbell / IRQ ACK.

**`stream_off`:** `aver_xilinx_enable_video_streaming(FALSE)` only.

---

## `board_probe` init sequence

1. PCI · I2C manager · GPIO · memory · task manager  
2. `aver_xilinx_init` + `aver_xilinx_init_registers`  
3. I2C bus · board GPIO · **`board_i2c_init`** (ITE6805 @ `0x58` → blob attaches IT6664)  
4. Bitmap overlay · ALSA · **`board_v4l2_init`**  
5. **`iTE6805_Hardware_Init()`** — Phase 4 boot bootstrap  

---

## FPGA MMIO reference

| Register | Role |
|:---|:---|
| **0x10** bit `0x2` | V-DESC complete (video frame done) |
| **0x10** bit `0x800` | I2C engine (opt-in ACK) |
| **0x300** `& 7` | Active descriptor slot index |
| **0x304** bit `0` | Stream run / doorbell — use **`0x01`** |
| **0x308 + n·0xc** | Descriptor **chain** bus addr (low) — not the frame buffer |
| **0x30c + n·0xc** | Descriptor chain bus addr (high) |
| **0x310 + n·0xc** | **Descriptor count** (SG fragments, e.g. `0x7`) — not byte size |
| **0x1040** | CSC + dual-pixel — **programmed by blob**, not by us at stream time |
| **0x1088** | Working mode (dual-pixel downscale) — **programmed by blob** |

Chain entry (16 bytes): `[0]`/`[1]` target addr, `[2]` size in dwords, `[3]` control `0x80006000`.

---

## Build & quick start

### Prerequisites

Kernel cmdline (recommended):

```bash
ibt=off iommu=pt
```

- `ibt=off` — blob lacks ENDBR64; module sets `MODULE_INFO(ibt, "N")` and `-fcf-protection=none`
- `iommu=pt` — passthrough; still need correct **`q->dev`** for vb2 DMA mapping

### Build & load

```bash
./build.sh LLVM=1 CC=clang
modinfo driver/cx511h.ko | grep vermagic
sudo ./insmod.sh
```

Find device (listing only — do **not** stream with `v4l2-ctl`):

```bash
v4l2-ctl --list-devices
```

Wait for `cx511h_ite6805_event locked fe …` in `dmesg`, then:

```bash
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/videoX
sudo ./unload.sh
```

### Debug log filter

```bash
dmesg | grep -iE 'cx511h-phase4|cx511h-scale|cx511h-dma|gc573-payload|gc573-handoff|ite6805_event locked|ITE6805_LOCK'
```

### Dump one frame (userspace)

```bash
sudo dd if=/dev/videoX of=frame_raw.bin bs=4147200 count=1
xxd frame_raw.bin | head -4
```

| Hex pattern | Meaning |
|:---|:---|
| `00 00 00 00…` | Stale cache, wrong CSC path, or no DMA |
| `10 80 10 80…` | UYVY limited black — DMA likely OK |
| `80 10 80 10…` | YUYV order — try different `-input_format` |

---

## Module parameters

| Parameter | Default | Description |
|:---|:---:|:---|
| `force_input_mode` | 0 | 0=auto, 1=YUV422, 2=YUV444, 3=RGB full, 4=RGB limited |
| `debug_pixel_format` | -1 | -1=auto; 0–3 force YUV byte order |
| `auto_test_byteorder` | 0 | Cycle formats on stream_on (MMIO peek — prefer `dd`/`xxd`) |
| `no_signal_pic` | NULL | Bitmap path when no signal |
| `copy_protection_pic` | NULL | Bitmap when content is HDCP-protected. **Insmod name today:** `copy_protetion_pic` — upstream typo in `board_config.c` (missing `c` in *protection*) |
| `led_pin_r/g/b` | 3/4/5 | GPIO LED pins (-1=off) |

---

## Architecture

| Layer | Path | Role |
|:---|:---|:---|
| Entry | `driver/entry.c` | Module init, PCI IDs |
| Board | `driver/board/cx511h/board_config.c` | Probe, Phase 4 hardware init |
| Board | `driver/board/cx511h/board_v4l2.c` | V4L2, stream_on, buffer done |
| PCI | `driver/utils/pci/pci_model.c` | MMIO, V-DESC hook, I2C opt-in ACK |
| V4L2 | `driver/utils/v4l2/` | videobuf2, framegrabber, cache sync |
| Blob | `AverMediaLib_64.a` | ITE6805, IT6664, aver_xilinx, scaler |

---

## Known issues (honest list)

1. **Picture not reliable** — DMA IRQ and sync path work; userspace may still see green, zeros, or corrupt frames. Next: HDCP-off testing, handoff timing, confirm `[gc573-payload]` vs `dd`/`xxd`.

2. **No I2C writes while streaming** — by design. Do not re-enable TTL/unmute/streaming I2C blocks without new safety analysis.

3. **4K metadata split** — `ITE6805_LOCK` forces **1920×1080** into framegrabber for caps; FPGA **`vip_cfg`** uses **physical** `fe_frameinfo` for scaler. Both are intentional.

4. **`v4l2-ctl` streaming** — can trigger I2C and freeze. Use ffplay/ffmpeg with explicit UYVY.

5. **GStreamer scripts** (`gst_1.0_raw_video*.sh`) — broken / risky; call `v4l2-ctl`.

6. **Module refcnt −1** — killed clients can strand the module; `unload.sh` escalates to PCI remove + `rmmod -f` + rescan.

7. **Audio** — stub only.

8. **Legacy suspend/resume** — not migrated to `dev_pm_ops`.

9. **`vactive`/`hactive` naming** — vendor convention in `vip_cfg`: **`vactive` = horizontal width**, **`hactive` = vertical height** (not Linux/V4L2 semantics). In `stream_on`, `fe_frameinfo->width` → `vip_cfg.in_videoformat.vactive` and `fe_frameinfo->height` → `vip_cfg.in_videoformat.hactive`. Do not “fix” without checking bypass tables.

---

## Scripts

| Script | Purpose |
|:---|:---|
| `build.sh` | Build module, copy `.ko` to root |
| `insmod.sh` | Load deps + `driver/cx511h.ko` |
| `unload.sh` | Stop clients, rmmod, PCI remove/rescan if needed |
| `install.sh` | Install under `/lib/modules/.../avermedia/` |
| `gst_1.0_raw_video*.sh` | Legacy — avoid (see Known Issues) |

---

## Reverse engineering

- Windows driver comparison and live hardware testing
- **`AverMediaLib_64.a` disassembly** (`aver_xilinx.o`, `ite6805_sys.o`, `ite6664*.o`) for descriptor chain, scaler, and ITE6805 downscale logic
- Forensic tags: `[gc573-payload]`, `[gc573-chain]`, `[gc573-handoff]`, `[cx511h-scale]`, `[cx511h-phase4]`

---

## Legal / compliance

Interoperability-focused community project (EU Directive 2009/24/EC Art. 6). AVerMedia trademarks and the vendor blob belong to their respective owners.

> [!CAUTION]
> `AverMediaLib_64.a` is precompiled — check license before redistributing binaries.

---

## Disclaimer

Community project, not supported by AVerMedia. Use at your own risk.

**Repository:** [github.com/Everlite/Avermedia-GC573-Linux](https://github.com/Everlite/Avermedia-GC573-Linux)  
**Maintained by [Everlite](https://github.com/Everlite)** · Thanks to [derrod](https://github.com/derrod) for earlier work.
