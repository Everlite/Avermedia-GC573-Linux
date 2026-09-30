# AVerMedia Live Gamer 4K (GC573) — Linux Driver (Kernel 6.19–7.x)
[![Status](https://img.shields.io/badge/status-experimental%20alpha-orange.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#status)
[![Kernel](https://img.shields.io/badge/kernel-6.19–7.x%20tested-2e7d32.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#kernel-compatibility)
[![AI-Assisted](https://img.shields.io/badge/AI-assisted-blue.svg)](https://github.com/Everlite/Avermedia-GC573-Linux#reverse-engineering-methods)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.md)

Community-maintained, AI-assisted Linux driver for the AVerMedia GC573 (PCI `1461:0054`, subsystem `1461:5730`).
Modernized for recent kernels. **Experimental — development and testing only.**

**Last aligned with code:** 2026-09-30 · **Live 1080p picture confirmed**

> [!NOTE]
> **Vendor blob:** Links against `AverMediaLib_64.a` in the **repository root** (~565 KB). The Makefile copies it to `driver/AverMediaLib_64.o` at build time. Redistribution of the blob may be restricted — see [Legal](#legal--compliance).

> [!NOTE]
> Rebuild after every kernel upgrade. `vermagic` must match `uname -r` (`modinfo cx511h`).

> [!IMPORTANT]
> **2026-09-30 — first picture on the maintainer's machine.** CachyOS 7.2.8, Intel Z690, PS5 with HDCP off. The card locked `1920×1080` YUYV at 60 fps on `/dev/video2`, and OBS 32 showed the PS5 home screen through a V4L2 source. Two captured frames differed and were not the old constant filler.
>
> Thanks to [Lou Perret](https://github.com/lou-perret). [#7](https://github.com/Everlite/Avermedia-GC573-Linux/pull/7) and [#8](https://github.com/Everlite/Avermedia-GC573-Linux/pull/8) are what turned "DMA delivers filler" into something you can watch. Audio and native 4K in the tables below are still his measurements; this machine has only confirmed the 1080p picture so far.

---

## Status

**Kernel:** Builds on **6.19.x–7.x** with matching headers (tested on CachyOS / Clang kernels). No portable prebuilt `.ko` — run `./build.sh LLVM=1 CC=clang` on the target machine.

| Area | Status | Notes |
|:---|:---:|:---|
| **Build / load** | ✅ | `LLVM=1 CC=clang`; `insmod.sh` / `unload.sh` |
| **reload / unload script** | ✅ | `insmod.sh` delegates to `unload.sh` when already loaded; the teardown page fault that wedged the module is fixed (Phase 5) |
| **Probe / insmod** | ✅ | No hard-freeze when I2C IRQ opt-in ACK is active |
| **HDMI lock** | ✅ | ITE6805 events; 1080p-max EDID by default, vendor 4K60 EDID with `GC573_EDID=vendor` |
| **Phase 4 pipeline** | ✅ | Boot-time `iTE6805_Hardware_Init()`; MMIO-only `stream_on`; blob owns scaler/CSC |
| **V-DESC / DMA IRQ** | ✅ | Hook on `0x10` bit `0x2`; descriptor chain + handoff guards |
| **DMA to host RAM** | ✅ | `q->dev` binding + `dma_sync_*` on buffer done; full 1920×1080 frames delivered |
| **Userspace picture** | ✅ | YUYV 1920×1080 at 60 fps. Maintainer: live PS5 image, 2026-09-30. Lou: SMPTE bars bit-exact — see Phase 5 |
| **4K capture** | ✅ | YUYV 3840×2160, 24–60 Hz sources captured at 60 fps (`GC573_EDID=vendor`, `normalize_timing=0`) — see Phase 6 |
| **Audio** | ✅ | ALSA capture, 16-bit stereo LPCM at the source rate (32/44.1/48 kHz) — see Phase 5 |
| **Daily use** | 🟡 | Works with `v4l2-ctl` / `ffmpeg`. HDCP is garbled, and a mid-stream format change (Discord) cuts audio |

### Development phases

| Phase | Status | Summary |
|:---|:---:|:---|
| **1** — RE & bring-up | ✅ | Kernel port, probe, FPGA / ITE6805 attach |
| **2** — DMA / IRQ | ✅ | V-DESC hook, descriptor chain understood (the `0x304 ← 0x01` doorbell was later found harmful, see Phase 5) |
| **3** — DMA coherency | ✅ | `q->dev = dev` in vb2; cache sync before handoff |
| **4** — Stable streaming path | ✅ **BREAKTHROUGH** | No I2C writes at stream time; boot bootstrap; 1080p-max EDID; full frames delivered |
| **4b** — Picture quality | ✅ | TTL 444 + deferred re-assert + 1080p60 timing normalization; the remaining filler came from the hot-plug GPIO and the source never locking (see Phase 5) |
| **5** — Continuous capture & audio | ✅ **BREAKTHROUGH** | Ring owned by the blob, red zone patch, zero-size DMA fix, HDMI audio EDID, ALSA constraints |
| **6** — Native 4K | ✅ | Pixel clock unit fix, frame rate correction, vendor EDID build switch |

---

## Phase 6 — Native 4K (2026-09)

Build with `./build.sh GC573_EDID=vendor` to keep the vendor 4K60 EDID tables, and load with
`normalize_timing=0` so the lock handler does not force 1080p.

| # | Problem | Fix |
|:---|:---|:---|
| 1 | The ITE6805 reports `pixel_clock` in kHz, but the dual-pixel and TTL output format decisions compared it with 170000000 Hz: 4K never selected the `2X24_INTERLEAVE` format | `cx511h_pclk_khz()`, compare in kHz whatever the unit |
| 2 | The ITE6805 under-reads the frame rate by the same factor as the pixel clock (26 fps for 2160p30, 51 for 1080p60) | `cx511h_true_framerate()`, rescaled against the nearest standard pixel clock |
| 3 | FPGA registers cannot be read from userspace while the driver holds BAR0 | `reg_read` debug parameter |

**Results:** 3840×2160 YUYV sources at 24, 25, 30, 50 and 60 Hz captured at 60 fps (the FPGA repeats
frames for lower source rates). A 4K60 moving pattern gives 120/120 unique frames. RGB and YCbCr 4:2:0 sources
have correct colours. The blob does not implement a 4K → 1080p downscale (clip and scaler are
programmed with the output size), so capture at the source resolution.

---

## Phase 5 — Continuous capture and audio (2026-09)

Tested on kernel 7.2 (Arch, GCC build) in a VFIO passthrough VM, with an HDMI loopback from an
AMD GPU and SMPTE bars / a 1 kHz tone as references.

| # | Problem | Fix |
|:---|:---|:---|
| 1 | `rmmod` page fault in `pci_model_mmio_read()`, module wedged (`refcnt -1`) | BARs unmapped only after `cxt_manager_release()` (`pci_model.c`) |
| 2 | FPGA GPIO pin 5 ("blue LED") driven low on lock drops the source hot-plug: FPGA sees `size 0x0` | `led_pin_b` defaults to `-1` |
| 3 | The FPGA ignores `vip_cfg.pixel_format` and always delivers YUYV | Only YUYV is advertised |
| 4 | `VIDIOC_ENUM_FMT` NULL dereference past the last format | `-EINVAL` before dereferencing |
| 5 | No `S_FMT` → frame size 0 → zero-length DMA descriptors → the FPGA drops off the PCIe bus (MMIO reads `0xffffffff`, host platform reset) | Default 1920×1080, descriptor loop stops at `remain == 0` |
| 6 | First V-DESC fires before `START_STREAMING` is set; `framegrabber_mask_s_status()` calls `stream_on` again and `config_video_process()` wipes the ring | `cx511h_dma_streaming` flag, `stream_on` ignored while streaming |
| 7 | The board path wrote `0x304 = 0x01` after every frame, clearing the slot arm bits set by the blob: capture stalled after a few frames | The blob owns the ring (`legacy_doorbell=0`) |
| 8 | `AverMediaLib_64.a` built without `-mno-red-zone`: interrupts corrupt 80 leaf functions (oops `0x297 + 0x58`, `0x297` = RFLAGS) | `driver/patch_redzone.py`: `push %rbp; mov %rsp,%rbp` → `enter $0x100,$0`, `pop %rbp` → `leave` |
| 9 | Injected EDID had no audio flag and no HDMI VSDB: sources treat the card as DVI, no audio | CEA block rewritten (LPCM 2ch 32/44.1/48 kHz, speaker allocation, VSDB) |
| 10 | ALSA advertised S24 and up to 192 kHz without any rate constraint | S16_LE at 32/44.1/48 kHz, rate locked to the source rate measured by the FPGA |
| 11 | ~760 kernel log lines per second while streaming | Per-buffer logs behind `dma_debug` |

**Results:** SMPTE bars captured bit-exact (mean absolute error 0.3/255), 6000 frames at 60 fps
without any oops, 1 kHz tone captured at 48 kHz (THD+N −76 dB, no dropout), combined A/V capture
with `v4l2-ctl` + ALSA.

**HDCP:** not solved. Sources that enable HDCP (iPhone, MacBook) do not get a clean
copy-protection frame; the picture comes out garbled, split, or green.

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

## Phase 4b — Picture quality (historical, resolved in Phase 5)

**Where we are (2026-08-01):** With the 1080p EDID fix the card locks to `1920x1080p`,
`dual=0`, `bypass=0`, and **full 4,147,200-byte DMA frames** reach userspace
(`buffer_prepare` programs an 8-frag SG descriptor list, e.g.
`desc[0..7] = 0x200000 … 0x1000`). Pictures are **not** there yet.

**Symptom:** every frame is a constant filler — `unique bytes ≈ 3` (`{0, 0x10, 0x80}`;
hex dump reads `10 80 10 80 …` = UYVY with **Y=128** and U/V=16/0). That is a typical
“video idle / no pixel data” fill injected by the scaler, not real HDMI content.

**Root cause found (Windows-driver decompilation cross-check):**
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

**Fixes applied (in `board_v4l2.c`, module rebuilt `cx511h.ko`, 2026-08-01).** All timing
overrides are gated by the **module param `normalize_timing`** (default `1`; toggle at runtime
via `/sys/module/cx511h/parameters/normalize_timing`, no rebuild):

1. **TTL re-assert deferred task** — `ITE6805_LOCK` fires *before* the AVI info-frame
   negotiation settles, so `ite6805_get_colorspace()`/`get_sampingmode()` return unstable
   values there (`eff_cs=0`, `sampling=4`). The TTL format choice is therefore re-applied in
   the already-scheduled **`check_signal_stable_task`** (~1.5 s after lock) where the values
   are stable: it logs `[cx511h-ttl] check_signal_stable_task: eff_cs=%u sampling=%u
   -> out_format=0x%02x` and confirms `eff_cs=1 sampling=0 → out_format=0x40` (RGB 444 TTL)
   for the PS5. I2C writes are safe in that task context (NOT in `stream_on`).

2. **1080p60 timing normalization.** Two places, distinct scope:
   - In **`ITE6805_LOCK`**, when the input is `1920×1080` but the readback is not valid
     1080p60, the override forces `width/height=1920×1080`, `framerate=60`, `denominator=1`,
     `pixel_clock=148500000`, `dual_pixel=0`, `dual_pixel_like=0`, `sampling_mode=0`,
     `ddr_mode=0`.
   - In **`stream_on`** (before `aver_xilinx_config_video_process`), a lighter normalizer
     corrects **only** `pixel_clock=148500000` and `fps=60` when input is `1920×1080` and
     readback `pclk`/`fps` are off. It deliberately does **not** touch `out_videoformat`,
     `clip_size`, `dual_pixel`, or `in_ddrmode` — those are derived from the width/height and
     pclk-based dual-pixel logic above, and touching them (incl. forcing `in_ddrmode`) was
     correlated with the FPGA ingest producing no V-DESC transfers at all.
   NOTE vendor naming (README #9): **`vactive`=horizontal width, `hactive`=vertical height** —
   so a 1920×1080 input is `vactive=1920 && hactive=1080`.
   Log: `[cx511h-debug] stream_on: normalizing 1080p60 timing (was vactive=%u hactive=%u pclk=%u fps=%u)`.

**Confirmed:**
- **DMA datapath works end-to-end.** The `[gc573-intercept] V-DESC slot …` hook shows the
  FPGA executing transfers: `slot N: chain ptr 0x308=0x67ce0000 … desc_count 0x310=0x000000NN`
  (0x310 matches our SG list fragment count; 0x308 holds the blob's internal chain address,
  **not** the frame buffer — see FPGA MMIO reference). The blob re-arms desc slots on the *next*
  V-DESC after stream start, which is why `0x308` may read `0x00000000` right after
  `enable_video_streaming(TRUE)` and is **not** an error.
- `[cx511h-color] AUTO(src): RGB Limited BT709` (in_colorspacemode=1, in_packetsamplingmode=0).

**Windows-driver context (still relevant):** `FUN_140045168` shows Windows explicitly programs the
ITE6805 CSC registers `0x6b`/`0x6c`/`0x6e` (+ a 22-byte CSC table in `0x70`) from the decoded
input AVI info-frame, and bypasses CSC for RGB→RGB (`0x6c=0`). Our Linux driver has no direct
I2C write for those registers (the blob only exposes getters + `ite6805_set_out_format`), so
FPGA-side color grading is driven purely by `vip_cfg.currentCSC`; the TTL-bus fix is the
actionable driver-level equivalent.
- The physical card LED flashes **red** regardless of lock state — cosmetic only, no verified
  GPIO wiring; ignore it. Trust the `dmesg` lock line.

**Current blocker (still open):** despite the above (deterministic `out_format=0x40` RGB TTL and
normalized 1080p60 timing), streamed payload stays constant filler (`unique=2/3`). The colour/
TTL/timing path is therefore **ruled out as the sole blocker**. A continuous `[gc573-intercept]`
V-DESC series has not yet been observed during a locked + normalized stream (only single transfer
events around lock/stream transitions; a session with `in=0x0 ddr=1` showed valid `0x308`,
locked sessions with `ddr=0` did not). Next suspects to investigate (in order):

1. The **driver unload hang**: `rmmod` can die with `refcnt=-1 / initstate=going` (module WEDGED)
   when the teardown path hangs, forcing a reboot despite `unload.sh`'s safe release order. The
   PCI-remove fallback stays removed (never touches the bus). Fixing this makes iteration cheap.
2. Lower the `AVER_LIVE_HEX_DUMP` trigger from frame 100 to an early frame to expose the real raw
   bytes of a locked frame (byte-pair order / scaler ingest vs. content).
3. Defined A/B on the DMA-ingest datapath: `normalize_timing=0/1` and letting `ddr_mode` pass
   through unchanged (the LOCK override currently forces `ddr_mode=0`), then observe which combo
   produces a **continuous** V-DESC series and `unique>50`.
4. Probe the scaler/ingest **active `clip`/window** and `0x300` slot index advance across frames;
   try `v4l2-ctl -input_format` UYVY vs YUYV (README hex-table). As a last resort, capture
   intermediate MMIO at the FPGA input via `debug_pixel_format` (0–3) (reading `0x308` early is
   invalid).

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
| Any board-side write to `0x304` or ACK of `0x10` while streaming | Clears the slot arm bits set by the blob → **capture stalls** |
| Write V4L2 frame address into `0x308` | Wrong semantics — `0x308` is **chain pointer**, not frame |
| Capture without a frame size (no `S_FMT` on an old build) | Zero-length DMA descriptors → **FPGA off the bus, host reset** |
| Linking the blob without `patch_redzone.py` | Random corruption from interrupts |
| GStreamer helper scripts (`gst_1.0_raw_video*.sh`) | Legacy, risky — use `v4l2-ctl` instead |

**Ring ownership:** the blob programs and arms the descriptor slots itself; the board path only
prepares descriptor lists and hands buffers back.

---

## `stream_on` flow (current code)

Source: `driver/board/cx511h/board_v4l2.c` → `cx511h_stream_on()`.

1. **Read-only blob APIs** — `ite6805_get_frameinfo()`, `get_workingmode()`, `get_colorspace()`, `get_sampingmode()` (no register writes from our side)
2. Build **`vip_cfg`** — physical input dims, V4L2 output, colorspace (`force_input_mode` optional)
3. **Normalize bogus 1080p timing** — if input is `1920×1080` but readback `pclk`/`fps` are not valid 1080p60, force `pixel_clock=148500000` and `fps=60` (only those two; gated by `normalize_timing` module param). See Phase 4b.
4. **`aver_xilinx_enable_video_streaming(FALSE)`** + `msleep(50)`
5. Re-seal **`vip_cfg`** for downscale path if `fe_frameinfo` > output resolution
6. **`aver_xilinx_config_video_process(&vip_cfg)`** — blob only; no manual `0x1040`
7. `msleep(200)`; optional pixel-format debug (`debug_pixel_format`, `auto_test_byteorder`)
8. Set `cx511h_dma_streaming`, then **`aver_xilinx_enable_video_streaming(TRUE)`** (the blob transfers the ready descriptor lists and sets the run bit)

A second `stream_on` while streaming returns immediately.

**Per frame:** V-DESC IRQ → blob `aver_xilinx_irq_func()` (clears the completed slot bit, ACKs `0x10`) → blob video DPC → `cx511h_video_buffer_done()` → handoff guard → `dma_sync_*` → `v4l2_model_buffer_done()`. The blob then transfers the next ready descriptor list.

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
| **0x304** bit `0` | Stream run — set by the blob in `enable_video_streaming()` |
| **0x304** bits `1..4` | Slot arm bits (`1 << (slot + 1)`) — set by `tranfer_desclist()`, cleared by `irq_func()` |
| **0x10** bit `0x20` | A-DESC complete (audio chunk done) |
| **0x200..0x21c**, **0x2b4..0x2c0** | Audio DMA setup (`start_audio_streaming()`): chunk size, 2 buffer addresses, channel map |
| **0x8** bit `0x2` | Audio enable |
| **0x10a0** | Audio clock period: rate = 100000000 / value (`get_audioinfo()`) |
| **0x308 + n·0xc** | Descriptor **chain** bus addr (low) — not the frame buffer |
| **0x30c + n·0xc** | Descriptor chain bus addr (high) |
| **0x310 + n·0xc** | **Descriptor count** (SG fragments, e.g. `0x7`) — not byte size |
| **0x1040** | CSC + dual-pixel — **programmed by blob**, not by us at stream time |
| **0x1088** | Working mode (dual-pixel downscale) — **programmed by blob** |

Chain entry (16 bytes): `[0]`/`[1]` target addr, `[2]` size in dwords, `[3]` control `0x80006000`.

---

## Build & quick start

### Prerequisites

Kernel cmdline. On CachyOS 7.2 (`CONFIG_X86_KERNEL_IBT=y`) this is required, not optional. Without `ibt=off`, `insmod` dies in `init_module` with a control-protection fault (`kernel BUG at arch/x86/kernel/cet.c`).

```bash
ibt=off intel_iommu=on iommu=pt
```

- `ibt=off` — the module is built with `-fcf-protection=none` and the vendor blob has no ENDBR64. `MODULE_INFO(ibt, "N")` does not stop the kernel from requiring ENDBR on `init_module`.
- `intel_iommu=on iommu=pt` — on Intel, `iommu=pt` alone does not turn the IOMMU on. Passthrough mode is what the DMA path expects. AMD can use `iommu=pt` if the IOMMU is already enabled.

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
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/videoX
ffmpeg -f alsa -ch_layout stereo -sample_rate 48000 -i hw:CL511H -t 5 /tmp/audio.wav
```

Native 4K:

```bash
./build.sh LLVM=1 CC=clang GC573_EDID=vendor
sudo insmod cx511h.ko normalize_timing=0
sudo v4l2-ctl -d /dev/videoX --set-fmt-video=width=3840,height=2160,pixelformat=YUYV \
  --stream-mmap=4 --stream-count=60 --stream-to=/tmp/4k.yuyv
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
>
> `insmod.sh`, when the module is already loaded, now **delegates to `./unload.sh`** (safe
> release of capture/ALSA holders + clean rmmod) instead of a blind `rmmod -f`, so reloading
> after a rebuild works without a reboot in the normal case.
>
> The teardown page fault that used to wedge the module (`refcnt=-1 / initstate=going`) is fixed
> (Phase 5, BARs unmapped after the board contexts are released).


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
| `normalize_timing` | 1 | Normalize the unreliable ITE6805 timing to 1080p60 (`pixel_clock=148500000`, `fps=60`). Toggle at runtime via `/sys/module/cx511h/parameters/normalize_timing` (no rebuild). **Set 0 for native 4K** |
| `debug_pixel_format` | -1 | -1=auto; 0–3 force YUV byte order |
| `auto_test_byteorder` | 0 | Cycle formats on stream_on (MMIO peek) |
| `no_signal_pic` | NULL | Bitmap path when no signal |
| `copy_protection_pic` | NULL | Bitmap when content is HDCP-protected. **Insmod name today:** `copy_protetion_pic` — upstream typo in `board_config.c` (missing `c` in *protection*) |
| `led_pin_r/g` | 3/4 | GPIO LED pins (-1=off) |
| `led_pin_b` | -1 | Pin 5 is **not** a LED: driving it low drops the HDMI hot-plug |
| `legacy_doorbell` | 0 | 1 = also write `0x304=0x01` / ACK `0x10` from the board path (stalls capture) |
| `dma_debug` | 0 | 1 = log every DMA descriptor list programmed in `buffer_prepare` |
| `reg_read` | — | Write-only: `echo 0x300,0x304 > /sys/module/cx511h/parameters/reg_read` logs those FPGA registers |

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
| Build | `driver/patch_edid.py` | 1080p-max EDID with HDMI audio, written into the blob |
| Build | `driver/patch_redzone.py` | Makes the blob's leaf functions reserve their stack frame |

---

## Known issues (honest list)

1. **HDCP is broken** — encrypted sources (iPhone, MacBook, protected video) produce a garbled, split, or green picture. This is not a clean copy-protection mask.

2. **No I2C writes while streaming** — by design. Do not re-enable TTL/unmute/streaming I2C blocks without new safety analysis.

3. **4K metadata split** — `ITE6805_LOCK` forces **1920×1080** into framegrabber for caps; FPGA **`vip_cfg`** uses **physical** `fe_frameinfo` for scaler. Both are intentional. (With the default 1080p-max EDID the source negotiates 1080p; for native 4K use `normalize_timing=0`.)

4. **No reconfiguration on a new lock while streaming** — `stream_on` is ignored while streaming (Phase 5, fix 6), so a source changing resolution mid-capture needs the capture to be restarted. Discord does this and the audio capture stops; that is still open.

5. **GStreamer scripts** (`gst_1.0_raw_video*.sh`) — legacy / risky; use `v4l2-ctl` or `ffplay`.

6. **Module refcnt pinned** — audio holders (PipeWire / Discord) keep the CL511H PCM open and hold `refcnt` at 1, blocking `rmmod`/`reload`. `unload.sh` releases them; `insmod.sh` delegates to it on reload. If the module truly wedges (`refcnt −1`, `GOING` — driver teardown hang) only a reboot helps. The dangerous PCI-remove path stays removed.

7. **Audio is 16-bit stereo LPCM only** — the capture rate follows the source (32/44.1/48 kHz, measured by the FPGA) and is locked when the PCM is opened; no compressed audio passthrough.

8. **Legacy suspend/resume** — not migrated to `dev_pm_ops`.

9. **`vactive`/`hactive` naming** — vendor convention in `vip_cfg`: **`vactive` = horizontal width**, **`hactive` = vertical height** (not Linux/V4L2 semantics). In `stream_on`, `fe_frameinfo->width` → `vip_cfg.in_videoformat.vactive` and `fe_frameinfo->height` → `vip_cfg.in_videoformat.hactive`. Do not “fix” without checking bypass tables.

10. **1080p max by default** — the injected EDID advertises 1080p60 as the highest mode. Build with `GC573_EDID=vendor` for 4K; there is no hardware 4K → 1080p downscale, and the frame rate reported for 25 Hz sources can read 24 (integer measurement).

11. **Vendor blob patched at build time** — `patch_edid.py` (EDID tables) and `patch_redzone.py` (stack frames) run on every build; linking the unpatched blob brings back random interrupt corruption.

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
**Maintained by [Everlite](https://github.com/Everlite)** · Thanks to [Lou Perret](https://github.com/lou-perret) for the capture and audio work in [#7](https://github.com/Everlite/Avermedia-GC573-Linux/pull/7) and [#8](https://github.com/Everlite/Avermedia-GC573-Linux/pull/8), and to [derrod](https://github.com/derrod) for earlier work.
