# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted Linux driver for the AVerMedia GC573 (PCI `1461:0054`, subsystem `1461:5730`).
Modernized for recent kernels. **Experimental — development and testing only.**

**Last aligned with code:** 2026-08-01 · **Phase 4 stable; Phase 4b (picture) open**

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
| **reload / unload script** | 🟡 | `reload.sh` added but not yet proven end-to-end (Known issues #4) |
| **Probe / insmod** | ✅ | No hard-freeze when I2C IRQ opt-in ACK is active |
| **HDMI lock** | ✅ | ITE6805 events; **1080p-max EDID now advertised** (patched at build time) |
| **Phase 4 pipeline** | ✅ | Boot-time `iTE6805_Hardware_Init()`; MMIO-only `stream_on`; blob owns scaler/CSC |
| **V-DESC / DMA IRQ** | ✅ | Hook on `0x10` bit `0x2`; descriptor chain + handoff guards |
| **DMA to host RAM** | ✅ | `q->dev` binding + `dma_sync_*` on buffer done; full 1920×1080 frames delivered |
| **Userspace picture** | 🟡 | 1080p lock OK + full frames, but still constant filler (`0x10 0x80`) — see Phase 4b |
| **Audio** | 🟡 | ALSA capture PCM registers (Card “CL511H Stereo”), recognized by PipeWire/Discord; no captured audio bits |
| **Daily use** | ❌ | Capture testing only |

### Development phases

| Phase | Status | Summary |
|:---|:---:|:---|
| **1** — RE & bring-up | ✅ | Kernel port, probe, FPGA / ITE6805 attach |
| **2** — DMA / IRQ | ✅ | V-DESC hook, doorbell `0x304 ← 0x01`, descriptor chain understood |
| **3** — DMA coherency | ✅ | `q->dev = dev` in vb2; cache sync before handoff |
| **4** — Stable streaming path | ✅ **BREAKTHROUGH** | No I2C writes at stream time; boot bootstrap; 1080p-max EDID; full frames delivered |
| **4b** — Picture quality | 🟡 | 1080p lock + full DMA frames + V-DESC IRQ confirmed; TTL color-space fix applied + TTL re-assert deferred task; timing normalized to 1080p60 — **needs a clean retest after reboot** |

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

**When is HDMI ready?** The reliable “go” signal is this line in `dmesg`:

```
cx511h_ite6805_event locked fe 1920x1080p (raw)
```

`(raw)` is the physical timing from ITE6805. Since the EDID fix (below) the card advertises
**1080p-max**, so a PS5/DVSI source configures to 1080p and this lock line shows `1920x1080p`;
only then open `/dev/videoX`.

> ⚠️ **Do not use the physical LED as a signal indicator.** The only LED code is
> `cx511h_set_led_color()` (board_v4l2.c:131), which pokes generic FPGA GPIO pins
> 3/4/5 via `aver_xilinx_set_gpio_output()` — but those pin numbers are
> unconfirmed guesses with **no verified wiring to the card's physical LED**. The
> LED is observed flashing red regardless of lock state. It is cosmetic-only and
> unrelated to card functionality. **Trust the `dmesg` lock line, not the LED.**

### 1080p-only EDID + HPD re-negotiation (2026-08-01)

**Problem:** The card’s vendor EDID tables (embedded in `AverMediaLib_64.a`) still advertised
**4K**. A source such as a PS5 locked to 4K → the dual-pixel downscale path ran and delivered
**black/empty frames**. The driver’s own 1080p EDID in `ite6805_EDID.h` was **dead code**
(never compiled in — the blob ships its own copies).

**Fix (at build time):**
- `driver/patch_edid.py` overwrites the **ITE6805** `Default_Edid_Block`/`Fix_Edid_Block` and the
  **IT6664** `Default_Edid_table4k`/`table2k` within `AverMediaLib_64.o` (an `ar` archive) with a
  checksum-correct **1080p-max** EDID.
- `driver/Makefile` reruns `patch_edid.py` after the blob is copied into the build.
- `driver/board/cx511h/board_config.c`: after `iTE6805_Hardware_Init()` (module-param-gated,
  default on) it pulses HPD (LOW→HIGH) via `x_IssueHotPlug()` so the source re-reads the new EDID:

```
[cx511h-edid] Forcing HPD re-negotiation (EDID is now 1080p-max)...
[cx511h-edid] HPD pulse complete
```

**Effect observed:** source renegotiates to `1920x1080p`, `dual=0`, `bypass=0` — full frames are
delivered over DMA. **Still open:** content is a constant filler (`0x10 0x80`) see Phase 4b.

### Sterile `stream_on` (MMIO / FPGA only)

**Policy:** **No ITE6805 register writes** in `cx511h_stream_on()`. Log line:

```
[cx511h-dma] === STREAM ON (MMIO/FPGA only — no I2C) ===
```

**4K → 1080p:** Physical timing comes from `ite6805_get_frameinfo()` (e.g. 3840×2160), not framegrabber metadata (which `ITE6805_LOCK` may force to 1920×1080 for V4L2 caps). Since the 1080p-max EDID fix this path is normally bypassed (source negotiates 1080p directly, `dual=0`, `bypass=0`), but it still guards against an unexpectedly 4K source. When input exceeds output:

- `vip_cfg.in_videoformat.vactive/hactive` = physical HDMI size  
- `vip_cfg.out_videoformat.width/height` = V4L2 output (e.g. 1920×1080)  
- `vip_cfg.dual_pixel = 1`, `vip_cfg.video_bypass = 0`  
- `valid_mask` includes `SCALER_CFG_SHRINK_MASK`  
- **Single call:** `aver_xilinx_config_video_process()` — blob programs XV scaler, **`0x1088`**, and **`0x1040`** (CSC + dual-pixel)

**Do not** manually write `0x1040` or `0x1088` after the blob call — post-patching CSC broke ingest and produced **`00`** frames.

### I2C IRQ deadlock (probe)

Hardware asserts IRQ bit **`0x800`** [I2C complete] continuously. **Fix in `pci_model.c`:** opt-in ACK only when `i2c_waiters > 0`. Unconditional ACK starved the blob’s I2C poll loop and froze `insmod`.

---

## Phase 4b — Picture quality (open — resume here)

**Where we are (2026-08-01):** With the 1080p EDID fix the card locks to `1920x1080p`,
`dual=0`, `bypass=0`, and **full 4,147,200-byte DMA frames** reach userspace
(`buffer_prepare` programs an 8-frag SG descriptor list, e.g.
`desc[0..7] = 0x200000 … 0x1000`). Pictures are **not** there yet.

**Symptom:** every frame is a constant filler — `unique bytes ≈ 3` (`{0, 0x10, 0x80}`;
hex dump reads `10 80 10 80 …` = UYVY with **Y=128** and U/V=16/0). That is a typical
“video idle / no pixel data” fill injected by the scaler, not real HDMI content.

**Root cause found this session (Windows-driver decompilation cross-check):**
The **TTL output format** of the IT6805 — the parallel bus that carries video to the FPGA —
was chosen in `ITE6805_LOCK` from `fe_frameinfo->packet_colorspace`. That field is
**unreliable** (the blob leaves it as `CS_YUV(0)` even when the PS5 is really sending RGB).
With the forced-1080p override (`fe_frameinfo->pixel_clock` is clamped to `148500000`
< `170000000`), the code always fell into the `else` branch and wrote
`ITE6805_OUT_FORMAT_SDR_ITU656_24_MODE0` — a **YUV/ITU-656** bus format. On an RGB source the
FPGA therefore ingests RGB bytes as YUV → constant blank/filler frames.

**Fix applied (in `board_v4l2.c`, `ITE6805_LOCK`):**
the TTL output format is selected from the live `ite6805_get_colorspace()` value
(`0=yuv,1=rgb-limited,2=rgb-full`). On an RGB source the low-pclk branch writes
`ITE6805_OUT_FORMAT_SDR_444_24` (24-bit RGB 4:4:4) instead of the YUV ITU-656 format.

**Timing problem recognised:** the ITE6805 timing readback is **unreliable** — it reports
`pixel_clock=124952…126457` and `fps_in=51` for what the PS5 actually sends as
`1920x1080p60` (should be `148500 kHz / 60`). Because the ITE6805 reports exactly
`1920×1080` (not >1920), the old forced-1080p override (`width>1920`) never fired for it.

**Two fixes applied (in `board_v4l2.c`, module rebuilt `cx511h.ko`, 2026-08-01):**

1. **TTL re-assert deferred task** — `ITE6805_LOCK` fires *before* the AVI info-frame
   negotiation settles, so `ite6805_get_colorspace()`/`get_sampingmode()` return unstable
   values there (`eff_cs=0`, `sampling=4`). The TTL format choice was therefore moved into
   the already-scheduled **`check_signal_stable_task`** (~1.5 s after lock) where the values
   are stable: it logs `[cx511h-ttl] check_signal_stable_task: eff_cs=%u sampling=%u
   -> out_format=0x%02x` and now **confirms `eff_cs=1 sampling=0 → out_format=0x40`** (RGB 444
   TTL) for the PS5. I2C writes are safe in that task context (NOT in `stream_on`).

2. **1080p60 timing normalizer** (in `stream_on`, before `aver_xilinx_config_video_process`)
   and an extended `ITE6805_LOCK` override: if input is `1920×1080` but the readback is not
   valid 1080p60, force `pixel_clock=148500000`, `fps=60`, `clip_size=1920×1080`, `dual_pixel=0`.
   NOTE vendor naming (README #9): **`vactive`=horizontal width, `hactive`=vertical height** —
   the first version of this fix checked `hactive==1920` and was silently inert; the corrected
   check is `vactive==1920 && hactive==1080`. Log: `[cx511h-debug] stream_on: normalizing
   1080p60 timing (was vactive=%u hactive=%u pclk=%u fps=%u)`.

**Confirmed this session:**
- **DMA datapath works end-to-end.** The `[gc573-intercept] V-DESC slot …` hook now shows the
  FPGA executing transfers: `slot 1: chain ptr 0x308=0x67ce0000 … desc_count 0x310=0x00000007`
  (0x310=7 matches our 8-frag SG list; 0x308 holds the blob's internal chain address, **not**
  the frame buffer — see FPGA MMIO reference). This settles the earlier README open question:
  the blob re-arms desc slots on the *next* V-DESC after stream start, which is why `0x308`
  reads `0x00000000` right after `enable_video_streaming(TRUE)` and is **not** an error.
- `[cx511h-color] AUTO(src): RGB Limited BT709` (in_colorspacemode=1, in_packetsamplingmode=0).

**Windows-driver context (still relevant):** `FUN_140045168` shows Windows explicitly programs the
ITE6805 CSC registers `0x6b`/`0x6c`/`0x6e` (+ a 22-byte CSC table in `0x70`) from the decoded
input AVI info-frame, and bypasses CSC for RGB→RGB (`0x6c=0`). Our Linux driver has no direct
I2C write for those registers (the blob only exposes getters + `ite6805_set_out_format`), so
FPGA-side color grading is driven purely by `vip_cfg.currentCSC`; the TTL-bus fix is the
actionable driver-level equivalent.
- The physical card LED flashes **red** regardless of lock state — cosmetic only, no verified
  GPIO wiring; ignore it. Trust the `dmesg` lock line.

**Wedge / retest caveat (why a clean stream test is still pending):** the last `./insmod.sh`
run hit the module **wedged** (audio/other holders kept `refcnt` ≥1 → `rmmod -f cx511h` was
*killed* and `insmod` failed with `Device or resource busy`). The `v4l2-ctl` test then read a
stale `/tmp/frame.raw` and the `dmesg` came from the *old*, pre-fix module instance — so the
1080p60 normalizer has **not been exercised yet**. A clean retest requires a reboot (or a
successful `./unload.sh`), then `sudo ./insmod.sh` + one stream. Re-checking the `unique bytes`
meter and the `[gc573-intercept]`/`normalizing` logs on that fresh instance is the decisive test.

**Next steps (do after a clean reload/reboot):**
1. Reload freshly built `cx511h.ko` (reboot → `sudo ./insmod.sh`), stream once, then grep:
   `dmesg | rg "normalizing 1080p60|cx511h-color AUTO|cx511h-ttl|gc573-intercept|AVER_LIVE_HEX_DUMP"`
   and re-check the frame. Expect `normalizing … (was vactive=1920 hactive=1080 pclk=125550)`
   and `[gc573-intercept]` transfer logs instead of the old `0x308=0` reads.
2. If still filler with a modern `AVER_LIVE_HEX_DUMP` (frame 100), the remaining suspects are
   the **scaler/ingest active window** and the **byte-pair order** — probe `0x300` slot-advance
   across frames, and try `v4l2-ctl -input_format` UYVY vs YUYV (README hex-table).
3. As a last resort, capture intermediate MMIO at the FPGA input via the existing
   `debug_pixel_format` (0–3) path (reading `0x308` early is invalid).

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
| GStreamer helper scripts (`gst_1.0_raw_video*.sh`) | Legacy, risky — use `v4l2-ctl` instead |

**Safe doorbell:** `0x304 ← 0x01` only (run bit, no slot arm at stream start).

---

## `stream_on` flow (current code)

Source: `driver/board/cx511h/board_v4l2.c` → `cx511h_stream_on()`.

1. **Read-only blob APIs** — `ite6805_get_frameinfo()`, `get_workingmode()`, `get_colorspace()`, `get_sampingmode()` (no register writes from our side)
2. Build **`vip_cfg`** — physical input dims, V4L2 output, colorspace (`force_input_mode` optional)
3. **Normalize bogus 1080p timing** — if input is `1920×1080` but readback `pclk`/`fps` are not valid 1080p60, force `pixel_clock=148500000`, `fps=60`, `clip_size=1920×1080`, `dual_pixel=0` (see Phase 4b)
4. **`aver_xilinx_enable_video_streaming(FALSE)`** + `msleep(50)`
5. Re-seal **`vip_cfg`** for downscale path if `fe_frameinfo` > output resolution
6. **`aver_xilinx_config_video_process(&vip_cfg)`** — blob only; no manual `0x1040`
7. `msleep(200)`; optional pixel-format debug (`debug_pixel_format`, `auto_test_byteorder`)
8. **`aver_xilinx_enable_video_streaming(TRUE)`**
9. Doorbell **`0x304 ← 0x01`**

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

Kernel cmdline (compatibility, usually only needed on some kernels):

```bash
ibt=off iommu=pt
```

- `ibt=off` — blob lacks ENDBR64; the module sets `MODULE_INFO(ibt, "N")` and `-fcf-protection=none`, so this is normally **not** required
- `iommu=pt` — passthrough IOMMU; keep it if DMA map failures occur, but still need correct **`q->dev`** for vb2 DMA mapping

### Build & load

```bash
./build.sh LLVM=1 CC=clang
modinfo cx511h.ko | grep vermagic        # must match `uname -r`
sudo ./insmod.sh
```

Find device (safe to list):

```bash
v4l2-ctl --list-devices
```

Wait for `cx511h_ite6805_event locked fe 1920x1080p …` in `dmesg`, then capture (or view in `ffplay`):

```bash
sudo v4l2-ctl -d /dev/videoX --set-fmt-video=width=1920,height=1080 \
  --stream-mmap=3 --stream-count=1 --stream-to=/tmp/frame.raw
xxd /tmp/frame.raw | head -4
ffplay -f v4l2 -input_format uyvy422 -video_size 1920x1080 -framerate 60 /dev/videoX
```

**Unload / reload without reboot:**

```bash
sudo ./unload.sh     # safe clean rmmod (kills audio holders) — never touches PCI 'remove'
sudo ./reload.sh     # unload + insmod the freshly built cx511h.ko
```

> `unload.sh` no longer uses the old "RADICAL PCI REMOVE" fallback
> (`echo 1 > /sys/bus/pci/devices/…/remove`). That path wedged the module in
> `MODULE_STATE_GOING` / `refcnt -1` and forced a reboot. It now only kills/restarts the
> capture+audio users and runs a clean `rmmod`; if the module is genuinely stuck it says so
> instead of making it worse.

> ⚠️ **`reload.sh` / `unload.sh` are still work-in-progress — treat as possibly broken.**
> The rewrite removed the dangerous removal path, but the reload workflow has **not** been
> proven end-to-end yet:
> - A first live attempt of `./reload.sh` hit `unload.sh: command not found` (needs `./`;
>   fixed since) and then `insmod: File exists` because the module had never actually been
>   unloaded.
> - The cleanest verified path is still: **reboot → `sudo ./insmod.sh`**. Audio/PipeWire
>   (and even Discord) may re-grab the CL511H PCM, which can keep `refcnt` at 1 and block
>   `rmmod`/`reload` until the audio holders are stopped.

### Debug log filter

```bash
dmesg | grep -iE 'cx511h-phase4|cx511h-scale|cx511h-dma|gc573-payload|gc573-handoff|ite6805_event locked|ITE6805_LOCK|cx511h-edid|cx511h-color'
```

### Dump one frame (userspace)

```bash
sudo v4l2-ctl -d /dev/videoX --set-fmt-video=width=1920,height=1080 \
  --stream-mmap=3 --stream-count=1 --stream-to=/tmp/frame.raw
python3 -c "
d=open('/tmp/frame.raw','rb').read(1_000_000)
print('unique bytes in first MB:', len(set(d)))   # >~50 = real pixels; ~2-3 = constant filler
"
xxd /tmp/frame.raw | head -4
```

| Hex pattern | Meaning |
|:---|:---|
| `00 00 00 00…` | Stale cache, wrong CSC path, or no DMA |
| `10 80 10 80…` | UYVY constant filler (Y=128 “no signal”) — DMA ok, no video content |
| `80 10 80 10…` | YUYV order — try different `-input_format` |
| many distinct bytes | Real pixels are flowing |

---

## Module parameters

| Parameter | Default | Description |
|:---|:---:|:---|
| `edid_force_hpd` | 1 | Pulse HPD after `iTE6805_Hardware_Init()` so the source re-reads the 1080p-max EDID (set 0 to skip) |
| `force_input_mode` | 0 | 0=auto, 1=YUV422, 2=YUV444, 3=RGB full, 4=RGB limited |
| `debug_pixel_format` | -1 | -1=auto; 0–3 force YUV byte order |
| `auto_test_byteorder` | 0 | Cycle formats on stream_on (MMIO peek) |
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

1. **Picture is still constant filler** — DMA fully works (full 1920×1080 frames), 1080p lock is correct, the FPGA `[gc573-intercept]` V-DESC hook confirms real DMA transfers, but the content is a constant `0x10 0x80` (Y=128). The video datapath is not feeding real pixels. See **Phase 4b** for the current fixes (TTL 444, 1080p60 timing normalizer) and remaining next steps. **This is the #1 open issue.**

2. **No I2C writes while streaming** — by design. Do not re-enable TTL/unmute/streaming I2C blocks without new safety analysis.

3. **4K metadata split** — `ITE6805_LOCK` forces **1920×1080** into framegrabber for caps; FPGA **`vip_cfg`** uses **physical** `fe_frameinfo` for scaler. Both are intentional. (With the 1080p-max EDID this is now mostly moot, source negotiates 1080p.)

4. **`reload.sh` / `unload.sh` / a wedged module block a clean retest** — the dangerous PCI-remove fallback was removed, but a live reload attempt exposed issues (missing `./`; `insmod File exists`), and on 2026-08-01 `insmod.sh` hit the module **wedged**: audio holders kept `refcnt`≥1, so its `rmmod -f cx511h` was **killed** and the follow-up `insmod` failed with `Device or resource busy` — the module stayed loaded with OLD code, and any `v4l2-ctl` ran against a stale `frame.raw`/old `dmesg`. **A genuine retest needs a reboot → `sudo ./insmod.sh`.** See Build & quick start warning.

5. **GStreamer scripts** (`gst_1.0_raw_video*.sh`) — legacy / risky; use `v4l2-ctl` or `ffplay`.

6. **Module refcnt pinned** — audio holders (PipeWire / Discord) keep the CL511H PCM open and hold `refcnt` at 1, blocking `rmmod`/`reload`; `insmod.sh`'s auto `rmmod -f` cannot clear a truly wedged module (it gets killed, then `insmod` → `Device or resource busy`). `unload.sh` kills the holders first; if the module stays wedged (`refcnt −1`, `GOING`) only a reboot helps — `unload.sh` no longer tries the dangerous PCI-remove path.

7. **Audio** — ALSA PCM registers and looks like a capture device ("CL511H Stereo"), but no captured audio bits are delivered yet.

8. **Legacy suspend/resume** — not migrated to `dev_pm_ops`.

9. **`vactive`/`hactive` naming** — vendor convention in `vip_cfg`: **`vactive` = horizontal width**, **`hactive` = vertical height** (not Linux/V4L2 semantics). In `stream_on`, `fe_frameinfo->width` → `vip_cfg.in_videoformat.vactive` and `fe_frameinfo->height` → `vip_cfg.in_videoformat.hactive`. Do not “fix” without checking bypass tables.

---

## Scripts

| Script | Purpose |
|:---|:---|
| `build.sh` | Build module (stages out of space-containing path), copy `cx511h.ko` to root |
| `insmod.sh` | Load deps (`videobuf2_dma_*`) + `./cx511h.ko` from project root, auto-find our V4L2 node |
| `unload.sh` | Safe unload: stop capture/audio holders, clean `rmmod` (no PCI remove) |
| `reload.sh` | Safe unload (`./unload.sh`) + `insmod ./cx511h.ko` — work in progress |
| `install.sh` | Install under `/lib/modules/.../avermedia/` |
| `gst_1.0_raw_video*.sh` | Legacy — avoid (see Known Issues) |

---

## Reverse engineering

- Windows driver comparison and live hardware testing
- **`AverMediaLib_64.a` disassembly** (`aver_xilinx.o`, `ite6805_sys.o`, `ite6664*.o`) for descriptor chain, scaler, and ITE6805 downscale logic
- Forensic tags: `[gc573-payload]`, `[gc573-chain]`, `[gc573-handoff]`, `[cx511h-scale]`, `[cx511h-phase4]`, `[cx511h-color]`, `[cx511h-edid]`, `[cx511h-dma]`, `[cx511h-desc]`, `[cx511h-pixfmt]`

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
