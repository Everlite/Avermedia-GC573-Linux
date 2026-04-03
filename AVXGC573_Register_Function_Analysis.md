# AVXGC573 Register & Function Analysis
Generated from `linuxextract.c` on 2026-03-31.
This report is based on the Ghidra C export, so many original Windows macro names are gone. No literal matches were found for `WRITE_REGISTER_*`, `READ_REGISTER_*`, `WRITE_PORT_*`, `READ_PORT_*`, `outp`, `outpw`, `outpd`, `inp`, `inpw`, or `inpd`. The file instead uses two dominant access styles:
- I2C register helpers: `FUN_140044528` (direct write), `FUN_140044484` (direct read), `FUN_1400444d0` (read-modify-write), `thunk_FUN_1400444d0` (bank select via register `0x0F`).
- MMIO helper via vtable slot `+0x58`, for the actual capture / DMA / IRQ engine.

## 1. Register Write Summary
- Direct I2C write-like hits with constant register arguments: **722**
- Direct I2C read hits with constant register arguments: **250**
- Range hit counts: `0x00-0x2F=523`, `0x50-0x5F=42`, `0x80-0x9F=48`, `0xA0-0xBF=78`, `0xC0-0xDF=87`

| Register | Value | Function | Line | Context |
|----------|-------|----------|------|---------|
| MMIO 0x10 | 0x1FFF | `FUN_14001a0b8` | 22553 | global IRQ/status clear during stream-on init |
| MMIO 0x0C | 0x01 | `FUN_14001a2e0` | 22602 | stop/flush video path |
| MMIO 0x0C | 0x100 | `FUN_14001a2e0` | 22615 | stop/flush audio path |
| MMIO 0x0C | 0x200 | `FUN_14001a2e0` | 22626 | stop/flush mixer path |
| MMIO 0x10 | 0x02 | `FUN_140019d50` | 22447 | ACK video descriptor complete IRQ |
| MMIO 0x10 | 0x20 | `FUN_140019d50` | 22464 | ACK audio descriptor complete IRQ |
| MMIO 0x10 | 0x200 | `FUN_140019d50` | 22481 | ACK mixer descriptor complete IRQ |
| MMIO 0x304 | bit set | `FUN_140016a24` | 20540 | video descriptor ring doorbell plane-1 |
| MMIO 0x504 | bit set | `FUN_140016a24` | 20543 | video descriptor ring doorbell plane-2 |
| MMIO 0x604 | bit set | `FUN_140016a24` | 20545 | video descriptor ring doorbell plane-3 |
| 0x23 | 0xB0 -> 0xA0 | `FUN_140043810` | 64406 | known unmute / mode transition sequence, bank 0 |
| 0x2B | 0xB0 -> 0xA0 | `FUN_140043810` | 64424 | known unmute / mode transition sequence, bank 1 |
| 0x23 | 0x02 | `FUN_140043810` | 64413 | known TTL-mode related RMW in bank 0 |
| 0x2B | 0x02 | `FUN_140043810` | 64431 | known TTL-mode related RMW in bank 1 |
| 0x81 | mask 0x40 | `FUN_140044cf8` | 65411 | audio reset / output re-enable control |
| 0x8C | mask 0x10 / 0x08 | `FUN_1400462e4 / FUN_140046400` | 66284 | IRQ-side audio gating / reset toggles |

## 2. Interrupt Handler Functions
No symbol names containing `Interrupt`, `ISR`, `Dpc`, or `Irq` survived the Ghidra export, so the handlers below were identified by API usage (`IoConnectInterruptEx`, `KeInitializeDpc`, `KeInsertQueueDpc`), register ACK patterns, and DbgPrint strings.

### Function: `FUN_140036e30`
- Line: 55868-55879
- Purpose: Interrupt callback thunk registered through `IoConnectInterruptEx`; immediately dispatches into an indirect device-method ISR.
- Note: Interrupt callback thunk; calls an indirect vtable ISR at `+0x28`.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140019d50`
- Line: 22414-22504
- Purpose: Main capture ISR (strong inference). Reads the MMIO status register, queues DPCs for completed descriptors, ACKs IRQ bits, and handles fatal bus errors.
- Note: Reads MMIO status register `BAR+0x10`.
- Note: ACK writes `0x10=0x02/0x01/0x20/0x10/0x200/0x100`.
- Note: Queues DPCs for video (`0x59`), audio (`0x81`), mixer (`0x99`), fatal (`0xB3`).
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_1400179d0`
- Line: 21041-21121
- Purpose: Audio DPC producer-side: stages audio payload blocks into a software FIFO / cache list.
- Note: Audio producer-side DPC.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140017c30`
- Line: 21125-21177
- Purpose: Audio-complete DPC consumer-side: removes the staged audio buffer and hands it upstream via `FUN_14003479c`.
- Note: Audio consumer-side DPC.
- Note: Signals upstream via `FUN_14003479c`.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140017e70`
- Line: 21181-21261
- Purpose: Audio-mixer DPC producer-side: stages mixer payload blocks.
- Note: Audio-mixer producer-side DPC.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_1400180d0`
- Line: 21265-21317
- Purpose: Audio-mixer complete DPC consumer-side: removes staged mixer blocks and hands them upstream via `FUN_1400347c4`.
- Note: Audio-mixer consumer-side DPC.
- Note: Signals upstream via `FUN_1400347c4`.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140018310`
- Line: 21321-21328
- Purpose: Fatal / bus-error DPC that sets the error event.
- Note: Fatal / bus-error DPC.
- Note: Sets event `param_2 + 0x580`.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140018340`
- Line: 21334-21409
- Purpose: Video completion DPC. Completes a video SP buffer, re-arms the next descriptor, and notifies the upper layer.
- Note: Video completion DPC.
- Note: Calls `FUN_140016a24` to program the next descriptor.
- Note: Signals completion via `FUN_1400352d8`.
- Called by (direct textual): none found; path is likely indirect / vtable based.

### Function: `FUN_140046828`
- Line: 66482-66872
- Purpose: iTE6805 HDMI-RX common IRQ handler (inferred from its own DbgPrint strings).
- Note: Clears/ACKs HDMI audio IRQ state at `0x09/0x10/0x11/0x12/0x1D/0xD4/0xD5`.
- Note: Conditionally toggles `0x81`, `0x86`.
- Called by (direct textual): `FUN_140045d68`

### IRQ / DPC Call Graph
```text
IoConnectInterruptEx
  -> FUN_140036e30
       -> indirect vtable ISR at +0x28 (likely FUN_140019d50)   [inference]
            -> read MMIO status @ 0x10
            -> ACK MMIO status @ 0x10
            -> queue video DPC FUN_140018340
            -> queue audio DPC FUN_140017c30 / producer FUN_1400179d0
            -> queue mixer DPC FUN_1400180d0 / producer FUN_140017e70
            -> queue fatal DPC FUN_140018310
FUN_140018340
  -> FUN_140016a24
  -> FUN_1400352d8
FUN_140017c30
  -> FUN_14003479c
FUN_1400180d0
  -> FUN_1400347c4
```

## 3. DMA / Buffer Functions
Again, original symbol names are gone. The functions below were identified by descriptor memory layout, ring doorbell writes, queue usage, and DbgPrint text.

### Function: `FUN_140016a24`
- Line: 20455-20563
- Purpose: Programs video DMA descriptors into FPGA MMIO rings and rings the doorbell bits.
- DMA-Descriptor programming: Pushes the prepared descriptor entries from software memory into FPGA MMIO slots (`0x308/0x30C/0x310`, `0x508/...`, `0x608/...`) and rings the corresponding valid-bit registers (`0x304`, `0x504`, `0x604`).
- Buffer completion logic: After ringing the hardware, it moves the SP buffer to the in-flight list.
- Called by (direct textual): `FUN_1400173f4`, `FUN_140018340`, `FUN_14001c220`
- Note: Descriptor slots `0x308/0x30C/0x310`, `0x508/0x50C/0x510`, `0x608/0x60C/0x610`.
- Note: Doorbells `0x304/0x504/0x604`.

### Function: `FUN_1400173f4`
- Line: 20830-21037
- Purpose: Per-video-channel worker path used by the event thread (`FUN_14001c640`).
- Called by (direct textual): `FUN_14001c640`

### Function: `FUN_14001c220`
- Line: 23694-23721
- Purpose: Queues a prepared video SP buffer into the software list and immediately pushes the next descriptor.
- Buffer re-queue logic: Inserts the SP buffer into the software queue and immediately calls `FUN_140016a24` so hardware always has the next descriptor ready.
- Called by (direct textual): none found; path is likely indirect / worker-thread driven.

### Function: `FUN_14001cbd4`
- Line: 24077-24326
- Purpose: Builds descriptor memory from the segment list stored in the SP buffer object.
- DMA-Descriptor creation: Computes plane count and per-plane sizes from the active pixel format, then maps the segment list at `param_3 + 0x18` into descriptor entries stored in common buffers at `param_1 + 0x128`, `+0x1A8`, and `+0x228`. Each descriptor gets physical low/high address, word-count, and control flags `0x80008000`.
- Buffer completion logic: Descriptor metadata keeps a back-pointer to the originating SP buffer and the segment count.
- Called by (direct textual): `FUN_14001c220`
- Note: Computes plane layout from the active pixel format.
- Note: Writes descriptor entries with control flags `0x80008000`.

### Function: `FUN_14001a480`
- Line: 22675-22754
- Purpose: Per-channel audio start/stop path.
- Called by (direct textual): none found; path is likely indirect / worker-thread driven.
- Note: Stop branch uses `FUN_14001a2e0(..., 0x100)` and removes DPC `0x81`.

### Function: `FUN_14001a740`
- Line: 22760-22839
- Purpose: Per-channel mixer start/stop path.
- Called by (direct textual): none found; path is likely indirect / worker-thread driven.
- Note: Stop branch uses `FUN_14001a2e0(..., 0x200)` and removes DPC `0x99`.

### Function: `FUN_14001c310`
- Line: 23725-23742
- Purpose: Global stream-on entry.
- Called by (direct textual): none found; path is likely indirect / worker-thread driven.
- Note: Calls `FUN_14001a2e0(0x301)` and `FUN_14001a0b8()`.

### Function: `FUN_14001c4a0`
- Line: 23803-23817
- Purpose: Global stream-off entry.
- Called by (direct textual): none found; path is likely indirect / worker-thread driven.
- Note: Cancels timers and calls an indirect stop routine at vtable slot `+0x1E8`.

### Function: `FUN_14001a2e0`
- Line: 22592-22637
- Purpose: Flush / completion helper writing to MMIO `0x0C`.
- Called by (direct textual): `FUN_1400173f4`, `FUN_14001a480`, `FUN_14001a740`, `FUN_14001a9f0`, `FUN_14001c310`
- Note: Writes `0x0C = 0x01 / 0x100 / 0x200` and polls for completion.

### Function: `FUN_14001a0b8`
- Line: 22510-22570
- Purpose: Engine init helper that programs the MMIO cluster before streaming begins.
- Called by (direct textual): `FUN_14001c310`
- Note: Programs `0x40`, `0x120`, `0x124`, `0x128`, `0x180`, `0x10`, optional `0x1C`.

### DMA / Buffer Findings
- No literal `GetScatterGatherList` / `PutScatterGatherList` calls were found. The driver obtains a DMA adapter with `IoGetDmaAdapter`, allocates common buffers, and then builds its own descriptor arrays from a segment list already stored in the SP buffer object.
- Video descriptor completion is signaled by MMIO status bit `0x02` in `FUN_140019d50`, which queues `FUN_140018340`.
- Audio descriptor completion is signaled by MMIO status bit `0x20`, and mixer completion by `0x200`.
- The actual "buffer done" handoff to upper layers happens in the DPCs via `FUN_1400352d8`, `FUN_14003479c`, and `FUN_1400347c4`.

## 4. Stream On / Off Sequence
Direct textual callers for `FUN_14001c310` / `FUN_14001c4a0` were not recovered from the export. These are therefore treated as KS / vtable stream state handlers by behavior, not by surviving names.

### Stream On Register Sequence
1. `FUN_14001c310` line 23731: calls `FUN_14001a2e0(param_1, 0x301)`
2. `FUN_14001a2e0` line 22602: `0x0C = 0x01` then poll status
3. `FUN_14001a2e0` line 22615: `0x0C = 0x100` then poll status
4. `FUN_14001a2e0` line 22626: `0x0C = 0x200` then poll status
5. `FUN_14001c310` line 23732: calls `FUN_14001a0b8(param_1)`
6. `FUN_14001a0b8` line 22540: `0x40 = old | 0x08`
7. `FUN_14001a0b8` line 22546: `0x120 = 0x3D` or `0xF9`
8. `FUN_14001a0b8` line 22547: `0x124 = 0x00`
9. `FUN_14001a0b8` line 22548: `0x128 = 0x80`
10. `FUN_14001a0b8` line 22551: `0x180 = 0x1E848 / sample_rate`
11. `FUN_14001a0b8` line 22553: `0x10 = 0x1FFF`
12. `FUN_14001a0b8` line 22563: optional `0x1C = 0x1B33` / `0x0B33` depending device/firmware
13. `FUN_14001c310` line 23734: arms timers via `FUN_14001c440` and sets running flag

### Stream Off Register Sequence
1. `FUN_14001c4a0` line 23806: clear running flag
2. `FUN_14001c4a0` line 23810: optionally signal event `0xB0` for the I2C/worker side
3. `FUN_14001c4a0` line 23812: cancel timers via `FUN_14001c5b0`
4. `FUN_14001c4a0` line 23813: indirect hardware stop via vtable slot `+0x1E8`
5. `FUN_14001c4a0` line 23815: remove fatal-error DPC `0xB3`
6. `FUN_14001a480` line 22722: per-channel audio stop removes DPC `0x81` after `FUN_14001a2e0(..., 0x100)`
7. `FUN_14001a740` line 22807: per-channel mixer stop removes DPC `0x99` after `FUN_14001a2e0(..., 0x200)`

### Capture Path Reconstruction
```text
User / KS stream-state change
  -> indirect dispatch (textual caller not recovered)
  -> FUN_14001c310  [global stream on]
       -> FUN_14001a2e0(0x301)
       -> FUN_14001a0b8()
       -> FUN_14001c440()  [timers]
Hardware interrupt
  -> FUN_140036e30  [IoConnectInterruptEx thunk]
  -> indirect ISR (likely FUN_140019d50)   [inference]
       -> ACK @ MMIO 0x10
       -> KeInsertQueueDpc(video/audio/mixer/fatal)
Video complete
  -> FUN_140018340
       -> FUN_140016a24  [program next descriptor]
       -> FUN_1400352d8  [signal buffer completion]
Audio complete
  -> FUN_140017c30 / FUN_1400180d0
       -> FUN_14003479c / FUN_1400347c4
Stop
  -> FUN_14001c4a0  [global stream off]
       -> FUN_14001c5b0  [cancel timers]
       -> indirect stop routine at +0x1E8
```

## 5. Critical Findings for IRQ / DMA
- `MMIO 0x10` is the central IRQ-status / ACK register in the capture engine. `FUN_140019d50` writes `0x02`, `0x01`, `0x20`, `0x10`, `0x200`, and `0x100` back to this register to acknowledge different IRQ causes.
- `MMIO 0x0C` is the stop/flush completion register. `FUN_14001a2e0` writes `0x01`, `0x100`, and `0x200` and then polls until the corresponding status bit becomes visible.
- `FUN_140016a24` is the key video DMA doorbell path. The descriptor payload goes into `0x308/0x30C/0x310` (and optional `0x508/...`, `0x608/...`), then a bit is set in `0x304/0x504/0x604` to tell the FPGA new descriptors are ready.
- `FUN_14001cbd4` builds those descriptor lists from a segment list already stored in the SP buffer object. The driver does not expose a literal scatter/gather API call in this export, but the data flow is still scatter/gather in practice.
- Buffer completion is not a single flag in software. Instead, hardware raises an IRQ, `FUN_140019d50` queues a DPC, and the DPC performs the final handoff through `FUN_1400352d8`, `FUN_14003479c`, or `FUN_1400347c4`.
- On the HDMI-RX / iTE6805 side, `FUN_140046828` acts like a common IRQ handler and explicitly clears status registers `0x09`, `0x10`, `0x11`, `0x12`, `0x1D`, `0xD4`, and `0xD5`.

## 6. Specific Registers We Need
The table below covers all direct read / write hits in the requested I2C register ranges. Full 5-line context for every direct hit is in Appendix A / Appendix B.

### Range 0x80-0x9F - Interrupt Control (IRQ-Enable, Status, Mask)
| Op | Register | Value / Mask | Function | Line | Context |
|----|----------|--------------|----------|------|---------|
| read | 0x9E | - | `FUN_14003b090` | 59668 | bVar12 = FUN_140044484(param_1,0x9e); |
| read | 0x9D | - | `FUN_14003b090` | 59669 | bVar13 = FUN_140044484(param_1,0x9d); |
| read | 0x98 | - | `FUN_14003b090` | 59761 | FUN_140044484(param_1,0x98); |
| rmw | 0x9A | mask=0x80 val=0 | `FUN_14003d18c` | 60560 | FUN_1400444d0(param_1,0x9a,0x80,0); |
| read | 0x9A | - | `FUN_14003d18c` | 60561 | bVar1 = FUN_140044484(param_1,0x9a); |
| read | 0x99 | - | `FUN_14003d18c` | 60562 | bVar2 = FUN_140044484(param_1,0x99); |
| rmw | 0x9A | mask=0x80 val=0x80 | `FUN_14003d18c` | 60563 | FUN_1400444d0(param_1,0x9a,0x80,0x80); |
| read | 0x98 | - | `FUN_14003d388` | 60710 | bVar3 = FUN_140044484(param_1,0x98); |
| read | 0x9C | - | `FUN_14003d388` | 60711 | bVar4 = FUN_140044484(param_1,0x9c); |
| read | 0x9B | - | `FUN_14003d388` | 60712 | bVar5 = FUN_140044484(param_1,0x9b); |
| read | 0x9E | - | `FUN_14003d388` | 60714 | bVar4 = FUN_140044484(param_1,0x9e); |
| read | 0x9D | - | `FUN_14003d388` | 60715 | bVar5 = FUN_140044484(param_1,0x9d); |
| read | 0x9F | - | `FUN_14003d388` | 60721 | bVar5 = FUN_140044484(param_1,0x9f); |
| rmw | 0x91 | mask=0x3f val=(byte)(*(uint *)(param_1 + 0x98) / 1000) & 0x3f | `FUN_14003e090` | 61042 | FUN_1400444d0(param_1,0x91,0x3f,(byte)(*(uint *)(param_1 + 0x98) / 1000) & 0x3f); |
| write | 0x92 | uVar2 | `FUN_14003e090` | 61047 | FUN_140044528(param_1,0x92,uVar2); |
| read | 0x92 | - | `FUN_14003e090` | 61049 | uVar2 = FUN_140044484(param_1,0x92); |
| read | 0x91 | - | `FUN_14003e090` | 61050 | uVar3 = FUN_140044484(param_1,0x91); |
| read | 0x91 | - | `FUN_14003e090` | 61098 | uVar2 = FUN_140044484(param_1,0x91); |
| read | 0x92 | - | `FUN_14003e090` | 61102 | uVar2 = FUN_140044484(param_1,0x92); |
| read | 0x8A | - | `FUN_14003ea04` | 61305 | uVar1 = FUN_140044484(param_1,0x8a); |
| write | 0x8A | uVar1 | `FUN_14003ea04` | 61306 | FUN_140044528(param_1,0x8a,uVar1); |
| write | 0x8A | uVar1 | `FUN_14003ea04` | 61307 | FUN_140044528(param_1,0x8a,uVar1); |
| write | 0x8A | uVar1 | `FUN_14003ea04` | 61308 | FUN_140044528(param_1,0x8a,uVar1); |
| write | 0x8A | uVar1 | `FUN_14003ea04` | 61309 | FUN_140044528(param_1,0x8a,uVar1); |
| read | 0x98 | - | `FUN_14003ee7c` | 61481 | bVar2 = FUN_140044484(param_1,0x98); |
| read | 0x98 | - | `FUN_14003ef68` | 61549 | FUN_140044484(param_1,0x98); |
| read | 0x98 | - | `FUN_14003f774` | 61818 | bVar1 = FUN_140044484(param_1,0x98); |
| read | 0x9E | - | `FUN_1400445c0` | 65075 | bVar1 = FUN_140044484(param_1,0x9e); |
| read | 0x9D | - | `FUN_1400445c0` | 65076 | bVar2 = FUN_140044484(param_1,0x9d); |
| rmw | 0x86 | mask=1 val=1 | `FUN_140044cf8` | 65267 | FUN_1400444d0(param_1,0x86,1,1); |
| read | 0x81 | - | `FUN_140044cf8` | 65410 | bVar3 = FUN_140044484(param_1,0x81); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_140044cf8` | 65411 | FUN_1400444d0(param_1,0x81,0x40,0); |
| rmw | 0x81 | mask=0x40 val=0x40 | `FUN_140044cf8` | 65425 | FUN_1400444d0(param_1,0x81,0x40,0x40); |
| rmw | 0x8A | mask=0x3f val=bVar5 | `FUN_140044cf8` | 65426 | FUN_1400444d0(param_1,0x8a,0x3f,bVar5); |
| write | 0x86 | 0 | `FUN_140045168` | 65496 | FUN_140044528(param_1,0x86,0); |
| write | 0x85 | 0 | `FUN_140045168` | 65578 | FUN_140044528(param_1,0x85,0); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_140045a28` | 65880 | FUN_1400444d0(param_1,0x81,0x40,0); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_1400462e4` | 66271 | FUN_1400444d0(param_1,0x81,0x40,0); |
| rmw | 0x8C | mask=0x10 val=0x10 | `FUN_1400462e4` | 66284 | FUN_1400444d0(param_1,0x8c,0x10,0x10); |
| rmw | 0x8C | mask=0x10 val=0 | `FUN_1400462e4` | 66285 | FUN_1400444d0(param_1,0x8c,0x10,0); |
| rmw | 0x8C | mask=8 val=0 | `FUN_140046400` | 66400 | FUN_1400444d0(param_1,0x8c,8,0); |
| rmw | 0x8C | mask=8 val=8 | `FUN_140046400` | 66408 | FUN_1400444d0(param_1,0x8c,8,8); |
| rmw | 0x86 | mask=1 val=1 | `FUN_140046400` | 66418 | FUN_1400444d0(param_1,0x86,1,1); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_140046400` | 66464 | FUN_1400444d0(param_1,0x81,0x40,0); |
| rmw | 0x86 | mask=1 val=1 | `FUN_140046828` | 66562 | FUN_1400444d0(param_1,0x86,1,1); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_140046828` | 66600 | FUN_1400444d0(param_1,0x81,0x40,0); |
| write | 0x90 | 0x8f | `FUN_1400480e0` | 67486 | FUN_140044528(param_1,0x90,0x8f); |
| rmw | 0x81 | mask=0x40 val=0 | `FUN_1400480e0` | 67510 | FUN_1400444d0(param_1,0x81,0x40,0); |

### Range 0xA0-0xBF - DMA Control (Buffer-Enable, Complete)
| Op | Register | Value / Mask | Function | Line | Context |
|----|----------|--------------|----------|------|---------|
| read | 0xA5 | - | `FUN_14003b090` | 59677 | bVar14 = FUN_140044484(param_1,0xa5); |
| read | 0xA4 | - | `FUN_14003b090` | 59678 | bVar15 = FUN_140044484(param_1,0xa4); |
| read | 0xB0 | - | `FUN_14003b090` | 59803 | bVar12 = FUN_140044484(param_1,0xb0); |
| read | 0xB1 | - | `FUN_14003b090` | 59805 | bVar15 = FUN_140044484(param_1,0xb1); |
| read | 0xB1 | - | `FUN_14003ccac` | 60273 | bVar2 = FUN_140044484(param_1,0xb1); |
| read | 0xAA | - | `FUN_14003ccf4` | 60286 | bVar1 = FUN_140044484(param_1,0xaa); |
| read | 0xAA | - | `FUN_14003ccf4` | 60290 | bVar1 = FUN_140044484(param_1,0xaa); |
| read | 0xA1 | - | `FUN_14003d388` | 60717 | bVar4 = FUN_140044484(param_1,0xa1); |
| read | 0xA0 | - | `FUN_14003d388` | 60718 | bVar5 = FUN_140044484(param_1,0xa0); |
| read | 0xA1 | - | `FUN_14003d388` | 60720 | bVar4 = FUN_140044484(param_1,0xa1); |
| read | 0xA3 | - | `FUN_14003d388` | 60723 | bVar4 = FUN_140044484(param_1,0xa3); |
| read | 0xA2 | - | `FUN_14003d388` | 60724 | bVar5 = FUN_140044484(param_1,0xa2); |
| read | 0xA5 | - | `FUN_14003d388` | 60726 | bVar4 = FUN_140044484(param_1,0xa5); |
| read | 0xA4 | - | `FUN_14003d388` | 60727 | bVar5 = FUN_140044484(param_1,0xa4); |
| read | 0xA8 | - | `FUN_14003d388` | 60729 | bVar4 = FUN_140044484(param_1,0xa8); |
| read | 0xA7 | - | `FUN_14003d388` | 60730 | bVar5 = FUN_140044484(param_1,0xa7); |
| read | 0xA8 | - | `FUN_14003d388` | 60732 | bVar4 = FUN_140044484(param_1,0xa8); |
| read | 0xA6 | - | `FUN_14003d388` | 60733 | bVar5 = FUN_140044484(param_1,0xa6); |
| read | 0xAA | - | `FUN_14003d388` | 60738 | uVar6 = FUN_140044484(param_1,0xaa); |
| read | 0xAA | - | `FUN_14003d388` | 60742 | bVar4 = FUN_140044484(param_1,0xaa); |
| read | 0xAA | - | `FUN_14003d388` | 60743 | bVar5 = FUN_140044484(param_1,0xaa); |
| rmw | 0xA0 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60790 | FUN_1400444d0(param_1,0xa0,0x80,0x80); |
| rmw | 0xA1 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60791 | FUN_1400444d0(param_1,0xa1,0x80,0x80); |
| rmw | 0xA2 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60792 | FUN_1400444d0(param_1,0xa2,0x80,0x80); |
| rmw | 0xA4 | mask=8 val=8 | `FUN_14003d7fc` | 60793 | FUN_1400444d0(param_1,0xa4,8,8); |
| rmw | 0xA7 | mask=0x10 val=0x10 | `FUN_14003d7fc` | 60795 | FUN_1400444d0(param_1,0xa7,0x10,0x10); |
| rmw | 0xA0 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60810 | FUN_1400444d0(param_1,0xa0,0x80,0x80); |
| rmw | 0xA1 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60811 | FUN_1400444d0(param_1,0xa1,0x80,0x80); |
| rmw | 0xA2 | mask=0x80 val=0x80 | `FUN_14003d7fc` | 60812 | FUN_1400444d0(param_1,0xa2,0x80,0x80); |
| rmw | 0xA4 | mask=8 val=8 | `FUN_14003d7fc` | 60813 | FUN_1400444d0(param_1,0xa4,8,8); |
| rmw | 0xA7 | mask=0x10 val=0x10 | `FUN_14003d7fc` | 60815 | FUN_1400444d0(param_1,0xa7,0x10,0x10); |
| rmw | 0xA0 | mask=0x80 val=0 | `FUN_14003d7fc` | 60925 | FUN_1400444d0(param_1,0xa0,0x80,0); |
| rmw | 0xA1 | mask=0x80 val=0 | `FUN_14003d7fc` | 60926 | FUN_1400444d0(param_1,0xa1,0x80,0); |
| rmw | 0xA2 | mask=0x80 val=0 | `FUN_14003d7fc` | 60927 | FUN_1400444d0(param_1,0xa2,0x80,0); |
| rmw | 0xA0 | mask=0x80 val=0 | `FUN_14003d7fc` | 60930 | FUN_1400444d0(param_1,0xa0,0x80,0); |
| rmw | 0xA1 | mask=0x80 val=0 | `FUN_14003d7fc` | 60931 | FUN_1400444d0(param_1,0xa1,0x80,0); |
| rmw | 0xA2 | mask=0x80 val=0 | `FUN_14003d7fc` | 60932 | FUN_1400444d0(param_1,0xa2,0x80,0); |
| rmw | 0xAA | mask=0x1f val=0xc | `FUN_14003e090` | 61032 | FUN_1400444d0(param_1,0xaa,0x1f,0xc); |
| rmw | 0xB0 | mask=1 val=0 | `FUN_14003eb0c` | 61360 | FUN_1400444d0(param_1,0xb0,1,0); |
| rmw | 0xBD | mask=0x30 val=bVar1 | `FUN_14003f59c` | 61729 | FUN_1400444d0(param_1,0xbd,0x30,bVar1); |
| write | 0xBE | 0 | `FUN_14003f59c` | 61731 | FUN_140044528(param_1,0xbe,0); |
| read | 0xB0 | - | `FUN_14003f8e0` | 61901 | bVar1 = FUN_140044484(param_1,0xb0); |
| read | 0xB0 | - | `FUN_14003f8e0` | 61902 | bVar2 = FUN_140044484(param_1,0xb0); |
| read | 0xB0 | - | `FUN_14003f8e0` | 61903 | bVar3 = FUN_140044484(param_1,0xb0); |
| read | 0xB0 | - | `FUN_14003f8e0` | 61904 | cVar4 = FUN_140044484(param_1,0xb0); |
| read | 0xB1 | - | `FUN_14003f8e0` | 61905 | bVar5 = FUN_140044484(param_1,0xb1); |
| read | 0xB5 | - | `FUN_14003f8e0` | 61907 | bVar6 = FUN_140044484(param_1,0xb5); |
| read | 0xB5 | - | `FUN_14003f8e0` | 61908 | bVar7 = FUN_140044484(param_1,0xb5); |
| write | 0xB9 | 0xff | `FUN_14004165c` | 63030 | FUN_140044528(param_1,0xb9,0xff); |
| read | 0xB9 | - | `FUN_14004165c` | 63031 | cVar4 = FUN_140044484(param_1,0xb9); |
| write | 0xBE | 0xff | `FUN_14004165c` | 63032 | FUN_140044528(param_1,0xbe,0xff); |
| read | 0xBE | - | `FUN_14004165c` | 63033 | cVar5 = FUN_140044484(param_1,0xbe); |
| write | 0xB9 | 0xff | `FUN_14004165c` | 63039 | FUN_140044528(param_1,0xb9,0xff); |
| read | 0xB9 | - | `FUN_14004165c` | 63040 | cVar4 = FUN_140044484(param_1,0xb9); |
| write | 0xBE | 0xff | `FUN_14004165c` | 63042 | FUN_140044528(param_1,0xbe,0xff); |
| read | 0xBE | - | `FUN_14004165c` | 63043 | cVar5 = FUN_140044484(param_1,0xbe); |
| rmw | 0xA7 | mask=0x40 val=bVar16 | `FUN_140041b38` | 63634 | FUN_1400444d0(param_1,0xa7,0x40,bVar16); |
| read | 0xA5 | - | `FUN_1400445c0` | 65078 | bVar1 = FUN_140044484(param_1,0xa5); |
| read | 0xA4 | - | `FUN_1400445c0` | 65079 | bVar2 = FUN_140044484(param_1,0xa4); |
| read | 0xBE | - | `FUN_140044cf8` | 65269 | bVar3 = FUN_140044484(param_1,0xbe); |
| read | 0xBF | - | `FUN_140044cf8` | 65270 | bVar4 = FUN_140044484(param_1,0xbf); |
| read | 0xB5 | - | `FUN_140044cf8` | 65353 | bVar3 = FUN_140044484(param_1,0xb5); |
| read | 0xB6 | - | `FUN_140044cf8` | 65354 | bVar4 = FUN_140044484(param_1,0xb6); |
| rmw | 0xA8 | mask=8 val=8 | `FUN_140045eec` | 66154 | FUN_1400444d0(param_1,0xa8,8,8); |
| rmw | 0xA8 | mask=8 val=0 | `FUN_140045eec` | 66158 | FUN_1400444d0(param_1,0xa8,8,0); |
| rmw | 0xA8 | mask=8 val=0 | `FUN_140045eec` | 66180 | FUN_1400444d0(param_1,0xa8,8,0); |
| rmw | 0xA8 | mask=8 val=8 | `FUN_140045eec` | 66184 | FUN_1400444d0(param_1,0xa8,8,8); |
| read | 0xB9 | - | `FUN_140046400` | 66355 | bVar2 = FUN_140044484(param_1,0xb9); |
| read | 0xB0 | - | `FUN_140046400` | 66392 | bVar2 = FUN_140044484(param_1,0xb0); |
| read | 0xB1 | - | `FUN_140046400` | 66394 | uVar3 = FUN_140044484(param_1,0xb1); |
| read | 0xB2 | - | `FUN_140046400` | 66396 | bVar2 = FUN_140044484(param_1,0xb2); |
| read | 0xBE | - | `FUN_140046400` | 66420 | bVar4 = FUN_140044484(param_1,0xbe); |
| read | 0xBF | - | `FUN_140046400` | 66421 | bVar5 = FUN_140044484(param_1,0xbf); |
| read | 0xB0 | - | `FUN_140046400` | 66437 | bVar4 = FUN_140044484(param_1,0xb0); |
| read | 0xB1 | - | `FUN_140046400` | 66438 | cVar7 = FUN_140044484(param_1,0xb1); |
| read | 0xB2 | - | `FUN_140046400` | 66439 | bVar5 = FUN_140044484(param_1,0xb2); |
| read | 0xBE | - | `FUN_140046828` | 66564 | bVar2 = FUN_140044484(param_1,0xbe); |
| read | 0xBF | - | `FUN_140046828` | 66565 | bVar9 = FUN_140044484(param_1,0xbf); |

### Range 0xC0-0xDF - TTL/Output (Schon teilweise gefunden)
| Op | Register | Value / Mask | Function | Line | Context |
|----|----------|--------------|----------|------|---------|
| read | 0xC0 | - | `FUN_14003b090` | 59787 | bVar13 = FUN_140044484(param_1,0xc0); |
| read | 0xC1 | - | `FUN_14003b090` | 59788 | FUN_140044484(param_1,0xc1); |
| write | 0xC4 | 0x10 | `FUN_14003cb50` | 60214 | FUN_140044528(param_1,0xc4,0x10); |
| write | 0xC4 | 0 | `FUN_14003cb50` | 60244 | FUN_140044528(param_1,0xc4,0); |
| read | 0xC0 | - | `FUN_14003cec4` | 60411 | bVar1 = FUN_140044484(param_1,0xc0); |
| rmw | 0xCF | mask=1 val=1 | `FUN_14003e4b8` | 61166 | FUN_1400444d0(param_1,0xcf,1,1); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_14003e83c` | 61258 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_14003e83c` | 61262 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_14003e83c` | 61281 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_14003e83c` | 61283 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x80 val=0x80 | `FUN_14003ecf4` | 61426 | FUN_1400444d0(param_1,0xc5,0x80,0x80); |
| rmw | 0xC5 | mask=1 val=1 | `FUN_14003ecf4` | 61440 | FUN_1400444d0(param_1,0xc5,1,1); |
| rmw | 0xC5 | mask=1 val=0 | `FUN_14003ecf4` | 61441 | FUN_1400444d0(param_1,0xc5,1,0); |
| rmw | 0xC5 | mask=1 val=1 | `FUN_14003ecf4` | 61445 | FUN_1400444d0(param_1,0xc5,1,1); |
| write | 0xC7 | -(param_2 != '\0') & 0x7f | `FUN_14003ee30` | 61467 | FUN_140044528(param_1,199,-(param_2 != '\0') & 0x7f); |
| rmw | 0xCB | mask=4 val=4 | `FUN_14003f424` | 61658 | FUN_1400444d0(param_1,0xcb,4,4); |
| rmw | 0xCB | mask=4 val=0 | `FUN_14003f424` | 61665 | FUN_1400444d0(param_1,0xcb,4,0); |
| rmw | 0xCB | mask=4 val=bVar1 | `FUN_14003f424` | 61669 | FUN_1400444d0(param_1,0xcb,4,bVar1); |
| rmw | 0xC0 | mask=1 val=1 | `FUN_14003f59c` | 61711 | FUN_1400444d0(param_1,0xc0,1,1); |
| rmw | 0xD1 | mask=1 val=0 | `FUN_14003f59c` | 61714 | FUN_1400444d0(param_1,0xd1,1,0); |
| rmw | 0xD1 | mask=0xc val=4 | `FUN_14003f59c` | 61715 | FUN_1400444d0(param_1,0xd1,0xc,4); |
| rmw | 0xDA | mask=0x10 val=0 | `FUN_14003f59c` | 61716 | FUN_1400444d0(param_1,0xda,0x10,0); |
| write | 0xD0 | 0xf3 | `FUN_14003f59c` | 61717 | FUN_140044528(param_1,0xd0,0xf3); |
| rmw | 0xC0 | mask=6 val=2 | `FUN_14003f690` | 61747 | FUN_1400444d0(param_1,0xc0,6,2); |
| rmw | 0xC1 | mask=bVar2 val=bVar3 | `FUN_14003f690` | 61753 | FUN_1400444d0(param_1,0xc1,bVar2,bVar3); |
| rmw | 0xC1 | mask=0x20 val=bVar3 | `FUN_14003f690` | 61784 | FUN_1400444d0(param_1,0xc1,0x20,bVar3); |
| rmw | 0xC5 | mask=0x80 val=0 | `FUN_14003f774` | 61822 | FUN_1400444d0(param_1,0xc5,0x80,0); |
| rmw | 0xC6 | mask=0x80 val=0 | `FUN_14003f774` | 61823 | FUN_1400444d0(param_1,0xc6,0x80,0); |
| read | 0xC0 | - | `FUN_14003f774` | 61839 | bVar1 = FUN_140044484(param_1,0xc0); |
| write | 0xC5 | 0xff | `FUN_14003f774` | 61876 | FUN_140044528(param_1,0xc5,0xff); |
| write | 0xC6 | 0xff | `FUN_14003f774` | 61877 | FUN_140044528(param_1,0xc6,0xff); |
| rmw | 0xC5 | mask=1 val=0 | `FUN_140040610` | 62541 | FUN_1400444d0(param_1,0xc5,1,0); |
| rmw | 0xC5 | mask=1 val=0 | `FUN_140040610` | 62545 | FUN_1400444d0(param_1,0xc5,1,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_140040610` | 62547 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_140040610` | 62551 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_140040610` | 62553 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_140040610` | 62557 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| write | 0xC9 | bVar1 | `FUN_140040960` | 62708 | FUN_140044528(param_1,0xc9,bVar1); |
| write | 0xC9 | bVar1 | `FUN_140040960` | 62710 | FUN_140044528(param_1,0xc9,bVar1); |
| write | 0xC6 | (char)uVar3 | `FUN_140040960` | 62722 | FUN_140044528(param_1,0xc6,(char)uVar3); |
| write | 0xC7 | *(undefined1 *)(param_1 + 0xab) | `FUN_140040960` | 62723 | FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab)); |
| write | 0xC8 | *(undefined1 *)(param_1 + 0xac) | `FUN_140040960` | 62724 | FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac)); |
| write | 0xCA | bVar1 | `FUN_140040960` | 62726 | FUN_140044528(param_1,0xca,bVar1); |
| write | 0xC7 | *(undefined1 *)(param_1 + 0xad) | `FUN_140040960` | 62728 | FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad)); |
| write | 0xC8 | *(undefined1 *)(param_1 + 0xae) | `FUN_140040960` | 62729 | FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae)); |
| write | 0xCA | bVar4 | `FUN_140040960` | 62731 | FUN_140044528(param_1,0xca,bVar4); |
| read | 0xD5 | - | `FUN_1400428d8` | 63823 | cVar2 = FUN_140044484(param_1,0xd5); |
| read | 0xD6 | - | `FUN_1400428d8` | 63832 | cVar2 = FUN_140044484(param_1,0xd6); |
| read | 0xD7 | - | `FUN_1400428d8` | 63840 | cVar2 = FUN_140044484(param_1,0xd7); |
| read | 0xD5 | - | `FUN_140042e78` | 64025 | bVar1 = FUN_140044484(param_1,0xd5); |
| read | 0xD6 | - | `FUN_140042e78` | 64027 | bVar1 = FUN_140044484(param_1,0xd6); |
| read | 0xD7 | - | `FUN_140042e78` | 64029 | bVar1 = FUN_140044484(param_1,0xd7); |
| rmw | 0xC0 | mask=1 val=0 | `FUN_1400445c0` | 64985 | FUN_1400444d0(param_1,0xc0,1,0); |
| write | 0xC4 | bVar1 | `FUN_1400445c0` | 65052 | FUN_140044528(param_1,0xc4,bVar1); |
| read | 0xC0 | - | `FUN_140044cf8` | 65271 | bVar5 = FUN_140044484(param_1,0xc0); |
| read | 0xC0 | - | `FUN_140044cf8` | 65273 | bVar3 = FUN_140044484(param_1,0xc0); |
| read | 0xC1 | - | `FUN_140044cf8` | 65275 | bVar4 = FUN_140044484(param_1,0xc1); |
| read | 0xC2 | - | `FUN_140044cf8` | 65278 | bVar3 = FUN_140044484(param_1,0xc2); |
| read | 0xDA | - | `FUN_1400457d0` | 65757 | uVar1 = FUN_140044484(param_1,0xda); |
| read | 0xDB | - | `FUN_1400457d0` | 65759 | uVar1 = FUN_140044484(param_1,0xdb); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_140045eec` | 66160 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_140045eec` | 66164 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_140045eec` | 66187 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_140045eec` | 66191 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_1400461f4` | 66229 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_1400461f4` | 66232 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_1400461f4` | 66234 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_1400461f4` | 66237 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| read | 0xC0 | - | `FUN_140046400` | 66422 | bVar6 = FUN_140044484(param_1,0xc0); |
| read | 0xC0 | - | `FUN_140046400` | 66424 | bVar4 = FUN_140044484(param_1,0xc0); |
| read | 0xC1 | - | `FUN_140046400` | 66425 | bVar5 = FUN_140044484(param_1,0xc1); |
| read | 0xC2 | - | `FUN_140046400` | 66426 | bVar6 = FUN_140044484(param_1,0xc2); |
| read | 0xD4 | - | `FUN_140046828` | 66526 | bVar7 = FUN_140044484(param_1,0xd4); |
| write | 0xD4 | bVar7 | `FUN_140046828` | 66527 | FUN_140044528(param_1,0xd4,bVar7); |
| read | 0xD5 | - | `FUN_140046828` | 66528 | bVar8 = FUN_140044484(param_1,0xd5); |
| write | 0xD5 | bVar8 | `FUN_140046828` | 66531 | FUN_140044528(param_1,0xd5,bVar8); |
| read | 0xCF | - | `FUN_140046828` | 66534 | bVar2 = FUN_140044484(param_1,0xcf); |
| read | 0xD6 | - | `FUN_140046828` | 66548 | bVar2 = FUN_140044484(param_1,0xd6); |
| read | 0xC0 | - | `FUN_140046828` | 66566 | bVar10 = FUN_140044484(param_1,0xc0); |
| read | 0xC0 | - | `FUN_140046828` | 66568 | bVar2 = FUN_140044484(param_1,0xc0); |
| read | 0xC1 | - | `FUN_140046828` | 66569 | bVar9 = FUN_140044484(param_1,0xc1); |
| read | 0xC2 | - | `FUN_140046828` | 66570 | bVar10 = FUN_140044484(param_1,0xc2); |
| read | 0xD6 | - | `FUN_140046828` | 66809 | bVar3 = FUN_140044484(param_1,0xd6); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_1400480e0` | 67417 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_1400480e0` | 67420 | FUN_1400444d0(param_1,0xc5,0x10,0); |
| rmw | 0xC5 | mask=0x10 val=0x10 | `FUN_1400480e0` | 67422 | FUN_1400444d0(param_1,0xc5,0x10,0x10); |
| rmw | 0xC5 | mask=0x10 val=0 | `FUN_1400480e0` | 67425 | FUN_1400444d0(param_1,0xc5,0x10,0); |

### Range 0x00-0x2F - HDMI-RX (HDMI-Lock, HPD)
| Op | Register | Value / Mask | Function | Line | Context |
|----|----------|--------------|----------|------|---------|
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400396f4` | 57984 | thunk_FUN_1400444d0(param_1,(byte)uVar2,(byte)puVar3,(byte)puVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400396f4` | 57997 | thunk_FUN_1400444d0(param_1,0,(byte)puVar3,(byte)puVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003a83c` | 58959 | thunk_FUN_1400444d0(param_1,DAT_1400a000d,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003a83c` | 58967 | thunk_FUN_1400444d0(param_1,DAT_1400a000d,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003a83c` | 58973 | uVar1 = thunk_FUN_1400444d0(param_1,0,bVar2,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003b090` | 59667 | thunk_FUN_1400444d0(param_1,0,bVar15,bVar14); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003b090` | 59676 | thunk_FUN_1400444d0(param_1,0,bVar15,bVar14); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003b090` | 59786 | thunk_FUN_1400444d0(param_1,1,bVar15,bVar12); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14003b090` | 59801 | thunk_FUN_1400444d0(param_1,0,bVar15,bVar12); |
| rmw | 0x0F | mask=7 val=param_2 & 7 | `thunk_FUN_1400444d0` | 60183 | FUN_1400444d0(param_1,0xf,7,param_2 & 7); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003cb50` | 60213 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003cb50` | 60243 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ccac` | 60271 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x10 | - | `FUN_14003ccac` | 60272 | bVar1 = FUN_140044484(param_1,0x10); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ccf4` | 60285 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003ccf4` | 60289 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ccf4` | 60291 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003cd5c` | 60303 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003cd90` | 60319 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003cdc4` | 60335 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ce08` | 60359 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x13 | - | `FUN_14003ce08` | 60360 | if ((param_2 == '\0') && (bVar1 = FUN_140044484(param_1,0x13), (bVar1 & 2) != 0)) { |
| read | 0x16 | - | `FUN_14003ce08` | 60364 | bVar1 = FUN_140044484(param_1,0x16); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ce60` | 60380 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x13 | - | `FUN_14003ce60` | 60382 | bVar1 = FUN_140044484(param_1,0x13); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ce9c` | 60398 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x19 | - | `FUN_14003ce9c` | 60399 | bVar1 = FUN_140044484(param_1,0x19); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003cec4` | 60410 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003cec4` | 60412 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d010` | 60472 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=2 | `FUN_14003d0a0` | 60520 | FUN_1400444d0(param_1,0xf,7,2); |
| read | 0x15 | - | `FUN_14003d0a0` | 60521 | bVar1 = FUN_140044484(param_1,0x15); |
| read | 0x16 | - | `FUN_14003d0a0` | 60523 | bVar1 = FUN_140044484(param_1,0x16); |
| read | 0x17 | - | `FUN_14003d0a0` | 60525 | bVar1 = FUN_140044484(param_1,0x17); |
| read | 0x17 | - | `FUN_14003d0a0` | 60527 | bVar1 = FUN_140044484(param_1,0x17); |
| read | 0x18 | - | `FUN_14003d0a0` | 60529 | bVar1 = FUN_140044484(param_1,0x18); |
| read | 0x19 | - | `FUN_14003d0a0` | 60531 | bVar1 = FUN_140044484(param_1,0x19); |
| read | 0x15 | - | `FUN_14003d0a0` | 60533 | bVar1 = FUN_140044484(param_1,0x15); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d0a0` | 60535 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d18c` | 60555 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=-(*(char *)(param_1 + 0x1ec) != '\0') & 4 | `FUN_14003d24c` | 60587 | FUN_1400444d0(param_1,0xf,7,-(*(char *)(param_1 + 0x1ec) != '\0') & 4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d24c` | 60600 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=-(*(char *)(param_1 + 0x1ec) != '\0') & 4 | `FUN_14003d388` | 60650 | FUN_1400444d0(param_1,0xf,7,-(*(char *)(param_1 + 0x1ec) != '\0') & 4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d388` | 60668 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003d388` | 60736 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d388` | 60744 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003d7fc` | 60788 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60797 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x29 | mask=1 val=1 | `FUN_14003d7fc` | 60798 | FUN_1400444d0(param_1,0x29,1,1); |
| rmw | 0x2A | mask=0x41 val=0x41 | `FUN_14003d7fc` | 60799 | FUN_1400444d0(param_1,0x2a,0x41,0x41); |
| rmw | 0x2A | mask=0x40 val=0 | `FUN_14003d7fc` | 60801 | FUN_1400444d0(param_1,0x2a,0x40,0); |
| rmw | 0x24 | mask=4 val=4 | `FUN_14003d7fc` | 60802 | FUN_1400444d0(param_1,0x24,4,4); |
| write | 0x25 | 0 | `FUN_14003d7fc` | 60803 | FUN_140044528(param_1,0x25,0); |
| write | 0x26 | 0 | `FUN_14003d7fc` | 60804 | FUN_140044528(param_1,0x26,0); |
| write | 0x27 | 0 | `FUN_14003d7fc` | 60805 | FUN_140044528(param_1,0x27,0); |
| write | 0x28 | 0 | `FUN_14003d7fc` | 60806 | FUN_140044528(param_1,0x28,0); |
| rmw | 0x0F | mask=7 val=7 | `FUN_14003d7fc` | 60808 | FUN_1400444d0(param_1,0xf,7,7); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60817 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x2C | mask=4 val=4 | `FUN_14003d7fc` | 60821 | FUN_1400444d0(param_1,0x2c,4,4); |
| write | 0x2D | 0 | `FUN_14003d7fc` | 60822 | FUN_140044528(param_1,0x2d,0); |
| write | 0x2E | 0 | `FUN_14003d7fc` | 60823 | FUN_140044528(param_1,0x2e,0); |
| write | 0x2F | 0 | `FUN_14003d7fc` | 60824 | FUN_140044528(param_1,0x2f,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003d7fc` | 60826 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003d7fc` | 60828 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=7 | `FUN_14003d7fc` | 60830 | FUN_1400444d0(param_1,0xf,7,7); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60832 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x08 | - | `FUN_14003d7fc` | 60833 | bVar1 = FUN_140044484(param_1,8); |
| read | 0x0D | - | `FUN_14003d7fc` | 60835 | bVar2 = FUN_140044484(param_1,0xd); |
| read | 0x08 | - | `FUN_14003d7fc` | 60841 | bVar1 = FUN_140044484(param_1,8); |
| read | 0x0D | - | `FUN_14003d7fc` | 60843 | bVar2 = FUN_140044484(param_1,0xd); |
| read | 0x0D | - | `FUN_14003d7fc` | 60849 | uVar3 = FUN_140044484(param_1,0xd); |
| read | 0x08 | - | `FUN_14003d7fc` | 60850 | uVar4 = FUN_140044484(param_1,8); |
| rmw | 0x2A | mask=0x40 val=0x40 | `FUN_14003d7fc` | 60861 | FUN_1400444d0(param_1,0x2a,0x40,0x40); |
| rmw | 0x2A | mask=0x40 val=0 | `FUN_14003d7fc` | 60863 | FUN_1400444d0(param_1,0x2a,0x40,0); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003d7fc` | 60881 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60883 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x2A | mask=0x40 val=0x40 | `FUN_14003d7fc` | 60884 | FUN_1400444d0(param_1,0x2a,0x40,0x40); |
| rmw | 0x2A | mask=0x40 val=0 | `FUN_14003d7fc` | 60886 | FUN_1400444d0(param_1,0x2a,0x40,0); |
| rmw | 0x0F | mask=7 val=7 | `FUN_14003d7fc` | 60889 | FUN_1400444d0(param_1,0xf,7,7); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60891 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003d7fc` | 60897 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=7 | `FUN_14003d7fc` | 60901 | FUN_1400444d0(param_1,0xf,7,7); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60914 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x08 | mask=0x30 val=0x30 | `FUN_14003d7fc` | 60915 | FUN_1400444d0(param_1,8,0x30,0x30); |
| rmw | 0x0D | mask=0x30 val=0x30 | `FUN_14003d7fc` | 60916 | FUN_1400444d0(param_1,0xd,0x30,0x30); |
| rmw | 0x29 | mask=1 val=0 | `FUN_14003d7fc` | 60917 | FUN_1400444d0(param_1,0x29,1,0); |
| rmw | 0x24 | mask=4 val=0 | `FUN_14003d7fc` | 60918 | FUN_1400444d0(param_1,0x24,4,0); |
| rmw | 0x2C | mask=4 val=0 | `FUN_14003d7fc` | 60920 | FUN_1400444d0(param_1,0x2c,4,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003d7fc` | 60921 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003d7fc` | 60923 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=7 | `FUN_14003d7fc` | 60928 | FUN_1400444d0(param_1,0xf,7,7); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003d7fc` | 60933 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x04 | - | `FUN_14003df30` | 60954 | uVar1 = FUN_140044484(param_1,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003df30` | 60970 | FUN_1400444d0(param_1,0xf,7,0); |
| write | 0x28 | 0x88 | `FUN_14003df30` | 60972 | FUN_140044528(param_1,0x28,0x88); |
| read | 0x13 | - | `FUN_14003df30` | 60986 | bVar2 = FUN_140044484(param_1,0x13); |
| read | 0x16 | - | `FUN_14003df30` | 60988 | bVar2 = FUN_140044484(param_1,0x16); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003e090` | 61028 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e090` | 61033 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=3 | `FUN_14003e090` | 61061 | FUN_1400444d0(param_1,0xf,7,3); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003e090` | 61065 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e090` | 61072 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e090` | 61096 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e4b8` | 61140 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003e4b8` | 61144 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e4b8` | 61165 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003e4b8` | 61167 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e4b8` | 61221 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e83c` | 61247 | FUN_1400444d0(param_1,0xf,7,0); |
| write | 0x08 | 4 | `FUN_14003e83c` | 61248 | FUN_140044528(param_1,8,4); |
| write | 0x22 | 0x12 | `FUN_14003e83c` | 61249 | FUN_140044528(param_1,0x22,0x12); |
| write | 0x22 | 0x10 | `FUN_14003e83c` | 61250 | FUN_140044528(param_1,0x22,0x10); |
| rmw | 0x23 | mask=0xfd val=0xac | `FUN_14003e83c` | 61252 | FUN_1400444d0(param_1,0x23,0xfd,0xac); |
| rmw | 0x23 | mask=0xfd val=0xa0 | `FUN_14003e83c` | 61255 | FUN_1400444d0(param_1,0x23,0xfd,0xa0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e83c` | 61269 | FUN_1400444d0(param_1,0xf,7,0); |
| write | 0x0D | 4 | `FUN_14003e83c` | 61270 | FUN_140044528(param_1,0xd,4); |
| write | 0x22 | 0x12 | `FUN_14003e83c` | 61271 | FUN_140044528(param_1,0x22,0x12); |
| write | 0x22 | 0x10 | `FUN_14003e83c` | 61272 | FUN_140044528(param_1,0x22,0x10); |
| rmw | 0x2B | mask=0xfd val=0xac | `FUN_14003e83c` | 61274 | FUN_1400444d0(param_1,0x2b,0xfd,0xac); |
| rmw | 0x2B | mask=0xfd val=0xa0 | `FUN_14003e83c` | 61277 | FUN_1400444d0(param_1,0x2b,0xfd,0xa0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003e83c` | 61280 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e83c` | 61286 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003e83c` | 61291 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ea04` | 61302 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x22 | mask=2 val=2 | `FUN_14003ea04` | 61303 | FUN_1400444d0(param_1,0x22,2,2); |
| rmw | 0x22 | mask=2 val=0 | `FUN_14003ea04` | 61304 | FUN_1400444d0(param_1,0x22,2,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003eaa4` | 61321 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x22 | mask=1 val=1 | `FUN_14003eaa4` | 61322 | FUN_1400444d0(param_1,0x22,1,1); |
| rmw | 0x22 | mask=1 val=0 | `FUN_14003eaa4` | 61324 | FUN_1400444d0(param_1,0x22,1,0); |
| rmw | 0x10 | mask=2 val=2 | `FUN_14003eaa4` | 61325 | FUN_1400444d0(param_1,0x10,2,2); |
| rmw | 0x12 | mask=0x80 val=0x80 | `FUN_14003eaa4` | 61326 | FUN_1400444d0(param_1,0x12,0x80,0x80); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003eb0c` | 61339 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x1B | - | `FUN_14003eb0c` | 61340 | bVar1 = FUN_140044484(param_1,0x1b); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003eb0c` | 61354 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003eb0c` | 61359 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003eb0c` | 61362 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ebd0` | 61377 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ecf4` | 61423 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003ecf4` | 61425 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003ecf4` | 61439 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003ecf4` | 61444 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ecf4` | 61447 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ecf4` | 61452 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003ee30` | 61466 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ee30` | 61468 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ee7c` | 61480 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ef68` | 61544 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=5 | `FUN_14003ef68` | 61591 | FUN_1400444d0(param_1,0xf,7,5); |
| write | 0x21 | (char)uVar15 | `FUN_14003ef68` | 61592 | FUN_140044528(param_1,0x21,(char)uVar15); |
| write | 0x22 | (char)(uVar15 >> 8) | `FUN_14003ef68` | 61593 | FUN_140044528(param_1,0x22,(char)(uVar15 >> 8)); |
| write | 0x23 | (char)uVar2 | `FUN_14003ef68` | 61594 | FUN_140044528(param_1,0x23,(char)uVar2); |
| write | 0x24 | (char)((ushort)uVar2 >> 8) | `FUN_14003ef68` | 61595 | FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8)); |
| write | 0x25 | (char)(uVar15 - 2) | `FUN_14003ef68` | 61596 | FUN_140044528(param_1,0x25,(char)(uVar15 - 2)); |
| write | 0x26 | (char)((ushort)(uVar15 - 2) >> 8) | `FUN_14003ef68` | 61597 | FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8)); |
| write | 0x27 | (char)sVar14 | `FUN_14003ef68` | 61598 | FUN_140044528(param_1,0x27,(char)sVar14); |
| write | 0x28 | uVar11 | `FUN_14003ef68` | 61600 | FUN_140044528(param_1,0x28,uVar11); |
| write | 0x29 | (char)sVar14 | `FUN_14003ef68` | 61601 | FUN_140044528(param_1,0x29,(char)sVar14); |
| write | 0x2A | uVar11 | `FUN_14003ef68` | 61602 | FUN_140044528(param_1,0x2a,uVar11); |
| write | 0x2B | 0xff | `FUN_14003ef68` | 61603 | FUN_140044528(param_1,0x2b,0xff); |
| write | 0x2C | 0 | `FUN_14003ef68` | 61604 | FUN_140044528(param_1,0x2c,0); |
| write | 0x2D | 0xf | `FUN_14003ef68` | 61605 | FUN_140044528(param_1,0x2d,0xf); |
| write | 0x2E | 0xff | `FUN_14003ef68` | 61606 | FUN_140044528(param_1,0x2e,0xff); |
| write | 0x2F | 0 | `FUN_14003ef68` | 61607 | FUN_140044528(param_1,0x2f,0); |
| rmw | 0x20 | mask=0x40 val=0 | `FUN_14003ef68` | 61626 | FUN_1400444d0(param_1,0x20,0x40,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003ef68` | 61633 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f424` | 61647 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f424` | 61654 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003f424` | 61659 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f424` | 61663 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=4 | `FUN_14003f424` | 61666 | FUN_1400444d0(param_1,0xf,7,4); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f424` | 61670 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x13 | - | `FUN_14003f500` | 61685 | bVar1 = FUN_140044484(param_1,0x13); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f500` | 61698 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f59c` | 61709 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=5 | `FUN_14003f59c` | 61713 | FUN_1400444d0(param_1,0xf,7,5); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f59c` | 61718 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f59c` | 61733 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f690` | 61746 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f690` | 61786 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f774` | 61817 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f774` | 61820 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f774` | 61826 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=1 | `FUN_14003f774` | 61838 | FUN_1400444d0(param_1,0xf,7,1); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f774` | 61883 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=2 | `FUN_14003f8e0` | 61910 | FUN_1400444d0(param_1,0xf,7,2); |
| rmw | 0x0F | mask=7 val=0 | `FUN_14003f8e0` | 61913 | FUN_1400444d0(param_1,0xf,7,0); |
| rmw | 0x0F | mask=7 val=0 | `FUN_140040140` | 62331 | FUN_1400444d0(param_1,0xf,7,0); |
| read | 0x15 | - | `FUN_140040140` | 62335 | bVar2 = FUN_140044484(param_1,0x15); |
| read | 0x14 | - | `FUN_140040140` | 62347 | bVar2 = FUN_140044484(param_1,0x14); |
| read | 0x19 | - | `FUN_140040140` | 62359 | bVar2 = FUN_140044484(param_1,0x19); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040610` | 62533 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040610` | 62542 | thunk_FUN_1400444d0(param_1,4,bVar3,bVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040610` | 62546 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040610` | 62552 | thunk_FUN_1400444d0(param_1,4,bVar3,bVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040610` | 62558 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62706 | thunk_FUN_1400444d0(param_1,0,bVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62709 | thunk_FUN_1400444d0(param_1,4,bVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62711 | thunk_FUN_1400444d0(param_1,0,bVar1,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62721 | thunk_FUN_1400444d0(param_1,0,bVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62727 | thunk_FUN_1400444d0(param_1,4,bVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140040960` | 62732 | thunk_FUN_1400444d0(param_1,0,bVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041280` | 62946 | thunk_FUN_1400444d0(param_1,bVar6,bVar7,1); |
| rmw | 0x22 | mask=8 val=0 | `FUN_140041280` | 62953 | FUN_1400444d0(param_1,0x22,8,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041280` | 62954 | thunk_FUN_1400444d0(param_1,0,bVar6,bVar7); |
| write | 0x07 | 0xf0 | `FUN_140041280` | 62955 | FUN_140044528(param_1,7,0xf0); |
| write | 0x0C | 0xf0 | `FUN_140041280` | 62956 | FUN_140044528(param_1,0xc,0xf0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14004165c` | 63025 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| read | 0x19 | - | `FUN_14004165c` | 63026 | cVar1 = FUN_140044484(param_1,0x19); |
| read | 0x13 | - | `FUN_14004165c` | 63028 | cVar2 = FUN_140044484(param_1,0x13); |
| read | 0x14 | - | `FUN_14004165c` | 63029 | bVar3 = FUN_140044484(param_1,0x14); |
| read | 0x16 | - | `FUN_14004165c` | 63036 | cVar2 = FUN_140044484(param_1,0x16); |
| read | 0x17 | - | `FUN_14004165c` | 63037 | bVar3 = FUN_140044484(param_1,0x17); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14004165c` | 63038 | thunk_FUN_1400444d0(param_1,4,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14004165c` | 63044 | thunk_FUN_1400444d0(param_1,0,bVar6,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041784` | 63071 | thunk_FUN_1400444d0(param_1,bVar4,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041784` | 63087 | thunk_FUN_1400444d0(param_1,bVar5,bVar4,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041784` | 63175 | thunk_FUN_1400444d0(param_1,0,bVar4,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63264 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| rmw | 0x22 | mask=0x40 val=0x40 | `FUN_140041b38` | 63272 | FUN_1400444d0(param_1,0x22,0x40,0x40); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63278 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| rmw | 0x22 | mask=0x40 val=0 | `FUN_140041b38` | 63283 | FUN_1400444d0(param_1,0x22,0x40,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63287 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63338 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63352 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63364 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63383 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63538 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| write | 0x27 | *(char *)(param_1 + 0x164) + -0x80 | `FUN_140041b38` | 63545 | FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80); |
| write | 0x28 | *(char *)(param_1 + 0x165) + -0x80 | `FUN_140041b38` | 63546 | FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80); |
| write | 0x29 | cVar1 | `FUN_140041b38` | 63549 | FUN_140044528(param_1,0x29,cVar1); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63551 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63563 | thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4); |
| write | 0x2C | 0 | `FUN_140041b38` | 63567 | FUN_140044528(param_1,0x2c,0); |
| write | 0x2D | 7 | `FUN_140041b38` | 63569 | FUN_140044528(param_1,0x2d,7); |
| write | 0x27 | bVar8 \| 0x80 | `FUN_140041b38` | 63577 | FUN_140044528(param_1,0x27,bVar8 \\| 0x80); |
| write | 0x28 | bVar8 \| 0x80 | `FUN_140041b38` | 63591 | FUN_140044528(param_1,0x28,bVar8 \\| 0x80); |
| write | 0x29 | bVar8 \| 0x80 | `FUN_140041b38` | 63605 | FUN_140044528(param_1,0x29,bVar8 \\| 0x80); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63612 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63629 | thunk_FUN_1400444d0(param_1,bVar8,bVar17,bVar13); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63635 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140041b38` | 63637 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14004265c` | 63766 | thunk_FUN_1400444d0(param_1,0,bVar6,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400428d8` | 63812 | thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4); |
| write | 0x27 | cVar13 + -0x80 | `FUN_1400428d8` | 63974 | FUN_140044528(param_1,0x27,cVar13 + -0x80); |
| write | 0x29 | cVar13 + -0x80 | `FUN_1400428d8` | 63981 | FUN_140044528(param_1,0x29,cVar13 + -0x80); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400428d8` | 63988 | thunk_FUN_1400444d0(param_1,bVar4,bVar3,bVar10); |
| write | 0x28 | cVar13 + -0x80 | `FUN_1400428d8` | 63999 | FUN_140044528(param_1,0x28,cVar13 + -0x80); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400428d8` | 64005 | thunk_FUN_1400444d0(param_1,0,bVar15,bVar10); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140042e78` | 64023 | thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140042e78` | 64049 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140042f8c` | 64069 | thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140042f8c` | 64144 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043184` | 64166 | thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4); |
| write | 0x2C | 0 | `FUN_140043184` | 64168 | FUN_140044528(param_1,0x2c,0); |
| write | 0x2D | 7 | `FUN_140043184` | 64170 | FUN_140044528(param_1,0x2d,7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043184` | 64171 | thunk_FUN_1400444d0(param_1,0,bVar1,param_4); |
| write | 0x07 | 0xff | `FUN_140043184` | 64173 | FUN_140044528(param_1,7,0xff); |
| write | 0x23 | 0xb0 | `FUN_140043184` | 64174 | FUN_140044528(param_1,0x23,0xb0); |
| write | 0x23 | 0xa0 | `FUN_140043184` | 64177 | FUN_140044528(param_1,0x23,0xa0); |
| rmw | 0x23 | mask=2 val=2 | `FUN_140043184` | 64181 | FUN_1400444d0(param_1,0x23,2,2); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043184` | 64183 | thunk_FUN_1400444d0(param_1,3,bVar1,param_4); |
| write | 0x27 | 0x9f | `FUN_140043184` | 64184 | FUN_140044528(param_1,0x27,0x9f); |
| write | 0x28 | 0x9f | `FUN_140043184` | 64185 | FUN_140044528(param_1,0x28,0x9f); |
| write | 0x29 | 0x9f | `FUN_140043184` | 64186 | FUN_140044528(param_1,0x29,0x9f); |
| write | 0x22 | 0 | `FUN_140043184` | 64187 | FUN_140044528(param_1,0x22,0); |
| write | 0x0C | 0xff | `FUN_140043184` | 64193 | FUN_140044528(param_1,0xc,0xff); |
| write | 0x2B | 0xb0 | `FUN_140043184` | 64194 | FUN_140044528(param_1,0x2b,0xb0); |
| write | 0x2B | 0xa0 | `FUN_140043184` | 64197 | FUN_140044528(param_1,0x2b,0xa0); |
| rmw | 0x2B | mask=2 val=2 | `FUN_140043184` | 64201 | FUN_1400444d0(param_1,0x2b,2,2); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043184` | 64203 | thunk_FUN_1400444d0(param_1,7,bVar1,param_4); |
| write | 0x27 | 0x9f | `FUN_140043184` | 64204 | FUN_140044528(param_1,0x27,0x9f); |
| write | 0x28 | 0x9f | `FUN_140043184` | 64205 | FUN_140044528(param_1,0x28,0x9f); |
| write | 0x29 | 0x9f | `FUN_140043184` | 64206 | FUN_140044528(param_1,0x29,0x9f); |
| write | 0x22 | 0 | `FUN_140043184` | 64207 | FUN_140044528(param_1,0x22,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043184` | 64212 | thunk_FUN_1400444d0(param_1,0,bVar1,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043370` | 64240 | thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043370` | 64322 | thunk_FUN_1400444d0(param_1,0,bVar6,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043660` | 64350 | thunk_FUN_1400444d0(param_1,bVar9,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043810` | 64402 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| write | 0x07 | 0xff | `FUN_140043810` | 64405 | FUN_140044528(param_1,7,0xff); |
| write | 0x23 | 0xb0 | `FUN_140043810` | 64406 | FUN_140044528(param_1,0x23,0xb0); |
| write | 0x23 | 0xa0 | `FUN_140043810` | 64409 | FUN_140044528(param_1,0x23,0xa0); |
| rmw | 0x23 | mask=2 val=2 | `FUN_140043810` | 64413 | FUN_1400444d0(param_1,0x23,2,2); |
| read | 0x14 | - | `FUN_140043810` | 64416 | uVar1 = FUN_140044484(param_1,0x14); |
| read | 0x14 | - | `FUN_140043810` | 64419 | bVar2 = FUN_140044484(param_1,0x14); |
| write | 0x0C | 0xff | `FUN_140043810` | 64423 | FUN_140044528(param_1,0xc,0xff); |
| write | 0x2B | 0xb0 | `FUN_140043810` | 64424 | FUN_140044528(param_1,0x2b,0xb0); |
| write | 0x2B | 0xa0 | `FUN_140043810` | 64427 | FUN_140044528(param_1,0x2b,0xa0); |
| rmw | 0x2B | mask=2 val=2 | `FUN_140043810` | 64431 | FUN_1400444d0(param_1,0x2b,2,2); |
| read | 0x17 | - | `FUN_140043810` | 64434 | uVar1 = FUN_140044484(param_1,0x17); |
| read | 0x17 | - | `FUN_140043810` | 64437 | bVar3 = FUN_140044484(param_1,0x17); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043810` | 64451 | thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4); |
| write | 0x20 | uVar4 | `FUN_140043810` | 64466 | FUN_140044528(param_1,0x20,uVar4); |
| write | 0x21 | uVar1 | `FUN_140043810` | 64467 | FUN_140044528(param_1,0x21,uVar1); |
| write | 0x26 | 0 | `FUN_140043810` | 64468 | FUN_140044528(param_1,0x26,0); |
| write | 0x27 | 0 | `FUN_140043810` | 64469 | FUN_140044528(param_1,0x27,0); |
| write | 0x28 | 0 | `FUN_140043810` | 64470 | FUN_140044528(param_1,0x28,0); |
| write | 0x29 | 0 | `FUN_140043810` | 64471 | FUN_140044528(param_1,0x29,0); |
| write | 0x22 | 0x38 | `FUN_140043810` | 64472 | FUN_140044528(param_1,0x22,0x38); |
| rmw | 0x22 | mask=4 val=4 | `FUN_140043810` | 64473 | FUN_1400444d0(param_1,0x22,4,4); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043810` | 64477 | FUN_1400444d0(param_1,0x22,4,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043810` | 64478 | thunk_FUN_1400444d0(param_1,0,bVar2,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043a60` | 64495 | thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043a60` | 64502 | FUN_1400444d0(param_1,0x22,4,0); |
| write | 0x27 | *(char *)(param_1 + 0x164) + -0x80 | `FUN_140043a60` | 64512 | FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80); |
| write | 0x28 | *(char *)(param_1 + 0x165) + -0x80 | `FUN_140043a60` | 64513 | FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80); |
| write | 0x29 | bVar3 | `FUN_140043a60` | 64515 | FUN_140044528(param_1,0x29,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043a60` | 64524 | thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043a60` | 64541 | thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1); |
| write | 0x2D | 0 | `FUN_140043a60` | 64543 | FUN_140044528(param_1,0x2d,0); |
| rmw | 0x22 | mask=4 val=4 | `FUN_140043a60` | 64550 | FUN_1400444d0(param_1,0x22,4,4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043a60` | 64551 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043c30` | 64569 | thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4); |
| write | 0x20 | 0x1b | `FUN_140043c30` | 64575 | FUN_140044528(param_1,0x20,0x1b); |
| write | 0x21 | 3 | `FUN_140043c30` | 64576 | FUN_140044528(param_1,0x21,3); |
| rmw | 0x20 | mask=0x80 val=0 | `FUN_140043c30` | 64578 | FUN_1400444d0(param_1,0x20,0x80,0); |
| write | 0x22 | 0 | `FUN_140043c30` | 64580 | FUN_140044528(param_1,0x22,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043c30` | 64581 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar4); |
| write | 0x07 | 0xff | `FUN_140043c30` | 64583 | FUN_140044528(param_1,7,0xff); |
| write | 0x23 | 0xb0 | `FUN_140043c30` | 64584 | FUN_140044528(param_1,0x23,0xb0); |
| write | 0x23 | 0xa0 | `FUN_140043c30` | 64587 | FUN_140044528(param_1,0x23,0xa0); |
| write | 0x0C | 0xff | `FUN_140043c30` | 64597 | FUN_140044528(param_1,0xc,0xff); |
| write | 0x2B | 0xb0 | `FUN_140043c30` | 64598 | FUN_140044528(param_1,0x2b,0xb0); |
| write | 0x2B | 0xa0 | `FUN_140043c30` | 64601 | FUN_140044528(param_1,0x2b,0xa0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043c30` | 64610 | thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043c30` | 64625 | thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4); |
| write | 0x26 | 0 | `FUN_140043c30` | 64627 | FUN_140044528(param_1,0x26,0); |
| write | 0x27 | 0x1f | `FUN_140043c30` | 64628 | FUN_140044528(param_1,0x27,0x1f); |
| write | 0x28 | 0x1f | `FUN_140043c30` | 64629 | FUN_140044528(param_1,0x28,0x1f); |
| write | 0x29 | 0x1f | `FUN_140043c30` | 64630 | FUN_140044528(param_1,0x29,0x1f); |
| rmw | 0x2C | mask=0xc0 val=0xc0 | `FUN_140043c30` | 64641 | FUN_1400444d0(param_1,0x2c,0xc0,0xc0); |
| rmw | 0x2D | mask=0xf0 val=0x20 | `FUN_140043c30` | 64642 | FUN_1400444d0(param_1,0x2d,0xf0,0x20); |
| rmw | 0x2D | mask=7 val=0 | `FUN_140043c30` | 64643 | FUN_1400444d0(param_1,0x2d,7,0); |
| rmw | 0x22 | mask=4 val=4 | `FUN_140043c30` | 64662 | FUN_1400444d0(param_1,0x22,4,4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043c30` | 64663 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64680 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| read | 0x07 | - | `FUN_140043f14` | 64681 | bVar1 = FUN_140044484(param_1,7); |
| write | 0x07 | bVar4 | `FUN_140043f14` | 64683 | FUN_140044528(param_1,7,bVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64705 | thunk_FUN_1400444d0(param_1,bVar2,bVar4,bVar6); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043f14` | 64713 | FUN_1400444d0(param_1,0x22,4,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64714 | thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64723 | thunk_FUN_1400444d0(param_1,3,bVar4,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64726 | thunk_FUN_1400444d0(param_1,7,bVar4,bVar6); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043f14` | 64730 | FUN_1400444d0(param_1,0x22,4,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64732 | thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64749 | thunk_FUN_1400444d0(param_1,bVar2,bVar4,bVar6); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043f14` | 64757 | FUN_1400444d0(param_1,0x22,4,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64758 | thunk_FUN_1400444d0(param_1,0,bVar4,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64774 | thunk_FUN_1400444d0(param_1,bVar1,bVar4,bVar6); |
| rmw | 0x22 | mask=4 val=0 | `FUN_140043f14` | 64778 | FUN_1400444d0(param_1,0x22,4,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140043f14` | 64779 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_14004419c` | 64790 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| read | 0x0C | - | `FUN_14004419c` | 64791 | bVar1 = FUN_140044484(param_1,0xc); |
| write | 0x0C | bVar1 | `FUN_14004419c` | 64792 | FUN_140044528(param_1,0xc,bVar1); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 64972 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 64978 | thunk_FUN_1400444d0(param_1,5,bVar7,bVar8); |
| rmw | 0x20 | mask=0x40 val=0x40 | `FUN_1400445c0` | 64981 | FUN_1400444d0(param_1,0x20,0x40,0x40); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 64982 | thunk_FUN_1400444d0(param_1,1,bVar7,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 64986 | thunk_FUN_1400444d0(param_1,0,bVar7,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 65051 | thunk_FUN_1400444d0(param_1,1,bVar7,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 65053 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 65074 | thunk_FUN_1400444d0(param_1,0,bVar7,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400445c0` | 65106 | thunk_FUN_1400444d0(param_1,0,bVar7,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044a88` | 65163 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| read | 0x11 | - | `FUN_140044a88` | 65164 | bVar1 = FUN_140044484(param_1,0x11); |
| write | 0x11 | bVar1 | `FUN_140044a88` | 65167 | FUN_140044528(param_1,0x11,bVar1); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044a88` | 65169 | thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4); |
| read | 0x24 | - | `FUN_140044a88` | 65170 | cVar2 = FUN_140044484(param_1,0x24); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044a88` | 65171 | thunk_FUN_1400444d0(param_1,0,(byte)uVar5,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044a88` | 65194 | thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4); |
| read | 0x24 | - | `FUN_140044a88` | 65195 | uVar3 = FUN_140044484(param_1,0x24); |
| read | 0x25 | - | `FUN_140044a88` | 65197 | uVar3 = FUN_140044484(param_1,0x25); |
| read | 0x26 | - | `FUN_140044a88` | 65199 | uVar3 = FUN_140044484(param_1,0x26); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044a88` | 65228 | thunk_FUN_1400444d0(param_1,0,bVar1,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044cf8` | 65264 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044cf8` | 65268 | thunk_FUN_1400444d0(param_1,2,bVar9,bVar11); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044cf8` | 65290 | thunk_FUN_1400444d0(param_1,0,bVar9,bVar11); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140044cf8` | 65352 | thunk_FUN_1400444d0(param_1,0,bVar9,bVar11); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65453 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65477 | thunk_FUN_1400444d0(param_1,4,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65480 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65488 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65494 | thunk_FUN_1400444d0(param_1,1,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65497 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65576 | thunk_FUN_1400444d0(param_1,1,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65619 | thunk_FUN_1400444d0(param_1,1,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045168` | 65625 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045654` | 65665 | thunk_FUN_1400444d0(param_1,2,param_3,param_4); |
| read | 0x13 | - | `FUN_140045654` | 65666 | uVar1 = FUN_140044484(param_1,0x13); |
| read | 0x12 | - | `FUN_140045654` | 65668 | uVar1 = FUN_140044484(param_1,0x12); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045654` | 65677 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400456c4` | 65690 | thunk_FUN_1400444d0(param_1,2,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400456c4` | 65705 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045788` | 65737 | thunk_FUN_1400444d0(param_1,2,param_3,param_4); |
| read | 0x10 | - | `FUN_140045788` | 65738 | uVar1 = FUN_140044484(param_1,0x10); |
| read | 0x11 | - | `FUN_140045788` | 65740 | uVar1 = FUN_140044484(param_1,0x11); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045788` | 65742 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400457d0` | 65756 | thunk_FUN_1400444d0(param_1,2,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400457d0` | 65768 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045a28` | 65879 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045b30` | 65926 | thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045b30` | 65936 | thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4); |
| read | 0x00 | - | `FUN_140045bfc` | 65964 | cVar1 = FUN_140044484(param_1,0); |
| read | 0x01 | - | `FUN_140045bfc` | 65965 | cVar2 = FUN_140044484(param_1,1); |
| read | 0x02 | - | `FUN_140045bfc` | 65966 | cVar3 = FUN_140044484(param_1,2); |
| read | 0x03 | - | `FUN_140045bfc` | 65967 | cVar4 = FUN_140044484(param_1,3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66106 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66124 | thunk_FUN_1400444d0(param_1,0,(byte)uVar4,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66129 | thunk_FUN_1400444d0(param_1,3,(byte)uVar4,bVar3); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66133 | thunk_FUN_1400444d0(param_1,7,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66137 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66138 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar7); |
| write | 0x25 | 0 | `FUN_140045eec` | 66139 | FUN_140044528(param_1,0x25,0); |
| write | 0x26 | 0 | `FUN_140045eec` | 66141 | FUN_140044528(param_1,0x26,0); |
| write | 0x27 | 0 | `FUN_140045eec` | 66143 | FUN_140044528(param_1,0x27,0); |
| write | 0x2A | 1 | `FUN_140045eec` | 66145 | FUN_140044528(param_1,0x2a,1); |
| write | 0x2D | 0xff | `FUN_140045eec` | 66146 | FUN_140044528(param_1,0x2d,0xff); |
| write | 0x2E | 0xff | `FUN_140045eec` | 66147 | FUN_140044528(param_1,0x2e,0xff); |
| write | 0x2F | 0xff | `FUN_140045eec` | 66148 | FUN_140044528(param_1,0x2f,0xff); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66151 | thunk_FUN_1400444d0(param_1,3,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66155 | thunk_FUN_1400444d0(param_1,7,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66159 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar7); |
| write | 0x26 | 0xff | `FUN_140045eec` | 66167 | FUN_140044528(param_1,0x26,0xff); |
| write | 0x27 | 0xff | `FUN_140045eec` | 66168 | FUN_140044528(param_1,0x27,0xff); |
| write | 0x2A | 0x3a | `FUN_140045eec` | 66169 | FUN_140044528(param_1,0x2a,0x3a); |
| write | 0x2D | 0 | `FUN_140045eec` | 66170 | FUN_140044528(param_1,0x2d,0); |
| write | 0x2E | 0 | `FUN_140045eec` | 66171 | FUN_140044528(param_1,0x2e,0); |
| write | 0x2F | 0 | `FUN_140045eec` | 66173 | FUN_140044528(param_1,0x2f,0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66177 | thunk_FUN_1400444d0(param_1,3,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66181 | thunk_FUN_1400444d0(param_1,7,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66185 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66186 | thunk_FUN_1400444d0(param_1,4,bVar3,bVar7); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140045eec` | 66192 | thunk_FUN_1400444d0(param_1,0,(byte)uVar5,(byte)uVar8); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400461f4` | 66228 | thunk_FUN_1400444d0(param_1,0,bVar2,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400461f4` | 66233 | thunk_FUN_1400444d0(param_1,4,bVar2,bVar4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400461f4` | 66238 | thunk_FUN_1400444d0(param_1,0,bVar2,(byte)uVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400462e4` | 66270 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400462e4` | 66283 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66354 | thunk_FUN_1400444d0(param_1,-(*(char *)(param_1 + 0x1ec) != '\0') & 4,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66356 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66390 | thunk_FUN_1400444d0(param_1,0,param_3,param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66419 | thunk_FUN_1400444d0(param_1,2,bVar10,bVar2); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66428 | thunk_FUN_1400444d0(param_1,0,bVar10,bVar2); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046400` | 66463 | thunk_FUN_1400444d0(param_1,0,bVar10,bVar2); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66509 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| read | 0x09 | - | `FUN_140046828` | 66511 | bVar2 = FUN_140044484(param_1,9); |
| write | 0x09 | bVar2 & 4 | `FUN_140046828` | 66513 | FUN_140044528(param_1,9,bVar2 & 4); |
| read | 0x10 | - | `FUN_140046828` | 66514 | bVar3 = FUN_140044484(param_1,0x10); |
| write | 0x10 | bVar3 | `FUN_140046828` | 66516 | FUN_140044528(param_1,0x10,bVar3); |
| read | 0x11 | - | `FUN_140046828` | 66517 | bVar4 = FUN_140044484(param_1,0x11); |
| write | 0x11 | (char)uVar14 | `FUN_140046828` | 66519 | FUN_140044528(param_1,0x11,(char)uVar14); |
| read | 0x12 | - | `FUN_140046828` | 66520 | bVar5 = FUN_140044484(param_1,0x12); |
| write | 0x12 | (char)uVar14 | `FUN_140046828` | 66522 | FUN_140044528(param_1,0x12,(char)uVar14); |
| read | 0x1D | - | `FUN_140046828` | 66523 | bVar6 = FUN_140044484(param_1,0x1d); |
| write | 0x1D | bVar6 | `FUN_140046828` | 66525 | FUN_140044528(param_1,0x1d,bVar6); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66546 | thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66563 | thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66573 | thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66596 | thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4); |
| write | 0x11 | 2 | `FUN_140046828` | 66661 | FUN_140044528(param_1,0x11,2); |
| write | 0x11 | 1 | `FUN_140046828` | 66666 | FUN_140044528(param_1,0x11,1); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66672 | thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4); |
| read | 0x1A | - | `FUN_140046828` | 66674 | bVar2 = FUN_140044484(param_1,0x1a); |
| read | 0x1B | - | `FUN_140046828` | 66676 | bVar3 = FUN_140044484(param_1,0x1b); |
| write | 0x12 | 0x80 | `FUN_140046828` | 66687 | FUN_140044528(param_1,0x12,0x80); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66714 | thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4); |
| read | 0x14 | - | `FUN_140046828` | 66715 | bVar2 = FUN_140044484(param_1,0x14); |
| read | 0x15 | - | `FUN_140046828` | 66717 | bVar3 = FUN_140044484(param_1,0x15); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140046828` | 66720 | thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140047318` | 66900 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15); |
| read | 0x05 | - | `FUN_140047318` | 66902 | bVar1 = FUN_140044484(param_1,5); |
| write | 0x05 | bVar1 | `FUN_140047318` | 66903 | FUN_140044528(param_1,5,bVar1); |
| read | 0x06 | - | `FUN_140047318` | 66904 | bVar2 = FUN_140044484(param_1,6); |
| write | 0x06 | bVar2 | `FUN_140047318` | 66905 | FUN_140044528(param_1,6,bVar2); |
| read | 0x08 | - | `FUN_140047318` | 66906 | bVar3 = FUN_140044484(param_1,8); |
| write | 0x08 | bVar3 | `FUN_140047318` | 66907 | FUN_140044528(param_1,8,bVar3); |
| read | 0x09 | - | `FUN_140047318` | 66908 | bVar4 = FUN_140044484(param_1,9); |
| write | 0x09 | bVar13 | `FUN_140047318` | 66910 | FUN_140044528(param_1,9,bVar13); |
| read | 0x13 | - | `FUN_140047318` | 66911 | cVar5 = FUN_140044484(param_1,0x13); |
| read | 0x14 | - | `FUN_140047318` | 66912 | bVar6 = FUN_140044484(param_1,0x14); |
| read | 0x15 | - | `FUN_140047318` | 66914 | bVar7 = FUN_140044484(param_1,0x15); |
| read | 0x13 | - | `FUN_140047318` | 66953 | bVar8 = FUN_140044484(param_1,0x13); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140047318` | 67003 | thunk_FUN_1400444d0(param_1,3,bVar13,bVar15); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140047318` | 67007 | thunk_FUN_1400444d0(param_1,0,bVar13,bVar15); |
| read | 0x14 | - | `FUN_140047318` | 67024 | FUN_140044484(param_1,0x14); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140047318` | 67027 | thunk_FUN_1400444d0(param_1,0,bVar13,bVar15); |
| read | 0x14 | - | `FUN_140047318` | 67028 | bVar1 = FUN_140044484(param_1,0x14); |
| read | 0x15 | - | `FUN_140047318` | 67114 | bVar1 = FUN_140044484(param_1,0x15); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400479f8` | 67161 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16); |
| read | 0x0A | - | `FUN_1400479f8` | 67163 | bVar1 = FUN_140044484(param_1,10); |
| write | 0x0A | bVar1 | `FUN_1400479f8` | 67164 | FUN_140044528(param_1,10,bVar1); |
| read | 0x0B | - | `FUN_1400479f8` | 67165 | bVar2 = FUN_140044484(param_1,0xb); |
| write | 0x0B | bVar2 | `FUN_1400479f8` | 67166 | FUN_140044528(param_1,0xb,bVar2); |
| read | 0x0D | - | `FUN_1400479f8` | 67167 | bVar3 = FUN_140044484(param_1,0xd); |
| write | 0x0D | bVar3 | `FUN_1400479f8` | 67168 | FUN_140044528(param_1,0xd,bVar3); |
| read | 0x0E | - | `FUN_1400479f8` | 67169 | bVar4 = FUN_140044484(param_1,0xe); |
| write | 0x0E | bVar4 | `FUN_1400479f8` | 67171 | FUN_140044528(param_1,0xe,bVar4); |
| read | 0x16 | - | `FUN_1400479f8` | 67172 | cVar5 = FUN_140044484(param_1,0x16); |
| read | 0x17 | - | `FUN_1400479f8` | 67173 | bVar6 = FUN_140044484(param_1,0x17); |
| read | 0x18 | - | `FUN_1400479f8` | 67175 | bVar7 = FUN_140044484(param_1,0x18); |
| read | 0x16 | - | `FUN_1400479f8` | 67214 | bVar8 = FUN_140044484(param_1,0x16); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400479f8` | 67263 | thunk_FUN_1400444d0(param_1,7,bVar14,bVar16); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400479f8` | 67267 | thunk_FUN_1400444d0(param_1,0,bVar14,bVar16); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400479f8` | 67283 | thunk_FUN_1400444d0(param_1,0,bVar14,bVar16); |
| read | 0x17 | - | `FUN_1400479f8` | 67285 | bVar1 = FUN_140044484(param_1,0x17); |
| read | 0x17 | - | `FUN_1400479f8` | 67291 | bVar1 = FUN_140044484(param_1,0x17); |
| read | 0x15 | - | `FUN_1400479f8` | 67377 | bVar14 = FUN_140044484(param_1,0x15); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67416 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67421 | thunk_FUN_1400444d0(param_1,4,bVar3,bVar5); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67426 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67431 | thunk_FUN_1400444d0(param_1,bVar3,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67440 | thunk_FUN_1400444d0(param_1,0,bVar3,bVar5); |
| write | 0x08 | 4 | `FUN_1400480e0` | 67452 | FUN_140044528(param_1,8,4); |
| write | 0x0D | 4 | `FUN_1400480e0` | 67453 | FUN_140044528(param_1,0xd,4); |
| write | 0x22 | 0x12 | `FUN_1400480e0` | 67454 | FUN_140044528(param_1,0x22,0x12); |
| write | 0x22 | 0x10 | `FUN_1400480e0` | 67455 | FUN_140044528(param_1,0x22,0x10); |
| rmw | 0x23 | mask=0xfd val=0xac | `FUN_1400480e0` | 67456 | FUN_1400444d0(param_1,0x23,0xfd,0xac); |
| rmw | 0x23 | mask=0xfd val=0xa0 | `FUN_1400480e0` | 67457 | FUN_1400444d0(param_1,0x23,0xfd,0xa0); |
| rmw | 0x2B | mask=0xfd val=0xac | `FUN_1400480e0` | 67458 | FUN_1400444d0(param_1,0x2b,0xfd,0xac); |
| rmw | 0x2B | mask=0xfd val=0xa0 | `FUN_1400480e0` | 67459 | FUN_1400444d0(param_1,0x2b,0xfd,0xa0); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67483 | thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_1400480e0` | 67507 | thunk_FUN_1400444d0(param_1,0,(byte)uVar4,(byte)param_4); |
| bank | 0x0F | mask=0x07 val=param_1 & 7 | `FUN_140048478` | 67613 | thunk_FUN_1400444d0(param_1,0,bVar1,bVar5); |

### Range 0x50-0x5F - FPGA Control (Unknown)
| Op | Register | Value / Mask | Function | Line | Context |
|----|----------|--------------|----------|------|---------|
| read | 0x5A | - | `FUN_14003d7fc` | 60898 | cVar5 = FUN_140044484(param_1,0x5a); |
| read | 0x59 | - | `FUN_14003d7fc` | 60899 | bVar1 = FUN_140044484(param_1,0x59); |
| read | 0x59 | - | `FUN_14003d7fc` | 60900 | bVar2 = FUN_140044484(param_1,0x59); |
| read | 0x5A | - | `FUN_14003d7fc` | 60902 | cVar6 = FUN_140044484(param_1,0x5a); |
| read | 0x59 | - | `FUN_14003d7fc` | 60903 | bVar7 = FUN_140044484(param_1,0x59); |
| read | 0x59 | - | `FUN_14003d7fc` | 60904 | bVar8 = FUN_140044484(param_1,0x59); |
| write | 0x5F | 4 | `FUN_14003e4b8` | 61145 | FUN_140044528(param_1,0x5f,4); |
| write | 0x5F | 5 | `FUN_14003e4b8` | 61146 | FUN_140044528(param_1,0x5f,5); |
| write | 0x58 | 0x12 | `FUN_14003e4b8` | 61147 | FUN_140044528(param_1,0x58,0x12); |
| write | 0x58 | 2 | `FUN_14003e4b8` | 61148 | FUN_140044528(param_1,0x58,2); |
| write | 0x5F | 4 | `FUN_14003e4b8` | 61154 | FUN_140044528(param_1,0x5f,4); |
| write | 0x58 | 0x12 | `FUN_14003e4b8` | 61155 | FUN_140044528(param_1,0x58,0x12); |
| write | 0x58 | 2 | `FUN_14003e4b8` | 61156 | FUN_140044528(param_1,0x58,2); |
| write | 0x57 | 1 | `FUN_14003e4b8` | 61169 | FUN_140044528(param_1,0x57,1); |
| write | 0x50 | 0 | `FUN_14003e4b8` | 61170 | FUN_140044528(param_1,0x50,0); |
| write | 0x51 | 0 | `FUN_14003e4b8` | 61171 | FUN_140044528(param_1,0x51,0); |
| write | 0x54 | 4 | `FUN_14003e4b8` | 61172 | FUN_140044528(param_1,0x54,4); |
| write | 0x50 | 0 | `FUN_14003e4b8` | 61175 | FUN_140044528(param_1,0x50,0); |
| write | 0x51 | 1 | `FUN_14003e4b8` | 61176 | FUN_140044528(param_1,0x51,1); |
| write | 0x54 | 4 | `FUN_14003e4b8` | 61177 | FUN_140044528(param_1,0x54,4); |
| write | 0x50 | uVar10 | `FUN_14003e4b8` | 61183 | FUN_140044528(param_1,0x50,uVar10); |
| write | 0x51 | 0xb0 | `FUN_14003e4b8` | 61184 | FUN_140044528(param_1,0x51,0xb0); |
| write | 0x54 | 4 | `FUN_14003e4b8` | 61185 | FUN_140044528(param_1,0x54,4); |
| write | 0x50 | uVar10 | `FUN_14003e4b8` | 61188 | FUN_140044528(param_1,0x50,uVar10); |
| write | 0x51 | 0xb1 | `FUN_14003e4b8` | 61189 | FUN_140044528(param_1,0x51,0xb1); |
| write | 0x54 | 4 | `FUN_14003e4b8` | 61190 | FUN_140044528(param_1,0x54,4); |
| write | 0x5F | 0 | `FUN_14003e4b8` | 61220 | FUN_140044528(param_1,0x5f,0); |
| rmw | 0x55 | mask=0x80 val=0 | `FUN_140041784` | 63090 | FUN_1400444d0(param_1,0x55,0x80,0); |
| read | 0x5D | - | `FUN_1400428d8` | 63859 | bVar3 = FUN_140044484(param_1,0x5d); |
| read | 0x5E | - | `FUN_1400428d8` | 63861 | bVar4 = FUN_140044484(param_1,0x5e); |
| read | 0x5F | - | `FUN_1400428d8` | 63862 | bVar5 = FUN_140044484(param_1,0x5f); |
| write | 0x50 | bVar6 | `FUN_140043370` | 64277 | FUN_140044528(param_1,0x50,bVar6); |
| read | 0x50 | - | `FUN_140043370` | 64290 | FUN_140044484(param_1,0x50); |
| write | 0x51 | *(undefined1 *)(uVar5 + 0xb5 + lVar1) | `FUN_140043370` | 64301 | FUN_140044528(param_1,0x51,*(undefined1 *)(uVar5 + 0xb5 + lVar1)); |
| write | 0x52 | *(undefined1 *)(uVar5 + 0xb6 + lVar1) | `FUN_140043370` | 64302 | FUN_140044528(param_1,0x52,*(undefined1 *)(uVar5 + 0xb6 + lVar1)); |
| write | 0x53 | bVar6 | `FUN_140043370` | 64304 | FUN_140044528(param_1,0x53,bVar6); |
| read | 0x51 | - | `FUN_140043370` | 64309 | FUN_140044484(param_1,0x51); |
| read | 0x52 | - | `FUN_140043370` | 64313 | FUN_140044484(param_1,0x52); |
| read | 0x53 | - | `FUN_140043370` | 64317 | FUN_140044484(param_1,0x53); |
| rmw | 0x54 | mask=0x80 val=0 | `FUN_140043a60` | 64547 | FUN_1400444d0(param_1,0x54,0x80,0); |
| rmw | 0x54 | mask=0x80 val=0x80 | `FUN_140043c30` | 64658 | FUN_1400444d0(param_1,0x54,0x80,0x80); |
| write | 0x55 | 0x40 | `FUN_140043c30` | 64659 | FUN_140044528(param_1,0x55,0x40); |

## 7. Magic Values / Constants
| Value | Count in direct wrapper args | Notes |
|-------|-------------------------------|-------|
| `0xb0` | 10 | Seen in the known `0x23` / `0x2B` transition sequence before `0xA0`. |
| `0xa0` | 18 | Part of the known `0xB0 -> 0xA0` sequence; also used in other mode-control RMWs. |
| `0x02` | 27 | Very common in bank-select / mode-control writes; this includes the known TTL-mode pattern. |
| `0x00` | 187 | Most common data value; many resets / clears use zero. |
| `0x0f` | 272 | This is overwhelmingly bank-select register `0x0F`. |
| `0x07` | 281 | Mask used with bank-select helper `thunk_FUN_1400444d0`. |
| `0x80` | 64 |  |
| `0x01` | 58 |  |
| `0x10` | 57 |  |
| `0x04` | 42 |  |
| `0x40` | 35 |  |
| `0xc5` | 28 |  |
| `0x22` | 27 |  |
| `0x08` | 23 |  |
| `0xff` | 19 |  |
| `0x03` | 16 |  |
| `0x23` | 14 |  |

### Focused Sequences Requested by the User
- `0xB0 -> 0xA0` sequence:
  - `FUN_140043810`: lines 64406 / 64409 on register `0x23`, and 64424 / 64427 on `0x2B`.
  - `FUN_140043184`: lines 64174 / 64177 on `0x23`, and 64194 / 64197 on `0x2B`.
  - `FUN_140043c30`: lines 64584 / 64587 on `0x23`, and 64598 / 64601 on `0x2B`.
- `0x02` TTL-style writes:
  - `FUN_140043810`: line 64413 writes `FUN_1400444d0(param_1, 0x23, 2, 2);` and line 64431 writes the same pattern for `0x2B`.
  - `FUN_140043184`: same pattern at lines 64181 / 64201.

## 8. Unknown Registers
These are the registers / offsets that still look important, but are not fully decoded by behavior alone:
- `MMIO 0x40`: Set to `old | 0x08` in `FUN_14001a0b8`; likely global engine/IRQ enable, but exact bit meaning is still unknown.
- `MMIO 0x120`: Written with `0x3D` or `0xF9` based on rate / device; appears timing-related.
- `MMIO 0x124`: Always written `0x00` during stream-on init.
- `MMIO 0x128`: Always written `0x80` during stream-on init.
- `MMIO 0x180`: Derived from sample rate (`0x1E848 / rate`); likely pacing or IRQ interval register.
- `MMIO 0x1C`: Conditional device/firmware-specific write `0x1B33` / `0x0B33`.
- `MMIO 0x107C`: DDR status/reset register used by the worker thread (`8`, then `1`, then poll bit 0).
- `I2C 0x81`: Audio reset / output state register, clearly important but exact bitfield layout remains partial.
- `I2C 0x86`: Used as an enable/toggle bit in multiple audio IRQ paths; exact semantic name still unknown.
- `I2C 0x8C`: IRQ-side toggle register with mask `0x10` / `0x08`; exact function still partial.
- `I2C 0x90`: Single direct write `0x8F` at line 67486 (`FUN_1400480e0`); purpose unresolved.
- `I2C 0xA0/0xA1/0xA2/0xA4/0xA7`: Heavily manipulated in `FUN_14003d7fc`; looks like output / DMA / lane gating, but bitfields are not fully named.

## Appendix A - All Direct Register Write Hits (constant register arguments)
Each entry contains the requested metadata plus 5 lines of context before and after.

### W0001 - `FUN_140039640` line 57931
- Register: `0x6E `
- Kind: `direct_write`
- Value: `bVar5`
```c
 57926:     bVar1 = FUN_140044484((longlong)param_1,0x6e);
 57927:     bVar5 = bVar1 | 2;
 57928:     if (cVar4 == '\0') {
 57929:       bVar5 = bVar1 & 0xfd;
 57930:     }
 57931:     FUN_140044528((longlong)param_1,0x6e,bVar5);
 57932:     *(char *)((longlong)param_1 + 0x22b) = cVar4;
 57933:     if ((DAT_1400a04f8 & 0x10) != 0) {
 57934:       DbgPrint("Update iTE6805_DATA.AVT_Flag_Enable_Dither = %d\n",cVar4);
 57935:     }
 57936:   }
```

### W0002 - `FUN_1400396f4` line 57984
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 57979:     if (iVar1 == 0) {
 57980:       if (*(char *)(param_1 + 0x40c) == 'R') {
 57981:         local_190 = 0xffffffffffffffff;
 57982:         uVar2 = 0;
 57983:         do {
 57984:           thunk_FUN_1400444d0(param_1,(byte)uVar2,(byte)puVar3,(byte)puVar4);
 57985:           uVar5 = 0;
 57986:           do {
 57987:             FUN_1400392c0(param_1,0x90,(byte)uVar5,1,local_198);
 57988:             FUN_14001a414(local_128,0x100,"%02x(a)  %04x(r) %02x(d)\r\n",0x90);
 57989:             strlen(local_128);
```

### W0003 - `FUN_1400396f4` line 57997
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 57992:             ZwWriteFile(local_188);
 57993:             uVar5 = uVar5 + 1;
 57994:           } while (uVar5 < 0x100);
 57995:           uVar2 = uVar2 + 1;
 57996:         } while (uVar2 < 8);
 57997:         thunk_FUN_1400444d0(param_1,0,(byte)puVar3,(byte)puVar4);
 57998:         ZwWriteFile(local_188,0,0,0);
 57999:       }
 58000:       if (local_188 != 0) {
 58001:         ZwClose();
 58002:       }
```

### W0004 - `FUN_14003a83c` line 58959
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 58954:   byte bVar3;
 58955: 
 58956:   bVar2 = DAT_1400a000c;
 58957:   uVar1 = (ulonglong)DAT_14009fee4;
 58958:   if (DAT_14009fee4 == 1) {
 58959:     thunk_FUN_1400444d0(param_1,DAT_1400a000d,param_3,param_4);
 58960:     uVar1 = (ulonglong)bVar2;
 58961:     bVar2 = DAT_1400a0008;
 58962:     FUN_14003a2e8(param_1,DAT_1400a0020,DAT_1400a0008,uVar1,1,DAT_14009fee8,1);
 58963:     bVar3 = (byte)uVar1;
 58964:   }
```

### W0005 - `FUN_14003a83c` line 58967
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 58962:     FUN_14003a2e8(param_1,DAT_1400a0020,DAT_1400a0008,uVar1,1,DAT_14009fee8,1);
 58963:     bVar3 = (byte)uVar1;
 58964:   }
 58965:   else {
 58966:     if (DAT_14009fee4 != 2) goto LAB_14003a8f1;
 58967:     thunk_FUN_1400444d0(param_1,DAT_1400a000d,param_3,param_4);
 58968:     uVar1 = (ulonglong)bVar2;
 58969:     bVar2 = DAT_1400a0008;
 58970:     FUN_14003a35c(param_1,DAT_1400a0020,DAT_1400a0008,uVar1,1,&DAT_14009ff08,1);
 58971:     bVar3 = (byte)uVar1;
 58972:   }
```

### W0006 - `FUN_14003a83c` line 58973
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 58968:     uVar1 = (ulonglong)bVar2;
 58969:     bVar2 = DAT_1400a0008;
 58970:     FUN_14003a35c(param_1,DAT_1400a0020,DAT_1400a0008,uVar1,1,&DAT_14009ff08,1);
 58971:     bVar3 = (byte)uVar1;
 58972:   }
 58973:   uVar1 = thunk_FUN_1400444d0(param_1,0,bVar2,bVar3);
 58974:   DAT_14009fee4 = 0;
 58975: LAB_14003a8f1:
 58976:   return CONCAT71((int7)(uVar1 >> 8),1);
 58977: }
 58978: 
```

### W0007 - `FUN_14003b090` line 59667
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 59662:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59663:     bVar14 = (byte)iVar16;
 59664:     bVar15 = 0;
 59665:     DbgPrint("(_d:%d)  xxDevice:%d, repetition: 0x%02x [68051 vfmt]");
 59666:   }
 59667:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59668:   bVar12 = FUN_140044484(param_1,0x9e);
 59669:   bVar13 = FUN_140044484(param_1,0x9d);
 59670:   local_88 = (uint)bVar13 + (bVar12 & 0x3f) * 0x100;
 59671:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59672:     bVar14 = (byte)local_88;
```

### W0008 - `FUN_14003b090` line 59676
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 59671:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59672:     bVar14 = (byte)local_88;
 59673:     bVar15 = 0;
 59674:     DbgPrint("(_d:%d)  xxDevice:%d, packet_width: 0x%04x(%lu) [68051 vfmt]");
 59675:   }
 59676:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59677:   bVar14 = FUN_140044484(param_1,0xa5);
 59678:   bVar15 = FUN_140044484(param_1,0xa4);
 59679:   uVar24 = (uint)bVar15 + (bVar14 & 0x3f) * 0x100;
 59680:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59681:     DbgPrint("(_d:%d)  xxDevice:%d, packet_height: 0x%04x(%lu) [68051 vfmt]",
```

### W0009 - `FUN_14003b090` line 59786
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 59781:     }
 59782:     bVar15 = 0;
 59783:     DbgPrint("(_d:%d)  xxDevice:%d, 68051(0x6b,98) In CS: %s, In ColorDepth: %s, Out CS: %s, Out ColorDepth: %s,  PP: %s, InForce: %s[68051 vfmt]"
 59784:              ,*(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8));
 59785:   }
 59786:   thunk_FUN_1400444d0(param_1,1,bVar15,bVar12);
 59787:   bVar13 = FUN_140044484(param_1,0xc0);
 59788:   FUN_140044484(param_1,0xc1);
 59789:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59790:     bVar12 = 0x10;
 59791:     uVar25 = 1;
```

### W0010 - `FUN_14003b090` line 59801
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 59796:     DbgPrint(
 59797:             "(_d:%d)  xxDevice:%d, 68051(0xc0,c1),  i/o: TTL: %s, TTL Out: %s, rgb for Single Pixel: %d, outDDR: %s, DDR Mode: %s [68051 vfmt]"
 59798:             );
 59799:   }
 59800:   uVar20 = 0;
 59801:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar12);
 59802:   uVar22 = (undefined7)((ulonglong)uVar20 >> 8);
 59803:   bVar12 = FUN_140044484(param_1,0xb0);
 59804:   uVar21 = CONCAT71(uVar22,0xb1);
 59805:   bVar15 = FUN_140044484(param_1,0xb1);
 59806:   for (bVar15 = bVar15 & 0x3f; bVar15 != 0; bVar15 = bVar15 >> 1) {
```

### W0011 - `thunk_FUN_1400444d0` line 60183
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `param_2 & 7`
```c
 60178: 
 60179: 
 60180: void thunk_FUN_1400444d0(longlong param_1,byte param_2,byte param_3,byte param_4)
 60181: 
 60182: {
 60183:   FUN_1400444d0(param_1,0xf,7,param_2 & 7);
 60184:   return;
 60185: }
 60186: 
 60187: 
 60188: 
```

### W0012 - `FUN_14003cb50` line 60213
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 60208:       if ((DAT_1400a04f8 & 0x10) != 0) {
 60209:         DbgPrint(
 60210:                 "------------- (HActive >= 1920 || VActive >= 2160), set to DDR Mode - 02 (vfmt-S)\n\n"
 60211:                 );
 60212:       }
 60213:       FUN_1400444d0(param_1,0xf,7,1);
 60214:       FUN_140044528(param_1,0xc4,0x10);
 60215:       return CONCAT71(extraout_var_00,1);
 60216:     }
 60217:     if (*(uint *)(param_1 + 0x234) < 0x249f1) {
 60218:       *(undefined4 *)(param_1 + 500) = 0;
```

### W0013 - `FUN_14003cb50` line 60214
- Register: `0xC4 `
- Kind: `direct_write`
- Value: `0x10`
```c
 60209:         DbgPrint(
 60210:                 "------------- (HActive >= 1920 || VActive >= 2160), set to DDR Mode - 02 (vfmt-S)\n\n"
 60211:                 );
 60212:       }
 60213:       FUN_1400444d0(param_1,0xf,7,1);
 60214:       FUN_140044528(param_1,0xc4,0x10);
 60215:       return CONCAT71(extraout_var_00,1);
 60216:     }
 60217:     if (*(uint *)(param_1 + 0x234) < 0x249f1) {
 60218:       *(undefined4 *)(param_1 + 500) = 0;
 60219:       FUN_14003f690(param_1);
```

### W0014 - `FUN_14003cb50` line 60243
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 60238:     "------------- (HActive >= 1920 || VActive >= 2160) **YUV420**, force set to SDR Mode - 02 (vfmt-S)\n\n"
 60239:     ;
 60240:   }
 60241:   DbgPrint(pcVar2);
 60242: LAB_14003cc24:
 60243:   FUN_1400444d0(param_1,0xf,7,1);
 60244:   FUN_140044528(param_1,0xc4,0);
 60245:   return (ulonglong)extraout_var << 8;
 60246: }
 60247: 
 60248: 
```

### W0015 - `FUN_14003cb50` line 60244
- Register: `0xC4 `
- Kind: `direct_write`
- Value: `0`
```c
 60239:     ;
 60240:   }
 60241:   DbgPrint(pcVar2);
 60242: LAB_14003cc24:
 60243:   FUN_1400444d0(param_1,0xf,7,1);
 60244:   FUN_140044528(param_1,0xc4,0);
 60245:   return (ulonglong)extraout_var << 8;
 60246: }
 60247: 
 60248: 
 60249: 
```

### W0016 - `FUN_14003ccac` line 60271
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60266: 
 60267: {
 60268:   byte bVar1;
 60269:   byte bVar2;
 60270: 
 60271:   FUN_1400444d0(param_1,0xf,7,0);
 60272:   bVar1 = FUN_140044484(param_1,0x10);
 60273:   bVar2 = FUN_140044484(param_1,0xb1);
 60274:   return (bVar2 & ~bVar1) >> 7;
 60275: }
 60276: 
```

### W0017 - `FUN_14003ccf4` line 60285
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60280: 
 60281: {
 60282:   byte bVar1;
 60283: 
 60284:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 60285:     FUN_1400444d0(param_1,0xf,7,0);
 60286:     bVar1 = FUN_140044484(param_1,0xaa);
 60287:   }
 60288:   else {
 60289:     FUN_1400444d0(param_1,0xf,7,4);
 60290:     bVar1 = FUN_140044484(param_1,0xaa);
```

### W0018 - `FUN_14003ccf4` line 60289
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 60284:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 60285:     FUN_1400444d0(param_1,0xf,7,0);
 60286:     bVar1 = FUN_140044484(param_1,0xaa);
 60287:   }
 60288:   else {
 60289:     FUN_1400444d0(param_1,0xf,7,4);
 60290:     bVar1 = FUN_140044484(param_1,0xaa);
 60291:     FUN_1400444d0(param_1,0xf,7,0);
 60292:   }
 60293:   return bVar1 & 8;
 60294: }
```

### W0019 - `FUN_14003ccf4` line 60291
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60286:     bVar1 = FUN_140044484(param_1,0xaa);
 60287:   }
 60288:   else {
 60289:     FUN_1400444d0(param_1,0xf,7,4);
 60290:     bVar1 = FUN_140044484(param_1,0xaa);
 60291:     FUN_1400444d0(param_1,0xf,7,0);
 60292:   }
 60293:   return bVar1 & 8;
 60294: }
 60295: 
 60296: 
```

### W0020 - `FUN_14003cd5c` line 60303
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60298: byte FUN_14003cd5c(longlong param_1)
 60299: 
 60300: {
 60301:   byte bVar1;
 60302: 
 60303:   FUN_1400444d0(param_1,0xf,7,0);
 60304:   bVar1 = 0x13;
 60305:   if (*(char *)(param_1 + 0x1ec) != '\0') {
 60306:     bVar1 = 0x16;
 60307:   }
 60308:   bVar1 = FUN_140044484(param_1,bVar1);
```

### W0021 - `FUN_14003cd90` line 60319
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60314: byte FUN_14003cd90(longlong param_1)
 60315: 
 60316: {
 60317:   byte bVar1;
 60318: 
 60319:   FUN_1400444d0(param_1,0xf,7,0);
 60320:   bVar1 = 0x13;
 60321:   if (*(char *)(param_1 + 0x1ec) != '\0') {
 60322:     bVar1 = 0x16;
 60323:   }
 60324:   bVar1 = FUN_140044484(param_1,bVar1);
```

### W0022 - `FUN_14003cdc4` line 60335
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60330: undefined1 FUN_14003cdc4(longlong param_1)
 60331: 
 60332: {
 60333:   byte bVar1;
 60334: 
 60335:   FUN_1400444d0(param_1,0xf,7,0);
 60336:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 60337:     bVar1 = 0x14;
 60338:   }
 60339:   else {
 60340:     if (*(char *)(param_1 + 0x1ec) != '\x01') {
```

### W0023 - `FUN_14003ce08` line 60359
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60354: byte FUN_14003ce08(longlong param_1,char param_2)
 60355: 
 60356: {
 60357:   byte bVar1;
 60358: 
 60359:   FUN_1400444d0(param_1,0xf,7,0);
 60360:   if ((param_2 == '\0') && (bVar1 = FUN_140044484(param_1,0x13), (bVar1 & 2) != 0)) {
 60361:     return 0;
 60362:   }
 60363:   if (param_2 == '\x01') {
 60364:     bVar1 = FUN_140044484(param_1,0x16);
```

### W0024 - `FUN_14003ce60` line 60380
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60375: byte FUN_14003ce60(longlong param_1,char param_2)
 60376: 
 60377: {
 60378:   byte bVar1;
 60379: 
 60380:   FUN_1400444d0(param_1,0xf,7,0);
 60381:   if (param_2 == '\0') {
 60382:     bVar1 = FUN_140044484(param_1,0x13);
 60383:     bVar1 = bVar1 & 0x40;
 60384:   }
 60385:   else {
```

### W0025 - `FUN_14003ce9c` line 60398
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60393: byte FUN_14003ce9c(longlong param_1)
 60394: 
 60395: {
 60396:   byte bVar1;
 60397: 
 60398:   FUN_1400444d0(param_1,0xf,7,0);
 60399:   bVar1 = FUN_140044484(param_1,0x19);
 60400:   return bVar1 & 0x80;
 60401: }
 60402: 
 60403: 
```

### W0026 - `FUN_14003cec4` line 60410
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 60405: bool FUN_14003cec4(longlong param_1)
 60406: 
 60407: {
 60408:   byte bVar1;
 60409: 
 60410:   FUN_1400444d0(param_1,0xf,7,1);
 60411:   bVar1 = FUN_140044484(param_1,0xc0);
 60412:   FUN_1400444d0(param_1,0xf,7,0);
 60413:   return (bVar1 & 1) != 0;
 60414: }
 60415: 
```

### W0027 - `FUN_14003cec4` line 60412
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60407: {
 60408:   byte bVar1;
 60409: 
 60410:   FUN_1400444d0(param_1,0xf,7,1);
 60411:   bVar1 = FUN_140044484(param_1,0xc0);
 60412:   FUN_1400444d0(param_1,0xf,7,0);
 60413:   return (bVar1 & 1) != 0;
 60414: }
 60415: 
 60416: 
 60417: 
```

### W0028 - `FUN_14003d010` line 60472
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60467: byte FUN_14003d010(longlong param_1)
 60468: 
 60469: {
 60470:   byte bVar1;
 60471: 
 60472:   FUN_1400444d0(param_1,0xf,7,0);
 60473:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 60474:     bVar1 = 0x14;
 60475:   }
 60476:   else {
 60477:     if (*(char *)(param_1 + 0x1ec) != '\x01') {
```

### W0029 - `FUN_14003d0a0` line 60520
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `2`
```c
 60515: void FUN_14003d0a0(longlong param_1)
 60516: 
 60517: {
 60518:   byte bVar1;
 60519: 
 60520:   FUN_1400444d0(param_1,0xf,7,2);
 60521:   bVar1 = FUN_140044484(param_1,0x15);
 60522:   *(uint *)(param_1 + 0x20c) = bVar1 >> 5 & 3;
 60523:   bVar1 = FUN_140044484(param_1,0x16);
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
```

### W0030 - `FUN_14003d0a0` line 60535
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
 60531:   bVar1 = FUN_140044484(param_1,0x19);
 60532:   *(uint *)(param_1 + 0x218) = (uint)(bVar1 >> 6);
 60533:   bVar1 = FUN_140044484(param_1,0x15);
 60534:   *(byte *)(param_1 + 0x21f) = bVar1 & 3;
 60535:   FUN_1400444d0(param_1,0xf,7,0);
 60536:   if (*(int *)(param_1 + 0x214) == 0) {
 60537:     *(uint *)(param_1 + 0x214) = (*(byte *)(param_1 + 0x21c) < 2) + 1;
 60538:   }
 60539:   *(bool *)(param_1 + 0x227) = *(int *)(param_1 + 0x20c) == 3;
 60540:   return;
```

### W0031 - `FUN_14003d18c` line 60555
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60550:   uint uVar3;
 60551:   ulonglong uVar4;
 60552:   longlong lVar5;
 60553: 
 60554:   uVar3 = 0;
 60555:   FUN_1400444d0(param_1,0xf,7,0);
 60556:   uVar4 = 1;
 60557:   lVar5 = 5;
 60558:   do {
 60559:     FUN_140048644(param_1,3);
 60560:     FUN_1400444d0(param_1,0x9a,0x80,0);
```

### W0032 - `FUN_14003d18c` line 60560
- Register: `0x9A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60555:   FUN_1400444d0(param_1,0xf,7,0);
 60556:   uVar4 = 1;
 60557:   lVar5 = 5;
 60558:   do {
 60559:     FUN_140048644(param_1,3);
 60560:     FUN_1400444d0(param_1,0x9a,0x80,0);
 60561:     bVar1 = FUN_140044484(param_1,0x9a);
 60562:     bVar2 = FUN_140044484(param_1,0x99);
 60563:     FUN_1400444d0(param_1,0x9a,0x80,0x80);
 60564:     uVar3 = uVar3 + (bVar1 & 7) * 0x100 + (uint)bVar2;
 60565:     lVar5 = lVar5 + -1;
```

### W0033 - `FUN_14003d18c` line 60563
- Register: `0x9A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60558:   do {
 60559:     FUN_140048644(param_1,3);
 60560:     FUN_1400444d0(param_1,0x9a,0x80,0);
 60561:     bVar1 = FUN_140044484(param_1,0x9a);
 60562:     bVar2 = FUN_140044484(param_1,0x99);
 60563:     FUN_1400444d0(param_1,0x9a,0x80,0x80);
 60564:     uVar3 = uVar3 + (bVar1 & 7) * 0x100 + (uint)bVar2;
 60565:     lVar5 = lVar5 + -1;
 60566:   } while (lVar5 != 0);
 60567:   if (uVar3 != 0) {
 60568:     uVar4 = (ulonglong)uVar3;
```

### W0034 - `FUN_14003d24c` line 60587
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `-(*(char *)(param_1 + 0x1ec) != '\0') & 4`
```c
 60582:   uint uVar4;
 60583:   uint uVar5;
 60584:   ulonglong uVar6;
 60585: 
 60586:   uVar5 = 0;
 60587:   FUN_1400444d0(param_1,0xf,7,-(*(char *)(param_1 + 0x1ec) != '\0') & 4);
 60588:   uVar4 = 0;
 60589:   if (param_2 != 0) {
 60590:     uVar6 = (ulonglong)param_2;
 60591:     do {
 60592:       FUN_140048644(param_1,3);
```

### W0035 - `FUN_14003d24c` line 60600
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60595:       uVar6 = uVar6 - 1;
 60596:       uVar4 = (uint)param_2;
 60597:     } while (uVar6 != 0);
 60598:   }
 60599:   bVar2 = FUN_140044484(param_1,0x43);
 60600:   FUN_1400444d0(param_1,0xf,7,0);
 60601:   if ((char)(bVar2 & 0xe0) < '\0') {
 60602:     uVar3 = uVar4 * *(int *)(param_1 + 0x98) * 0x400;
 60603:   }
 60604:   else if ((bVar2 & 0x40) == 0) {
 60605:     if ((bVar2 & 0x20) == 0) goto LAB_14003d31f;
```

### W0036 - `FUN_14003d388` line 60650
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `-(*(char *)(param_1 + 0x1ec) != '\0') & 4`
```c
 60645:   short sVar17;
 60646:   ushort uVar18;
 60647:   longlong lVar19;
 60648: 
 60649:   uVar12 = 0;
 60650:   FUN_1400444d0(param_1,0xf,7,-(*(char *)(param_1 + 0x1ec) != '\0') & 4);
 60651:   uVar14 = 0;
 60652:   uVar2 = 1;
 60653:   uVar15 = 1;
 60654:   lVar19 = 10;
 60655:   do {
```

### W0037 - `FUN_14003d388` line 60668
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60663:   } while (lVar19 != 0);
 60664:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60665:     DbgPrint("sumt = %lu \n",uVar14);
 60666:   }
 60667:   bVar3 = FUN_140044484(param_1,0x43);
 60668:   FUN_1400444d0(param_1,0xf,7,0);
 60669:   puVar1 = (uint *)(param_1 + 0x98);
 60670:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60671:     DbgPrint("RCLKVALUE=%lu.%02luMHz\n",*puVar1 / 1000,(ulonglong)(*puVar1 % 1000) / 10);
 60672:   }
 60673:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0038 - `FUN_14003d388` line 60736
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60735:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 60736:     FUN_1400444d0(param_1,0xf,7,4);
 60737:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60738:       uVar6 = FUN_140044484(param_1,0xaa);
 60739:       DbgPrint("hdmirxrd(0xAA) = 0x%02X \n",uVar6);
 60740:     }
 60741:   }
```

### W0039 - `FUN_14003d388` line 60744
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60739:       DbgPrint("hdmirxrd(0xAA) = 0x%02X \n",uVar6);
 60740:     }
 60741:   }
 60742:   bVar4 = FUN_140044484(param_1,0xaa);
 60743:   bVar5 = FUN_140044484(param_1,0xaa);
 60744:   FUN_1400444d0(param_1,0xf,7,0);
 60745:   *(short *)(param_1 + 0x240) = sVar7;
 60746:   *(ushort *)(param_1 + 0x244) = ((uVar16 - sVar8) - sVar7) - sVar17;
 60747:   *(short *)(param_1 + 0x242) = sVar8;
 60748:   *(short *)(param_1 + 0x22e) = sVar9;
 60749:   *(uint *)(param_1 + 0x238) = uVar12;
```

### W0040 - `FUN_14003d7fc` line 60788
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 60783:   char cVar5;
 60784:   char cVar6;
 60785:   byte bVar7;
 60786:   byte bVar8;
 60787: 
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
```

### W0041 - `FUN_14003d7fc` line 60789
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60784:   char cVar6;
 60785:   byte bVar7;
 60786:   byte bVar8;
 60787: 
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
```

### W0042 - `FUN_14003d7fc` line 60790
- Register: `0xA0 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60785:   byte bVar7;
 60786:   byte bVar8;
 60787: 
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
```

### W0043 - `FUN_14003d7fc` line 60791
- Register: `0xA1 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60786:   byte bVar8;
 60787: 
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
```

### W0044 - `FUN_14003d7fc` line 60792
- Register: `0xA2 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60787: 
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0045 - `FUN_14003d7fc` line 60793
- Register: `0xA4 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 60788:   FUN_1400444d0(param_1,0xf,7,3);
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
```

### W0046 - `FUN_14003d7fc` line 60794
- Register: `0x3B `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0`
```c
 60789:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
```

### W0047 - `FUN_14003d7fc` line 60795
- Register: `0xA7 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 60790:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
```

### W0048 - `FUN_14003d7fc` line 60796
- Register: `0x48 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60791:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
```

### W0049 - `FUN_14003d7fc` line 60797
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60792:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
```

### W0050 - `FUN_14003d7fc` line 60798
- Register: `0x29 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 60793:   FUN_1400444d0(param_1,0xa4,8,8);
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
```

### W0051 - `FUN_14003d7fc` line 60799
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x41`
- Value: `0x41`
```c
 60794:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60795:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
```

### W0052 - `FUN_14003d7fc` line 60801
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60796:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
```

### W0053 - `FUN_14003d7fc` line 60802
- Register: `0x24 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 60797:   FUN_1400444d0(param_1,0xf,7,0);
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
```

### W0054 - `FUN_14003d7fc` line 60803
- Register: `0x25 `
- Kind: `direct_write`
- Value: `0`
```c
 60798:   FUN_1400444d0(param_1,0x29,1,1);
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
```

### W0055 - `FUN_14003d7fc` line 60804
- Register: `0x26 `
- Kind: `direct_write`
- Value: `0`
```c
 60799:   FUN_1400444d0(param_1,0x2a,0x41,0x41);
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
```

### W0056 - `FUN_14003d7fc` line 60805
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0`
```c
 60800:   FUN_140048644(param_1,10);
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
```

### W0057 - `FUN_14003d7fc` line 60806
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0`
```c
 60801:   FUN_1400444d0(param_1,0x2a,0x40,0);
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
```

### W0058 - `FUN_14003d7fc` line 60807
- Register: `0x3C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 60802:   FUN_1400444d0(param_1,0x24,4,4);
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
```

### W0059 - `FUN_14003d7fc` line 60808
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `7`
```c
 60803:   FUN_140044528(param_1,0x25,0);
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
```

### W0060 - `FUN_14003d7fc` line 60809
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60804:   FUN_140044528(param_1,0x26,0);
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
```

### W0061 - `FUN_14003d7fc` line 60810
- Register: `0xA0 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60805:   FUN_140044528(param_1,0x27,0);
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
```

### W0062 - `FUN_14003d7fc` line 60811
- Register: `0xA1 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60806:   FUN_140044528(param_1,0x28,0);
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
```

### W0063 - `FUN_14003d7fc` line 60812
- Register: `0xA2 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60807:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0064 - `FUN_14003d7fc` line 60813
- Register: `0xA4 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 60808:   FUN_1400444d0(param_1,0xf,7,7);
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
```

### W0065 - `FUN_14003d7fc` line 60814
- Register: `0x3B `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0`
```c
 60809:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
```

### W0066 - `FUN_14003d7fc` line 60815
- Register: `0xA7 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 60810:   FUN_1400444d0(param_1,0xa0,0x80,0x80);
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
```

### W0067 - `FUN_14003d7fc` line 60816
- Register: `0x48 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60811:   FUN_1400444d0(param_1,0xa1,0x80,0x80);
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
```

### W0068 - `FUN_14003d7fc` line 60817
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60812:   FUN_1400444d0(param_1,0xa2,0x80,0x80);
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
```

### W0069 - `FUN_14003d7fc` line 60818
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x41`
- Value: `0x41`
```c
 60813:   FUN_1400444d0(param_1,0xa4,8,8);
 60814:   FUN_1400444d0(param_1,0x3b,0xc0,0);
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
```

### W0070 - `FUN_14003d7fc` line 60820
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60815:   FUN_1400444d0(param_1,0xa7,0x10,0x10);
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
```

### W0071 - `FUN_14003d7fc` line 60821
- Register: `0x2C `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 60816:   FUN_1400444d0(param_1,0x48,0x80,0x80);
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
```

### W0072 - `FUN_14003d7fc` line 60822
- Register: `0x2D `
- Kind: `direct_write`
- Value: `0`
```c
 60817:   FUN_1400444d0(param_1,0xf,7,0);
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
```

### W0073 - `FUN_14003d7fc` line 60823
- Register: `0x2E `
- Kind: `direct_write`
- Value: `0`
```c
 60818:   FUN_1400444d0(param_1,0x32,0x41,0x41);
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
```

### W0074 - `FUN_14003d7fc` line 60824
- Register: `0x2F `
- Kind: `direct_write`
- Value: `0`
```c
 60819:   FUN_140048644(param_1,10);
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
```

### W0075 - `FUN_14003d7fc` line 60825
- Register: `0x30 `
- Kind: `direct_write`
- Value: `0`
```c
 60820:   FUN_1400444d0(param_1,0x32,0x40,0);
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
```

### W0076 - `FUN_14003d7fc` line 60826
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 60821:   FUN_1400444d0(param_1,0x2c,4,4);
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
```

### W0077 - `FUN_14003d7fc` line 60827
- Register: `0x3C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 60822:   FUN_140044528(param_1,0x2d,0);
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0078 - `FUN_14003d7fc` line 60828
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 60823:   FUN_140044528(param_1,0x2e,0);
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
```

### W0079 - `FUN_14003d7fc` line 60829
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60824:   FUN_140044528(param_1,0x2f,0);
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
```

### W0080 - `FUN_14003d7fc` line 60830
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `7`
```c
 60825:   FUN_140044528(param_1,0x30,0);
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
 60835:   bVar2 = FUN_140044484(param_1,0xd);
```

### W0081 - `FUN_14003d7fc` line 60831
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 60826:   FUN_1400444d0(param_1,0xf,7,4);
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
 60835:   bVar2 = FUN_140044484(param_1,0xd);
 60836:   bVar7 = 0;
```

### W0082 - `FUN_14003d7fc` line 60832
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60827:   FUN_1400444d0(param_1,0x3c,0x10,0);
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
 60835:   bVar2 = FUN_140044484(param_1,0xd);
 60836:   bVar7 = 0;
 60837:   bVar2 = bVar2 & 0x30;
```

### W0083 - `FUN_14003d7fc` line 60861
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 60856:       }
 60857:       if ((DAT_1400a04f8 & 0x10) != 0) {
 60858:         DbgPrint("CAOF Fail to Finish!! \n");
 60859:       }
 60860:       if (bVar1 == 0) {
 60861:         FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60862:         FUN_140048644(param_1,10);
 60863:         FUN_1400444d0(param_1,0x2a,0x40,0);
 60864:       }
 60865:       if (bVar2 == 0) {
 60866:         FUN_1400444d0(param_1,0x32,0x40,0x40);
```

### W0084 - `FUN_14003d7fc` line 60863
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60858:         DbgPrint("CAOF Fail to Finish!! \n");
 60859:       }
 60860:       if (bVar1 == 0) {
 60861:         FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60862:         FUN_140048644(param_1,10);
 60863:         FUN_1400444d0(param_1,0x2a,0x40,0);
 60864:       }
 60865:       if (bVar2 == 0) {
 60866:         FUN_1400444d0(param_1,0x32,0x40,0x40);
 60867:         FUN_140048644(param_1,10);
 60868:         FUN_1400444d0(param_1,0x32,0x40,0);
```

### W0085 - `FUN_14003d7fc` line 60866
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 60861:         FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60862:         FUN_140048644(param_1,10);
 60863:         FUN_1400444d0(param_1,0x2a,0x40,0);
 60864:       }
 60865:       if (bVar2 == 0) {
 60866:         FUN_1400444d0(param_1,0x32,0x40,0x40);
 60867:         FUN_140048644(param_1,10);
 60868:         FUN_1400444d0(param_1,0x32,0x40,0);
 60869:       }
 60870:       bVar7 = 0;
 60871:       bVar8 = bVar8 + 1;
```

### W0086 - `FUN_14003d7fc` line 60868
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60863:         FUN_1400444d0(param_1,0x2a,0x40,0);
 60864:       }
 60865:       if (bVar2 == 0) {
 60866:         FUN_1400444d0(param_1,0x32,0x40,0x40);
 60867:         FUN_140048644(param_1,10);
 60868:         FUN_1400444d0(param_1,0x32,0x40,0);
 60869:       }
 60870:       bVar7 = 0;
 60871:       bVar8 = bVar8 + 1;
 60872:     }
 60873:     if (5 < bVar8) break;
```

### W0087 - `FUN_14003d7fc` line 60881
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 60876:   }
 60877:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60878:     DbgPrint("CAOF Fail !! \n");
 60879:   }
 60880:   if (bVar1 == 0) {
 60881:     FUN_1400444d0(param_1,0xf,7,3);
 60882:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60883:     FUN_1400444d0(param_1,0xf,7,0);
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
```

### W0088 - `FUN_14003d7fc` line 60882
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60877:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60878:     DbgPrint("CAOF Fail !! \n");
 60879:   }
 60880:   if (bVar1 == 0) {
 60881:     FUN_1400444d0(param_1,0xf,7,3);
 60882:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60883:     FUN_1400444d0(param_1,0xf,7,0);
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
```

### W0089 - `FUN_14003d7fc` line 60883
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60878:     DbgPrint("CAOF Fail !! \n");
 60879:   }
 60880:   if (bVar1 == 0) {
 60881:     FUN_1400444d0(param_1,0xf,7,3);
 60882:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60883:     FUN_1400444d0(param_1,0xf,7,0);
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
```

### W0090 - `FUN_14003d7fc` line 60884
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 60879:   }
 60880:   if (bVar1 == 0) {
 60881:     FUN_1400444d0(param_1,0xf,7,3);
 60882:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60883:     FUN_1400444d0(param_1,0xf,7,0);
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
```

### W0091 - `FUN_14003d7fc` line 60886
- Register: `0x2A `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60881:     FUN_1400444d0(param_1,0xf,7,3);
 60882:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60883:     FUN_1400444d0(param_1,0xf,7,0);
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
```

### W0092 - `FUN_14003d7fc` line 60889
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `7`
```c
 60884:     FUN_1400444d0(param_1,0x2a,0x40,0x40);
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
```

### W0093 - `FUN_14003d7fc` line 60890
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60885:     FUN_140048644(param_1,10);
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
```

### W0094 - `FUN_14003d7fc` line 60891
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60886:     FUN_1400444d0(param_1,0x2a,0x40,0);
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
```

### W0095 - `FUN_14003d7fc` line 60892
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 60887:   }
 60888:   if (bVar2 == 0) {
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
```

### W0096 - `FUN_14003d7fc` line 60894
- Register: `0x32 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 60889:     FUN_1400444d0(param_1,0xf,7,7);
 60890:     FUN_1400444d0(param_1,0x3a,0x80,0);
 60891:     FUN_1400444d0(param_1,0xf,7,0);
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
```

### W0097 - `FUN_14003d7fc` line 60897
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 60892:     FUN_1400444d0(param_1,0x32,0x40,0x40);
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
```

### W0098 - `FUN_14003d7fc` line 60901
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `7`
```c
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
 60905:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60906:     DbgPrint("CAOF     CAOF    CAOF     CAOF    CAOF     CAOF\n");
```

### W0099 - `FUN_14003d7fc` line 60914
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60909:     DbgPrint("Port 0 CAOF Int =%x , CAOF Status=%3x\n",bVar2 & 0xc0,(bVar1 & 0xf) + cVar5 * '\x10');
 60910:   }
 60911:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60912:     DbgPrint("Port 1 CAOF Int =%x , CAOF Status=%3x\n",bVar8 & 0xc0,(bVar7 & 0xf) + cVar6 * '\x10');
 60913:   }
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
```

### W0100 - `FUN_14003d7fc` line 60915
- Register: `0x08 `
- Kind: `rmw_write`
- Mask: `0x30`
- Value: `0x30`
```c
 60910:   }
 60911:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60912:     DbgPrint("Port 1 CAOF Int =%x , CAOF Status=%3x\n",bVar8 & 0xc0,(bVar7 & 0xf) + cVar6 * '\x10');
 60913:   }
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
```

### W0101 - `FUN_14003d7fc` line 60916
- Register: `0x0D `
- Kind: `rmw_write`
- Mask: `0x30`
- Value: `0x30`
```c
 60911:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60912:     DbgPrint("Port 1 CAOF Int =%x , CAOF Status=%3x\n",bVar8 & 0xc0,(bVar7 & 0xf) + cVar6 * '\x10');
 60913:   }
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
```

### W0102 - `FUN_14003d7fc` line 60917
- Register: `0x29 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 60912:     DbgPrint("Port 1 CAOF Int =%x , CAOF Status=%3x\n",bVar8 & 0xc0,(bVar7 & 0xf) + cVar6 * '\x10');
 60913:   }
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
```

### W0103 - `FUN_14003d7fc` line 60918
- Register: `0x24 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 60913:   }
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
```

### W0104 - `FUN_14003d7fc` line 60919
- Register: `0x3C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 60914:   FUN_1400444d0(param_1,0xf,7,0);
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
```

### W0105 - `FUN_14003d7fc` line 60920
- Register: `0x2C `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 60915:   FUN_1400444d0(param_1,8,0x30,0x30);
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
```

### W0106 - `FUN_14003d7fc` line 60921
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 60916:   FUN_1400444d0(param_1,0xd,0x30,0x30);
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
```

### W0107 - `FUN_14003d7fc` line 60922
- Register: `0x3C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 60917:   FUN_1400444d0(param_1,0x29,1,0);
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
```

### W0108 - `FUN_14003d7fc` line 60923
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 60918:   FUN_1400444d0(param_1,0x24,4,0);
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
```

### W0109 - `FUN_14003d7fc` line 60924
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60919:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
```

### W0110 - `FUN_14003d7fc` line 60925
- Register: `0xA0 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60920:   FUN_1400444d0(param_1,0x2c,4,0);
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
```

### W0111 - `FUN_14003d7fc` line 60926
- Register: `0xA1 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60921:   FUN_1400444d0(param_1,0xf,7,4);
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
```

### W0112 - `FUN_14003d7fc` line 60927
- Register: `0xA2 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60922:   FUN_1400444d0(param_1,0x3c,0x10,0x10);
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
```

### W0113 - `FUN_14003d7fc` line 60928
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `7`
```c
 60923:   FUN_1400444d0(param_1,0xf,7,3);
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0114 - `FUN_14003d7fc` line 60929
- Register: `0x3A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60924:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
 60934:   return;
```

### W0115 - `FUN_14003d7fc` line 60930
- Register: `0xA0 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60925:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
 60934:   return;
 60935: }
```

### W0116 - `FUN_14003d7fc` line 60931
- Register: `0xA1 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60926:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
 60934:   return;
 60935: }
 60936: 
```

### W0117 - `FUN_14003d7fc` line 60932
- Register: `0xA2 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 60927:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
 60934:   return;
 60935: }
 60936: 
 60937: 
```

### W0118 - `FUN_14003d7fc` line 60933
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60928:   FUN_1400444d0(param_1,0xf,7,7);
 60929:   FUN_1400444d0(param_1,0x3a,0x80,0);
 60930:   FUN_1400444d0(param_1,0xa0,0x80,0);
 60931:   FUN_1400444d0(param_1,0xa1,0x80,0);
 60932:   FUN_1400444d0(param_1,0xa2,0x80,0);
 60933:   FUN_1400444d0(param_1,0xf,7,0);
 60934:   return;
 60935: }
 60936: 
 60937: 
 60938: 
```

### W0119 - `FUN_14003df30` line 60970
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 60965:   *(undefined8 *)(param_1 + 0x1dc) = 1;
 60966:   FUN_140044360(param_1,&DAT_14009f250);
 60967:   FUN_14003d7fc(param_1);
 60968:   uVar10 = 0;
 60969:   uVar6 = (undefined7)((ulonglong)puVar4 >> 8);
 60970:   FUN_1400444d0(param_1,0xf,7,0);
 60971:   bVar8 = 0x88;
 60972:   FUN_140044528(param_1,0x28,0x88);
 60973:   uVar5 = CONCAT71(uVar6,1);
 60974:   FUN_14003f774(param_1,'\x01');
 60975:   FUN_14003e090(param_1);
```

### W0120 - `FUN_14003df30` line 60972
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0x88`
```c
 60967:   FUN_14003d7fc(param_1);
 60968:   uVar10 = 0;
 60969:   uVar6 = (undefined7)((ulonglong)puVar4 >> 8);
 60970:   FUN_1400444d0(param_1,0xf,7,0);
 60971:   bVar8 = 0x88;
 60972:   FUN_140044528(param_1,0x28,0x88);
 60973:   uVar5 = CONCAT71(uVar6,1);
 60974:   FUN_14003f774(param_1,'\x01');
 60975:   FUN_14003e090(param_1);
 60976:   *(undefined1 *)(param_1 + 0x204) = 0;
 60977:   *(undefined1 *)(param_1 + 0x209) = 0;
```

### W0121 - `FUN_14003e090` line 61028
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 61023:   }
 61024:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61025:     DbgPrint("CPOCLK=%lu.%03luMHz\n",(ulonglong)uVar4 / 1000,
 61026:              uVar4 + (int)((ulonglong)uVar4 / 1000) * -1000);
 61027:   }
 61028:   FUN_1400444d0(param_1,0xf,7,3);
 61029:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61030:     DbgPrint("adjust AA from 0x0C to 0x%02X\n",0xc);
 61031:   }
 61032:   FUN_1400444d0(param_1,0xaa,0x1f,0xc);
 61033:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0122 - `FUN_14003e090` line 61032
- Register: `0xAA `
- Kind: `rmw_write`
- Mask: `0x1f`
- Value: `0xc`
```c
 61027:   }
 61028:   FUN_1400444d0(param_1,0xf,7,3);
 61029:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61030:     DbgPrint("adjust AA from 0x0C to 0x%02X\n",0xc);
 61031:   }
 61032:   FUN_1400444d0(param_1,0xaa,0x1f,0xc);
 61033:   FUN_1400444d0(param_1,0xf,7,0);
 61034:   uVar6 = uVar4 >> 1;
 61035:   uVar5 = uVar6 / 10 + uVar6;
 61036:   *(uint *)(param_1 + 0x98) = uVar5;
 61037:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0123 - `FUN_14003e090` line 61033
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61028:   FUN_1400444d0(param_1,0xf,7,3);
 61029:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61030:     DbgPrint("adjust AA from 0x0C to 0x%02X\n",0xc);
 61031:   }
 61032:   FUN_1400444d0(param_1,0xaa,0x1f,0xc);
 61033:   FUN_1400444d0(param_1,0xf,7,0);
 61034:   uVar6 = uVar4 >> 1;
 61035:   uVar5 = uVar6 / 10 + uVar6;
 61036:   *(uint *)(param_1 + 0x98) = uVar5;
 61037:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61038:     DbgPrint("RCLKVALUE=%lu.%02luMHz\n",(ulonglong)uVar5 / 1000,
```

### W0124 - `FUN_14003e090` line 61042
- Register: `0x91 `
- Kind: `rmw_write`
- Mask: `0x3f`
- Value: `(byte)(*(uint *)(param_1 + 0x98) / 1000) & 0x3f`
```c
 61037:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61038:     DbgPrint("RCLKVALUE=%lu.%02luMHz\n",(ulonglong)uVar5 / 1000,
 61039:              (ulonglong)(uVar5 + (int)((ulonglong)uVar5 / 1000) * -1000) / 10);
 61040:   }
 61041:   uVar5 = (*(uint *)(param_1 + 0x98) % 1000 << 8) / 1000;
 61042:   FUN_1400444d0(param_1,0x91,0x3f,(byte)(*(uint *)(param_1 + 0x98) / 1000) & 0x3f);
 61043:   uVar2 = (undefined1)uVar5;
 61044:   if (0xff < uVar5) {
 61045:     uVar2 = 0xff;
 61046:   }
 61047:   FUN_140044528(param_1,0x92,uVar2);
```

### W0125 - `FUN_14003e090` line 61047
- Register: `0x92 `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61042:   FUN_1400444d0(param_1,0x91,0x3f,(byte)(*(uint *)(param_1 + 0x98) / 1000) & 0x3f);
 61043:   uVar2 = (undefined1)uVar5;
 61044:   if (0xff < uVar5) {
 61045:     uVar2 = 0xff;
 61046:   }
 61047:   FUN_140044528(param_1,0x92,uVar2);
 61048:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61049:     uVar2 = FUN_140044484(param_1,0x92);
 61050:     uVar3 = FUN_140044484(param_1,0x91);
 61051:     DbgPrint("1us Timer reg91=%02x, reg92=%02x \n",uVar3,uVar2);
 61052:   }
```

### W0126 - `FUN_14003e090` line 61061
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `3`
```c
 61056:   }
 61057:   else {
 61058:     uVar2 = 0xff;
 61059:   }
 61060:   if (*(char *)(param_1 + 0x1d4) == -0x50) {
 61061:     FUN_1400444d0(param_1,0xf,7,3);
 61062:     FUN_140044528(param_1,0xfa,uVar2);
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
```

### W0127 - `FUN_14003e090` line 61062
- Register: `0xFA `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61057:   else {
 61058:     uVar2 = 0xff;
 61059:   }
 61060:   if (*(char *)(param_1 + 0x1d4) == -0x50) {
 61061:     FUN_1400444d0(param_1,0xf,7,3);
 61062:     FUN_140044528(param_1,0xfa,uVar2);
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
```

### W0128 - `FUN_14003e090` line 61065
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61060:   if (*(char *)(param_1 + 0x1d4) == -0x50) {
 61061:     FUN_1400444d0(param_1,0xf,7,3);
 61062:     FUN_140044528(param_1,0xfa,uVar2);
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
```

### W0129 - `FUN_14003e090` line 61066
- Register: `0xFD `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61061:     FUN_1400444d0(param_1,0xf,7,3);
 61062:     FUN_140044528(param_1,0xfa,uVar2);
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
```

### W0130 - `FUN_14003e090` line 61067
- Register: `0xFE `
- Kind: `rmw_write`
- Mask: `0x20`
- Value: `0`
```c
 61062:     FUN_140044528(param_1,0xfa,uVar2);
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
 61072:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0131 - `FUN_14003e090` line 61068
- Register: `0xFE `
- Kind: `rmw_write`
- Mask: `0xf`
- Value: `0xc`
```c
 61063:   }
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
 61072:   FUN_1400444d0(param_1,0xf,7,0);
 61073:   uVar2 = (undefined1)((uint)((ulonglong)uVar4 * 0x1a41a41a5 >> 0x21) >> 8);
```

### W0132 - `FUN_14003e090` line 61069
- Register: `0xFE `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 61064:   if (*(char *)(param_1 + 0x1d4) == -0x4f) {
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
 61072:   FUN_1400444d0(param_1,0xf,7,0);
 61073:   uVar2 = (undefined1)((uint)((ulonglong)uVar4 * 0x1a41a41a5 >> 0x21) >> 8);
 61074:   if (0xff < uVar4 / 0x138) {
```

### W0133 - `FUN_14003e090` line 61070
- Register: `0xFE `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 61065:     FUN_1400444d0(param_1,0xf,7,1);
 61066:     FUN_140044528(param_1,0xfd,uVar2);
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
 61072:   FUN_1400444d0(param_1,0xf,7,0);
 61073:   uVar2 = (undefined1)((uint)((ulonglong)uVar4 * 0x1a41a41a5 >> 0x21) >> 8);
 61074:   if (0xff < uVar4 / 0x138) {
 61075:     uVar2 = 0xff;
```

### W0134 - `FUN_14003e090` line 61072
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61067:     FUN_1400444d0(param_1,0xfe,0x20,0);
 61068:     FUN_1400444d0(param_1,0xfe,0xf,0xc);
 61069:     FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61070:     FUN_1400444d0(param_1,0xfe,0x80,0x80);
 61071:   }
 61072:   FUN_1400444d0(param_1,0xf,7,0);
 61073:   uVar2 = (undefined1)((uint)((ulonglong)uVar4 * 0x1a41a41a5 >> 0x21) >> 8);
 61074:   if (0xff < uVar4 / 0x138) {
 61075:     uVar2 = 0xff;
 61076:   }
 61077:   FUN_140044528(param_1,0x45,uVar2);
```

### W0135 - `FUN_14003e090` line 61077
- Register: `0x45 `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61072:   FUN_1400444d0(param_1,0xf,7,0);
 61073:   uVar2 = (undefined1)((uint)((ulonglong)uVar4 * 0x1a41a41a5 >> 0x21) >> 8);
 61074:   if (0xff < uVar4 / 0x138) {
 61075:     uVar2 = 0xff;
 61076:   }
 61077:   FUN_140044528(param_1,0x45,uVar2);
 61078:   uVar1 = (ulonglong)(uVar4 / 0x138) / 5;
 61079:   uVar2 = (undefined1)uVar1;
 61080:   if (0xff < (uint)uVar1) {
 61081:     uVar2 = 0xff;
 61082:   }
```

### W0136 - `FUN_14003e090` line 61083
- Register: `0x44 `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61078:   uVar1 = (ulonglong)(uVar4 / 0x138) / 5;
 61079:   uVar2 = (undefined1)uVar1;
 61080:   if (0xff < (uint)uVar1) {
 61081:     uVar2 = 0xff;
 61082:   }
 61083:   FUN_140044528(param_1,0x44,uVar2);
 61084:   uVar5 = ((((uVar4 / 0x910) * 100) % 100) / 0x32) * 0x40 + uVar4 / 0x910;
 61085:   uVar2 = (undefined1)uVar5;
 61086:   if (0xff < uVar5) {
 61087:     uVar2 = 0xff;
 61088:   }
```

### W0137 - `FUN_14003e090` line 61089
- Register: `0x46 `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61084:   uVar5 = ((((uVar4 / 0x910) * 100) % 100) / 0x32) * 0x40 + uVar4 / 0x910;
 61085:   uVar2 = (undefined1)uVar5;
 61086:   if (0xff < uVar5) {
 61087:     uVar2 = 0xff;
 61088:   }
 61089:   FUN_140044528(param_1,0x46,uVar2);
 61090:   uVar4 = ((((uVar4 / 0x14c0) * 100) % 100) / 0x19) * 0x40 + uVar4 / 0x14c0;
 61091:   uVar2 = (undefined1)uVar4;
 61092:   if (0xff < uVar4) {
 61093:     uVar2 = 0xff;
 61094:   }
```

### W0138 - `FUN_14003e090` line 61095
- Register: `0x47 `
- Kind: `direct_write`
- Value: `uVar2`
```c
 61090:   uVar4 = ((((uVar4 / 0x14c0) * 100) % 100) / 0x19) * 0x40 + uVar4 / 0x14c0;
 61091:   uVar2 = (undefined1)uVar4;
 61092:   if (0xff < uVar4) {
 61093:     uVar2 = 0xff;
 61094:   }
 61095:   FUN_140044528(param_1,0x47,uVar2);
 61096:   FUN_1400444d0(param_1,0xf,7,0);
 61097:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61098:     uVar2 = FUN_140044484(param_1,0x91);
 61099:     DbgPrint("read 0x91=0x%02X, \n",uVar2);
 61100:   }
```

### W0139 - `FUN_14003e090` line 61096
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61091:   uVar2 = (undefined1)uVar4;
 61092:   if (0xff < uVar4) {
 61093:     uVar2 = 0xff;
 61094:   }
 61095:   FUN_140044528(param_1,0x47,uVar2);
 61096:   FUN_1400444d0(param_1,0xf,7,0);
 61097:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61098:     uVar2 = FUN_140044484(param_1,0x91);
 61099:     DbgPrint("read 0x91=0x%02X, \n",uVar2);
 61100:   }
 61101:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0140 - `FUN_14003e4b8` line 61140
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61135:   byte bVar7;
 61136:   byte bVar8;
 61137:   uint uVar9;
 61138:   undefined1 uVar10;
 61139: 
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
```

### W0141 - `FUN_14003e4b8` line 61141
- Register: `0xF8 `
- Kind: `direct_write`
- Value: `0xc3`
```c
 61136:   byte bVar8;
 61137:   uint uVar9;
 61138:   undefined1 uVar10;
 61139: 
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
```

### W0142 - `FUN_14003e4b8` line 61142
- Register: `0xF8 `
- Kind: `direct_write`
- Value: `0xa5`
```c
 61137:   uint uVar9;
 61138:   undefined1 uVar10;
 61139: 
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
```

### W0143 - `FUN_14003e4b8` line 61143
- Register: `0x34 `
- Kind: `direct_write`
- Value: `0`
```c
 61138:   undefined1 uVar10;
 61139: 
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
```

### W0144 - `FUN_14003e4b8` line 61144
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61139: 
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
```

### W0145 - `FUN_14003e4b8` line 61145
- Register: `0x5F `
- Kind: `direct_write`
- Value: `4`
```c
 61140:   FUN_1400444d0(param_1,0xf,7,0);
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
```

### W0146 - `FUN_14003e4b8` line 61146
- Register: `0x5F `
- Kind: `direct_write`
- Value: `5`
```c
 61141:   FUN_140044528(param_1,0xf8,0xc3);
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
```

### W0147 - `FUN_14003e4b8` line 61147
- Register: `0x58 `
- Kind: `direct_write`
- Value: `0x12`
```c
 61142:   FUN_140044528(param_1,0xf8,0xa5);
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
```

### W0148 - `FUN_14003e4b8` line 61148
- Register: `0x58 `
- Kind: `direct_write`
- Value: `2`
```c
 61143:   FUN_140044528(param_1,0x34,0);
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
```

### W0149 - `FUN_14003e4b8` line 61152
- Register: `0xF8 `
- Kind: `direct_write`
- Value: `0xc3`
```c
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
```

### W0150 - `FUN_14003e4b8` line 61153
- Register: `0xF8 `
- Kind: `direct_write`
- Value: `0xa5`
```c
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
 61158:     do {
```

### W0151 - `FUN_14003e4b8` line 61154
- Register: `0x5F `
- Kind: `direct_write`
- Value: `4`
```c
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
 61158:     do {
 61159:       cVar2 = FUN_140044484(param_1,0x60);
```

### W0152 - `FUN_14003e4b8` line 61155
- Register: `0x58 `
- Kind: `direct_write`
- Value: `0x12`
```c
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
 61158:     do {
 61159:       cVar2 = FUN_140044484(param_1,0x60);
 61160:       FUN_140048644(param_1,1);
```

### W0153 - `FUN_14003e4b8` line 61156
- Register: `0x58 `
- Kind: `direct_write`
- Value: `2`
```c
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
 61158:     do {
 61159:       cVar2 = FUN_140044484(param_1,0x60);
 61160:       FUN_140048644(param_1,1);
 61161:       cVar1 = cVar1 + -1;
```

### W0154 - `FUN_14003e4b8` line 61165
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61160:       FUN_140048644(param_1,1);
 61161:       cVar1 = cVar1 + -1;
 61162:       if (cVar2 == '\x19') break;
 61163:     } while (cVar1 != '\0');
 61164:     FUN_140048644(param_1,10);
 61165:     FUN_1400444d0(param_1,0xf,7,0);
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
```

### W0155 - `FUN_14003e4b8` line 61166
- Register: `0xCF `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 61161:       cVar1 = cVar1 + -1;
 61162:       if (cVar2 == '\x19') break;
 61163:     } while (cVar1 != '\0');
 61164:     FUN_140048644(param_1,10);
 61165:     FUN_1400444d0(param_1,0xf,7,0);
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
```

### W0156 - `FUN_14003e4b8` line 61167
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61162:       if (cVar2 == '\x19') break;
 61163:     } while (cVar1 != '\0');
 61164:     FUN_140048644(param_1,10);
 61165:     FUN_1400444d0(param_1,0xf,7,0);
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
```

### W0157 - `FUN_14003e4b8` line 61169
- Register: `0x57 `
- Kind: `direct_write`
- Value: `1`
```c
 61164:     FUN_140048644(param_1,10);
 61165:     FUN_1400444d0(param_1,0xf,7,0);
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
```

### W0158 - `FUN_14003e4b8` line 61170
- Register: `0x50 `
- Kind: `direct_write`
- Value: `0`
```c
 61165:     FUN_1400444d0(param_1,0xf,7,0);
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
```

### W0159 - `FUN_14003e4b8` line 61171
- Register: `0x51 `
- Kind: `direct_write`
- Value: `0`
```c
 61166:     FUN_1400444d0(param_1,0xcf,1,1);
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
```

### W0160 - `FUN_14003e4b8` line 61172
- Register: `0x54 `
- Kind: `direct_write`
- Value: `4`
```c
 61167:     FUN_1400444d0(param_1,0xf,7,1);
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
```

### W0161 - `FUN_14003e4b8` line 61175
- Register: `0x50 `
- Kind: `direct_write`
- Value: `0`
```c
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
```

### W0162 - `FUN_14003e4b8` line 61176
- Register: `0x51 `
- Kind: `direct_write`
- Value: `1`
```c
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
```

### W0163 - `FUN_14003e4b8` line 61177
- Register: `0x54 `
- Kind: `direct_write`
- Value: `4`
```c
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
```

### W0164 - `FUN_14003e4b8` line 61183
- Register: `0x50 `
- Kind: `direct_write`
- Value: `uVar10`
```c
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
```

### W0165 - `FUN_14003e4b8` line 61184
- Register: `0x51 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
```

### W0166 - `FUN_14003e4b8` line 61185
- Register: `0x54 `
- Kind: `direct_write`
- Value: `4`
```c
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
```

### W0167 - `FUN_14003e4b8` line 61188
- Register: `0x50 `
- Kind: `direct_write`
- Value: `uVar10`
```c
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
 61193:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0168 - `FUN_14003e4b8` line 61189
- Register: `0x51 `
- Kind: `direct_write`
- Value: `0xb1`
```c
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
 61193:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61194:     DbgPrint("read 0x61=0x%02X, \n",bVar5);
```

### W0169 - `FUN_14003e4b8` line 61190
- Register: `0x54 `
- Kind: `direct_write`
- Value: `4`
```c
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
 61193:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61194:     DbgPrint("read 0x61=0x%02X, \n",bVar5);
 61195:   }
```

### W0170 - `FUN_14003e4b8` line 61220
- Register: `0x5F `
- Kind: `direct_write`
- Value: `0`
```c
 61215:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61216:       DbgPrint(" Readback value invalid: 0x%08lX, use typical value instead.\n",uVar9);
 61217:     }
 61218:     uVar9 = 38000;
 61219:   }
 61220:   FUN_140044528(param_1,0x5f,0);
 61221:   FUN_1400444d0(param_1,0xf,7,0);
 61222:   FUN_140044528(param_1,0xf8,0);
 61223:   return uVar9;
 61224: }
 61225: 
```

### W0171 - `FUN_14003e4b8` line 61221
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61216:       DbgPrint(" Readback value invalid: 0x%08lX, use typical value instead.\n",uVar9);
 61217:     }
 61218:     uVar9 = 38000;
 61219:   }
 61220:   FUN_140044528(param_1,0x5f,0);
 61221:   FUN_1400444d0(param_1,0xf,7,0);
 61222:   FUN_140044528(param_1,0xf8,0);
 61223:   return uVar9;
 61224: }
 61225: 
 61226: 
```

### W0172 - `FUN_14003e4b8` line 61222
- Register: `0xF8 `
- Kind: `direct_write`
- Value: `0`
```c
 61217:     }
 61218:     uVar9 = 38000;
 61219:   }
 61220:   FUN_140044528(param_1,0x5f,0);
 61221:   FUN_1400444d0(param_1,0xf,7,0);
 61222:   FUN_140044528(param_1,0xf8,0);
 61223:   return uVar9;
 61224: }
 61225: 
 61226: 
 61227: 
```

### W0173 - `FUN_14003e83c` line 61247
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61242:   uVar2 = 0;
 61243:   if (param_2 == '\0') {
 61244:     FUN_14003f500(param_1,0,0);
 61245:     uVar4 = 0;
 61246:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61247:     FUN_1400444d0(param_1,0xf,7,0);
 61248:     FUN_140044528(param_1,8,4);
 61249:     FUN_140044528(param_1,0x22,0x12);
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
```

### W0174 - `FUN_14003e83c` line 61248
- Register: `0x08 `
- Kind: `direct_write`
- Value: `4`
```c
 61243:   if (param_2 == '\0') {
 61244:     FUN_14003f500(param_1,0,0);
 61245:     uVar4 = 0;
 61246:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61247:     FUN_1400444d0(param_1,0xf,7,0);
 61248:     FUN_140044528(param_1,8,4);
 61249:     FUN_140044528(param_1,0x22,0x12);
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
```

### W0175 - `FUN_14003e83c` line 61249
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x12`
```c
 61244:     FUN_14003f500(param_1,0,0);
 61245:     uVar4 = 0;
 61246:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61247:     FUN_1400444d0(param_1,0xf,7,0);
 61248:     FUN_140044528(param_1,8,4);
 61249:     FUN_140044528(param_1,0x22,0x12);
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
 61254:     uVar2 = CONCAT71(uVar3,0xfd);
```

### W0176 - `FUN_14003e83c` line 61250
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x10`
```c
 61245:     uVar4 = 0;
 61246:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61247:     FUN_1400444d0(param_1,0xf,7,0);
 61248:     FUN_140044528(param_1,8,4);
 61249:     FUN_140044528(param_1,0x22,0x12);
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
 61254:     uVar2 = CONCAT71(uVar3,0xfd);
 61255:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
```

### W0177 - `FUN_14003e83c` line 61252
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xac`
```c
 61247:     FUN_1400444d0(param_1,0xf,7,0);
 61248:     FUN_140044528(param_1,8,4);
 61249:     FUN_140044528(param_1,0x22,0x12);
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
 61254:     uVar2 = CONCAT71(uVar3,0xfd);
 61255:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 61256:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61257:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
```

### W0178 - `FUN_14003e83c` line 61255
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xa0`
```c
 61250:     FUN_140044528(param_1,0x22,0x10);
 61251:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61252:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
 61254:     uVar2 = CONCAT71(uVar3,0xfd);
 61255:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 61256:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61257:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61258:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61259:       FUN_140048644(param_1,1);
 61260:       uVar5 = 0;
```

### W0179 - `FUN_14003e83c` line 61258
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 61253:     uVar5 = CONCAT71(uVar6,0xa0);
 61254:     uVar2 = CONCAT71(uVar3,0xfd);
 61255:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 61256:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61257:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61258:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61259:       FUN_140048644(param_1,1);
 61260:       uVar5 = 0;
 61261:       uVar2 = CONCAT71(uVar3,0x10);
 61262:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61263:     }
```

### W0180 - `FUN_14003e83c` line 61262
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 61257:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61258:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61259:       FUN_140048644(param_1,1);
 61260:       uVar5 = 0;
 61261:       uVar2 = CONCAT71(uVar3,0x10);
 61262:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61263:     }
 61264:   }
 61265:   else {
 61266:     FUN_14003f500(param_1,1,0);
 61267:     uVar4 = 0;
```

### W0181 - `FUN_14003e83c` line 61269
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61264:   }
 61265:   else {
 61266:     FUN_14003f500(param_1,1,0);
 61267:     uVar4 = 0;
 61268:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61269:     FUN_1400444d0(param_1,0xf,7,0);
 61270:     FUN_140044528(param_1,0xd,4);
 61271:     FUN_140044528(param_1,0x22,0x12);
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
```

### W0182 - `FUN_14003e83c` line 61270
- Register: `0x0D `
- Kind: `direct_write`
- Value: `4`
```c
 61265:   else {
 61266:     FUN_14003f500(param_1,1,0);
 61267:     uVar4 = 0;
 61268:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61269:     FUN_1400444d0(param_1,0xf,7,0);
 61270:     FUN_140044528(param_1,0xd,4);
 61271:     FUN_140044528(param_1,0x22,0x12);
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
```

### W0183 - `FUN_14003e83c` line 61271
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x12`
```c
 61266:     FUN_14003f500(param_1,1,0);
 61267:     uVar4 = 0;
 61268:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61269:     FUN_1400444d0(param_1,0xf,7,0);
 61270:     FUN_140044528(param_1,0xd,4);
 61271:     FUN_140044528(param_1,0x22,0x12);
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
```

### W0184 - `FUN_14003e83c` line 61272
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x10`
```c
 61267:     uVar4 = 0;
 61268:     uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61269:     FUN_1400444d0(param_1,0xf,7,0);
 61270:     FUN_140044528(param_1,0xd,4);
 61271:     FUN_140044528(param_1,0x22,0x12);
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
 61277:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
```

### W0185 - `FUN_14003e83c` line 61274
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xac`
```c
 61269:     FUN_1400444d0(param_1,0xf,7,0);
 61270:     FUN_140044528(param_1,0xd,4);
 61271:     FUN_140044528(param_1,0x22,0x12);
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
 61277:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 61278:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61279:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
```

### W0186 - `FUN_14003e83c` line 61277
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xa0`
```c
 61272:     FUN_140044528(param_1,0x22,0x10);
 61273:     uVar6 = (undefined7)((ulonglong)uVar4 >> 8);
 61274:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
 61277:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 61278:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61279:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61280:       FUN_1400444d0(param_1,0xf,7,4);
 61281:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61282:       FUN_140048644(param_1,1);
```

### W0187 - `FUN_14003e83c` line 61280
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 61275:     uVar5 = CONCAT71(uVar6,0xa0);
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
 61277:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 61278:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61279:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61280:       FUN_1400444d0(param_1,0xf,7,4);
 61281:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61282:       FUN_140048644(param_1,1);
 61283:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61284:       uVar5 = 0;
 61285:       uVar2 = CONCAT71(uVar3,7);
```

### W0188 - `FUN_14003e83c` line 61281
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 61276:     uVar2 = CONCAT71(uVar3,0xfd);
 61277:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 61278:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61279:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61280:       FUN_1400444d0(param_1,0xf,7,4);
 61281:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61282:       FUN_140048644(param_1,1);
 61283:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61284:       uVar5 = 0;
 61285:       uVar2 = CONCAT71(uVar3,7);
 61286:       FUN_1400444d0(param_1,0xf,7,0);
```

### W0189 - `FUN_14003e83c` line 61283
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 61278:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 61279:       uVar3 = (undefined7)((ulonglong)uVar2 >> 8);
 61280:       FUN_1400444d0(param_1,0xf,7,4);
 61281:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61282:       FUN_140048644(param_1,1);
 61283:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61284:       uVar5 = 0;
 61285:       uVar2 = CONCAT71(uVar3,7);
 61286:       FUN_1400444d0(param_1,0xf,7,0);
 61287:     }
 61288:     bVar1 = 1;
```

### W0190 - `FUN_14003e83c` line 61286
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61281:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 61282:       FUN_140048644(param_1,1);
 61283:       FUN_1400444d0(param_1,0xc5,0x10,0);
 61284:       uVar5 = 0;
 61285:       uVar2 = CONCAT71(uVar3,7);
 61286:       FUN_1400444d0(param_1,0xf,7,0);
 61287:     }
 61288:     bVar1 = 1;
 61289:   }
 61290:   FUN_1400458b4(param_1,bVar1,uVar2,uVar5);
 61291:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0191 - `FUN_14003e83c` line 61291
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61286:       FUN_1400444d0(param_1,0xf,7,0);
 61287:     }
 61288:     bVar1 = 1;
 61289:   }
 61290:   FUN_1400458b4(param_1,bVar1,uVar2,uVar5);
 61291:   FUN_1400444d0(param_1,0xf,7,0);
 61292:   return;
 61293: }
 61294: 
 61295: 
 61296: 
```

### W0192 - `FUN_14003ea04` line 61302
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61297: void FUN_14003ea04(longlong param_1)
 61298: 
 61299: {
 61300:   undefined1 uVar1;
 61301: 
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
```

### W0193 - `FUN_14003ea04` line 61303
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 61298: 
 61299: {
 61300:   undefined1 uVar1;
 61301: 
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
```

### W0194 - `FUN_14003ea04` line 61304
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `0`
```c
 61299: {
 61300:   undefined1 uVar1;
 61301: 
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
```

### W0195 - `FUN_14003ea04` line 61306
- Register: `0x8A `
- Kind: `direct_write`
- Value: `uVar1`
```c
 61301: 
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
 61310:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61311:     DbgPrint("[xAud-chg] 68051 # after reset audio ! 0x8a = 0x%02x #\n",uVar1);
```

### W0196 - `FUN_14003ea04` line 61307
- Register: `0x8A `
- Kind: `direct_write`
- Value: `uVar1`
```c
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
 61310:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61311:     DbgPrint("[xAud-chg] 68051 # after reset audio ! 0x8a = 0x%02x #\n",uVar1);
 61312:   }
```

### W0197 - `FUN_14003ea04` line 61308
- Register: `0x8A `
- Kind: `direct_write`
- Value: `uVar1`
```c
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
 61310:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61311:     DbgPrint("[xAud-chg] 68051 # after reset audio ! 0x8a = 0x%02x #\n",uVar1);
 61312:   }
 61313:   return;
```

### W0198 - `FUN_14003ea04` line 61309
- Register: `0x8A `
- Kind: `direct_write`
- Value: `uVar1`
```c
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
 61310:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61311:     DbgPrint("[xAud-chg] 68051 # after reset audio ! 0x8a = 0x%02x #\n",uVar1);
 61312:   }
 61313:   return;
 61314: }
```

### W0199 - `FUN_14003eaa4` line 61321
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61316: 
 61317: 
 61318: void FUN_14003eaa4(longlong param_1)
 61319: 
 61320: {
 61321:   FUN_1400444d0(param_1,0xf,7,0);
 61322:   FUN_1400444d0(param_1,0x22,1,1);
 61323:   FUN_140048644(param_1,1);
 61324:   FUN_1400444d0(param_1,0x22,1,0);
 61325:   FUN_1400444d0(param_1,0x10,2,2);
 61326:   FUN_1400444d0(param_1,0x12,0x80,0x80);
```

### W0200 - `FUN_14003eaa4` line 61322
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 61317: 
 61318: void FUN_14003eaa4(longlong param_1)
 61319: 
 61320: {
 61321:   FUN_1400444d0(param_1,0xf,7,0);
 61322:   FUN_1400444d0(param_1,0x22,1,1);
 61323:   FUN_140048644(param_1,1);
 61324:   FUN_1400444d0(param_1,0x22,1,0);
 61325:   FUN_1400444d0(param_1,0x10,2,2);
 61326:   FUN_1400444d0(param_1,0x12,0x80,0x80);
 61327:   return;
```

### W0201 - `FUN_14003eaa4` line 61324
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 61319: 
 61320: {
 61321:   FUN_1400444d0(param_1,0xf,7,0);
 61322:   FUN_1400444d0(param_1,0x22,1,1);
 61323:   FUN_140048644(param_1,1);
 61324:   FUN_1400444d0(param_1,0x22,1,0);
 61325:   FUN_1400444d0(param_1,0x10,2,2);
 61326:   FUN_1400444d0(param_1,0x12,0x80,0x80);
 61327:   return;
 61328: }
 61329: 
```

### W0202 - `FUN_14003eaa4` line 61325
- Register: `0x10 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 61320: {
 61321:   FUN_1400444d0(param_1,0xf,7,0);
 61322:   FUN_1400444d0(param_1,0x22,1,1);
 61323:   FUN_140048644(param_1,1);
 61324:   FUN_1400444d0(param_1,0x22,1,0);
 61325:   FUN_1400444d0(param_1,0x10,2,2);
 61326:   FUN_1400444d0(param_1,0x12,0x80,0x80);
 61327:   return;
 61328: }
 61329: 
 61330: 
```

### W0203 - `FUN_14003eaa4` line 61326
- Register: `0x12 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 61321:   FUN_1400444d0(param_1,0xf,7,0);
 61322:   FUN_1400444d0(param_1,0x22,1,1);
 61323:   FUN_140048644(param_1,1);
 61324:   FUN_1400444d0(param_1,0x22,1,0);
 61325:   FUN_1400444d0(param_1,0x10,2,2);
 61326:   FUN_1400444d0(param_1,0x12,0x80,0x80);
 61327:   return;
 61328: }
 61329: 
 61330: 
 61331: 
```

### W0204 - `FUN_14003eb0c` line 61339
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61334: {
 61335:   byte bVar1;
 61336:   byte bVar2;
 61337:   bool bVar3;
 61338: 
 61339:   FUN_1400444d0(param_1,0xf,7,0);
 61340:   bVar1 = FUN_140044484(param_1,0x1b);
 61341:   bVar2 = bVar1 >> 4 & 3;
 61342:   if ((bVar1 & 0x30) != 0) {
 61343:     if (bVar2 == 1) {
 61344:       *(undefined1 *)(param_1 + 0x228) = 2;
```

### W0205 - `FUN_14003eb0c` line 61354
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61349:       goto LAB_14003eb57;
 61350:     }
 61351:   }
 61352:   *(undefined1 *)(param_1 + 0x228) = 1;
 61353: LAB_14003eb57:
 61354:   FUN_1400444d0(param_1,0xf,7,1);
 61355:   FUN_1400444d0(param_1,0xb0,1,24999 < *(uint *)(param_1 + 0x234) / (uint)*(byte *)(param_1 + 0x228)
 61356:                );
 61357:   bVar3 = FUN_14003cec4(param_1);
 61358:   if (!bVar3) {
 61359:     FUN_1400444d0(param_1,0xf,7,1);
```

### W0206 - `FUN_14003eb0c` line 61359
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61354:   FUN_1400444d0(param_1,0xf,7,1);
 61355:   FUN_1400444d0(param_1,0xb0,1,24999 < *(uint *)(param_1 + 0x234) / (uint)*(byte *)(param_1 + 0x228)
 61356:                );
 61357:   bVar3 = FUN_14003cec4(param_1);
 61358:   if (!bVar3) {
 61359:     FUN_1400444d0(param_1,0xf,7,1);
 61360:     FUN_1400444d0(param_1,0xb0,1,0);
 61361:   }
 61362:   FUN_1400444d0(param_1,0xf,7,0);
 61363:   return;
 61364: }
```

### W0207 - `FUN_14003eb0c` line 61360
- Register: `0xB0 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 61355:   FUN_1400444d0(param_1,0xb0,1,24999 < *(uint *)(param_1 + 0x234) / (uint)*(byte *)(param_1 + 0x228)
 61356:                );
 61357:   bVar3 = FUN_14003cec4(param_1);
 61358:   if (!bVar3) {
 61359:     FUN_1400444d0(param_1,0xf,7,1);
 61360:     FUN_1400444d0(param_1,0xb0,1,0);
 61361:   }
 61362:   FUN_1400444d0(param_1,0xf,7,0);
 61363:   return;
 61364: }
 61365: 
```

### W0208 - `FUN_14003eb0c` line 61362
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61357:   bVar3 = FUN_14003cec4(param_1);
 61358:   if (!bVar3) {
 61359:     FUN_1400444d0(param_1,0xf,7,1);
 61360:     FUN_1400444d0(param_1,0xb0,1,0);
 61361:   }
 61362:   FUN_1400444d0(param_1,0xf,7,0);
 61363:   return;
 61364: }
 61365: 
 61366: 
 61367: 
```

### W0209 - `FUN_14003ebd0` line 61377
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61372:   byte bVar2;
 61373:   byte bVar3;
 61374:   undefined1 uVar4;
 61375:   ulonglong uVar5;
 61376: 
 61377:   FUN_1400444d0(param_1,0xf,7,0);
 61378:   bVar2 = FUN_140044484(param_1,0x43);
 61379:   FUN_140044484(param_1,0x47);
 61380:   bVar3 = FUN_140044484(param_1,0x48);
 61381:   if ((char)bVar2 < '\0') {
 61382:     bVar3 = bVar3 >> 3;
```

### W0210 - `FUN_14003ebd0` line 61409
- Register: `0x47 `
- Kind: `direct_write`
- Value: `uVar4`
```c
 61404:   if ((bVar3 < (byte)uVar5) && ((bVar2 & 2) == 0)) {
 61405:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61406:       DbgPrint("OverWrite IPLL/OPLL \n");
 61407:     }
 61408:     uVar4 = FUN_140044484(param_1,0x47);
 61409:     FUN_140044528(param_1,0x47,uVar4);
 61410:   }
 61411:   return;
 61412: }
 61413: 
 61414: 
```

### W0211 - `FUN_14003ecf4` line 61423
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61418: {
 61419:   byte bVar1;
 61420:   bool bVar2;
 61421: 
 61422:   if (param_2 == '\x01') {
 61423:     FUN_1400444d0(param_1,0xf,7,0);
 61424:     FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61425:     FUN_1400444d0(param_1,0xf,7,1);
 61426:     FUN_1400444d0(param_1,0xc5,0x80,0x80);
 61427:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61428:       DbgPrint("+++++++++++ iTE6805_Set_AVMute -> On\n");
```

### W0212 - `FUN_14003ecf4` line 61424
- Register: `0x4F `
- Kind: `rmw_write`
- Mask: `0xa0`
- Value: `0xa0`
```c
 61419:   byte bVar1;
 61420:   bool bVar2;
 61421: 
 61422:   if (param_2 == '\x01') {
 61423:     FUN_1400444d0(param_1,0xf,7,0);
 61424:     FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61425:     FUN_1400444d0(param_1,0xf,7,1);
 61426:     FUN_1400444d0(param_1,0xc5,0x80,0x80);
 61427:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61428:       DbgPrint("+++++++++++ iTE6805_Set_AVMute -> On\n");
 61429:       return;
```

### W0213 - `FUN_14003ecf4` line 61425
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61420:   bool bVar2;
 61421: 
 61422:   if (param_2 == '\x01') {
 61423:     FUN_1400444d0(param_1,0xf,7,0);
 61424:     FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61425:     FUN_1400444d0(param_1,0xf,7,1);
 61426:     FUN_1400444d0(param_1,0xc5,0x80,0x80);
 61427:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61428:       DbgPrint("+++++++++++ iTE6805_Set_AVMute -> On\n");
 61429:       return;
 61430:     }
```

### W0214 - `FUN_14003ecf4` line 61426
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 61421: 
 61422:   if (param_2 == '\x01') {
 61423:     FUN_1400444d0(param_1,0xf,7,0);
 61424:     FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61425:     FUN_1400444d0(param_1,0xf,7,1);
 61426:     FUN_1400444d0(param_1,0xc5,0x80,0x80);
 61427:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61428:       DbgPrint("+++++++++++ iTE6805_Set_AVMute -> On\n");
 61429:       return;
 61430:     }
 61431:   }
```

### W0215 - `FUN_14003ecf4` line 61439
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61434:       DbgPrint("+++++++++++  iTE6805_Set_AVMute -> Off\n");
 61435:     }
 61436:     if (*(int *)(param_1 + 0x1dc) == 4) {
 61437:       bVar1 = FUN_14003ccf4(param_1);
 61438:       if (bVar1 == 0) {
 61439:         FUN_1400444d0(param_1,0xf,7,1);
 61440:         FUN_1400444d0(param_1,0xc5,1,1);
 61441:         FUN_1400444d0(param_1,0xc5,1,0);
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
```

### W0216 - `FUN_14003ecf4` line 61440
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 61435:     }
 61436:     if (*(int *)(param_1 + 0x1dc) == 4) {
 61437:       bVar1 = FUN_14003ccf4(param_1);
 61438:       if (bVar1 == 0) {
 61439:         FUN_1400444d0(param_1,0xf,7,1);
 61440:         FUN_1400444d0(param_1,0xc5,1,1);
 61441:         FUN_1400444d0(param_1,0xc5,1,0);
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
```

### W0217 - `FUN_14003ecf4` line 61441
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 61436:     if (*(int *)(param_1 + 0x1dc) == 4) {
 61437:       bVar1 = FUN_14003ccf4(param_1);
 61438:       if (bVar1 == 0) {
 61439:         FUN_1400444d0(param_1,0xf,7,1);
 61440:         FUN_1400444d0(param_1,0xc5,1,1);
 61441:         FUN_1400444d0(param_1,0xc5,1,0);
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
 61446:         }
```

### W0218 - `FUN_14003ecf4` line 61444
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61439:         FUN_1400444d0(param_1,0xf,7,1);
 61440:         FUN_1400444d0(param_1,0xc5,1,1);
 61441:         FUN_1400444d0(param_1,0xc5,1,0);
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
 61446:         }
 61447:         FUN_1400444d0(param_1,0xf,7,0);
 61448:         FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61449:         bVar1 = 0xa0;
```

### W0219 - `FUN_14003ecf4` line 61445
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 61440:         FUN_1400444d0(param_1,0xc5,1,1);
 61441:         FUN_1400444d0(param_1,0xc5,1,0);
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
 61446:         }
 61447:         FUN_1400444d0(param_1,0xf,7,0);
 61448:         FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61449:         bVar1 = 0xa0;
 61450:       }
```

### W0220 - `FUN_14003ecf4` line 61447
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61442:         bVar2 = FUN_14003cec4(param_1);
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
 61446:         }
 61447:         FUN_1400444d0(param_1,0xf,7,0);
 61448:         FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61449:         bVar1 = 0xa0;
 61450:       }
 61451:       else {
 61452:         FUN_1400444d0(param_1,0xf,7,0);
```

### W0221 - `FUN_14003ecf4` line 61448
- Register: `0x4F `
- Kind: `rmw_write`
- Mask: `0xa0`
- Value: `0xa0`
```c
 61443:         if (bVar2) {
 61444:           FUN_1400444d0(param_1,0xf,7,1);
 61445:           FUN_1400444d0(param_1,0xc5,1,1);
 61446:         }
 61447:         FUN_1400444d0(param_1,0xf,7,0);
 61448:         FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61449:         bVar1 = 0xa0;
 61450:       }
 61451:       else {
 61452:         FUN_1400444d0(param_1,0xf,7,0);
 61453:         bVar1 = 0x80;
```

### W0222 - `FUN_14003ecf4` line 61452
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61447:         FUN_1400444d0(param_1,0xf,7,0);
 61448:         FUN_1400444d0(param_1,0x4f,0xa0,0xa0);
 61449:         bVar1 = 0xa0;
 61450:       }
 61451:       else {
 61452:         FUN_1400444d0(param_1,0xf,7,0);
 61453:         bVar1 = 0x80;
 61454:       }
 61455:       FUN_1400444d0(param_1,0x4f,bVar1,0x80);
 61456:     }
 61457:   }
```

### W0223 - `FUN_14003ecf4` line 61455
- Register: `0x4F `
- Kind: `rmw_write`
- Mask: `bVar1`
- Value: `0x80`
```c
 61450:       }
 61451:       else {
 61452:         FUN_1400444d0(param_1,0xf,7,0);
 61453:         bVar1 = 0x80;
 61454:       }
 61455:       FUN_1400444d0(param_1,0x4f,bVar1,0x80);
 61456:     }
 61457:   }
 61458:   return;
 61459: }
 61460: 
```

### W0224 - `FUN_14003ee30` line 61466
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61461: 
 61462: 
 61463: void FUN_14003ee30(longlong param_1,char param_2)
 61464: 
 61465: {
 61466:   FUN_1400444d0(param_1,0xf,7,1);
 61467:   FUN_140044528(param_1,199,-(param_2 != '\0') & 0x7f);
 61468:   FUN_1400444d0(param_1,0xf,7,0);
 61469:   return;
 61470: }
 61471: 
```

### W0225 - `FUN_14003ee30` line 61467
- Register: `0xC7 `
- Kind: `direct_write`
- Value: `-(param_2 != '\0') & 0x7f`
```c
 61462: 
 61463: void FUN_14003ee30(longlong param_1,char param_2)
 61464: 
 61465: {
 61466:   FUN_1400444d0(param_1,0xf,7,1);
 61467:   FUN_140044528(param_1,199,-(param_2 != '\0') & 0x7f);
 61468:   FUN_1400444d0(param_1,0xf,7,0);
 61469:   return;
 61470: }
 61471: 
 61472: 
```

### W0226 - `FUN_14003ee30` line 61468
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61463: void FUN_14003ee30(longlong param_1,char param_2)
 61464: 
 61465: {
 61466:   FUN_1400444d0(param_1,0xf,7,1);
 61467:   FUN_140044528(param_1,199,-(param_2 != '\0') & 0x7f);
 61468:   FUN_1400444d0(param_1,0xf,7,0);
 61469:   return;
 61470: }
 61471: 
 61472: 
 61473: 
```

### W0227 - `FUN_14003ee7c` line 61480
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61475: 
 61476: {
 61477:   ushort uVar1;
 61478:   byte bVar2;
 61479: 
 61480:   FUN_1400444d0(param_1,0xf,7,0);
 61481:   bVar2 = FUN_140044484(param_1,0x98);
 61482:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar2 & 0xf0);
 61483:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61484:     DbgPrint("\n Input ColorDepth = ");
 61485:   }
```

### W0228 - `FUN_14003ee7c` line 61501
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `3`
- Value: `0`
```c
 61496:     }
 61497:     bVar2 = 1;
 61498:   }
 61499:   else {
 61500:     if (uVar1 != 0x60) {
 61501:       FUN_1400444d0(param_1,0x6b,3,0);
 61502:       if ((DAT_1400a04f8 & 0x10) == 0) {
 61503:         return;
 61504:       }
 61505:       DbgPrint("Force set Output ColorDepth = 8b (vfmt-S)\n");
 61506:       return;
```

### W0229 - `FUN_14003ee7c` line 61513
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `3`
- Value: `bVar2`
```c
 61508:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61509:       DbgPrint("Force set Output ColorDepth = 12b (vfmt-S)\n");
 61510:     }
 61511:     bVar2 = 2;
 61512:   }
 61513:   FUN_1400444d0(param_1,0x6b,3,bVar2);
 61514:   return;
 61515: }
 61516: 
 61517: 
 61518: 
```

### W0230 - `FUN_14003ef68` line 61544
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61539:   bool bVar18;
 61540: 
 61541:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61542:     DbgPrint("---- Video DownScale Start! ----\n");
 61543:   }
 61544:   FUN_1400444d0(param_1,0xf,7,0);
 61545:   FUN_1400444d0(param_1,0x33,8,8);
 61546:   uVar2 = *(undefined2 *)(param_1 + 0x22e);
 61547:   bVar18 = *(int *)(param_1 + 0x20c) == 3;
 61548:   uVar15 = *(short *)(param_1 + 0x22c) << bVar18;
 61549:   FUN_140044484(param_1,0x98);
```

### W0231 - `FUN_14003ef68` line 61545
- Register: `0x33 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 61540: 
 61541:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61542:     DbgPrint("---- Video DownScale Start! ----\n");
 61543:   }
 61544:   FUN_1400444d0(param_1,0xf,7,0);
 61545:   FUN_1400444d0(param_1,0x33,8,8);
 61546:   uVar2 = *(undefined2 *)(param_1 + 0x22e);
 61547:   bVar18 = *(int *)(param_1 + 0x20c) == 3;
 61548:   uVar15 = *(short *)(param_1 + 0x22c) << bVar18;
 61549:   FUN_140044484(param_1,0x98);
 61550:   cVar1 = (char)*(undefined2 *)(param_1 + 0x250);
```

### W0232 - `FUN_14003ef68` line 61591
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `5`
```c
 61586:   }
 61587:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61588:     DbgPrint("Target VActive= %d , VFrontPorch=%d, VSync Width=%d, VBackPorch=%d \n",uVar10,uVar7,
 61589:              uVar8,uVar9);
 61590:   }
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
```

### W0233 - `FUN_14003ef68` line 61592
- Register: `0x21 `
- Kind: `direct_write`
- Value: `(char)uVar15`
```c
 61587:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61588:     DbgPrint("Target VActive= %d , VFrontPorch=%d, VSync Width=%d, VBackPorch=%d \n",uVar10,uVar7,
 61589:              uVar8,uVar9);
 61590:   }
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
```

### W0234 - `FUN_14003ef68` line 61593
- Register: `0x22 `
- Kind: `direct_write`
- Value: `(char)(uVar15 >> 8)`
```c
 61588:     DbgPrint("Target VActive= %d , VFrontPorch=%d, VSync Width=%d, VBackPorch=%d \n",uVar10,uVar7,
 61589:              uVar8,uVar9);
 61590:   }
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
```

### W0235 - `FUN_14003ef68` line 61594
- Register: `0x23 `
- Kind: `direct_write`
- Value: `(char)uVar2`
```c
 61589:              uVar8,uVar9);
 61590:   }
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
```

### W0236 - `FUN_14003ef68` line 61595
- Register: `0x24 `
- Kind: `direct_write`
- Value: `(char)((ushort)uVar2 >> 8)`
```c
 61590:   }
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
```

### W0237 - `FUN_14003ef68` line 61596
- Register: `0x25 `
- Kind: `direct_write`
- Value: `(char)(uVar15 - 2)`
```c
 61591:   FUN_1400444d0(param_1,0xf,7,5);
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
```

### W0238 - `FUN_14003ef68` line 61597
- Register: `0x26 `
- Kind: `direct_write`
- Value: `(char)((ushort)(uVar15 - 2) >> 8)`
```c
 61592:   FUN_140044528(param_1,0x21,(char)uVar15);
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
```

### W0239 - `FUN_14003ef68` line 61598
- Register: `0x27 `
- Kind: `direct_write`
- Value: `(char)sVar14`
```c
 61593:   FUN_140044528(param_1,0x22,(char)(uVar15 >> 8));
 61594:   FUN_140044528(param_1,0x23,(char)uVar2);
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
```

### W0240 - `FUN_14003ef68` line 61600
- Register: `0x28 `
- Kind: `direct_write`
- Value: `uVar11`
```c
 61595:   FUN_140044528(param_1,0x24,(char)((ushort)uVar2 >> 8));
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
```

### W0241 - `FUN_14003ef68` line 61601
- Register: `0x29 `
- Kind: `direct_write`
- Value: `(char)sVar14`
```c
 61596:   FUN_140044528(param_1,0x25,(char)(uVar15 - 2));
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
```

### W0242 - `FUN_14003ef68` line 61602
- Register: `0x2A `
- Kind: `direct_write`
- Value: `uVar11`
```c
 61597:   FUN_140044528(param_1,0x26,(char)((ushort)(uVar15 - 2) >> 8));
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
```

### W0243 - `FUN_14003ef68` line 61603
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xff`
```c
 61598:   FUN_140044528(param_1,0x27,(char)sVar14);
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
```

### W0244 - `FUN_14003ef68` line 61604
- Register: `0x2C `
- Kind: `direct_write`
- Value: `0`
```c
 61599:   uVar11 = (undefined1)((ushort)sVar14 >> 8);
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
```

### W0245 - `FUN_14003ef68` line 61605
- Register: `0x2D `
- Kind: `direct_write`
- Value: `0xf`
```c
 61600:   FUN_140044528(param_1,0x28,uVar11);
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
```

### W0246 - `FUN_14003ef68` line 61606
- Register: `0x2E `
- Kind: `direct_write`
- Value: `0xff`
```c
 61601:   FUN_140044528(param_1,0x29,(char)sVar14);
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
```

### W0247 - `FUN_14003ef68` line 61607
- Register: `0x2F `
- Kind: `direct_write`
- Value: `0`
```c
 61602:   FUN_140044528(param_1,0x2a,uVar11);
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
```

### W0248 - `FUN_14003ef68` line 61608
- Register: `0x30 `
- Kind: `direct_write`
- Value: `0xf`
```c
 61603:   FUN_140044528(param_1,0x2b,0xff);
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
```

### W0249 - `FUN_14003ef68` line 61609
- Register: `0x31 `
- Kind: `direct_write`
- Value: `(cVar1 + '\b') * '\x03'`
```c
 61604:   FUN_140044528(param_1,0x2c,0);
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
```

### W0250 - `FUN_14003ef68` line 61610
- Register: `0x32 `
- Kind: `direct_write`
- Value: `(char)sVar13`
```c
 61605:   FUN_140044528(param_1,0x2d,0xf);
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
```

### W0251 - `FUN_14003ef68` line 61611
- Register: `0x33 `
- Kind: `direct_write`
- Value: `(byte)((ushort)sVar13 >> 8) & 0x3f`
```c
 61606:   FUN_140044528(param_1,0x2e,0xff);
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
```

### W0252 - `FUN_14003ef68` line 61612
- Register: `0x34 `
- Kind: `direct_write`
- Value: `(char)uVar16`
```c
 61607:   FUN_140044528(param_1,0x2f,0);
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
```

### W0253 - `FUN_14003ef68` line 61613
- Register: `0x35 `
- Kind: `direct_write`
- Value: `(byte)(uVar16 >> 8) & 0x3f`
```c
 61608:   FUN_140044528(param_1,0x30,0xf);
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
```

### W0254 - `FUN_14003ef68` line 61614
- Register: `0x36 `
- Kind: `direct_write`
- Value: `(char)uVar17`
```c
 61609:   FUN_140044528(param_1,0x31,(cVar1 + '\b') * '\x03');
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
```

### W0255 - `FUN_14003ef68` line 61615
- Register: `0x37 `
- Kind: `direct_write`
- Value: `(byte)(uVar17 >> 8) & 0x3f`
```c
 61610:   FUN_140044528(param_1,0x32,(char)sVar13);
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
```

### W0256 - `FUN_14003ef68` line 61616
- Register: `0x38 `
- Kind: `direct_write`
- Value: `(char)sVar12`
```c
 61611:   FUN_140044528(param_1,0x33,(byte)((ushort)sVar13 >> 8) & 0x3f);
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
```

### W0257 - `FUN_14003ef68` line 61617
- Register: `0x39 `
- Kind: `direct_write`
- Value: `(byte)((ushort)sVar12 >> 8) & 0x3f`
```c
 61612:   FUN_140044528(param_1,0x34,(char)uVar16);
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
```

### W0258 - `FUN_14003ef68` line 61618
- Register: `0x3A `
- Kind: `direct_write`
- Value: `(char)uVar7`
```c
 61613:   FUN_140044528(param_1,0x35,(byte)(uVar16 >> 8) & 0x3f);
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
```

### W0259 - `FUN_14003ef68` line 61619
- Register: `0x3B `
- Kind: `direct_write`
- Value: `(byte)(uVar3 >> 9) & 0x3f`
```c
 61614:   FUN_140044528(param_1,0x36,(char)uVar17);
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
```

### W0260 - `FUN_14003ef68` line 61620
- Register: `0x3C `
- Kind: `direct_write`
- Value: `(char)uVar8`
```c
 61615:   FUN_140044528(param_1,0x37,(byte)(uVar17 >> 8) & 0x3f);
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
```

### W0261 - `FUN_14003ef68` line 61621
- Register: `0x3D `
- Kind: `direct_write`
- Value: `(byte)(uVar4 >> 9) & 0x3f`
```c
 61616:   FUN_140044528(param_1,0x38,(char)sVar12);
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
```

### W0262 - `FUN_14003ef68` line 61622
- Register: `0x3E `
- Kind: `direct_write`
- Value: `(char)uVar9`
```c
 61617:   FUN_140044528(param_1,0x39,(byte)((ushort)sVar12 >> 8) & 0x3f);
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
 61627:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0263 - `FUN_14003ef68` line 61623
- Register: `0x3F `
- Kind: `direct_write`
- Value: `(byte)(uVar5 >> 9) & 0x3f`
```c
 61618:   FUN_140044528(param_1,0x3a,(char)uVar7);
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
 61627:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61628:     DbgPrint("---- Video DownScale Enable! Video DownScale Enable! ----\n");
```

### W0264 - `FUN_14003ef68` line 61624
- Register: `0x40 `
- Kind: `direct_write`
- Value: `(char)uVar10`
```c
 61619:   FUN_140044528(param_1,0x3b,(byte)(uVar3 >> 9) & 0x3f);
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
 61627:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61628:     DbgPrint("---- Video DownScale Enable! Video DownScale Enable! ----\n");
 61629:   }
```

### W0265 - `FUN_14003ef68` line 61625
- Register: `0x41 `
- Kind: `direct_write`
- Value: `(byte)(uVar6 >> 9) & 0x3f`
```c
 61620:   FUN_140044528(param_1,0x3c,(char)uVar8);
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
 61627:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61628:     DbgPrint("---- Video DownScale Enable! Video DownScale Enable! ----\n");
 61629:   }
 61630:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0266 - `FUN_14003ef68` line 61626
- Register: `0x20 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 61621:   FUN_140044528(param_1,0x3d,(byte)(uVar4 >> 9) & 0x3f);
 61622:   FUN_140044528(param_1,0x3e,(char)uVar9);
 61623:   FUN_140044528(param_1,0x3f,(byte)(uVar5 >> 9) & 0x3f);
 61624:   FUN_140044528(param_1,0x40,(char)uVar10);
 61625:   FUN_140044528(param_1,0x41,(byte)(uVar6 >> 9) & 0x3f);
 61626:   FUN_1400444d0(param_1,0x20,0x40,0);
 61627:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61628:     DbgPrint("---- Video DownScale Enable! Video DownScale Enable! ----\n");
 61629:   }
 61630:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61631:     DbgPrint("---- Video DownScale End! ----\n");
```

### W0267 - `FUN_14003ef68` line 61633
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61628:     DbgPrint("---- Video DownScale Enable! Video DownScale Enable! ----\n");
 61629:   }
 61630:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61631:     DbgPrint("---- Video DownScale End! ----\n");
 61632:   }
 61633:   FUN_1400444d0(param_1,0xf,7,0);
 61634:   FUN_1400444d0(param_1,0x33,8,0);
 61635:   return;
 61636: }
 61637: 
 61638: 
```

### W0268 - `FUN_14003ef68` line 61634
- Register: `0x33 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `0`
```c
 61629:   }
 61630:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61631:     DbgPrint("---- Video DownScale End! ----\n");
 61632:   }
 61633:   FUN_1400444d0(param_1,0xf,7,0);
 61634:   FUN_1400444d0(param_1,0x33,8,0);
 61635:   return;
 61636: }
 61637: 
 61638: 
 61639: 
```

### W0269 - `FUN_14003f424` line 61647
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61642: {
 61643:   byte bVar1;
 61644: 
 61645:   if (param_2 == '\x01') {
 61646:     if (*(int *)(param_1 + 0x3ec) == 2) {
 61647:       FUN_1400444d0(param_1,0xf,7,0);
 61648:       bVar1 = 1;
 61649:     }
 61650:     else {
 61651:       if (*(int *)(param_1 + 0x3ec) != 1) {
 61652:         return;
```

### W0270 - `FUN_14003f424` line 61654
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61649:     }
 61650:     else {
 61651:       if (*(int *)(param_1 + 0x3ec) != 1) {
 61652:         return;
 61653:       }
 61654:       FUN_1400444d0(param_1,0xf,7,0);
 61655:       bVar1 = 0;
 61656:     }
 61657:     FUN_1400444d0(param_1,0xe2,1,bVar1);
 61658:     FUN_1400444d0(param_1,0xcb,4,4);
 61659:     FUN_1400444d0(param_1,0xf,7,4);
```

### W0271 - `FUN_14003f424` line 61657
- Register: `0xE2 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `bVar1`
```c
 61652:         return;
 61653:       }
 61654:       FUN_1400444d0(param_1,0xf,7,0);
 61655:       bVar1 = 0;
 61656:     }
 61657:     FUN_1400444d0(param_1,0xe2,1,bVar1);
 61658:     FUN_1400444d0(param_1,0xcb,4,4);
 61659:     FUN_1400444d0(param_1,0xf,7,4);
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
```

### W0272 - `FUN_14003f424` line 61658
- Register: `0xCB `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 61653:       }
 61654:       FUN_1400444d0(param_1,0xf,7,0);
 61655:       bVar1 = 0;
 61656:     }
 61657:     FUN_1400444d0(param_1,0xe2,1,bVar1);
 61658:     FUN_1400444d0(param_1,0xcb,4,4);
 61659:     FUN_1400444d0(param_1,0xf,7,4);
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
```

### W0273 - `FUN_14003f424` line 61659
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 61654:       FUN_1400444d0(param_1,0xf,7,0);
 61655:       bVar1 = 0;
 61656:     }
 61657:     FUN_1400444d0(param_1,0xe2,1,bVar1);
 61658:     FUN_1400444d0(param_1,0xcb,4,4);
 61659:     FUN_1400444d0(param_1,0xf,7,4);
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
```

### W0274 - `FUN_14003f424` line 61663
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61658:     FUN_1400444d0(param_1,0xcb,4,4);
 61659:     FUN_1400444d0(param_1,0xf,7,4);
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
```

### W0275 - `FUN_14003f424` line 61664
- Register: `0xE2 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 61659:     FUN_1400444d0(param_1,0xf,7,4);
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
 61669:   FUN_1400444d0(param_1,0xcb,4,bVar1);
```

### W0276 - `FUN_14003f424` line 61665
- Register: `0xCB `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 61660:     bVar1 = 4;
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
 61669:   FUN_1400444d0(param_1,0xcb,4,bVar1);
 61670:   FUN_1400444d0(param_1,0xf,7,0);
```

### W0277 - `FUN_14003f424` line 61666
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `4`
```c
 61661:   }
 61662:   else {
 61663:     FUN_1400444d0(param_1,0xf,7,0);
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
 61669:   FUN_1400444d0(param_1,0xcb,4,bVar1);
 61670:   FUN_1400444d0(param_1,0xf,7,0);
 61671:   return;
```

### W0278 - `FUN_14003f424` line 61669
- Register: `0xCB `
- Kind: `rmw_write`
- Mask: `4`
- Value: `bVar1`
```c
 61664:     FUN_1400444d0(param_1,0xe2,1,0);
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
 61669:   FUN_1400444d0(param_1,0xcb,4,bVar1);
 61670:   FUN_1400444d0(param_1,0xf,7,0);
 61671:   return;
 61672: }
 61673: 
 61674: 
```

### W0279 - `FUN_14003f424` line 61670
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61665:     FUN_1400444d0(param_1,0xcb,4,0);
 61666:     FUN_1400444d0(param_1,0xf,7,4);
 61667:     bVar1 = 0;
 61668:   }
 61669:   FUN_1400444d0(param_1,0xcb,4,bVar1);
 61670:   FUN_1400444d0(param_1,0xf,7,0);
 61671:   return;
 61672: }
 61673: 
 61674: 
 61675: 
```

### W0280 - `FUN_14003f500` line 61698
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61693:   }
 61694:   else if (param_2 == 1) {
 61695:     *(undefined1 *)(param_1 + 0x225) = 0;
 61696:     return;
 61697:   }
 61698:   FUN_1400444d0(param_1,0xf,7,0);
 61699:   return;
 61700: }
 61701: 
 61702: 
 61703: 
```

### W0281 - `FUN_14003f59c` line 61709
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61704: void FUN_14003f59c(longlong param_1,char param_2)
 61705: 
 61706: {
 61707:   byte bVar1;
 61708: 
 61709:   FUN_1400444d0(param_1,0xf,7,1);
 61710:   if (param_2 == '\x02') {
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
```

### W0282 - `FUN_14003f59c` line 61711
- Register: `0xC0 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 61706: {
 61707:   byte bVar1;
 61708: 
 61709:   FUN_1400444d0(param_1,0xf,7,1);
 61710:   if (param_2 == '\x02') {
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
```

### W0283 - `FUN_14003f59c` line 61713
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `5`
```c
 61708: 
 61709:   FUN_1400444d0(param_1,0xf,7,1);
 61710:   if (param_2 == '\x02') {
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
```

### W0284 - `FUN_14003f59c` line 61714
- Register: `0xD1 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 61709:   FUN_1400444d0(param_1,0xf,7,1);
 61710:   if (param_2 == '\x02') {
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
 61719:   if (param_2 == '\x01') {
```

### W0285 - `FUN_14003f59c` line 61715
- Register: `0xD1 `
- Kind: `rmw_write`
- Mask: `0xc`
- Value: `4`
```c
 61710:   if (param_2 == '\x02') {
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
 61719:   if (param_2 == '\x01') {
 61720:     bVar1 = 0;
```

### W0286 - `FUN_14003f59c` line 61716
- Register: `0xDA `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 61711:     FUN_1400444d0(param_1,0xc0,1,1);
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
 61719:   if (param_2 == '\x01') {
 61720:     bVar1 = 0;
 61721:   }
```

### W0287 - `FUN_14003f59c` line 61717
- Register: `0xD0 `
- Kind: `direct_write`
- Value: `0xf3`
```c
 61712:   }
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
 61719:   if (param_2 == '\x01') {
 61720:     bVar1 = 0;
 61721:   }
 61722:   else if (param_2 == '\x02') {
```

### W0288 - `FUN_14003f59c` line 61718
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61713:   FUN_1400444d0(param_1,0xf,7,5);
 61714:   FUN_1400444d0(param_1,0xd1,1,0);
 61715:   FUN_1400444d0(param_1,0xd1,0xc,4);
 61716:   FUN_1400444d0(param_1,0xda,0x10,0);
 61717:   FUN_140044528(param_1,0xd0,0xf3);
 61718:   FUN_1400444d0(param_1,0xf,7,1);
 61719:   if (param_2 == '\x01') {
 61720:     bVar1 = 0;
 61721:   }
 61722:   else if (param_2 == '\x02') {
 61723:     bVar1 = 0x10;
```

### W0289 - `FUN_14003f59c` line 61729
- Register: `0xBD `
- Kind: `rmw_write`
- Mask: `0x30`
- Value: `bVar1`
```c
 61724:   }
 61725:   else {
 61726:     if (param_2 != '\x04') goto LAB_14003f657;
 61727:     bVar1 = 0x20;
 61728:   }
 61729:   FUN_1400444d0(param_1,0xbd,0x30,bVar1);
 61730: LAB_14003f657:
 61731:   FUN_140044528(param_1,0xbe,0);
 61732:   FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61733:   FUN_1400444d0(param_1,0xf,7,0);
 61734:   return;
```

### W0290 - `FUN_14003f59c` line 61731
- Register: `0xBE `
- Kind: `direct_write`
- Value: `0`
```c
 61726:     if (param_2 != '\x04') goto LAB_14003f657;
 61727:     bVar1 = 0x20;
 61728:   }
 61729:   FUN_1400444d0(param_1,0xbd,0x30,bVar1);
 61730: LAB_14003f657:
 61731:   FUN_140044528(param_1,0xbe,0);
 61732:   FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61733:   FUN_1400444d0(param_1,0xf,7,0);
 61734:   return;
 61735: }
 61736: 
```

### W0291 - `FUN_14003f59c` line 61732
- Register: `0xFE `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 61727:     bVar1 = 0x20;
 61728:   }
 61729:   FUN_1400444d0(param_1,0xbd,0x30,bVar1);
 61730: LAB_14003f657:
 61731:   FUN_140044528(param_1,0xbe,0);
 61732:   FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61733:   FUN_1400444d0(param_1,0xf,7,0);
 61734:   return;
 61735: }
 61736: 
 61737: 
```

### W0292 - `FUN_14003f59c` line 61733
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61728:   }
 61729:   FUN_1400444d0(param_1,0xbd,0x30,bVar1);
 61730: LAB_14003f657:
 61731:   FUN_140044528(param_1,0xbe,0);
 61732:   FUN_1400444d0(param_1,0xfe,0x10,0x10);
 61733:   FUN_1400444d0(param_1,0xf,7,0);
 61734:   return;
 61735: }
 61736: 
 61737: 
 61738: 
```

### W0293 - `FUN_14003f690` line 61746
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61741: {
 61742:   int iVar1;
 61743:   byte bVar2;
 61744:   byte bVar3;
 61745: 
 61746:   FUN_1400444d0(param_1,0xf,7,1);
 61747:   FUN_1400444d0(param_1,0xc0,6,2);
 61748:   iVar1 = *(int *)(param_1 + 500);
 61749:   if (iVar1 == 0) {
 61750:     bVar2 = 2;
 61751:     bVar3 = 0;
```

### W0294 - `FUN_14003f690` line 61747
- Register: `0xC0 `
- Kind: `rmw_write`
- Mask: `6`
- Value: `2`
```c
 61742:   int iVar1;
 61743:   byte bVar2;
 61744:   byte bVar3;
 61745: 
 61746:   FUN_1400444d0(param_1,0xf,7,1);
 61747:   FUN_1400444d0(param_1,0xc0,6,2);
 61748:   iVar1 = *(int *)(param_1 + 500);
 61749:   if (iVar1 == 0) {
 61750:     bVar2 = 2;
 61751:     bVar3 = 0;
 61752: LAB_14003f6f1:
```

### W0295 - `FUN_14003f690` line 61753
- Register: `0xC1 `
- Kind: `rmw_write`
- Mask: `bVar2`
- Value: `bVar3`
```c
 61748:   iVar1 = *(int *)(param_1 + 500);
 61749:   if (iVar1 == 0) {
 61750:     bVar2 = 2;
 61751:     bVar3 = 0;
 61752: LAB_14003f6f1:
 61753:     FUN_1400444d0(param_1,0xc1,bVar2,bVar3);
 61754:   }
 61755:   else {
 61756:     if (iVar1 == 1) {
 61757:       bVar2 = 2;
 61758:       bVar3 = bVar2;
```

### W0296 - `FUN_14003f690` line 61784
- Register: `0xC1 `
- Kind: `rmw_write`
- Mask: `0x20`
- Value: `bVar3`
```c
 61779:   }
 61780:   else {
 61781:     if (*(int *)(param_1 + 0x1f8) != 1) goto LAB_14003f722;
 61782:     bVar3 = 0x20;
 61783:   }
 61784:   FUN_1400444d0(param_1,0xc1,0x20,bVar3);
 61785: LAB_14003f722:
 61786:   FUN_1400444d0(param_1,0xf,7,0);
 61787:   iVar1 = *(int *)(param_1 + 0x1fc);
 61788:   if (iVar1 == 0) {
 61789:     bVar3 = 0;
```

### W0297 - `FUN_14003f690` line 61786
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61781:     if (*(int *)(param_1 + 0x1f8) != 1) goto LAB_14003f722;
 61782:     bVar3 = 0x20;
 61783:   }
 61784:   FUN_1400444d0(param_1,0xc1,0x20,bVar3);
 61785: LAB_14003f722:
 61786:   FUN_1400444d0(param_1,0xf,7,0);
 61787:   iVar1 = *(int *)(param_1 + 0x1fc);
 61788:   if (iVar1 == 0) {
 61789:     bVar3 = 0;
 61790:   }
 61791:   else if (iVar1 == 1) {
```

### W0298 - `FUN_14003f690` line 61803
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `0xc`
- Value: `bVar3`
```c
 61798:     if (iVar1 != 3) {
 61799:       return;
 61800:     }
 61801:     bVar3 = 0xc;
 61802:   }
 61803:   FUN_1400444d0(param_1,0x6b,0xc,bVar3);
 61804:   return;
 61805: }
 61806: 
 61807: 
 61808: 
```

### W0299 - `FUN_14003f774` line 61817
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61812:   byte bVar1;
 61813:   bool bVar2;
 61814:   char *pcVar3;
 61815:   undefined1 uVar4;
 61816: 
 61817:   FUN_1400444d0(param_1,0xf,7,0);
 61818:   bVar1 = FUN_140044484(param_1,0x98);
 61819:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar1 & 0xf0);
 61820:   FUN_1400444d0(param_1,0xf,7,1);
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
```

### W0300 - `FUN_14003f774` line 61820
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61815:   undefined1 uVar4;
 61816: 
 61817:   FUN_1400444d0(param_1,0xf,7,0);
 61818:   bVar1 = FUN_140044484(param_1,0x98);
 61819:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar1 & 0xf0);
 61820:   FUN_1400444d0(param_1,0xf,7,1);
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
 61823:     FUN_1400444d0(param_1,0xc6,0x80,0);
 61824:     bVar2 = FUN_14003cec4(param_1);
 61825:     if (bVar2) {
```

### W0301 - `FUN_14003f774` line 61822
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 61817:   FUN_1400444d0(param_1,0xf,7,0);
 61818:   bVar1 = FUN_140044484(param_1,0x98);
 61819:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar1 & 0xf0);
 61820:   FUN_1400444d0(param_1,0xf,7,1);
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
 61823:     FUN_1400444d0(param_1,0xc6,0x80,0);
 61824:     bVar2 = FUN_14003cec4(param_1);
 61825:     if (bVar2) {
 61826:       FUN_1400444d0(param_1,0xf,7,1);
 61827:       bVar1 = 0xc6;
```

### W0302 - `FUN_14003f774` line 61823
- Register: `0xC6 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 61818:   bVar1 = FUN_140044484(param_1,0x98);
 61819:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar1 & 0xf0);
 61820:   FUN_1400444d0(param_1,0xf,7,1);
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
 61823:     FUN_1400444d0(param_1,0xc6,0x80,0);
 61824:     bVar2 = FUN_14003cec4(param_1);
 61825:     if (bVar2) {
 61826:       FUN_1400444d0(param_1,0xf,7,1);
 61827:       bVar1 = 0xc6;
 61828:       if (*(short *)(param_1 + 0x252) == 0x10) {
```

### W0303 - `FUN_14003f774` line 61826
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
 61823:     FUN_1400444d0(param_1,0xc6,0x80,0);
 61824:     bVar2 = FUN_14003cec4(param_1);
 61825:     if (bVar2) {
 61826:       FUN_1400444d0(param_1,0xf,7,1);
 61827:       bVar1 = 0xc6;
 61828:       if (*(short *)(param_1 + 0x252) == 0x10) {
 61829:         uVar4 = 0x40;
 61830:       }
 61831:       else {
```

### W0304 - `FUN_14003f774` line 61838
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `1`
```c
 61833:       }
 61834: LAB_14003f881:
 61835:       FUN_140044528(param_1,bVar1,uVar4);
 61836:     }
 61837:     else {
 61838:       FUN_1400444d0(param_1,0xf,7,1);
 61839:       bVar1 = FUN_140044484(param_1,0xc0);
 61840:       if (((bVar1 & 0xc0) == 0) || ((bVar1 & 0xc0) == 0x40)) {
 61841:         if (*(short *)(param_1 + 0x252) == 0x10) {
 61842:           uVar4 = 0x78;
 61843: LAB_14003f83a:
```

### W0305 - `FUN_14003f774` line 61876
- Register: `0xC5 `
- Kind: `direct_write`
- Value: `0xff`
```c
 61871:     }
 61872:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14003f8c4;
 61873:     pcVar3 = "---------------- iTE6805_Set_Video_Tristate -> VIDEO_ON = Tristate off\n";
 61874:   }
 61875:   else {
 61876:     FUN_140044528(param_1,0xc5,0xff);
 61877:     FUN_140044528(param_1,0xc6,0xff);
 61878:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14003f8c4;
 61879:     pcVar3 = "---------------- iTE6805_Set_Video_Tristate -> VIDEO_OFF = Tristate on\n";
 61880:   }
 61881:   DbgPrint(pcVar3);
```

### W0306 - `FUN_14003f774` line 61877
- Register: `0xC6 `
- Kind: `direct_write`
- Value: `0xff`
```c
 61872:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14003f8c4;
 61873:     pcVar3 = "---------------- iTE6805_Set_Video_Tristate -> VIDEO_ON = Tristate off\n";
 61874:   }
 61875:   else {
 61876:     FUN_140044528(param_1,0xc5,0xff);
 61877:     FUN_140044528(param_1,0xc6,0xff);
 61878:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14003f8c4;
 61879:     pcVar3 = "---------------- iTE6805_Set_Video_Tristate -> VIDEO_OFF = Tristate on\n";
 61880:   }
 61881:   DbgPrint(pcVar3);
 61882: LAB_14003f8c4:
```

### W0307 - `FUN_14003f774` line 61883
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61878:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14003f8c4;
 61879:     pcVar3 = "---------------- iTE6805_Set_Video_Tristate -> VIDEO_OFF = Tristate on\n";
 61880:   }
 61881:   DbgPrint(pcVar3);
 61882: LAB_14003f8c4:
 61883:   FUN_1400444d0(param_1,0xf,7,0);
 61884:   return;
 61885: }
 61886: 
 61887: 
 61888: 
```

### W0308 - `FUN_14003f8e0` line 61910
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `2`
```c
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
 61911:   bVar6 = FUN_140044484(param_1,0x46);
 61912:   bVar6 = bVar6 >> 2 & 7;
 61913:   FUN_1400444d0(param_1,0xf,7,0);
 61914:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61915:     DbgPrint("Audio Format: ");
```

### W0309 - `FUN_14003f8e0` line 61913
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
 61911:   bVar6 = FUN_140044484(param_1,0x46);
 61912:   bVar6 = bVar6 >> 2 & 7;
 61913:   FUN_1400444d0(param_1,0xf,7,0);
 61914:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61915:     DbgPrint("Audio Format: ");
 61916:   }
 61917:   if ((bVar1 & 0x10) == 0) {
 61918:     if ((bVar2 & 0x20) == 0) {
```

### W0310 - `FUN_140040140` line 62331
- Register: `0x0F `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 62326:   byte bVar2;
 62327:   byte bVar3;
 62328:   char *pcVar4;
 62329:   uint uVar5;
 62330: 
 62331:   FUN_1400444d0(param_1,0xf,7,0);
 62332:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62333:     DbgPrint("\n\n -------Video Timing-------\n");
 62334:   }
 62335:   bVar2 = FUN_140044484(param_1,0x15);
 62336:   if ((bVar2 & 2) == 0) {
```

### W0311 - `FUN_140040610` line 62533
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62528:   byte bVar5;
 62529: 
 62530:   uVar4 = (undefined7)((ulonglong)param_3 >> 8);
 62531:   uVar1 = 0;
 62532:   *(undefined2 *)(param_1 + 0x201) = 0;
 62533:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,param_4);
 62534:   uVar2 = 0xa9;
 62535:   uVar1 = CONCAT71((int7)((ulonglong)uVar1 >> 8),0x4b);
 62536:   FUN_140044528(param_1,0x4b,0xa9);
 62537:   FUN_140040960(param_1,uVar1,CONCAT71(uVar4,uVar2),param_4);
 62538:   FUN_1400406f4(param_1);
```

### W0312 - `FUN_140040610` line 62536
- Register: `0x4B `
- Kind: `direct_write`
- Value: `0xa9`
```c
 62531:   uVar1 = 0;
 62532:   *(undefined2 *)(param_1 + 0x201) = 0;
 62533:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,param_4);
 62534:   uVar2 = 0xa9;
 62535:   uVar1 = CONCAT71((int7)((ulonglong)uVar1 >> 8),0x4b);
 62536:   FUN_140044528(param_1,0x4b,0xa9);
 62537:   FUN_140040960(param_1,uVar1,CONCAT71(uVar4,uVar2),param_4);
 62538:   FUN_1400406f4(param_1);
 62539:   bVar5 = 0;
 62540:   bVar3 = 1;
 62541:   FUN_1400444d0(param_1,0xc5,1,0);
```

### W0313 - `FUN_140040610` line 62541
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 62536:   FUN_140044528(param_1,0x4b,0xa9);
 62537:   FUN_140040960(param_1,uVar1,CONCAT71(uVar4,uVar2),param_4);
 62538:   FUN_1400406f4(param_1);
 62539:   bVar5 = 0;
 62540:   bVar3 = 1;
 62541:   FUN_1400444d0(param_1,0xc5,1,0);
 62542:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62543:   bVar3 = 1;
 62544:   bVar5 = 0;
 62545:   FUN_1400444d0(param_1,0xc5,1,0);
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
```

### W0314 - `FUN_140040610` line 62542
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62537:   FUN_140040960(param_1,uVar1,CONCAT71(uVar4,uVar2),param_4);
 62538:   FUN_1400406f4(param_1);
 62539:   bVar5 = 0;
 62540:   bVar3 = 1;
 62541:   FUN_1400444d0(param_1,0xc5,1,0);
 62542:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62543:   bVar3 = 1;
 62544:   bVar5 = 0;
 62545:   FUN_1400444d0(param_1,0xc5,1,0);
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
```

### W0315 - `FUN_140040610` line 62545
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 62540:   bVar3 = 1;
 62541:   FUN_1400444d0(param_1,0xc5,1,0);
 62542:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62543:   bVar3 = 1;
 62544:   bVar5 = 0;
 62545:   FUN_1400444d0(param_1,0xc5,1,0);
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
```

### W0316 - `FUN_140040610` line 62546
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62541:   FUN_1400444d0(param_1,0xc5,1,0);
 62542:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62543:   bVar3 = 1;
 62544:   bVar5 = 0;
 62545:   FUN_1400444d0(param_1,0xc5,1,0);
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
 62551:   FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0317 - `FUN_140040610` line 62547
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 62542:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62543:   bVar3 = 1;
 62544:   bVar5 = 0;
 62545:   FUN_1400444d0(param_1,0xc5,1,0);
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
 62551:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62552:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
```

### W0318 - `FUN_140040610` line 62551
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 62546:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
 62551:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62552:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62553:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62554:   FUN_140048644(param_1,1);
 62555:   bVar3 = 0x10;
 62556:   bVar5 = 0;
```

### W0319 - `FUN_140040610` line 62552
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62547:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
 62551:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62552:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62553:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62554:   FUN_140048644(param_1,1);
 62555:   bVar3 = 0x10;
 62556:   bVar5 = 0;
 62557:   FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0320 - `FUN_140040610` line 62553
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 62548:   FUN_140048644(param_1,1);
 62549:   bVar3 = 0x10;
 62550:   bVar5 = 0;
 62551:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62552:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62553:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62554:   FUN_140048644(param_1,1);
 62555:   bVar3 = 0x10;
 62556:   bVar5 = 0;
 62557:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62558:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
```

### W0321 - `FUN_140040610` line 62557
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 62552:   thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 62553:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62554:   FUN_140048644(param_1,1);
 62555:   bVar3 = 0x10;
 62556:   bVar5 = 0;
 62557:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62558:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62559:   return;
 62560: }
 62561: 
 62562: 
```

### W0322 - `FUN_140040610` line 62558
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62553:   FUN_1400444d0(param_1,0xc5,0x10,0x10);
 62554:   FUN_140048644(param_1,1);
 62555:   bVar3 = 0x10;
 62556:   bVar5 = 0;
 62557:   FUN_1400444d0(param_1,0xc5,0x10,0);
 62558:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 62559:   return;
 62560: }
 62561: 
 62562: 
 62563: 
```

### W0323 - `FUN_140040960` line 62706
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62701:   }
 62702:   FUN_14003f500(param_1,0,0);
 62703:   FUN_14003f500(param_1,1,0);
 62704:   bVar5 = 0;
 62705:   bVar1 = FUN_140040b58(param_1,0x14009f3b0,0);
 62706:   thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62707:   bVar5 = bVar1;
 62708:   FUN_140044528(param_1,0xc9,bVar1);
 62709:   thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62710:   FUN_140044528(param_1,0xc9,bVar1);
 62711:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
```

### W0324 - `FUN_140040960` line 62708
- Register: `0xC9 `
- Kind: `direct_write`
- Value: `bVar1`
```c
 62703:   FUN_14003f500(param_1,1,0);
 62704:   bVar5 = 0;
 62705:   bVar1 = FUN_140040b58(param_1,0x14009f3b0,0);
 62706:   thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62707:   bVar5 = bVar1;
 62708:   FUN_140044528(param_1,0xc9,bVar1);
 62709:   thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62710:   FUN_140044528(param_1,0xc9,bVar1);
 62711:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 62712:   bVar5 = 1;
 62713:   cVar2 = FUN_140040b58(param_1,0x14009f3b0,1);
```

### W0325 - `FUN_140040960` line 62709
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62704:   bVar5 = 0;
 62705:   bVar1 = FUN_140040b58(param_1,0x14009f3b0,0);
 62706:   thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62707:   bVar5 = bVar1;
 62708:   FUN_140044528(param_1,0xc9,bVar1);
 62709:   thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62710:   FUN_140044528(param_1,0xc9,bVar1);
 62711:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 62712:   bVar5 = 1;
 62713:   cVar2 = FUN_140040b58(param_1,0x14009f3b0,1);
 62714:   uVar3 = FUN_14004052c(param_1,0x14009f3b0);
```

### W0326 - `FUN_140040960` line 62710
- Register: `0xC9 `
- Kind: `direct_write`
- Value: `bVar1`
```c
 62705:   bVar1 = FUN_140040b58(param_1,0x14009f3b0,0);
 62706:   thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62707:   bVar5 = bVar1;
 62708:   FUN_140044528(param_1,0xc9,bVar1);
 62709:   thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62710:   FUN_140044528(param_1,0xc9,bVar1);
 62711:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 62712:   bVar5 = 1;
 62713:   cVar2 = FUN_140040b58(param_1,0x14009f3b0,1);
 62714:   uVar3 = FUN_14004052c(param_1,0x14009f3b0);
 62715:   uVar6 = uVar3 & 0xff;
```

### W0327 - `FUN_140040960` line 62711
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62706:   thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62707:   bVar5 = bVar1;
 62708:   FUN_140044528(param_1,0xc9,bVar1);
 62709:   thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62710:   FUN_140044528(param_1,0xc9,bVar1);
 62711:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 62712:   bVar5 = 1;
 62713:   cVar2 = FUN_140040b58(param_1,0x14009f3b0,1);
 62714:   uVar3 = FUN_14004052c(param_1,0x14009f3b0);
 62715:   uVar6 = uVar3 & 0xff;
 62716:   if ((char)uVar3 != '\0') {
```

### W0328 - `FUN_140040960` line 62721
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62716:   if ((char)uVar3 != '\0') {
 62717:     *(undefined4 *)(param_1 + 0xab) = 0x200010;
 62718:     cVar2 = cVar2 + (&DAT_14009f3b1)[uVar6] + (&DAT_14009f3b0)[uVar6];
 62719:     bVar1 = cVar2 - 0x10;
 62720:     bVar4 = cVar2 - 0x20;
 62721:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
```

### W0329 - `FUN_140040960` line 62722
- Register: `0xC6 `
- Kind: `direct_write`
- Value: `(char)uVar3`
```c
 62717:     *(undefined4 *)(param_1 + 0xab) = 0x200010;
 62718:     cVar2 = cVar2 + (&DAT_14009f3b1)[uVar6] + (&DAT_14009f3b0)[uVar6];
 62719:     bVar1 = cVar2 - 0x10;
 62720:     bVar4 = cVar2 - 0x20;
 62721:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
```

### W0330 - `FUN_140040960` line 62723
- Register: `0xC7 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(param_1 + 0xab)`
```c
 62718:     cVar2 = cVar2 + (&DAT_14009f3b1)[uVar6] + (&DAT_14009f3b0)[uVar6];
 62719:     bVar1 = cVar2 - 0x10;
 62720:     bVar4 = cVar2 - 0x20;
 62721:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
```

### W0331 - `FUN_140040960` line 62724
- Register: `0xC8 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(param_1 + 0xac)`
```c
 62719:     bVar1 = cVar2 - 0x10;
 62720:     bVar4 = cVar2 - 0x20;
 62721:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
```

### W0332 - `FUN_140040960` line 62726
- Register: `0xCA `
- Kind: `direct_write`
- Value: `bVar1`
```c
 62721:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
```

### W0333 - `FUN_140040960` line 62727
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62722:     FUN_140044528(param_1,0xc6,(char)uVar3);
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
 62732:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
```

### W0334 - `FUN_140040960` line 62728
- Register: `0xC7 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(param_1 + 0xad)`
```c
 62723:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xab));
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
 62732:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62733:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0335 - `FUN_140040960` line 62729
- Register: `0xC8 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(param_1 + 0xae)`
```c
 62724:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xac));
 62725:     bVar5 = bVar1;
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
 62732:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62733:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62734:       DbgPrint(" PORT0_EDID_Block1_CheckSum = %02X\n",bVar1);
```

### W0336 - `FUN_140040960` line 62731
- Register: `0xCA `
- Kind: `direct_write`
- Value: `bVar4`
```c
 62726:     FUN_140044528(param_1,0xca,bVar1);
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
 62732:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62733:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62734:       DbgPrint(" PORT0_EDID_Block1_CheckSum = %02X\n",bVar1);
 62735:     }
 62736:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0337 - `FUN_140040960` line 62732
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62727:     thunk_FUN_1400444d0(param_1,4,bVar5,param_4);
 62728:     FUN_140044528(param_1,199,*(undefined1 *)(param_1 + 0xad));
 62729:     FUN_140044528(param_1,200,*(undefined1 *)(param_1 + 0xae));
 62730:     bVar5 = bVar4;
 62731:     FUN_140044528(param_1,0xca,bVar4);
 62732:     thunk_FUN_1400444d0(param_1,0,bVar5,param_4);
 62733:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62734:       DbgPrint(" PORT0_EDID_Block1_CheckSum = %02X\n",bVar1);
 62735:     }
 62736:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62737:       DbgPrint(" PORT1_EDID_Block1_CheckSum = %02X\n",bVar4);
```

### W0338 - `FUN_140041280` line 62946
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62941:   }
 62942:   else {
 62943:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140041502;
 62944:     bVar6 = 7;
 62945:   }
 62946:   thunk_FUN_1400444d0(param_1,bVar6,bVar7,1);
 62947: LAB_140041502:
 62948:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62949:     DbgPrint("****** EQ Check Valid ******\n");
 62950:   }
 62951:   bVar6 = 8;
```

### W0339 - `FUN_140041280` line 62953
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `0`
```c
 62948:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62949:     DbgPrint("****** EQ Check Valid ******\n");
 62950:   }
 62951:   bVar6 = 8;
 62952:   bVar7 = 0;
 62953:   FUN_1400444d0(param_1,0x22,8,0);
 62954:   thunk_FUN_1400444d0(param_1,0,bVar6,bVar7);
 62955:   FUN_140044528(param_1,7,0xf0);
 62956:   FUN_140044528(param_1,0xc,0xf0);
 62957:   return true;
 62958: }
```

### W0340 - `FUN_140041280` line 62954
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 62949:     DbgPrint("****** EQ Check Valid ******\n");
 62950:   }
 62951:   bVar6 = 8;
 62952:   bVar7 = 0;
 62953:   FUN_1400444d0(param_1,0x22,8,0);
 62954:   thunk_FUN_1400444d0(param_1,0,bVar6,bVar7);
 62955:   FUN_140044528(param_1,7,0xf0);
 62956:   FUN_140044528(param_1,0xc,0xf0);
 62957:   return true;
 62958: }
 62959: 
```

### W0341 - `FUN_140041280` line 62955
- Register: `0x07 `
- Kind: `direct_write`
- Value: `0xf0`
```c
 62950:   }
 62951:   bVar6 = 8;
 62952:   bVar7 = 0;
 62953:   FUN_1400444d0(param_1,0x22,8,0);
 62954:   thunk_FUN_1400444d0(param_1,0,bVar6,bVar7);
 62955:   FUN_140044528(param_1,7,0xf0);
 62956:   FUN_140044528(param_1,0xc,0xf0);
 62957:   return true;
 62958: }
 62959: 
 62960: 
```

### W0342 - `FUN_140041280` line 62956
- Register: `0x0C `
- Kind: `direct_write`
- Value: `0xf0`
```c
 62951:   bVar6 = 8;
 62952:   bVar7 = 0;
 62953:   FUN_1400444d0(param_1,0x22,8,0);
 62954:   thunk_FUN_1400444d0(param_1,0,bVar6,bVar7);
 62955:   FUN_140044528(param_1,7,0xf0);
 62956:   FUN_140044528(param_1,0xc,0xf0);
 62957:   return true;
 62958: }
 62959: 
 62960: 
 62961: 
```

### W0343 - `FUN_14004165c` line 63025
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63020:   char cVar4;
 63021:   char cVar5;
 63022:   byte bVar6;
 63023:   bool bVar7;
 63024: 
 63025:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
```

### W0344 - `FUN_14004165c` line 63030
- Register: `0xB9 `
- Kind: `direct_write`
- Value: `0xff`
```c
 63025:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
```

### W0345 - `FUN_14004165c` line 63032
- Register: `0xBE `
- Kind: `direct_write`
- Value: `0xff`
```c
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
```

### W0346 - `FUN_14004165c` line 63038
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
```

### W0347 - `FUN_14004165c` line 63039
- Register: `0xB9 `
- Kind: `direct_write`
- Value: `0xff`
```c
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
 63044:     thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
```

### W0348 - `FUN_14004165c` line 63042
- Register: `0xBE `
- Kind: `direct_write`
- Value: `0xff`
```c
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
 63044:     thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
 63045:   }
 63046:   if ((((cVar2 == -0x41) && ((bVar3 & 0x38) == 0x38)) && (cVar1 < '\0')) && (cVar4 == '\0')) {
 63047:     bVar7 = cVar5 == '\0';
```

### W0349 - `FUN_14004165c` line 63044
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
 63044:     thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
 63045:   }
 63046:   if ((((cVar2 == -0x41) && ((bVar3 & 0x38) == 0x38)) && (cVar1 < '\0')) && (cVar4 == '\0')) {
 63047:     bVar7 = cVar5 == '\0';
 63048:   }
 63049:   else {
```

### W0350 - `FUN_140041784` line 63071
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63066:   int iVar7;
 63067: 
 63068:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63069:     bVar4 = 0;
 63070: LAB_1400417a8:
 63071:     thunk_FUN_1400444d0(param_1,bVar4,param_3,param_4);
 63072:   }
 63073:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63074:     bVar4 = 4;
 63075:     goto LAB_1400417a8;
 63076:   }
```

### W0351 - `FUN_140041784` line 63079
- Register: `0x3B `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 63074:     bVar4 = 4;
 63075:     goto LAB_1400417a8;
 63076:   }
 63077:   bVar6 = 8;
 63078:   bVar4 = 8;
 63079:   FUN_1400444d0(param_1,0x3b,8,8);
 63080:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63081:     bVar5 = 3;
 63082:   }
 63083:   else {
 63084:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_1400417df;
```

### W0352 - `FUN_140041784` line 63087
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63082:   }
 63083:   else {
 63084:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_1400417df;
 63085:     bVar5 = 7;
 63086:   }
 63087:   thunk_FUN_1400444d0(param_1,bVar5,bVar4,bVar6);
 63088: LAB_1400417df:
 63089:   bVar6 = 0;
 63090:   FUN_1400444d0(param_1,0x55,0x80,0);
 63091:   FUN_140044528(param_1,0xe9,0x80);
 63092:   FUN_140048644(param_1,100);
```

### W0353 - `FUN_140041784` line 63090
- Register: `0x55 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 63085:     bVar5 = 7;
 63086:   }
 63087:   thunk_FUN_1400444d0(param_1,bVar5,bVar4,bVar6);
 63088: LAB_1400417df:
 63089:   bVar6 = 0;
 63090:   FUN_1400444d0(param_1,0x55,0x80,0);
 63091:   FUN_140044528(param_1,0xe9,0x80);
 63092:   FUN_140048644(param_1,100);
 63093:   FUN_140044528(param_1,0xe9,0);
 63094:   cVar1 = *(char *)(param_1 + 0x17c);
 63095:   uVar2 = FUN_140044484(param_1,0xea);
```

### W0354 - `FUN_140041784` line 63091
- Register: `0xE9 `
- Kind: `direct_write`
- Value: `0x80`
```c
 63086:   }
 63087:   thunk_FUN_1400444d0(param_1,bVar5,bVar4,bVar6);
 63088: LAB_1400417df:
 63089:   bVar6 = 0;
 63090:   FUN_1400444d0(param_1,0x55,0x80,0);
 63091:   FUN_140044528(param_1,0xe9,0x80);
 63092:   FUN_140048644(param_1,100);
 63093:   FUN_140044528(param_1,0xe9,0);
 63094:   cVar1 = *(char *)(param_1 + 0x17c);
 63095:   uVar2 = FUN_140044484(param_1,0xea);
 63096:   *(undefined1 *)(param_1 + ((longlong)cVar1 + 0x34) * 6) = uVar2;
```

### W0355 - `FUN_140041784` line 63093
- Register: `0xE9 `
- Kind: `direct_write`
- Value: `0`
```c
 63088: LAB_1400417df:
 63089:   bVar6 = 0;
 63090:   FUN_1400444d0(param_1,0x55,0x80,0);
 63091:   FUN_140044528(param_1,0xe9,0x80);
 63092:   FUN_140048644(param_1,100);
 63093:   FUN_140044528(param_1,0xe9,0);
 63094:   cVar1 = *(char *)(param_1 + 0x17c);
 63095:   uVar2 = FUN_140044484(param_1,0xea);
 63096:   *(undefined1 *)(param_1 + ((longlong)cVar1 + 0x34) * 6) = uVar2;
 63097:   cVar1 = *(char *)(param_1 + 0x17c);
 63098:   uVar2 = FUN_140044484(param_1,0xeb);
```

### W0356 - `FUN_140041784` line 63100
- Register: `0xE9 `
- Kind: `direct_write`
- Value: `0x20`
```c
 63095:   uVar2 = FUN_140044484(param_1,0xea);
 63096:   *(undefined1 *)(param_1 + ((longlong)cVar1 + 0x34) * 6) = uVar2;
 63097:   cVar1 = *(char *)(param_1 + 0x17c);
 63098:   uVar2 = FUN_140044484(param_1,0xeb);
 63099:   *(undefined1 *)(param_1 + 0x139 + (longlong)cVar1 * 6) = uVar2;
 63100:   FUN_140044528(param_1,0xe9,0x20);
 63101:   cVar1 = *(char *)(param_1 + 0x17c);
 63102:   uVar2 = FUN_140044484(param_1,0xea);
 63103:   *(undefined1 *)(param_1 + 0x13a + (longlong)cVar1 * 6) = uVar2;
 63104:   cVar1 = *(char *)(param_1 + 0x17c);
 63105:   uVar2 = FUN_140044484(param_1,0xeb);
```

### W0357 - `FUN_140041784` line 63108
- Register: `0xE9 `
- Kind: `direct_write`
- Value: `0x40`
```c
 63103:   *(undefined1 *)(param_1 + 0x13a + (longlong)cVar1 * 6) = uVar2;
 63104:   cVar1 = *(char *)(param_1 + 0x17c);
 63105:   uVar2 = FUN_140044484(param_1,0xeb);
 63106:   bVar4 = 0x40;
 63107:   *(undefined1 *)(param_1 + 0x13b + (longlong)cVar1 * 6) = uVar2;
 63108:   FUN_140044528(param_1,0xe9,0x40);
 63109:   cVar1 = *(char *)(param_1 + 0x17c);
 63110:   uVar2 = FUN_140044484(param_1,0xea);
 63111:   *(undefined1 *)(param_1 + 0x13c + (longlong)cVar1 * 6) = uVar2;
 63112:   cVar1 = *(char *)(param_1 + 0x17c);
 63113:   uVar2 = FUN_140044484(param_1,0xeb);
```

### W0358 - `FUN_140041784` line 63175
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63170:     DbgPrint("......................................................\n");
 63171:   }
 63172:   if ((DAT_1400a04f8 & 0x10) != 0) {
 63173:     DbgPrint(&LAB_14006bf90);
 63174:   }
 63175:   thunk_FUN_1400444d0(param_1,0,bVar4,bVar6);
 63176:   return;
 63177: }
 63178: 
 63179: 
 63180: 
```

### W0359 - `FUN_140041b38` line 63264
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63259:             FUN_1400428d8(param_1,1,(byte)param_3,(byte)param_4);
 63260:             FUN_1400428d8(param_1,2,(byte)param_3,(byte)param_4);
 63261:             if (*(char *)(param_1 + 0x1ec) == '\0') {
 63262:               bVar8 = 3;
 63263: LAB_140042517:
 63264:               thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63265:             }
 63266:             else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63267:               bVar8 = 7;
 63268:               goto LAB_140042517;
 63269:             }
```

### W0360 - `FUN_140041b38` line 63272
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 63267:               bVar8 = 7;
 63268:               goto LAB_140042517;
 63269:             }
 63270:             param_4 = CONCAT71((int7)(param_4 >> 8),0x40);
 63271:             param_3 = CONCAT71((int7)(param_3 >> 8),0x40);
 63272:             FUN_1400444d0(param_1,0x22,0x40,0x40);
 63273:           }
 63274:           else {
 63275:             if (*(char *)(param_1 + 0x1ec) == '\0') {
 63276:               bVar8 = 3;
 63277: LAB_140042549:
```

### W0361 - `FUN_140041b38` line 63278
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63273:           }
 63274:           else {
 63275:             if (*(char *)(param_1 + 0x1ec) == '\0') {
 63276:               bVar8 = 3;
 63277: LAB_140042549:
 63278:               thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63279:             }
 63280:             else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_140042549;
 63281:             param_4 = 0;
 63282:             param_3 = CONCAT71((int7)(param_3 >> 8),0x40);
 63283:             FUN_1400444d0(param_1,0x22,0x40,0);
```

### W0362 - `FUN_140041b38` line 63283
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 63278:               thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63279:             }
 63280:             else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_140042549;
 63281:             param_4 = 0;
 63282:             param_3 = CONCAT71((int7)(param_3 >> 8),0x40);
 63283:             FUN_1400444d0(param_1,0x22,0x40,0);
 63284:             FUN_140041b38(param_1,5,param_3,param_4);
 63285:           }
 63286:           uVar7 = 0;
 63287:           thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63288:           *(undefined1 *)(param_1 + 0x175) = 0;
```

### W0363 - `FUN_140041b38` line 63287
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63282:             param_3 = CONCAT71((int7)(param_3 >> 8),0x40);
 63283:             FUN_1400444d0(param_1,0x22,0x40,0);
 63284:             FUN_140041b38(param_1,5,param_3,param_4);
 63285:           }
 63286:           uVar7 = 0;
 63287:           thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63288:           *(undefined1 *)(param_1 + 0x175) = 0;
 63289:         }
 63290:         if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 63291:           if (*(char *)(param_1 + 0x174) == '\0') {
 63292:             *(undefined1 *)(param_1 + 0x174) = 1;
```

### W0364 - `FUN_140041b38` line 63338
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63333:           DbgPrint("               Flag_Need_to_Force_SCDC_Clock_Ratio \n");
 63334:         }
 63335:         if (*(char *)(param_1 + 0x1ec) == '\0') {
 63336:           bVar8 = 3;
 63337: LAB_14004231a:
 63338:           thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63339:         }
 63340:         else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63341:           bVar8 = 7;
 63342:           goto LAB_14004231a;
 63343:         }
```

### W0365 - `FUN_140041b38` line 63352
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63347:           cVar1 = *(char *)(param_1 + 0x1ec);
 63348:           if (cVar2 != '\0') {
 63349:             if (cVar1 == '\0') {
 63350:               bVar8 = 3;
 63351: LAB_140042374:
 63352:               thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63353:             }
 63354:             else if (cVar1 == '\x01') {
 63355:               bVar8 = 7;
 63356:               goto LAB_140042374;
 63357:             }
```

### W0366 - `FUN_140041b38` line 63364
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63359:             goto LAB_140042337;
 63360:           }
 63361:           if (cVar1 == '\0') {
 63362:             bVar8 = 3;
 63363: LAB_140042397:
 63364:             thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63365:           }
 63366:           else if (cVar1 == '\x01') {
 63367:             bVar8 = 7;
 63368:             goto LAB_140042397;
 63369:           }
```

### W0367 - `FUN_140041b38` line 63372
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0x1c`
```c
 63367:             bVar8 = 7;
 63368:             goto LAB_140042397;
 63369:           }
 63370:           param_4 = CONCAT71((int7)(param_4 >> 8),0x1c);
 63371:           param_3 = CONCAT71((int7)(param_3 >> 8),0x1c);
 63372:           FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 63373:           if ((DAT_1400a04f8 & 0x10) != 0) {
 63374:             DbgPrint("               set clock = 1/40 and enable scramble \n");
 63375:           }
 63376:         }
 63377:         else {
```

### W0368 - `FUN_140041b38` line 63381
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `(byte)param_4`
```c
 63376:         }
 63377:         else {
 63378:           param_4 = 0;
 63379: LAB_140042337:
 63380:           param_3 = CONCAT71((int7)(param_3 >> 8),0x1c);
 63381:           FUN_1400444d0(param_1,0xe5,0x1c,(byte)param_4);
 63382:         }
 63383:         thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63384:         uVar9 = 0;
 63385:       }
 63386:       else {
```

### W0369 - `FUN_140041b38` line 63383
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63378:           param_4 = 0;
 63379: LAB_140042337:
 63380:           param_3 = CONCAT71((int7)(param_3 >> 8),0x1c);
 63381:           FUN_1400444d0(param_1,0xe5,0x1c,(byte)param_4);
 63382:         }
 63383:         thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63384:         uVar9 = 0;
 63385:       }
 63386:       else {
 63387:         uVar6 = iVar5 - 5;
 63388:         uVar7 = (ulonglong)uVar6;
```

### W0370 - `FUN_140041b38` line 63538
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63533:             goto LAB_14004247b;
 63534:           }
 63535:           if (*(char *)(param_1 + 0x1ec) == '\0') {
 63536:             bVar8 = 3;
 63537: LAB_1400421f7:
 63538:             thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63539:           }
 63540:           else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63541:             bVar8 = 7;
 63542:             goto LAB_1400421f7;
 63543:           }
```

### W0371 - `FUN_140041b38` line 63545
- Register: `0x27 `
- Kind: `direct_write`
- Value: `*(char *)(param_1 + 0x164) + -0x80`
```c
 63540:           else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63541:             bVar8 = 7;
 63542:             goto LAB_1400421f7;
 63543:           }
 63544:           uVar12 = (undefined7)(param_3 >> 8);
 63545:           FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 63546:           FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 63547:           cVar1 = *(char *)(param_1 + 0x166) + -0x80;
 63548:           param_3 = CONCAT71(uVar12,cVar1);
 63549:           FUN_140044528(param_1,0x29,cVar1);
 63550:           uVar9 = 0;
```

### W0372 - `FUN_140041b38` line 63546
- Register: `0x28 `
- Kind: `direct_write`
- Value: `*(char *)(param_1 + 0x165) + -0x80`
```c
 63541:             bVar8 = 7;
 63542:             goto LAB_1400421f7;
 63543:           }
 63544:           uVar12 = (undefined7)(param_3 >> 8);
 63545:           FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 63546:           FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 63547:           cVar1 = *(char *)(param_1 + 0x166) + -0x80;
 63548:           param_3 = CONCAT71(uVar12,cVar1);
 63549:           FUN_140044528(param_1,0x29,cVar1);
 63550:           uVar9 = 0;
 63551:           thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
```

### W0373 - `FUN_140041b38` line 63549
- Register: `0x29 `
- Kind: `direct_write`
- Value: `cVar1`
```c
 63544:           uVar12 = (undefined7)(param_3 >> 8);
 63545:           FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 63546:           FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 63547:           cVar1 = *(char *)(param_1 + 0x166) + -0x80;
 63548:           param_3 = CONCAT71(uVar12,cVar1);
 63549:           FUN_140044528(param_1,0x29,cVar1);
 63550:           uVar9 = 0;
 63551:           thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63552:           bVar10 = FUN_140041558(param_1,uVar9,(byte)param_3,(byte)param_4);
 63553:           if (extraout_AL == '\x01') goto LAB_1400422a8;
 63554:           if ((bVar10 & (byte)DAT_1400a04f8) != 0) {
```

### W0374 - `FUN_140041b38` line 63551
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63546:           FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 63547:           cVar1 = *(char *)(param_1 + 0x166) + -0x80;
 63548:           param_3 = CONCAT71(uVar12,cVar1);
 63549:           FUN_140044528(param_1,0x29,cVar1);
 63550:           uVar9 = 0;
 63551:           thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63552:           bVar10 = FUN_140041558(param_1,uVar9,(byte)param_3,(byte)param_4);
 63553:           if (extraout_AL == '\x01') goto LAB_1400422a8;
 63554:           if ((bVar10 & (byte)DAT_1400a04f8) != 0) {
 63555:             DbgPrint("***EQ iTE6805_EQ_chg(STATEEQ_WaitForManualEQ) ***\n");
 63556:           }
```

### W0375 - `FUN_140041b38` line 63563
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63558:           goto LAB_14004247b;
 63559:         }
 63560:         if (*(char *)(param_1 + 0x1ec) == '\0') {
 63561:           bVar8 = 3;
 63562: LAB_140041c72:
 63563:           thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63564:         }
 63565:         else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_140041c72;
 63566:         uVar9 = 0;
 63567:         FUN_140044528(param_1,0x2c,0);
 63568:         param_3 = CONCAT71((int7)((ulonglong)uVar9 >> 8),7);
```

### W0376 - `FUN_140041b38` line 63567
- Register: `0x2C `
- Kind: `direct_write`
- Value: `0`
```c
 63562: LAB_140041c72:
 63563:           thunk_FUN_1400444d0(param_1,bVar8,(byte)param_3,(byte)param_4);
 63564:         }
 63565:         else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_140041c72;
 63566:         uVar9 = 0;
 63567:         FUN_140044528(param_1,0x2c,0);
 63568:         param_3 = CONCAT71((int7)((ulonglong)uVar9 >> 8),7);
 63569:         FUN_140044528(param_1,0x2d,7);
 63570:         if (*(char *)(param_1 + 0x167) == '\0') {
 63571:           if ((DAT_1400a04f8 & 0x10) != 0) {
 63572:             DbgPrint("**Manual EQ Force B RS to 0x%x **\n",
```

### W0377 - `FUN_140041b38` line 63569
- Register: `0x2D `
- Kind: `direct_write`
- Value: `7`
```c
 63564:         }
 63565:         else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_140041c72;
 63566:         uVar9 = 0;
 63567:         FUN_140044528(param_1,0x2c,0);
 63568:         param_3 = CONCAT71((int7)((ulonglong)uVar9 >> 8),7);
 63569:         FUN_140044528(param_1,0x2d,7);
 63570:         if (*(char *)(param_1 + 0x167) == '\0') {
 63571:           if ((DAT_1400a04f8 & 0x10) != 0) {
 63572:             DbgPrint("**Manual EQ Force B RS to 0x%x **\n",
 63573:                      *(undefined1 *)(uVar7 + *(longlong *)(param_1 + 0x130)));
 63574:           }
```

### W0378 - `FUN_140041b38` line 63577
- Register: `0x27 `
- Kind: `direct_write`
- Value: `bVar8 | 0x80`
```c
 63572:             DbgPrint("**Manual EQ Force B RS to 0x%x **\n",
 63573:                      *(undefined1 *)(uVar7 + *(longlong *)(param_1 + 0x130)));
 63574:           }
 63575:           bVar8 = *(byte *)(uVar7 + *(longlong *)(param_1 + 0x130));
 63576:           param_3 = CONCAT71((int7)(param_3 >> 8),bVar8) | 0x80;
 63577:           FUN_140044528(param_1,0x27,bVar8 | 0x80);
 63578:           if ((*(char *)(param_1 + 0x179) == '\x01') && (*(char *)(param_1 + 0x176) == '\0')) {
 63579:             param_3 = 0;
 63580:             FUN_140043370(param_1,*(char *)(*(longlong *)(param_1 + 0x130) + uVar7),0,(byte)param_4)
 63581:             ;
 63582:           }
```

### W0379 - `FUN_140041b38` line 63591
- Register: `0x28 `
- Kind: `direct_write`
- Value: `bVar8 | 0x80`
```c
 63586:             DbgPrint("**Manual EQ Force G RS to 0x%x **\n",
 63587:                      *(undefined1 *)(uVar7 + *(longlong *)(param_1 + 0x130)));
 63588:           }
 63589:           bVar8 = *(byte *)(uVar7 + *(longlong *)(param_1 + 0x130));
 63590:           param_3 = CONCAT71((int7)(param_3 >> 8),bVar8) | 0x80;
 63591:           FUN_140044528(param_1,0x28,bVar8 | 0x80);
 63592:           if ((*(char *)(param_1 + 0x179) == '\x01') && (*(char *)(param_1 + 0x176) == '\0')) {
 63593:             param_3 = CONCAT71((int7)(param_3 >> 8),1);
 63594:             FUN_140043370(param_1,*(char *)(*(longlong *)(param_1 + 0x130) + uVar7),1,(byte)param_4)
 63595:             ;
 63596:           }
```

### W0380 - `FUN_140041b38` line 63605
- Register: `0x29 `
- Kind: `direct_write`
- Value: `bVar8 | 0x80`
```c
 63600:             DbgPrint("**Manual EQ Force R RS to 0x%x **\n",
 63601:                      *(undefined1 *)(uVar7 + *(longlong *)(param_1 + 0x130)));
 63602:           }
 63603:           bVar8 = *(byte *)(uVar7 + *(longlong *)(param_1 + 0x130));
 63604:           param_3 = CONCAT71((int7)(param_3 >> 8),bVar8) | 0x80;
 63605:           FUN_140044528(param_1,0x29,bVar8 | 0x80);
 63606:           if ((*(char *)(param_1 + 0x179) == '\x01') && (*(char *)(param_1 + 0x176) == '\0')) {
 63607:             param_3 = CONCAT71((int7)(param_3 >> 8),2);
 63608:             FUN_140043370(param_1,*(char *)(uVar7 + *(longlong *)(param_1 + 0x130)),2,(byte)param_4)
 63609:             ;
 63610:           }
```

### W0381 - `FUN_140041b38` line 63612
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63607:             param_3 = CONCAT71((int7)(param_3 >> 8),2);
 63608:             FUN_140043370(param_1,*(char *)(uVar7 + *(longlong *)(param_1 + 0x130)),2,(byte)param_4)
 63609:             ;
 63610:           }
 63611:         }
 63612:         thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63613:         uVar9 = 4;
 63614:       }
 63615:     }
 63616: LAB_14004247b:
 63617:     FUN_140041b38(param_1,uVar9,param_3,param_4);
```

### W0382 - `FUN_140041b38` line 63629
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63624:     bVar16 = 0x40;
 63625:   }
 63626:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63627:     bVar8 = 3;
 63628: LAB_1400425fc:
 63629:     thunk_FUN_1400444d0(param_1,bVar8,bVar17,bVar13);
 63630:   }
 63631:   else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_1400425fc;
 63632:   param_4 = (ulonglong)bVar16;
 63633:   param_3 = 0x40;
 63634:   FUN_1400444d0(param_1,0xa7,0x40,bVar16);
```

### W0383 - `FUN_140041b38` line 63634
- Register: `0xA7 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `bVar16`
```c
 63629:     thunk_FUN_1400444d0(param_1,bVar8,bVar17,bVar13);
 63630:   }
 63631:   else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_1400425fc;
 63632:   param_4 = (ulonglong)bVar16;
 63633:   param_3 = 0x40;
 63634:   FUN_1400444d0(param_1,0xa7,0x40,bVar16);
 63635:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63636: LAB_140042642:
 63637:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63638:   return;
 63639: }
```

### W0384 - `FUN_140041b38` line 63635
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63630:   }
 63631:   else if (*(char *)(param_1 + 0x1ec) == '\x01') goto LAB_1400425fc;
 63632:   param_4 = (ulonglong)bVar16;
 63633:   param_3 = 0x40;
 63634:   FUN_1400444d0(param_1,0xa7,0x40,bVar16);
 63635:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63636: LAB_140042642:
 63637:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63638:   return;
 63639: }
 63640: 
```

### W0385 - `FUN_140041b38` line 63637
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63632:   param_4 = (ulonglong)bVar16;
 63633:   param_3 = 0x40;
 63634:   FUN_1400444d0(param_1,0xa7,0x40,bVar16);
 63635:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63636: LAB_140042642:
 63637:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 63638:   return;
 63639: }
 63640: 
 63641: 
 63642: 
```

### W0386 - `FUN_14004265c` line 63766
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63761:     uVar5 = 5;
 63762:   }
 63763: LAB_1400428bf:
 63764:   FUN_140041b38(param_1,uVar5,CONCAT71(uVar7,bVar6),CONCAT71(uVar9,bVar8));
 63765: LAB_1400428c7:
 63766:   thunk_FUN_1400444d0(param_1,0,bVar6,bVar8);
 63767:   return;
 63768: }
 63769: 
 63770: 
 63771: 
```

### W0387 - `FUN_1400428d8` line 63812
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63807:     return;
 63808:   }
 63809:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63810:     bVar3 = 3;
 63811: LAB_14004294b:
 63812:     thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4);
 63813:   }
 63814:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63815:     bVar3 = 7;
 63816:     goto LAB_14004294b;
 63817:   }
```

### W0388 - `FUN_1400428d8` line 63822
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0`
```c
 63817:   }
 63818:   if ((DAT_1400a04f8 & 0x10) != 0) {
 63819:     DbgPrint("!!!!!!!!!!!!\n");
 63820:   }
 63821:   if (param_2 == 0) {
 63822:     FUN_1400444d0(param_1,0x37,0xc0,0);
 63823:     cVar2 = FUN_140044484(param_1,0xd5);
 63824:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63825:       pcVar9 = "!! Report Channel B DFE Value , EQ TargetRS = 0x%x!!\n";
 63826: LAB_140042a1a:
 63827:       DbgPrint(pcVar9,cVar2);
```

### W0389 - `FUN_1400428d8` line 63831
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0x40`
```c
 63826: LAB_140042a1a:
 63827:       DbgPrint(pcVar9,cVar2);
 63828:     }
 63829:   }
 63830:   else if (param_2 == 1) {
 63831:     FUN_1400444d0(param_1,0x37,0xc0,0x40);
 63832:     cVar2 = FUN_140044484(param_1,0xd6);
 63833:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63834:       pcVar9 = "!! Report Channel G DFE Value , EQ TargetRS = 0x%x!!\n";
 63835:       goto LAB_140042a1a;
 63836:     }
```

### W0390 - `FUN_1400428d8` line 63839
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0x80`
```c
 63834:       pcVar9 = "!! Report Channel G DFE Value , EQ TargetRS = 0x%x!!\n";
 63835:       goto LAB_140042a1a;
 63836:     }
 63837:   }
 63838:   else if (param_2 == 2) {
 63839:     FUN_1400444d0(param_1,0x37,0xc0,0x80);
 63840:     cVar2 = FUN_140044484(param_1,0xd7);
 63841:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63842:       pcVar9 = "!! Report Channel R DFE Value , EQ TargetRS = 0x%x!!\n";
 63843:       goto LAB_140042a1a;
 63844:     }
```

### W0391 - `FUN_1400428d8` line 63858
- Register: `0x36 `
- Kind: `rmw_write`
- Mask: `0xf`
- Value: `local_58`
```c
 63853:   local_48 = (char *)(param_1 + 0xb0 + (ulonglong)param_2 * 3);
 63854:   local_58 = 0;
 63855:   pcVar9 = "\x7f~?>\x1f\x1e\x0f\x0e\a\x06\x03\x02\x01";
 63856:   cVar13 = cVar2;
 63857:   do {
 63858:     FUN_1400444d0(param_1,0x36,0xf,local_58);
 63859:     bVar3 = FUN_140044484(param_1,0x5d);
 63860:     uVar17 = (uint)bVar3;
 63861:     bVar4 = FUN_140044484(param_1,0x5e);
 63862:     bVar5 = FUN_140044484(param_1,0x5f);
 63863:     bVar6 = FUN_140044484(param_1,0x60);
```

### W0392 - `FUN_1400428d8` line 63974
- Register: `0x27 `
- Kind: `direct_write`
- Value: `cVar13 + -0x80`
```c
 63969:   goto LAB_140042e55;
 63970:   if ((DAT_1400a04f8 & 0x10) != 0) {
 63971:     DbgPrint("#   Adjust RS to %02x     #\n");
 63972:   }
 63973:   if (param_2 == 0) {
 63974:     FUN_140044528(param_1,0x27,cVar13 + -0x80);
 63975:     bVar15 = 0;
 63976:     *(char *)(param_1 + 0x164) = cVar13;
 63977:   }
 63978:   else {
 63979:     if (param_2 != 1) {
```

### W0393 - `FUN_1400428d8` line 63981
- Register: `0x29 `
- Kind: `direct_write`
- Value: `cVar13 + -0x80`
```c
 63976:     *(char *)(param_1 + 0x164) = cVar13;
 63977:   }
 63978:   else {
 63979:     if (param_2 != 1) {
 63980:       if (param_2 != 2) goto LAB_140042e55;
 63981:       FUN_140044528(param_1,0x29,cVar13 + -0x80);
 63982:       bVar3 = 2;
 63983:       *(char *)(param_1 + 0x166) = cVar13;
 63984:       FUN_140043370(param_1,cVar13,2,bVar10);
 63985:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 63986:         bVar4 = 3;
```

### W0394 - `FUN_1400428d8` line 63988
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 63983:       *(char *)(param_1 + 0x166) = cVar13;
 63984:       FUN_140043370(param_1,cVar13,2,bVar10);
 63985:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 63986:         bVar4 = 3;
 63987: LAB_140042df8:
 63988:         thunk_FUN_1400444d0(param_1,bVar4,bVar3,bVar10);
 63989:       }
 63990:       else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63991:         bVar4 = 7;
 63992:         goto LAB_140042df8;
 63993:       }
```

### W0395 - `FUN_1400428d8` line 63996
- Register: `0x4B `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 63991:         bVar4 = 7;
 63992:         goto LAB_140042df8;
 63993:       }
 63994:       bVar10 = 0x80;
 63995:       bVar15 = 0x80;
 63996:       FUN_1400444d0(param_1,0x4b,0x80,0x80);
 63997:       goto LAB_140042e55;
 63998:     }
 63999:     FUN_140044528(param_1,0x28,cVar13 + -0x80);
 64000:     bVar15 = 1;
 64001:     *(char *)(param_1 + 0x165) = cVar13;
```

### W0396 - `FUN_1400428d8` line 63999
- Register: `0x28 `
- Kind: `direct_write`
- Value: `cVar13 + -0x80`
```c
 63994:       bVar10 = 0x80;
 63995:       bVar15 = 0x80;
 63996:       FUN_1400444d0(param_1,0x4b,0x80,0x80);
 63997:       goto LAB_140042e55;
 63998:     }
 63999:     FUN_140044528(param_1,0x28,cVar13 + -0x80);
 64000:     bVar15 = 1;
 64001:     *(char *)(param_1 + 0x165) = cVar13;
 64002:   }
 64003:   FUN_140043370(param_1,cVar13,bVar15,bVar10);
 64004: LAB_140042e55:
```

### W0397 - `FUN_1400428d8` line 64005
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64000:     bVar15 = 1;
 64001:     *(char *)(param_1 + 0x165) = cVar13;
 64002:   }
 64003:   FUN_140043370(param_1,cVar13,bVar15,bVar10);
 64004: LAB_140042e55:
 64005:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar10);
 64006:   return;
 64007: }
 64008: 
 64009: 
 64010: 
```

### W0398 - `FUN_140042e78` line 64023
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64018:   }
 64019:   else {
 64020:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140042ea0;
 64021:     bVar1 = 7;
 64022:   }
 64023:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64024: LAB_140042ea0:
 64025:   bVar1 = FUN_140044484(param_1,0xd5);
 64026:   *(byte *)(param_1 + 0x164) = bVar1 & 0x7f;
 64027:   bVar1 = FUN_140044484(param_1,0xd6);
 64028:   *(byte *)(param_1 + 0x165) = bVar1 & 0x7f;
```

### W0399 - `FUN_140042e78` line 64049
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64044:     DbgPrint(&LAB_14006b500);
 64045:   }
 64046:   *(undefined1 *)(param_1 + 0x1d1) = *(undefined1 *)(param_1 + 0x164);
 64047:   *(undefined1 *)(param_1 + 0x1d2) = *(undefined1 *)(param_1 + 0x165);
 64048:   *(undefined1 *)(param_1 + 0x1d3) = *(undefined1 *)(param_1 + 0x166);
 64049:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64050:   return;
 64051: }
 64052: 
 64053: 
 64054: 
```

### W0400 - `FUN_140042f8c` line 64069
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64064: 
 64065:   bVar5 = 0;
 64066:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64067:     bVar1 = 3;
 64068: LAB_140042fc3:
 64069:     thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64070:   }
 64071:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64072:     bVar1 = 7;
 64073:     goto LAB_140042fc3;
 64074:   }
```

### W0401 - `FUN_140042f8c` line 64127
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `bVar6`
```c
 64122:   DbgPrint(pcVar4);
 64123: LAB_140043099:
 64124:   do {
 64125:     bVar1 = 0xc0;
 64126:     bVar6 = bVar5 << 6;
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
```

### W0402 - `FUN_140042f8c` line 64144
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64139:       bVar6 = bVar3;
 64140:       DbgPrint(" SKEW00 : BErr =%d, SKEWP0 : BErr =%d, SKEWP1 : BErr =%d \n\n");
 64141:     }
 64142:     bVar5 = bVar5 + 1;
 64143:   } while (bVar5 < 3);
 64144:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar6);
 64145:   return;
 64146: }
 64147: 
 64148: 
 64149: 
```

### W0403 - `FUN_140043184` line 64166
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64161:   }
 64162:   else {
 64163:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_1400431d3;
 64164:     bVar1 = 7;
 64165:   }
 64166:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64167: LAB_1400431d3:
 64168:   FUN_140044528(param_1,0x2c,0);
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
```

### W0404 - `FUN_140043184` line 64168
- Register: `0x2C `
- Kind: `direct_write`
- Value: `0`
```c
 64163:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_1400431d3;
 64164:     bVar1 = 7;
 64165:   }
 64166:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64167: LAB_1400431d3:
 64168:   FUN_140044528(param_1,0x2c,0);
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
```

### W0405 - `FUN_140043184` line 64170
- Register: `0x2D `
- Kind: `direct_write`
- Value: `7`
```c
 64165:   }
 64166:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64167: LAB_1400431d3:
 64168:   FUN_140044528(param_1,0x2c,0);
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
 64174:     FUN_140044528(param_1,0x23,0xb0);
 64175:     FUN_140048644(param_1,1);
```

### W0406 - `FUN_140043184` line 64171
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64166:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64167: LAB_1400431d3:
 64168:   FUN_140044528(param_1,0x2c,0);
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
 64174:     FUN_140044528(param_1,0x23,0xb0);
 64175:     FUN_140048644(param_1,1);
 64176:     bVar1 = 0xa0;
```

### W0407 - `FUN_140043184` line 64173
- Register: `0x07 `
- Kind: `direct_write`
- Value: `0xff`
```c
 64168:   FUN_140044528(param_1,0x2c,0);
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
 64174:     FUN_140044528(param_1,0x23,0xb0);
 64175:     FUN_140048644(param_1,1);
 64176:     bVar1 = 0xa0;
 64177:     FUN_140044528(param_1,0x23,0xa0);
 64178:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0408 - `FUN_140043184` line 64174
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64169:   bVar1 = 7;
 64170:   FUN_140044528(param_1,0x2d,7);
 64171:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
 64174:     FUN_140044528(param_1,0x23,0xb0);
 64175:     FUN_140048644(param_1,1);
 64176:     bVar1 = 0xa0;
 64177:     FUN_140044528(param_1,0x23,0xa0);
 64178:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64179:       param_4 = 2;
```

### W0409 - `FUN_140043184` line 64177
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64172:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64173:     FUN_140044528(param_1,7,0xff);
 64174:     FUN_140044528(param_1,0x23,0xb0);
 64175:     FUN_140048644(param_1,1);
 64176:     bVar1 = 0xa0;
 64177:     FUN_140044528(param_1,0x23,0xa0);
 64178:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64179:       param_4 = 2;
 64180:       bVar1 = 2;
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
```

### W0410 - `FUN_140043184` line 64181
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 64176:     bVar1 = 0xa0;
 64177:     FUN_140044528(param_1,0x23,0xa0);
 64178:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64179:       param_4 = 2;
 64180:       bVar1 = 2;
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
```

### W0411 - `FUN_140043184` line 64183
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64178:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64179:       param_4 = 2;
 64180:       bVar1 = 2;
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
```

### W0412 - `FUN_140043184` line 64184
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64179:       param_4 = 2;
 64180:       bVar1 = 2;
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
```

### W0413 - `FUN_140043184` line 64185
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64180:       bVar1 = 2;
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
```

### W0414 - `FUN_140043184` line 64186
- Register: `0x29 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64181:       FUN_1400444d0(param_1,0x23,2,2);
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64191:   }
```

### W0415 - `FUN_140043184` line 64187
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0`
```c
 64182:     }
 64183:     thunk_FUN_1400444d0(param_1,3,bVar1,param_4);
 64184:     FUN_140044528(param_1,0x27,0x9f);
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64191:   }
 64192:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
```

### W0416 - `FUN_140043184` line 64190
- Register: `0x4B `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64185:     FUN_140044528(param_1,0x28,0x9f);
 64186:     FUN_140044528(param_1,0x29,0x9f);
 64187:     FUN_140044528(param_1,0x22,0);
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64191:   }
 64192:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64193:     FUN_140044528(param_1,0xc,0xff);
 64194:     FUN_140044528(param_1,0x2b,0xb0);
 64195:     FUN_140048644(param_1,1);
```

### W0417 - `FUN_140043184` line 64193
- Register: `0x0C `
- Kind: `direct_write`
- Value: `0xff`
```c
 64188:     param_4 = 0;
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64191:   }
 64192:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64193:     FUN_140044528(param_1,0xc,0xff);
 64194:     FUN_140044528(param_1,0x2b,0xb0);
 64195:     FUN_140048644(param_1,1);
 64196:     bVar1 = 0xa0;
 64197:     FUN_140044528(param_1,0x2b,0xa0);
 64198:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0418 - `FUN_140043184` line 64194
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64189:     bVar1 = 0x80;
 64190:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64191:   }
 64192:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64193:     FUN_140044528(param_1,0xc,0xff);
 64194:     FUN_140044528(param_1,0x2b,0xb0);
 64195:     FUN_140048644(param_1,1);
 64196:     bVar1 = 0xa0;
 64197:     FUN_140044528(param_1,0x2b,0xa0);
 64198:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64199:       param_4 = 2;
```

### W0419 - `FUN_140043184` line 64197
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64192:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64193:     FUN_140044528(param_1,0xc,0xff);
 64194:     FUN_140044528(param_1,0x2b,0xb0);
 64195:     FUN_140048644(param_1,1);
 64196:     bVar1 = 0xa0;
 64197:     FUN_140044528(param_1,0x2b,0xa0);
 64198:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64199:       param_4 = 2;
 64200:       bVar1 = 2;
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
```

### W0420 - `FUN_140043184` line 64201
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 64196:     bVar1 = 0xa0;
 64197:     FUN_140044528(param_1,0x2b,0xa0);
 64198:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64199:       param_4 = 2;
 64200:       bVar1 = 2;
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
```

### W0421 - `FUN_140043184` line 64203
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64198:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64199:       param_4 = 2;
 64200:       bVar1 = 2;
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
```

### W0422 - `FUN_140043184` line 64204
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64199:       param_4 = 2;
 64200:       bVar1 = 2;
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
```

### W0423 - `FUN_140043184` line 64205
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64200:       bVar1 = 2;
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
 64210:     FUN_1400444d0(param_1,0x4b,0x80,0);
```

### W0424 - `FUN_140043184` line 64206
- Register: `0x29 `
- Kind: `direct_write`
- Value: `0x9f`
```c
 64201:       FUN_1400444d0(param_1,0x2b,2,2);
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
 64210:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64211:   }
```

### W0425 - `FUN_140043184` line 64207
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0`
```c
 64202:     }
 64203:     thunk_FUN_1400444d0(param_1,7,bVar1,param_4);
 64204:     FUN_140044528(param_1,0x27,0x9f);
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
 64210:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64211:   }
 64212:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
```

### W0426 - `FUN_140043184` line 64210
- Register: `0x4B `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64205:     FUN_140044528(param_1,0x28,0x9f);
 64206:     FUN_140044528(param_1,0x29,0x9f);
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
 64210:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64211:   }
 64212:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64213:   return;
 64214: }
 64215: 
```

### W0427 - `FUN_140043184` line 64212
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64207:     FUN_140044528(param_1,0x22,0);
 64208:     param_4 = 0;
 64209:     bVar1 = 0x80;
 64210:     FUN_1400444d0(param_1,0x4b,0x80,0);
 64211:   }
 64212:   thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 64213:   return;
 64214: }
 64215: 
 64216: 
 64217: 
```

### W0428 - `FUN_140043370` line 64240
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64235: LAB_1400433a0:
 64236:   bVar6 = param_3;
 64237:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64238:     bVar3 = 3;
 64239: LAB_1400433ba:
 64240:     thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4);
 64241:   }
 64242:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64243:     bVar3 = 7;
 64244:     goto LAB_1400433ba;
 64245:   }
```

### W0429 - `FUN_140043370` line 64249
- Register: `0x4B `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xaf + lVar1)`
```c
 64244:     goto LAB_1400433ba;
 64245:   }
 64246:   if (param_3 == 0) {
 64247:     uVar5 = (ulonglong)bVar4;
 64248:     lVar1 = param_1 + uVar5 * 8;
 64249:     FUN_140044528(param_1,0x4b,*(undefined1 *)(uVar5 + 0xaf + lVar1));
 64250:     FUN_140044528(param_1,0x4c,*(undefined1 *)(uVar5 + 0xb0 + lVar1));
 64251:     bVar6 = *(byte *)(uVar5 + 0xb1 + lVar1);
 64252:     FUN_140044528(param_1,0x4d,bVar6);
 64253:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64254:       DbgPrint("--------------setting Channel B DFE--------------\n");
```

### W0430 - `FUN_140043370` line 64250
- Register: `0x4C `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xb0 + lVar1)`
```c
 64245:   }
 64246:   if (param_3 == 0) {
 64247:     uVar5 = (ulonglong)bVar4;
 64248:     lVar1 = param_1 + uVar5 * 8;
 64249:     FUN_140044528(param_1,0x4b,*(undefined1 *)(uVar5 + 0xaf + lVar1));
 64250:     FUN_140044528(param_1,0x4c,*(undefined1 *)(uVar5 + 0xb0 + lVar1));
 64251:     bVar6 = *(byte *)(uVar5 + 0xb1 + lVar1);
 64252:     FUN_140044528(param_1,0x4d,bVar6);
 64253:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64254:       DbgPrint("--------------setting Channel B DFE--------------\n");
 64255:     }
```

### W0431 - `FUN_140043370` line 64252
- Register: `0x4D `
- Kind: `direct_write`
- Value: `bVar6`
```c
 64247:     uVar5 = (ulonglong)bVar4;
 64248:     lVar1 = param_1 + uVar5 * 8;
 64249:     FUN_140044528(param_1,0x4b,*(undefined1 *)(uVar5 + 0xaf + lVar1));
 64250:     FUN_140044528(param_1,0x4c,*(undefined1 *)(uVar5 + 0xb0 + lVar1));
 64251:     bVar6 = *(byte *)(uVar5 + 0xb1 + lVar1);
 64252:     FUN_140044528(param_1,0x4d,bVar6);
 64253:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64254:       DbgPrint("--------------setting Channel B DFE--------------\n");
 64255:     }
 64256:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64257:       FUN_140044484(param_1,0x4b);
```

### W0432 - `FUN_140043370` line 64272
- Register: `0x4E `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xb2 + lVar1)`
```c
 64267:   }
 64268:   else {
 64269:     if (param_3 == 1) {
 64270:       uVar5 = (ulonglong)bVar4;
 64271:       lVar1 = param_1 + uVar5 * 8;
 64272:       FUN_140044528(param_1,0x4e,*(undefined1 *)(uVar5 + 0xb2 + lVar1));
 64273:       if (*(char *)(param_1 + 0x1d4) != -0x60) {
 64274:         FUN_140044528(param_1,0x4f,*(undefined1 *)(uVar5 + 0xb3 + lVar1));
 64275:       }
 64276:       bVar6 = *(byte *)(uVar5 * 9 + 0xb4 + param_1);
 64277:       FUN_140044528(param_1,0x50,bVar6);
```

### W0433 - `FUN_140043370` line 64274
- Register: `0x4F `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xb3 + lVar1)`
```c
 64269:     if (param_3 == 1) {
 64270:       uVar5 = (ulonglong)bVar4;
 64271:       lVar1 = param_1 + uVar5 * 8;
 64272:       FUN_140044528(param_1,0x4e,*(undefined1 *)(uVar5 + 0xb2 + lVar1));
 64273:       if (*(char *)(param_1 + 0x1d4) != -0x60) {
 64274:         FUN_140044528(param_1,0x4f,*(undefined1 *)(uVar5 + 0xb3 + lVar1));
 64275:       }
 64276:       bVar6 = *(byte *)(uVar5 * 9 + 0xb4 + param_1);
 64277:       FUN_140044528(param_1,0x50,bVar6);
 64278:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64279:         DbgPrint("--------------setting Channel G DFE--------------\n");
```

### W0434 - `FUN_140043370` line 64277
- Register: `0x50 `
- Kind: `direct_write`
- Value: `bVar6`
```c
 64272:       FUN_140044528(param_1,0x4e,*(undefined1 *)(uVar5 + 0xb2 + lVar1));
 64273:       if (*(char *)(param_1 + 0x1d4) != -0x60) {
 64274:         FUN_140044528(param_1,0x4f,*(undefined1 *)(uVar5 + 0xb3 + lVar1));
 64275:       }
 64276:       bVar6 = *(byte *)(uVar5 * 9 + 0xb4 + param_1);
 64277:       FUN_140044528(param_1,0x50,bVar6);
 64278:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64279:         DbgPrint("--------------setting Channel G DFE--------------\n");
 64280:       }
 64281:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64282:         FUN_140044484(param_1,0x4e);
```

### W0435 - `FUN_140043370` line 64301
- Register: `0x51 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xb5 + lVar1)`
```c
 64296:       goto LAB_140043646;
 64297:     }
 64298:     if (param_3 != 2) goto LAB_140043646;
 64299:     uVar5 = (ulonglong)bVar4;
 64300:     lVar1 = param_1 + uVar5 * 8;
 64301:     FUN_140044528(param_1,0x51,*(undefined1 *)(uVar5 + 0xb5 + lVar1));
 64302:     FUN_140044528(param_1,0x52,*(undefined1 *)(uVar5 + 0xb6 + lVar1));
 64303:     bVar6 = *(byte *)(uVar5 + 0xb7 + lVar1);
 64304:     FUN_140044528(param_1,0x53,bVar6);
 64305:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64306:       DbgPrint("--------------setting Channel R DFE--------------\n");
```

### W0436 - `FUN_140043370` line 64302
- Register: `0x52 `
- Kind: `direct_write`
- Value: `*(undefined1 *)(uVar5 + 0xb6 + lVar1)`
```c
 64297:     }
 64298:     if (param_3 != 2) goto LAB_140043646;
 64299:     uVar5 = (ulonglong)bVar4;
 64300:     lVar1 = param_1 + uVar5 * 8;
 64301:     FUN_140044528(param_1,0x51,*(undefined1 *)(uVar5 + 0xb5 + lVar1));
 64302:     FUN_140044528(param_1,0x52,*(undefined1 *)(uVar5 + 0xb6 + lVar1));
 64303:     bVar6 = *(byte *)(uVar5 + 0xb7 + lVar1);
 64304:     FUN_140044528(param_1,0x53,bVar6);
 64305:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64306:       DbgPrint("--------------setting Channel R DFE--------------\n");
 64307:     }
```

### W0437 - `FUN_140043370` line 64304
- Register: `0x53 `
- Kind: `direct_write`
- Value: `bVar6`
```c
 64299:     uVar5 = (ulonglong)bVar4;
 64300:     lVar1 = param_1 + uVar5 * 8;
 64301:     FUN_140044528(param_1,0x51,*(undefined1 *)(uVar5 + 0xb5 + lVar1));
 64302:     FUN_140044528(param_1,0x52,*(undefined1 *)(uVar5 + 0xb6 + lVar1));
 64303:     bVar6 = *(byte *)(uVar5 + 0xb7 + lVar1);
 64304:     FUN_140044528(param_1,0x53,bVar6);
 64305:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64306:       DbgPrint("--------------setting Channel R DFE--------------\n");
 64307:     }
 64308:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64309:       FUN_140044484(param_1,0x51);
```

### W0438 - `FUN_140043370` line 64322
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64317:     FUN_140044484(param_1,0x53);
 64318:     pcVar2 = "-setting 53 to 0x%x -\n";
 64319:   }
 64320:   DbgPrint(pcVar2);
 64321: LAB_140043646:
 64322:   thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
 64323:   return;
 64324: }
 64325: 
 64326: 
 64327: 
```

### W0439 - `FUN_140043660` line 64350
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64345:   }
 64346:   else {
 64347:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_1400436ab;
 64348:     bVar9 = 7;
 64349:   }
 64350:   thunk_FUN_1400444d0(param_1,bVar9,param_3,param_4);
 64351: LAB_1400436ab:
 64352:   bVar9 = 0;
 64353:   psVar8 = (short *)(param_1 + 0x17e);
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
```

### W0440 - `FUN_140043660` line 64355
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `bVar9 << 6`
```c
 64350:   thunk_FUN_1400444d0(param_1,bVar9,param_3,param_4);
 64351: LAB_1400436ab:
 64352:   bVar9 = 0;
 64353:   psVar8 = (short *)(param_1 + 0x17e);
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
 64356:     bVar2 = FUN_140044484(param_1,99);
 64357:     bVar3 = FUN_140044484(param_1,100);
 64358:     bVar4 = FUN_140044484(param_1,0x6d);
 64359:     bVar5 = FUN_140044484(param_1,0x6e);
 64360:     sVar7 = (ushort)bVar5 * 0x100 + (ushort)bVar4;
```

### W0441 - `FUN_140043810` line 64402
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64397:   byte bVar2;
 64398:   byte bVar3;
 64399:   undefined1 uVar4;
 64400: 
 64401:   *(undefined4 *)(param_1 + 0x170) = 0;
 64402:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64403:   bVar2 = 0;
 64404:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64405:     FUN_140044528(param_1,7,0xff);
 64406:     FUN_140044528(param_1,0x23,0xb0);
 64407:     FUN_140048644(param_1,10);
```

### W0442 - `FUN_140043810` line 64405
- Register: `0x07 `
- Kind: `direct_write`
- Value: `0xff`
```c
 64400: 
 64401:   *(undefined4 *)(param_1 + 0x170) = 0;
 64402:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64403:   bVar2 = 0;
 64404:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64405:     FUN_140044528(param_1,7,0xff);
 64406:     FUN_140044528(param_1,0x23,0xb0);
 64407:     FUN_140048644(param_1,10);
 64408:     param_3 = 0xa0;
 64409:     FUN_140044528(param_1,0x23,0xa0);
 64410:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0443 - `FUN_140043810` line 64406
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64401:   *(undefined4 *)(param_1 + 0x170) = 0;
 64402:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64403:   bVar2 = 0;
 64404:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64405:     FUN_140044528(param_1,7,0xff);
 64406:     FUN_140044528(param_1,0x23,0xb0);
 64407:     FUN_140048644(param_1,10);
 64408:     param_3 = 0xa0;
 64409:     FUN_140044528(param_1,0x23,0xa0);
 64410:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64411:       param_4 = 2;
```

### W0444 - `FUN_140043810` line 64409
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64404:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64405:     FUN_140044528(param_1,7,0xff);
 64406:     FUN_140044528(param_1,0x23,0xb0);
 64407:     FUN_140048644(param_1,10);
 64408:     param_3 = 0xa0;
 64409:     FUN_140044528(param_1,0x23,0xa0);
 64410:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64411:       param_4 = 2;
 64412:       param_3 = 2;
 64413:       FUN_1400444d0(param_1,0x23,2,2);
 64414:     }
```

### W0445 - `FUN_140043810` line 64413
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 64408:     param_3 = 0xa0;
 64409:     FUN_140044528(param_1,0x23,0xa0);
 64410:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64411:       param_4 = 2;
 64412:       param_3 = 2;
 64413:       FUN_1400444d0(param_1,0x23,2,2);
 64414:     }
 64415:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64416:       uVar1 = FUN_140044484(param_1,0x14);
 64417:       DbgPrint("******hdmirxrd(0x14) = 0x%X ******\n",uVar1);
 64418:     }
```

### W0446 - `FUN_140043810` line 64423
- Register: `0x0C `
- Kind: `direct_write`
- Value: `0xff`
```c
 64418:     }
 64419:     bVar2 = FUN_140044484(param_1,0x14);
 64420:     bVar2 = bVar2 & 1;
 64421:   }
 64422:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64423:     FUN_140044528(param_1,0xc,0xff);
 64424:     FUN_140044528(param_1,0x2b,0xb0);
 64425:     FUN_140048644(param_1,10);
 64426:     param_3 = 0xa0;
 64427:     FUN_140044528(param_1,0x2b,0xa0);
 64428:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0447 - `FUN_140043810` line 64424
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64419:     bVar2 = FUN_140044484(param_1,0x14);
 64420:     bVar2 = bVar2 & 1;
 64421:   }
 64422:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64423:     FUN_140044528(param_1,0xc,0xff);
 64424:     FUN_140044528(param_1,0x2b,0xb0);
 64425:     FUN_140048644(param_1,10);
 64426:     param_3 = 0xa0;
 64427:     FUN_140044528(param_1,0x2b,0xa0);
 64428:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64429:       param_4 = 2;
```

### W0448 - `FUN_140043810` line 64427
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64422:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64423:     FUN_140044528(param_1,0xc,0xff);
 64424:     FUN_140044528(param_1,0x2b,0xb0);
 64425:     FUN_140048644(param_1,10);
 64426:     param_3 = 0xa0;
 64427:     FUN_140044528(param_1,0x2b,0xa0);
 64428:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64429:       param_4 = 2;
 64430:       param_3 = 2;
 64431:       FUN_1400444d0(param_1,0x2b,2,2);
 64432:     }
```

### W0449 - `FUN_140043810` line 64431
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 64426:     param_3 = 0xa0;
 64427:     FUN_140044528(param_1,0x2b,0xa0);
 64428:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64429:       param_4 = 2;
 64430:       param_3 = 2;
 64431:       FUN_1400444d0(param_1,0x2b,2,2);
 64432:     }
 64433:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64434:       uVar1 = FUN_140044484(param_1,0x17);
 64435:       DbgPrint("******hdmirxrd(0x17) = 0x%X ******\n",uVar1);
 64436:     }
```

### W0450 - `FUN_140043810` line 64451
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64446:   }
 64447:   else {
 64448:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043988;
 64449:     bVar3 = 7;
 64450:   }
 64451:   thunk_FUN_1400444d0(param_1,bVar3,param_3,param_4);
 64452: LAB_140043988:
 64453:   if (bVar2 == 1) {
 64454:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64455:       DbgPrint("******EQ speed 1G ~ 3G ******\n");
 64456:     }
```

### W0451 - `FUN_140043810` line 64466
- Register: `0x20 `
- Kind: `direct_write`
- Value: `uVar4`
```c
 64461:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64462:       DbgPrint("******EQ speed under 1G ******\n");
 64463:     }
 64464:     uVar4 = 0x1b;
 64465:   }
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
```

### W0452 - `FUN_140043810` line 64467
- Register: `0x21 `
- Kind: `direct_write`
- Value: `uVar1`
```c
 64462:       DbgPrint("******EQ speed under 1G ******\n");
 64463:     }
 64464:     uVar4 = 0x1b;
 64465:   }
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
```

### W0453 - `FUN_140043810` line 64468
- Register: `0x26 `
- Kind: `direct_write`
- Value: `0`
```c
 64463:     }
 64464:     uVar4 = 0x1b;
 64465:   }
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
```

### W0454 - `FUN_140043810` line 64469
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0`
```c
 64464:     uVar4 = 0x1b;
 64465:   }
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
```

### W0455 - `FUN_140043810` line 64470
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0`
```c
 64465:   }
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
```

### W0456 - `FUN_140043810` line 64471
- Register: `0x29 `
- Kind: `direct_write`
- Value: `0`
```c
 64466:   FUN_140044528(param_1,0x20,uVar4);
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
 64476:   bVar3 = 0;
```

### W0457 - `FUN_140043810` line 64472
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x38`
```c
 64467:   FUN_140044528(param_1,0x21,uVar1);
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
 64476:   bVar3 = 0;
 64477:   FUN_1400444d0(param_1,0x22,4,0);
```

### W0458 - `FUN_140043810` line 64473
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 64468:   FUN_140044528(param_1,0x26,0);
 64469:   FUN_140044528(param_1,0x27,0);
 64470:   FUN_140044528(param_1,0x28,0);
 64471:   FUN_140044528(param_1,0x29,0);
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
 64476:   bVar3 = 0;
 64477:   FUN_1400444d0(param_1,0x22,4,0);
 64478:   thunk_FUN_1400444d0(param_1,0,bVar2,bVar3);
```

### W0459 - `FUN_140043810` line 64477
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64472:   FUN_140044528(param_1,0x22,0x38);
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
 64476:   bVar3 = 0;
 64477:   FUN_1400444d0(param_1,0x22,4,0);
 64478:   thunk_FUN_1400444d0(param_1,0,bVar2,bVar3);
 64479:   return;
 64480: }
 64481: 
 64482: 
```

### W0460 - `FUN_140043810` line 64478
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64473:   FUN_1400444d0(param_1,0x22,4,4);
 64474:   FUN_140048644(param_1,1);
 64475:   bVar2 = 4;
 64476:   bVar3 = 0;
 64477:   FUN_1400444d0(param_1,0x22,4,0);
 64478:   thunk_FUN_1400444d0(param_1,0,bVar2,bVar3);
 64479:   return;
 64480: }
 64481: 
 64482: 
 64483: 
```

### W0461 - `FUN_140043a60` line 64495
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64490: 
 64491:   *(undefined4 *)(param_1 + 0x170) = 0;
 64492:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64493:     bVar1 = 3;
 64494: LAB_140043a8d:
 64495:     thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64496:   }
 64497:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64498:     bVar1 = 7;
 64499:     goto LAB_140043a8d;
 64500:   }
```

### W0462 - `FUN_140043a60` line 64502
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64497:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64498:     bVar1 = 7;
 64499:     goto LAB_140043a8d;
 64500:   }
 64501:   bVar1 = 0;
 64502:   FUN_1400444d0(param_1,0x22,4,0);
 64503:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64504:     DbgPrint("Force Set Rec_B_RS =%x \n",*(undefined1 *)(param_1 + 0x164));
 64505:   }
 64506:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64507:     DbgPrint("Force Set Rec_G_RS =%x \n",*(undefined1 *)(param_1 + 0x165));
```

### W0463 - `FUN_140043a60` line 64512
- Register: `0x27 `
- Kind: `direct_write`
- Value: `*(char *)(param_1 + 0x164) + -0x80`
```c
 64507:     DbgPrint("Force Set Rec_G_RS =%x \n",*(undefined1 *)(param_1 + 0x165));
 64508:   }
 64509:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64510:     DbgPrint("Force Set Rec_R_RS =%x \n",*(undefined1 *)(param_1 + 0x166));
 64511:   }
 64512:   FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 64513:   FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 64514:   bVar3 = *(char *)(param_1 + 0x166) + 0x80;
 64515:   FUN_140044528(param_1,0x29,bVar3);
 64516:   if (*(char *)(param_1 + 0x176) == '\0') {
 64517:     FUN_140043370(param_1,*(char *)(param_1 + 0x164),0,bVar1);
```

### W0464 - `FUN_140043a60` line 64513
- Register: `0x28 `
- Kind: `direct_write`
- Value: `*(char *)(param_1 + 0x165) + -0x80`
```c
 64508:   }
 64509:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64510:     DbgPrint("Force Set Rec_R_RS =%x \n",*(undefined1 *)(param_1 + 0x166));
 64511:   }
 64512:   FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 64513:   FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 64514:   bVar3 = *(char *)(param_1 + 0x166) + 0x80;
 64515:   FUN_140044528(param_1,0x29,bVar3);
 64516:   if (*(char *)(param_1 + 0x176) == '\0') {
 64517:     FUN_140043370(param_1,*(char *)(param_1 + 0x164),0,bVar1);
 64518:     FUN_140043370(param_1,*(char *)(param_1 + 0x165),1,bVar1);
```

### W0465 - `FUN_140043a60` line 64515
- Register: `0x29 `
- Kind: `direct_write`
- Value: `bVar3`
```c
 64510:     DbgPrint("Force Set Rec_R_RS =%x \n",*(undefined1 *)(param_1 + 0x166));
 64511:   }
 64512:   FUN_140044528(param_1,0x27,*(char *)(param_1 + 0x164) + -0x80);
 64513:   FUN_140044528(param_1,0x28,*(char *)(param_1 + 0x165) + -0x80);
 64514:   bVar3 = *(char *)(param_1 + 0x166) + 0x80;
 64515:   FUN_140044528(param_1,0x29,bVar3);
 64516:   if (*(char *)(param_1 + 0x176) == '\0') {
 64517:     FUN_140043370(param_1,*(char *)(param_1 + 0x164),0,bVar1);
 64518:     FUN_140043370(param_1,*(char *)(param_1 + 0x165),1,bVar1);
 64519:     bVar3 = 2;
 64520:     FUN_140043370(param_1,*(char *)(param_1 + 0x166),2,bVar1);
```

### W0466 - `FUN_140043a60` line 64524
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64519:     bVar3 = 2;
 64520:     FUN_140043370(param_1,*(char *)(param_1 + 0x166),2,bVar1);
 64521:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 64522:       bVar2 = 3;
 64523: LAB_140043b8e:
 64524:       thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64525:     }
 64526:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64527:       bVar2 = 7;
 64528:       goto LAB_140043b8e;
 64529:     }
```

### W0467 - `FUN_140043a60` line 64532
- Register: `0x4B `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 64527:       bVar2 = 7;
 64528:       goto LAB_140043b8e;
 64529:     }
 64530:     bVar1 = 0x80;
 64531:     bVar3 = 0x80;
 64532:     FUN_1400444d0(param_1,0x4b,0x80,0x80);
 64533:   }
 64534:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64535:     bVar2 = 3;
 64536:   }
 64537:   else {
```

### W0468 - `FUN_140043a60` line 64541
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64536:   }
 64537:   else {
 64538:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043bc8;
 64539:     bVar2 = 7;
 64540:   }
 64541:   thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
```

### W0469 - `FUN_140043a60` line 64543
- Register: `0x2D `
- Kind: `direct_write`
- Value: `0`
```c
 64538:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043bc8;
 64539:     bVar2 = 7;
 64540:   }
 64541:   thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
```

### W0470 - `FUN_140043a60` line 64544
- Register: `0x30 `
- Kind: `direct_write`
- Value: `0x94`
```c
 64539:     bVar2 = 7;
 64540:   }
 64541:   thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
```

### W0471 - `FUN_140043a60` line 64545
- Register: `0x31 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64540:   }
 64541:   thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
 64550:   FUN_1400444d0(param_1,0x22,4,4);
```

### W0472 - `FUN_140043a60` line 64546
- Register: `0x37 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 64541:   thunk_FUN_1400444d0(param_1,bVar2,bVar3,bVar1);
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
 64550:   FUN_1400444d0(param_1,0x22,4,4);
 64551:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar3);
```

### W0473 - `FUN_140043a60` line 64547
- Register: `0x54 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64542: LAB_140043bc8:
 64543:   FUN_140044528(param_1,0x2d,0);
 64544:   FUN_140044528(param_1,0x30,0x94);
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
 64550:   FUN_1400444d0(param_1,0x22,4,4);
 64551:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar3);
 64552:   return;
```

### W0474 - `FUN_140043a60` line 64550
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 64545:   FUN_140044528(param_1,0x31,0xb0);
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
 64550:   FUN_1400444d0(param_1,0x22,4,4);
 64551:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar3);
 64552:   return;
 64553: }
 64554: 
 64555: 
```

### W0475 - `FUN_140043a60` line 64551
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64546:   FUN_1400444d0(param_1,0x37,0x10,0x10);
 64547:   FUN_1400444d0(param_1,0x54,0x80,0);
 64548:   bVar3 = 4;
 64549:   bVar1 = 4;
 64550:   FUN_1400444d0(param_1,0x22,4,4);
 64551:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar3);
 64552:   return;
 64553: }
 64554: 
 64555: 
 64556: 
```

### W0476 - `FUN_140043c30` line 64569
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64564: 
 64565:   *(undefined4 *)(param_1 + 0x170) = 0;
 64566:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64567:     bVar1 = 3;
 64568: LAB_140043c5d:
 64569:     thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64570:   }
 64571:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64572:     bVar1 = 7;
 64573:     goto LAB_140043c5d;
 64574:   }
```

### W0477 - `FUN_140043c30` line 64575
- Register: `0x20 `
- Kind: `direct_write`
- Value: `0x1b`
```c
 64570:   }
 64571:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64572:     bVar1 = 7;
 64573:     goto LAB_140043c5d;
 64574:   }
 64575:   FUN_140044528(param_1,0x20,0x1b);
 64576:   FUN_140044528(param_1,0x21,3);
 64577:   bVar4 = 0;
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
```

### W0478 - `FUN_140043c30` line 64576
- Register: `0x21 `
- Kind: `direct_write`
- Value: `3`
```c
 64571:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64572:     bVar1 = 7;
 64573:     goto LAB_140043c5d;
 64574:   }
 64575:   FUN_140044528(param_1,0x20,0x1b);
 64576:   FUN_140044528(param_1,0x21,3);
 64577:   bVar4 = 0;
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
```

### W0479 - `FUN_140043c30` line 64578
- Register: `0x20 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64573:     goto LAB_140043c5d;
 64574:   }
 64575:   FUN_140044528(param_1,0x20,0x1b);
 64576:   FUN_140044528(param_1,0x21,3);
 64577:   bVar4 = 0;
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
```

### W0480 - `FUN_140043c30` line 64580
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0`
```c
 64575:   FUN_140044528(param_1,0x20,0x1b);
 64576:   FUN_140044528(param_1,0x21,3);
 64577:   bVar4 = 0;
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
 64584:     FUN_140044528(param_1,0x23,0xb0);
 64585:     FUN_140048644(param_1,10);
```

### W0481 - `FUN_140043c30` line 64581
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64576:   FUN_140044528(param_1,0x21,3);
 64577:   bVar4 = 0;
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
 64584:     FUN_140044528(param_1,0x23,0xb0);
 64585:     FUN_140048644(param_1,10);
 64586:     bVar1 = 0xa0;
```

### W0482 - `FUN_140043c30` line 64583
- Register: `0x07 `
- Kind: `direct_write`
- Value: `0xff`
```c
 64578:   FUN_1400444d0(param_1,0x20,0x80,0);
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
 64584:     FUN_140044528(param_1,0x23,0xb0);
 64585:     FUN_140048644(param_1,10);
 64586:     bVar1 = 0xa0;
 64587:     FUN_140044528(param_1,0x23,0xa0);
 64588:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0483 - `FUN_140043c30` line 64584
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64579:   bVar1 = 0;
 64580:   FUN_140044528(param_1,0x22,0);
 64581:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
 64584:     FUN_140044528(param_1,0x23,0xb0);
 64585:     FUN_140048644(param_1,10);
 64586:     bVar1 = 0xa0;
 64587:     FUN_140044528(param_1,0x23,0xa0);
 64588:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64589:       bVar2 = 0x23;
```

### W0484 - `FUN_140043c30` line 64587
- Register: `0x23 `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64582:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64583:     FUN_140044528(param_1,7,0xff);
 64584:     FUN_140044528(param_1,0x23,0xb0);
 64585:     FUN_140048644(param_1,10);
 64586:     bVar1 = 0xa0;
 64587:     FUN_140044528(param_1,0x23,0xa0);
 64588:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64589:       bVar2 = 0x23;
 64590: LAB_140043d31:
 64591:       bVar4 = 2;
 64592:       bVar1 = 2;
```

### W0485 - `FUN_140043c30` line 64597
- Register: `0x0C `
- Kind: `direct_write`
- Value: `0xff`
```c
 64592:       bVar1 = 2;
 64593:       FUN_1400444d0(param_1,bVar2,2,2);
 64594:     }
 64595:   }
 64596:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64597:     FUN_140044528(param_1,0xc,0xff);
 64598:     FUN_140044528(param_1,0x2b,0xb0);
 64599:     FUN_140048644(param_1,10);
 64600:     bVar1 = 0xa0;
 64601:     FUN_140044528(param_1,0x2b,0xa0);
 64602:     if (*(char *)(param_1 + 0x1cd) == '\0') {
```

### W0486 - `FUN_140043c30` line 64598
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64593:       FUN_1400444d0(param_1,bVar2,2,2);
 64594:     }
 64595:   }
 64596:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64597:     FUN_140044528(param_1,0xc,0xff);
 64598:     FUN_140044528(param_1,0x2b,0xb0);
 64599:     FUN_140048644(param_1,10);
 64600:     bVar1 = 0xa0;
 64601:     FUN_140044528(param_1,0x2b,0xa0);
 64602:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64603:       bVar2 = 0x2b;
```

### W0487 - `FUN_140043c30` line 64601
- Register: `0x2B `
- Kind: `direct_write`
- Value: `0xa0`
```c
 64596:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64597:     FUN_140044528(param_1,0xc,0xff);
 64598:     FUN_140044528(param_1,0x2b,0xb0);
 64599:     FUN_140048644(param_1,10);
 64600:     bVar1 = 0xa0;
 64601:     FUN_140044528(param_1,0x2b,0xa0);
 64602:     if (*(char *)(param_1 + 0x1cd) == '\0') {
 64603:       bVar2 = 0x2b;
 64604:       goto LAB_140043d31;
 64605:     }
 64606:   }
```

### W0488 - `FUN_140043c30` line 64610
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64605:     }
 64606:   }
 64607:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64608:     bVar2 = 0;
 64609: LAB_140043d56:
 64610:     thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64611:   }
 64612:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64613:     bVar2 = 4;
 64614:     goto LAB_140043d56;
 64615:   }
```

### W0489 - `FUN_140043c30` line 64617
- Register: `0x3B `
- Kind: `direct_write`
- Value: `0x23`
```c
 64612:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64613:     bVar2 = 4;
 64614:     goto LAB_140043d56;
 64615:   }
 64616:   bVar1 = 0x23;
 64617:   FUN_140044528(param_1,0x3b,0x23);
 64618:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 64619:     bVar2 = 3;
 64620:   }
 64621:   else {
 64622:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043d8d;
```

### W0490 - `FUN_140043c30` line 64625
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64620:   }
 64621:   else {
 64622:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043d8d;
 64623:     bVar2 = 7;
 64624:   }
 64625:   thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64626: LAB_140043d8d:
 64627:   FUN_140044528(param_1,0x26,0);
 64628:   FUN_140044528(param_1,0x27,0x1f);
 64629:   FUN_140044528(param_1,0x28,0x1f);
 64630:   FUN_140044528(param_1,0x29,0x1f);
```

### W0491 - `FUN_140043c30` line 64627
- Register: `0x26 `
- Kind: `direct_write`
- Value: `0`
```c
 64622:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140043d8d;
 64623:     bVar2 = 7;
 64624:   }
 64625:   thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64626: LAB_140043d8d:
 64627:   FUN_140044528(param_1,0x26,0);
 64628:   FUN_140044528(param_1,0x27,0x1f);
 64629:   FUN_140044528(param_1,0x28,0x1f);
 64630:   FUN_140044528(param_1,0x29,0x1f);
 64631:   if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 64632:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0492 - `FUN_140043c30` line 64628
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0x1f`
```c
 64623:     bVar2 = 7;
 64624:   }
 64625:   thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64626: LAB_140043d8d:
 64627:   FUN_140044528(param_1,0x26,0);
 64628:   FUN_140044528(param_1,0x27,0x1f);
 64629:   FUN_140044528(param_1,0x28,0x1f);
 64630:   FUN_140044528(param_1,0x29,0x1f);
 64631:   if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 64632:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64633:       DbgPrint("****** SKEWOPT = 0 ******\n");
```

### W0493 - `FUN_140043c30` line 64629
- Register: `0x28 `
- Kind: `direct_write`
- Value: `0x1f`
```c
 64624:   }
 64625:   thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64626: LAB_140043d8d:
 64627:   FUN_140044528(param_1,0x26,0);
 64628:   FUN_140044528(param_1,0x27,0x1f);
 64629:   FUN_140044528(param_1,0x28,0x1f);
 64630:   FUN_140044528(param_1,0x29,0x1f);
 64631:   if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 64632:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64633:       DbgPrint("****** SKEWOPT = 0 ******\n");
 64634:     }
```

### W0494 - `FUN_140043c30` line 64630
- Register: `0x29 `
- Kind: `direct_write`
- Value: `0x1f`
```c
 64625:   thunk_FUN_1400444d0(param_1,bVar2,bVar1,bVar4);
 64626: LAB_140043d8d:
 64627:   FUN_140044528(param_1,0x26,0);
 64628:   FUN_140044528(param_1,0x27,0x1f);
 64629:   FUN_140044528(param_1,0x28,0x1f);
 64630:   FUN_140044528(param_1,0x29,0x1f);
 64631:   if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 64632:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64633:       DbgPrint("****** SKEWOPT = 0 ******\n");
 64634:     }
 64635:     uVar3 = 0x80;
```

### W0495 - `FUN_140043c30` line 64641
- Register: `0x2C `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0xc0`
```c
 64636:   }
 64637:   else {
 64638:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64639:       DbgPrint("****** SKEWOPT = 1 ******\n");
 64640:     }
 64641:     FUN_1400444d0(param_1,0x2c,0xc0,0xc0);
 64642:     FUN_1400444d0(param_1,0x2d,0xf0,0x20);
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
```

### W0496 - `FUN_140043c30` line 64642
- Register: `0x2D `
- Kind: `rmw_write`
- Mask: `0xf0`
- Value: `0x20`
```c
 64637:   else {
 64638:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64639:       DbgPrint("****** SKEWOPT = 1 ******\n");
 64640:     }
 64641:     FUN_1400444d0(param_1,0x2c,0xc0,0xc0);
 64642:     FUN_1400444d0(param_1,0x2d,0xf0,0x20);
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
```

### W0497 - `FUN_140043c30` line 64643
- Register: `0x2D `
- Kind: `rmw_write`
- Mask: `7`
- Value: `0`
```c
 64638:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64639:       DbgPrint("****** SKEWOPT = 1 ******\n");
 64640:     }
 64641:     FUN_1400444d0(param_1,0x2c,0xc0,0xc0);
 64642:     FUN_1400444d0(param_1,0x2d,0xf0,0x20);
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
```

### W0498 - `FUN_140043c30` line 64646
- Register: `0x30 `
- Kind: `direct_write`
- Value: `uVar3`
```c
 64641:     FUN_1400444d0(param_1,0x2c,0xc0,0xc0);
 64642:     FUN_1400444d0(param_1,0x2d,0xf0,0x20);
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
```

### W0499 - `FUN_140043c30` line 64647
- Register: `0x31 `
- Kind: `direct_write`
- Value: `0xb0`
```c
 64642:     FUN_1400444d0(param_1,0x2d,0xf0,0x20);
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
```

### W0500 - `FUN_140043c30` line 64648
- Register: `0x32 `
- Kind: `direct_write`
- Value: `0x43`
```c
 64643:     FUN_1400444d0(param_1,0x2d,7,0);
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
```

### W0501 - `FUN_140043c30` line 64649
- Register: `0x33 `
- Kind: `direct_write`
- Value: `0x47`
```c
 64644:     uVar3 = 0x8c;
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
```

### W0502 - `FUN_140043c30` line 64650
- Register: `0x34 `
- Kind: `direct_write`
- Value: `0x4b`
```c
 64645:   }
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
```

### W0503 - `FUN_140043c30` line 64651
- Register: `0x35 `
- Kind: `direct_write`
- Value: `0x53`
```c
 64646:   FUN_140044528(param_1,0x30,uVar3);
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
```

### W0504 - `FUN_140043c30` line 64652
- Register: `0x36 `
- Kind: `rmw_write`
- Mask: `0xc0`
- Value: `0`
```c
 64647:   FUN_140044528(param_1,0x31,0xb0);
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
```

### W0505 - `FUN_140043c30` line 64653
- Register: `0x37 `
- Kind: `direct_write`
- Value: `0xb`
```c
 64648:   FUN_140044528(param_1,0x32,0x43);
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
```

### W0506 - `FUN_140043c30` line 64654
- Register: `0x38 `
- Kind: `direct_write`
- Value: `0xf2`
```c
 64649:   FUN_140044528(param_1,0x33,0x47);
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
```

### W0507 - `FUN_140043c30` line 64655
- Register: `0x39 `
- Kind: `direct_write`
- Value: `0xd`
```c
 64650:   FUN_140044528(param_1,0x34,0x4b);
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
```

### W0508 - `FUN_140043c30` line 64656
- Register: `0x4A `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64651:   FUN_140044528(param_1,0x35,0x53);
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
```

### W0509 - `FUN_140043c30` line 64657
- Register: `0x4B `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0`
```c
 64652:   FUN_1400444d0(param_1,0x36,0xc0,0);
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
 64662:   FUN_1400444d0(param_1,0x22,4,4);
```

### W0510 - `FUN_140043c30` line 64658
- Register: `0x54 `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 64653:   FUN_140044528(param_1,0x37,0xb);
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
 64662:   FUN_1400444d0(param_1,0x22,4,4);
 64663:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
```

### W0511 - `FUN_140043c30` line 64659
- Register: `0x55 `
- Kind: `direct_write`
- Value: `0x40`
```c
 64654:   FUN_140044528(param_1,0x38,0xf2);
 64655:   FUN_140044528(param_1,0x39,0xd);
 64656:   FUN_1400444d0(param_1,0x4a,0x80,0);
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
 64662:   FUN_1400444d0(param_1,0x22,4,4);
 64663:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64664:   return;
```

### W0512 - `FUN_140043c30` line 64662
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `4`
```c
 64657:   FUN_1400444d0(param_1,0x4b,0x80,0);
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
 64662:   FUN_1400444d0(param_1,0x22,4,4);
 64663:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64664:   return;
 64665: }
 64666: 
 64667: 
```

### W0513 - `FUN_140043c30` line 64663
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64658:   FUN_1400444d0(param_1,0x54,0x80,0x80);
 64659:   FUN_140044528(param_1,0x55,0x40);
 64660:   bVar4 = 4;
 64661:   bVar1 = 4;
 64662:   FUN_1400444d0(param_1,0x22,4,4);
 64663:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64664:   return;
 64665: }
 64666: 
 64667: 
 64668: 
```

### W0514 - `FUN_140043f14` line 64680
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64675:   byte bVar4;
 64676:   undefined7 uVar5;
 64677:   byte bVar6;
 64678: 
 64679:   uVar5 = (undefined7)((ulonglong)param_3 >> 8);
 64680:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 64681:   bVar1 = FUN_140044484(param_1,7);
 64682:   bVar4 = bVar1 & 0xf7;
 64683:   FUN_140044528(param_1,7,bVar4);
 64684:   if (bVar1 == 0) {
 64685:     return;
```

### W0515 - `FUN_140043f14` line 64683
- Register: `0x07 `
- Kind: `direct_write`
- Value: `bVar4`
```c
 64678: 
 64679:   uVar5 = (undefined7)((ulonglong)param_3 >> 8);
 64680:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 64681:   bVar1 = FUN_140044484(param_1,7);
 64682:   bVar4 = bVar1 & 0xf7;
 64683:   FUN_140044528(param_1,7,bVar4);
 64684:   if (bVar1 == 0) {
 64685:     return;
 64686:   }
 64687:   if (((bVar1 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 64688:     DbgPrint("# Port 0 CH0 Lag Err#\n");
```

### W0516 - `FUN_140043f14` line 64705
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64700:     }
 64701:     *(undefined1 *)(param_1 + 0x171) = 1;
 64702:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 64703:       bVar2 = 3;
 64704: LAB_140043ff6:
 64705:       thunk_FUN_1400444d0(param_1,bVar2,bVar4,bVar6);
 64706:     }
 64707:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64708:       bVar2 = 7;
 64709:       goto LAB_140043ff6;
 64710:     }
```

### W0517 - `FUN_140043f14` line 64713
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64708:       bVar2 = 7;
 64709:       goto LAB_140043ff6;
 64710:     }
 64711:     param_4 = 0;
 64712:     bVar4 = 4;
 64713:     FUN_1400444d0(param_1,0x22,4,0);
 64714:     thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4);
 64715:   }
 64716:   bVar6 = (byte)param_4;
 64717:   if ((bVar1 & 0x20) != 0) {
 64718:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0518 - `FUN_140043f14` line 64714
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64709:       goto LAB_140043ff6;
 64710:     }
 64711:     param_4 = 0;
 64712:     bVar4 = 4;
 64713:     FUN_1400444d0(param_1,0x22,4,0);
 64714:     thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4);
 64715:   }
 64716:   bVar6 = (byte)param_4;
 64717:   if ((bVar1 & 0x20) != 0) {
 64718:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64719:       DbgPrint("# Port 0 SAREQ Fail!!#\n");
```

### W0519 - `FUN_140043f14` line 64723
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64718:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64719:       DbgPrint("# Port 0 SAREQ Fail!!#\n");
 64720:     }
 64721:     *(undefined1 *)(param_1 + 0x173) = 1;
 64722:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 64723:       thunk_FUN_1400444d0(param_1,3,bVar4,bVar6);
 64724:     }
 64725:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64726:       thunk_FUN_1400444d0(param_1,7,bVar4,bVar6);
 64727:     }
 64728:     param_4 = 0;
```

### W0520 - `FUN_140043f14` line 64726
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64721:     *(undefined1 *)(param_1 + 0x173) = 1;
 64722:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 64723:       thunk_FUN_1400444d0(param_1,3,bVar4,bVar6);
 64724:     }
 64725:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64726:       thunk_FUN_1400444d0(param_1,7,bVar4,bVar6);
 64727:     }
 64728:     param_4 = 0;
 64729:     bVar4 = 4;
 64730:     FUN_1400444d0(param_1,0x22,4,0);
 64731:     uVar3 = 0;
```

### W0521 - `FUN_140043f14` line 64730
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64725:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64726:       thunk_FUN_1400444d0(param_1,7,bVar4,bVar6);
 64727:     }
 64728:     param_4 = 0;
 64729:     bVar4 = 4;
 64730:     FUN_1400444d0(param_1,0x22,4,0);
 64731:     uVar3 = 0;
 64732:     thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4);
 64733:     FUN_140043660(param_1,uVar3,bVar4,(byte)param_4);
 64734:     if (*(char *)(param_1 + 0x177) == '\x01') {
 64735:       *(undefined1 *)(param_1 + 0x173) = 0;
```

### W0522 - `FUN_140043f14` line 64732
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64727:     }
 64728:     param_4 = 0;
 64729:     bVar4 = 4;
 64730:     FUN_1400444d0(param_1,0x22,4,0);
 64731:     uVar3 = 0;
 64732:     thunk_FUN_1400444d0(param_1,0,bVar4,(byte)param_4);
 64733:     FUN_140043660(param_1,uVar3,bVar4,(byte)param_4);
 64734:     if (*(char *)(param_1 + 0x177) == '\x01') {
 64735:       *(undefined1 *)(param_1 + 0x173) = 0;
 64736:       *(undefined1 *)(param_1 + 0x171) = 1;
 64737:       FUN_140041b38(param_1,3,CONCAT71(uVar5,bVar4),param_4);
```

### W0523 - `FUN_140043f14` line 64749
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64744:     }
 64745:     *(undefined1 *)(param_1 + 0x170) = 1;
 64746:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 64747:       bVar2 = 3;
 64748: LAB_1400440fe:
 64749:       thunk_FUN_1400444d0(param_1,bVar2,bVar4,bVar6);
 64750:     }
 64751:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64752:       bVar2 = 7;
 64753:       goto LAB_1400440fe;
 64754:     }
```

### W0524 - `FUN_140043f14` line 64757
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64752:       bVar2 = 7;
 64753:       goto LAB_1400440fe;
 64754:     }
 64755:     bVar6 = 0;
 64756:     bVar4 = 4;
 64757:     FUN_1400444d0(param_1,0x22,4,0);
 64758:     thunk_FUN_1400444d0(param_1,0,bVar4,bVar6);
 64759:   }
 64760:   if (-1 < (char)bVar1) {
 64761:     return;
 64762:   }
```

### W0525 - `FUN_140043f14` line 64758
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64753:       goto LAB_1400440fe;
 64754:     }
 64755:     bVar6 = 0;
 64756:     bVar4 = 4;
 64757:     FUN_1400444d0(param_1,0x22,4,0);
 64758:     thunk_FUN_1400444d0(param_1,0,bVar4,bVar6);
 64759:   }
 64760:   if (-1 < (char)bVar1) {
 64761:     return;
 64762:   }
 64763:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0526 - `FUN_140043f14` line 64774
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64769:   }
 64770:   else {
 64771:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140044165;
 64772:     bVar1 = 7;
 64773:   }
 64774:   thunk_FUN_1400444d0(param_1,bVar1,bVar4,bVar6);
 64775: LAB_140044165:
 64776:   bVar1 = 4;
 64777:   bVar4 = 0;
 64778:   FUN_1400444d0(param_1,0x22,4,0);
 64779:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
```

### W0527 - `FUN_140043f14` line 64778
- Register: `0x22 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64773:   }
 64774:   thunk_FUN_1400444d0(param_1,bVar1,bVar4,bVar6);
 64775: LAB_140044165:
 64776:   bVar1 = 4;
 64777:   bVar4 = 0;
 64778:   FUN_1400444d0(param_1,0x22,4,0);
 64779:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64780:   return;
 64781: }
 64782: 
 64783: 
```

### W0528 - `FUN_140043f14` line 64779
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64774:   thunk_FUN_1400444d0(param_1,bVar1,bVar4,bVar6);
 64775: LAB_140044165:
 64776:   bVar1 = 4;
 64777:   bVar4 = 0;
 64778:   FUN_1400444d0(param_1,0x22,4,0);
 64779:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar4);
 64780:   return;
 64781: }
 64782: 
 64783: 
 64784: 
```

### W0529 - `FUN_14004419c` line 64790
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64785: void FUN_14004419c(longlong param_1,undefined8 param_2,byte param_3,byte param_4)
 64786: 
 64787: {
 64788:   byte bVar1;
 64789: 
 64790:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64791:   bVar1 = FUN_140044484(param_1,0xc);
 64792:   FUN_140044528(param_1,0xc,bVar1);
 64793:   if (bVar1 != 0) {
 64794:     if (((bVar1 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 64795:       DbgPrint("# Port 1 CH0 Lag Err#\n");
```

### W0530 - `FUN_14004419c` line 64792
- Register: `0x0C `
- Kind: `direct_write`
- Value: `bVar1`
```c
 64787: {
 64788:   byte bVar1;
 64789: 
 64790:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64791:   bVar1 = FUN_140044484(param_1,0xc);
 64792:   FUN_140044528(param_1,0xc,bVar1);
 64793:   if (bVar1 != 0) {
 64794:     if (((bVar1 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 64795:       DbgPrint("# Port 1 CH0 Lag Err#\n");
 64796:     }
 64797:     if (((bVar1 & 2) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
```

### W0531 - `FUN_1400445c0` line 64972
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64967:   byte bVar7;
 64968:   byte bVar8;
 64969:   bool bVar9;
 64970: 
 64971:   *(undefined2 *)(param_1 + 0x223) = 0;
 64972:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64973:   FUN_1400444d0(param_1,100,4,0);
 64974:   FUN_1400444d0(param_1,100,2,2);
 64975:   bVar7 = 2;
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
```

### W0532 - `FUN_1400445c0` line 64973
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `0`
```c
 64968:   byte bVar8;
 64969:   bool bVar9;
 64970: 
 64971:   *(undefined2 *)(param_1 + 0x223) = 0;
 64972:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64973:   FUN_1400444d0(param_1,100,4,0);
 64974:   FUN_1400444d0(param_1,100,2,2);
 64975:   bVar7 = 2;
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
```

### W0533 - `FUN_1400445c0` line 64974
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 64969:   bool bVar9;
 64970: 
 64971:   *(undefined2 *)(param_1 + 0x223) = 0;
 64972:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64973:   FUN_1400444d0(param_1,100,4,0);
 64974:   FUN_1400444d0(param_1,100,2,2);
 64975:   bVar7 = 2;
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
 64979:   bVar8 = 0x40;
```

### W0534 - `FUN_1400445c0` line 64977
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `0`
```c
 64972:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64973:   FUN_1400444d0(param_1,100,4,0);
 64974:   FUN_1400444d0(param_1,100,2,2);
 64975:   bVar7 = 2;
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
 64979:   bVar8 = 0x40;
 64980:   bVar7 = 0x40;
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
```

### W0535 - `FUN_1400445c0` line 64978
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64973:   FUN_1400444d0(param_1,100,4,0);
 64974:   FUN_1400444d0(param_1,100,2,2);
 64975:   bVar7 = 2;
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
 64979:   bVar8 = 0x40;
 64980:   bVar7 = 0x40;
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 64983:   bVar8 = 0;
```

### W0536 - `FUN_1400445c0` line 64981
- Register: `0x20 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 64976:   bVar8 = 0;
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
 64979:   bVar8 = 0x40;
 64980:   bVar7 = 0x40;
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 64983:   bVar8 = 0;
 64984:   bVar7 = 1;
 64985:   FUN_1400444d0(param_1,0xc0,1,0);
 64986:   thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
```

### W0537 - `FUN_1400445c0` line 64982
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64977:   FUN_1400444d0(param_1,100,2,0);
 64978:   thunk_FUN_1400444d0(param_1,5,bVar7,bVar8);
 64979:   bVar8 = 0x40;
 64980:   bVar7 = 0x40;
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 64983:   bVar8 = 0;
 64984:   bVar7 = 1;
 64985:   FUN_1400444d0(param_1,0xc0,1,0);
 64986:   thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 64987:   *(undefined1 *)(param_1 + 0x1d5) = 1;
```

### W0538 - `FUN_1400445c0` line 64985
- Register: `0xC0 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 64980:   bVar7 = 0x40;
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 64983:   bVar8 = 0;
 64984:   bVar7 = 1;
 64985:   FUN_1400444d0(param_1,0xc0,1,0);
 64986:   thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 64987:   *(undefined1 *)(param_1 + 0x1d5) = 1;
 64988:   bVar9 = *(int *)(param_1 + 0x20c) != 3;
 64989:   uVar6 = *(ushort *)(param_1 + 0x22c) * 2;
 64990:   if (bVar9) {
```

### W0539 - `FUN_1400445c0` line 64986
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 64981:   FUN_1400444d0(param_1,0x20,0x40,0x40);
 64982:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 64983:   bVar8 = 0;
 64984:   bVar7 = 1;
 64985:   FUN_1400444d0(param_1,0xc0,1,0);
 64986:   thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 64987:   *(undefined1 *)(param_1 + 0x1d5) = 1;
 64988:   bVar9 = *(int *)(param_1 + 0x20c) != 3;
 64989:   uVar6 = *(ushort *)(param_1 + 0x22c) * 2;
 64990:   if (bVar9) {
 64991:     uVar6 = *(ushort *)(param_1 + 0x22c);
```

### W0540 - `FUN_1400445c0` line 65051
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65046:       ;
 65047: LAB_1400446bc:
 65048:       DbgPrint(pcVar5);
 65049:     }
 65050:   }
 65051:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 65052:   FUN_140044528(param_1,0xc4,bVar1);
 65053:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar8);
 65054:   if (*(char *)(param_1 + 0x1d5) == '\0') {
 65055:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65056:       DbgPrint("!! 4K MODE_EvenOdd !!\n");
```

### W0541 - `FUN_1400445c0` line 65052
- Register: `0xC4 `
- Kind: `direct_write`
- Value: `bVar1`
```c
 65047: LAB_1400446bc:
 65048:       DbgPrint(pcVar5);
 65049:     }
 65050:   }
 65051:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 65052:   FUN_140044528(param_1,0xc4,bVar1);
 65053:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar8);
 65054:   if (*(char *)(param_1 + 0x1d5) == '\0') {
 65055:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65056:       DbgPrint("!! 4K MODE_EvenOdd !!\n");
 65057:     }
```

### W0542 - `FUN_1400445c0` line 65053
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65048:       DbgPrint(pcVar5);
 65049:     }
 65050:   }
 65051:   thunk_FUN_1400444d0(param_1,1,bVar7,bVar8);
 65052:   FUN_140044528(param_1,0xc4,bVar1);
 65053:   thunk_FUN_1400444d0(param_1,0,bVar1,bVar8);
 65054:   if (*(char *)(param_1 + 0x1d5) == '\0') {
 65055:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65056:       DbgPrint("!! 4K MODE_EvenOdd !!\n");
 65057:     }
 65058:     bVar7 = 0;
```

### W0543 - `FUN_1400445c0` line 65067
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `4`
- Value: `bVar7`
```c
 65062:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65063:       DbgPrint("!! 4K MODE_LeftRight !!\n");
 65064:     }
 65065:     bVar7 = 4;
 65066:   }
 65067:   FUN_1400444d0(param_1,100,4,bVar7);
 65068: LAB_14004482e:
 65069:   FUN_1400444d0(param_1,100,2,2);
 65070:   bVar8 = 0;
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
```

### W0544 - `FUN_1400445c0` line 65069
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `2`
```c
 65064:     }
 65065:     bVar7 = 4;
 65066:   }
 65067:   FUN_1400444d0(param_1,100,4,bVar7);
 65068: LAB_14004482e:
 65069:   FUN_1400444d0(param_1,100,2,2);
 65070:   bVar8 = 0;
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
```

### W0545 - `FUN_1400445c0` line 65072
- Register: `0x64 `
- Kind: `rmw_write`
- Mask: `2`
- Value: `0`
```c
 65067:   FUN_1400444d0(param_1,100,4,bVar7);
 65068: LAB_14004482e:
 65069:   FUN_1400444d0(param_1,100,2,2);
 65070:   bVar8 = 0;
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
```

### W0546 - `FUN_1400445c0` line 65074
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65069:   FUN_1400444d0(param_1,100,2,2);
 65070:   bVar8 = 0;
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65078:     bVar1 = FUN_140044484(param_1,0xa5);
 65079:     bVar2 = FUN_140044484(param_1,0xa4);
```

### W0547 - `FUN_1400445c0` line 65106
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65101:       }
 65102:       FUN_14003f59c(param_1,'\x02');
 65103:       *(undefined1 *)(param_1 + 0x224) = 1;
 65104:     }
 65105:   }
 65106:   thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65107:   return;
 65108: }
 65109: 
 65110: 
 65111: 
```

### W0548 - `FUN_140044a88` line 65163
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65158:   undefined1 uVar3;
 65159:   byte bVar4;
 65160:   ulonglong uVar5;
 65161:   byte *pbVar6;
 65162: 
 65163:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65164:   bVar1 = FUN_140044484(param_1,0x11);
 65165:   bVar1 = bVar1 & 0x40;
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
```

### W0549 - `FUN_140044a88` line 65167
- Register: `0x11 `
- Kind: `direct_write`
- Value: `bVar1`
```c
 65162: 
 65163:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65164:   bVar1 = FUN_140044484(param_1,0x11);
 65165:   bVar1 = bVar1 & 0x40;
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
 65169:     thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65170:     cVar2 = FUN_140044484(param_1,0x24);
 65171:     thunk_FUN_1400444d0(param_1,0,(byte)uVar5,param_4);
 65172:     bVar4 = 0;
```

### W0550 - `FUN_140044a88` line 65169
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65164:   bVar1 = FUN_140044484(param_1,0x11);
 65165:   bVar1 = bVar1 & 0x40;
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
 65169:     thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65170:     cVar2 = FUN_140044484(param_1,0x24);
 65171:     thunk_FUN_1400444d0(param_1,0,(byte)uVar5,param_4);
 65172:     bVar4 = 0;
 65173:     if ((bVar1 == 0) && (cVar2 != '\0')) {
 65174:       if (*(char *)(param_1 + 0x1ac) == '\0') {
```

### W0551 - `FUN_140044a88` line 65171
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
 65169:     thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65170:     cVar2 = FUN_140044484(param_1,0x24);
 65171:     thunk_FUN_1400444d0(param_1,0,(byte)uVar5,param_4);
 65172:     bVar4 = 0;
 65173:     if ((bVar1 == 0) && (cVar2 != '\0')) {
 65174:       if (*(char *)(param_1 + 0x1ac) == '\0') {
 65175:         *(char *)(param_1 + 0x1a6) = *(char *)(param_1 + 0x1a6) + '\x01';
 65176:         cVar2 = '\0';
```

### W0552 - `FUN_140044a88` line 65194
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65189:           }
 65190:           *(undefined1 *)(param_1 + 0x1a6) = 0;
 65191:           *(undefined1 *)(param_1 + 0x1ac) = 1;
 65192:         }
 65193:       }
 65194:       thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65195:       uVar3 = FUN_140044484(param_1,0x24);
 65196:       *(undefined1 *)(param_1 + 0x1ae) = uVar3;
 65197:       uVar3 = FUN_140044484(param_1,0x25);
 65198:       *(undefined1 *)(param_1 + 0x1af) = uVar3;
 65199:       uVar3 = FUN_140044484(param_1,0x26);
```

### W0553 - `FUN_140044a88` line 65228
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65223:         pbVar6 = pbVar6 + 1;
 65224:       } while (bVar4 < 0x1c);
 65225:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65226:         DbgPrint(&LAB_14006b500);
 65227:       }
 65228:       thunk_FUN_1400444d0(param_1,0,bVar1,param_4);
 65229:       return;
 65230:     }
 65231:     *(undefined1 *)(param_1 + 0x1ac) = 0;
 65232:     *(undefined1 *)(param_1 + 0x1a6) = 0;
 65233:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0554 - `FUN_140044cf8` line 65264
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65259:   byte bVar9;
 65260:   char *pcVar10;
 65261:   byte bVar11;
 65262: 
 65263:   sVar8 = 2;
 65264:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65265:   bVar11 = 1;
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
```

### W0555 - `FUN_140044cf8` line 65267
- Register: `0x86 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 65262: 
 65263:   sVar8 = 2;
 65264:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65265:   bVar11 = 1;
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
```

### W0556 - `FUN_140044cf8` line 65268
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65263:   sVar8 = 2;
 65264:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65265:   bVar11 = 1;
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
```

### W0557 - `FUN_140044cf8` line 65290
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65285:     DbgPrint("[xAud] 68051 CTS = %lu \n");
 65286:   }
 65287:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65288:     DbgPrint("[xAud] 68051 TMDSCLK = %lu \n");
 65289:   }
 65290:   thunk_FUN_1400444d0(param_1,0,bVar9,bVar11);
 65291:   iVar7 = *(int *)(param_1 + 0x254);
 65292:   iVar1 = *(int *)(param_1 + 0x238);
 65293:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65294:     DbgPrint("[xAud] 68051 sum = %lu \n");
 65295:   }
```

### W0558 - `FUN_140044cf8` line 65352
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65347:         }
 65348:       }
 65349:     }
 65350:     bVar5 = (byte)sVar8;
 65351:     *(short *)(param_1 + 0x25e) = sVar8;
 65352:     thunk_FUN_1400444d0(param_1,0,bVar9,bVar11);
 65353:     bVar3 = FUN_140044484(param_1,0xb5);
 65354:     bVar4 = FUN_140044484(param_1,0xb6);
 65355:     bVar3 = bVar4 >> 2 & 0x30 | bVar3 & 0xf;
 65356:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65357:       DbgPrint("[xAud] 68051 Audio_CH_Status[24:27 - 30:31][bit0~bit5] = %x ,iTE6805_Enable_Audio_Output\n"
```

### W0559 - `FUN_140044cf8` line 65411
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 65406:         DbgPrint(
 65407:                 "[xAud] 68051 Audio_CH_Status == Force_Sampling_Frequency reset Audio, iTE6805_Enable_Audio_Output \n"
 65408:                 );
 65409:       }
 65410:       bVar3 = FUN_140044484(param_1,0x81);
 65411:       FUN_1400444d0(param_1,0x81,0x40,0);
 65412:       if ((bVar3 & 0x40) != 0) {
 65413:         FUN_14003ea04(param_1);
 65414:       }
 65415:       *(undefined1 *)(param_1 + 0x194) = 0;
 65416:     }
```

### W0560 - `FUN_140044cf8` line 65425
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0x40`
```c
 65420:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65421:         DbgPrint("[xAud] 68051 Current_AudioSamplingFreq_ErrorCount=%d, iTE6805_Enable_Audio_Output \n"
 65422:                  ,cVar6);
 65423:       }
 65424:       if (0xf < *(byte *)(param_1 + 0x194)) {
 65425:         FUN_1400444d0(param_1,0x81,0x40,0x40);
 65426:         FUN_1400444d0(param_1,0x8a,0x3f,bVar5);
 65427:         *(undefined1 *)(param_1 + 0x194) = 0;
 65428:         FUN_14003ea04(param_1);
 65429:         if ((DAT_1400a04f8 & 0x10) != 0) {
 65430:           DbgPrint("[xAud] 68051 ForceAudio Mode, iTE6805_Enable_Audio_Output\n");
```

### W0561 - `FUN_140044cf8` line 65426
- Register: `0x8A `
- Kind: `rmw_write`
- Mask: `0x3f`
- Value: `bVar5`
```c
 65421:         DbgPrint("[xAud] 68051 Current_AudioSamplingFreq_ErrorCount=%d, iTE6805_Enable_Audio_Output \n"
 65422:                  ,cVar6);
 65423:       }
 65424:       if (0xf < *(byte *)(param_1 + 0x194)) {
 65425:         FUN_1400444d0(param_1,0x81,0x40,0x40);
 65426:         FUN_1400444d0(param_1,0x8a,0x3f,bVar5);
 65427:         *(undefined1 *)(param_1 + 0x194) = 0;
 65428:         FUN_14003ea04(param_1);
 65429:         if ((DAT_1400a04f8 & 0x10) != 0) {
 65430:           DbgPrint("[xAud] 68051 ForceAudio Mode, iTE6805_Enable_Audio_Output\n");
 65431:         }
```

### W0562 - `FUN_140045168` line 65453
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65448:   char *pcVar6;
 65449:   char *pcVar7;
 65450:   byte bVar8;
 65451:   undefined1 *puVar9;
 65452: 
 65453:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65454:   bVar3 = FUN_14003ce08(param_1,*(char *)(param_1 + 0x1ec));
 65455:   if (bVar3 == 0) {
 65456:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65457:       DbgPrint("---- CSC HDMI mode ----(vfmt-S)\n");
 65458:     }
```

### W0563 - `FUN_140045168` line 65462
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `0x30`
- Value: `*(char *)(param_1 + 0x20c) << 4`
```c
 65457:       DbgPrint("---- CSC HDMI mode ----(vfmt-S)\n");
 65458:     }
 65459:     FUN_14003d0a0(param_1);
 65460:     FUN_14003ff14(param_1);
 65461:     FUN_14003d050(param_1);
 65462:     FUN_1400444d0(param_1,0x6b,0x30,*(char *)(param_1 + 0x20c) << 4);
 65463:     bVar8 = 0;
 65464:     bVar3 = 1;
 65465:     FUN_1400444d0(param_1,0x6b,1,0);
 65466:   }
 65467:   else {
```

### W0564 - `FUN_140045168` line 65465
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 65460:     FUN_14003ff14(param_1);
 65461:     FUN_14003d050(param_1);
 65462:     FUN_1400444d0(param_1,0x6b,0x30,*(char *)(param_1 + 0x20c) << 4);
 65463:     bVar8 = 0;
 65464:     bVar3 = 1;
 65465:     FUN_1400444d0(param_1,0x6b,1,0);
 65466:   }
 65467:   else {
 65468:     *(undefined1 *)(param_1 + 0x225) = 2;
 65469:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65470:       DbgPrint("---- CSC DVI mode ----(vfmt-S)\n");
```

### W0565 - `FUN_140045168` line 65472
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `0x30`
- Value: `0x10`
```c
 65467:   else {
 65468:     *(undefined1 *)(param_1 + 0x225) = 2;
 65469:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65470:       DbgPrint("---- CSC DVI mode ----(vfmt-S)\n");
 65471:     }
 65472:     FUN_1400444d0(param_1,0x6b,0x30,0x10);
 65473:     bVar8 = 1;
 65474:     bVar3 = 1;
 65475:     FUN_1400444d0(param_1,0x6b,1,1);
 65476:     if (*(char *)(param_1 + 0x1ec) == '\x01') {
 65477:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar8);
```

### W0566 - `FUN_140045168` line 65475
- Register: `0x6B `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 65470:       DbgPrint("---- CSC DVI mode ----(vfmt-S)\n");
 65471:     }
 65472:     FUN_1400444d0(param_1,0x6b,0x30,0x10);
 65473:     bVar8 = 1;
 65474:     bVar3 = 1;
 65475:     FUN_1400444d0(param_1,0x6b,1,1);
 65476:     if (*(char *)(param_1 + 0x1ec) == '\x01') {
 65477:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar8);
 65478:     }
 65479:     cVar4 = FUN_140044484(param_1,0x48);
 65480:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
```

### W0567 - `FUN_140045168` line 65477
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65472:     FUN_1400444d0(param_1,0x6b,0x30,0x10);
 65473:     bVar8 = 1;
 65474:     bVar3 = 1;
 65475:     FUN_1400444d0(param_1,0x6b,1,1);
 65476:     if (*(char *)(param_1 + 0x1ec) == '\x01') {
 65477:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar8);
 65478:     }
 65479:     cVar4 = FUN_140044484(param_1,0x48);
 65480:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65481:     *(uint *)(param_1 + 0x210) = (cVar4 < '4') + 1;
 65482:   }
```

### W0568 - `FUN_140045168` line 65480
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65475:     FUN_1400444d0(param_1,0x6b,1,1);
 65476:     if (*(char *)(param_1 + 0x1ec) == '\x01') {
 65477:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar8);
 65478:     }
 65479:     cVar4 = FUN_140044484(param_1,0x48);
 65480:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65481:     *(uint *)(param_1 + 0x210) = (cVar4 < '4') + 1;
 65482:   }
 65483:   uVar5 = 1;
 65484:   if (*(char *)(param_1 + 0x200) == '\x01') {
 65485:     *(int *)(param_1 + 0x1fc) = *(int *)(param_1 + 0x20c);
```

### W0569 - `FUN_140045168` line 65488
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65483:   uVar5 = 1;
 65484:   if (*(char *)(param_1 + 0x200) == '\x01') {
 65485:     *(int *)(param_1 + 0x1fc) = *(int *)(param_1 + 0x20c);
 65486:     FUN_14003f690(param_1);
 65487:   }
 65488:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65489:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65490:     DbgPrint("AVT_Flag_Enable_Dither => %d(vfmt-S)\n",*(char *)(param_1 + 0x22b));
 65491:   }
 65492:   bVar3 = (-(*(char *)(param_1 + 0x22b) != '\0') & 2U) + 0xa0;
 65493:   FUN_140044528(param_1,0x6e,bVar3);
```

### W0570 - `FUN_140045168` line 65493
- Register: `0x6E `
- Kind: `direct_write`
- Value: `bVar3`
```c
 65488:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65489:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65490:     DbgPrint("AVT_Flag_Enable_Dither => %d(vfmt-S)\n",*(char *)(param_1 + 0x22b));
 65491:   }
 65492:   bVar3 = (-(*(char *)(param_1 + 0x22b) != '\0') & 2U) + 0xa0;
 65493:   FUN_140044528(param_1,0x6e,bVar3);
 65494:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65495:   bVar3 = 0;
 65496:   FUN_140044528(param_1,0x86,0);
 65497:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65498:   pcVar6 = "Color_Format_RGB";
```

### W0571 - `FUN_140045168` line 65494
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65489:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65490:     DbgPrint("AVT_Flag_Enable_Dither => %d(vfmt-S)\n",*(char *)(param_1 + 0x22b));
 65491:   }
 65492:   bVar3 = (-(*(char *)(param_1 + 0x22b) != '\0') & 2U) + 0xa0;
 65493:   FUN_140044528(param_1,0x6e,bVar3);
 65494:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65495:   bVar3 = 0;
 65496:   FUN_140044528(param_1,0x86,0);
 65497:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65498:   pcVar6 = "Color_Format_RGB";
 65499:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0572 - `FUN_140045168` line 65496
- Register: `0x86 `
- Kind: `direct_write`
- Value: `0`
```c
 65491:   }
 65492:   bVar3 = (-(*(char *)(param_1 + 0x22b) != '\0') & 2U) + 0xa0;
 65493:   FUN_140044528(param_1,0x6e,bVar3);
 65494:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65495:   bVar3 = 0;
 65496:   FUN_140044528(param_1,0x86,0);
 65497:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65498:   pcVar6 = "Color_Format_RGB";
 65499:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65500:     iVar1 = *(int *)(param_1 + 0x1fc);
 65501:     if (iVar1 == 0) {
```

### W0573 - `FUN_140045168` line 65497
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65492:   bVar3 = (-(*(char *)(param_1 + 0x22b) != '\0') & 2U) + 0xa0;
 65493:   FUN_140044528(param_1,0x6e,bVar3);
 65494:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65495:   bVar3 = 0;
 65496:   FUN_140044528(param_1,0x86,0);
 65497:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65498:   pcVar6 = "Color_Format_RGB";
 65499:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65500:     iVar1 = *(int *)(param_1 + 0x1fc);
 65501:     if (iVar1 == 0) {
 65502:       pcVar7 = "Color_Format_RGB";
```

### W0574 - `FUN_140045168` line 65550
- Register: `0x6C `
- Kind: `rmw_write`
- Mask: `3`
- Value: `3`
```c
 65545:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65546:       DbgPrint("---- input YUV to output RGB ----(vfmt-S)\n");
 65547:     }
 65548:     bVar8 = 3;
 65549:     bVar3 = 3;
 65550:     FUN_1400444d0(param_1,0x6c,3,3);
 65551:     if ((*(char *)(param_1 + 0x1ac) == '\x01') && ((*(byte *)(param_1 + 0x1b2) & 7) != 0)) {
 65552: LAB_140045431:
 65553:       uVar5 = 8;
 65554:     }
 65555:     else if (*(int *)(param_1 + 0x210) == 2) {
```

### W0575 - `FUN_140045168` line 65572
- Register: `0x6C `
- Kind: `rmw_write`
- Mask: `3`
- Value: `0`
```c
 65567:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65568:         DbgPrint("---- YUV to YUV or RGB to RGB ----(vfmt-S)\n");
 65569:       }
 65570:       bVar8 = 0;
 65571:       bVar3 = 3;
 65572:       FUN_1400444d0(param_1,0x6c,3,0);
 65573:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65574:         DbgPrint("--- Don\'t do CSC Full/Limit Convert--- (vfmt-S)\n");
 65575:       }
 65576:       thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65577:       bVar3 = 0;
```

### W0576 - `FUN_140045168` line 65576
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65571:       bVar3 = 3;
 65572:       FUN_1400444d0(param_1,0x6c,3,0);
 65573:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65574:         DbgPrint("--- Don\'t do CSC Full/Limit Convert--- (vfmt-S)\n");
 65575:       }
 65576:       thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65577:       bVar3 = 0;
 65578:       FUN_140044528(param_1,0x85,0);
 65579:       goto LAB_1400455c2;
 65580:     }
 65581:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0577 - `FUN_140045168` line 65578
- Register: `0x85 `
- Kind: `direct_write`
- Value: `0`
```c
 65573:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65574:         DbgPrint("--- Don\'t do CSC Full/Limit Convert--- (vfmt-S)\n");
 65575:       }
 65576:       thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65577:       bVar3 = 0;
 65578:       FUN_140044528(param_1,0x85,0);
 65579:       goto LAB_1400455c2;
 65580:     }
 65581:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65582:       DbgPrint("---- input RGB to output YUV ---- (vfmt-S)\n");
 65583:     }
```

### W0578 - `FUN_140045168` line 65586
- Register: `0x6C `
- Kind: `rmw_write`
- Mask: `3`
- Value: `2`
```c
 65581:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65582:       DbgPrint("---- input RGB to output YUV ---- (vfmt-S)\n");
 65583:     }
 65584:     bVar8 = 2;
 65585:     bVar3 = 3;
 65586:     FUN_1400444d0(param_1,0x6c,3,2);
 65587:     if (*(int *)(param_1 + 0x210) == 2) {
 65588:       uVar5 = (*(int *)(param_1 + 0x214) != 1) + 2;
 65589:     }
 65590:     else if (*(int *)(param_1 + 0x214) == 1) {
 65591:       uVar5 = 0;
```

### W0579 - `FUN_140045168` line 65619
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65614:     if (uVar5 == 0) {
 65615:       bVar3 = 0xc0;
 65616:     }
 65617:     DbgPrint("--- Doing CSC Full/Limit Convert, Using CSC_TABLE = %d %s --- (vfmt-S)\n",uVar5);
 65618:   }
 65619:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65620:   bVar3 = 0x16;
 65621:   puVar9 = &DAT_14009f5c0 + (ulonglong)uVar5 * 0x16;
 65622:   FUN_1400443b0(param_1,0x70,0x16,puVar9);
 65623:   bVar8 = (byte)puVar9;
 65624: LAB_1400455c2:
```

### W0580 - `FUN_140045168` line 65622
- Register: `0x70 `
- Kind: `bulk_write`
- Count: `0x16`
- Value source: `bulk from puVar9`
```c
 65617:     DbgPrint("--- Doing CSC Full/Limit Convert, Using CSC_TABLE = %d %s --- (vfmt-S)\n",uVar5);
 65618:   }
 65619:   thunk_FUN_1400444d0(param_1,1,bVar3,bVar8);
 65620:   bVar3 = 0x16;
 65621:   puVar9 = &DAT_14009f5c0 + (ulonglong)uVar5 * 0x16;
 65622:   FUN_1400443b0(param_1,0x70,0x16,puVar9);
 65623:   bVar8 = (byte)puVar9;
 65624: LAB_1400455c2:
 65625:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65626:   FUN_14003ee7c(param_1);
 65627:   return;
```

### W0581 - `FUN_140045168` line 65625
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65620:   bVar3 = 0x16;
 65621:   puVar9 = &DAT_14009f5c0 + (ulonglong)uVar5 * 0x16;
 65622:   FUN_1400443b0(param_1,0x70,0x16,puVar9);
 65623:   bVar8 = (byte)puVar9;
 65624: LAB_1400455c2:
 65625:   thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65626:   FUN_14003ee7c(param_1);
 65627:   return;
 65628: }
 65629: 
 65630: 
```

### W0582 - `FUN_140045654` line 65665
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65660:   undefined1 uVar1;
 65661:   undefined1 *puVar2;
 65662:   byte bVar3;
 65663: 
 65664:   bVar3 = 2;
 65665:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65666:   uVar1 = FUN_140044484(param_1,0x13);
 65667:   *param_2 = uVar1;
 65668:   uVar1 = FUN_140044484(param_1,0x12);
 65669:   param_2[1] = uVar1;
 65670:   puVar2 = param_2 + 2;
```

### W0583 - `FUN_140045654` line 65677
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65672:     uVar1 = FUN_140044484(param_1,bVar3 + 0x12);
 65673:     *puVar2 = uVar1;
 65674:     bVar3 = bVar3 + 1;
 65675:     puVar2 = puVar2 + 1;
 65676:   } while (bVar3 < 0x12);
 65677:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65678:   return;
 65679: }
 65680: 
 65681: 
 65682: 
```

### W0584 - `FUN_1400456c4` line 65690
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65685: {
 65686:   undefined1 uVar1;
 65687:   byte bVar2;
 65688:   undefined1 *puVar3;
 65689: 
 65690:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65691:   uVar1 = FUN_140044484(param_1,0x43);
 65692:   *param_2 = uVar1;
 65693:   uVar1 = FUN_140044484(param_1,0x4a);
 65694:   param_2[1] = uVar1;
 65695:   puVar3 = param_2 + 2;
```

### W0585 - `FUN_1400456c4` line 65705
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65700:     bVar2 = bVar2 + 1;
 65701:     puVar3 = puVar3 + 1;
 65702:   } while (bVar2 < 6);
 65703:   *(undefined4 *)(param_2 + 8) = 0;
 65704:   param_2[0xc] = 0;
 65705:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65706:   return;
 65707: }
 65708: 
 65709: 
 65710: 
```

### W0586 - `FUN_140045788` line 65737
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65732: void FUN_140045788(longlong param_1,undefined1 *param_2,byte param_3,byte param_4)
 65733: 
 65734: {
 65735:   undefined1 uVar1;
 65736: 
 65737:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65738:   uVar1 = FUN_140044484(param_1,0x10);
 65739:   *param_2 = uVar1;
 65740:   uVar1 = FUN_140044484(param_1,0x11);
 65741:   param_2[1] = uVar1;
 65742:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
```

### W0587 - `FUN_140045788` line 65742
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65737:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65738:   uVar1 = FUN_140044484(param_1,0x10);
 65739:   *param_2 = uVar1;
 65740:   uVar1 = FUN_140044484(param_1,0x11);
 65741:   param_2[1] = uVar1;
 65742:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65743:   return;
 65744: }
 65745: 
 65746: 
 65747: 
```

### W0588 - `FUN_1400457d0` line 65756
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65751:   undefined1 uVar1;
 65752:   undefined1 *puVar2;
 65753:   byte bVar3;
 65754: 
 65755:   bVar3 = 2;
 65756:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65757:   uVar1 = FUN_140044484(param_1,0xda);
 65758:   *param_2 = uVar1;
 65759:   uVar1 = FUN_140044484(param_1,0xdb);
 65760:   param_2[1] = uVar1;
 65761:   puVar2 = param_2 + 2;
```

### W0589 - `FUN_1400457d0` line 65768
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65763:     uVar1 = FUN_140044484(param_1,bVar3 - 0x26);
 65764:     *puVar2 = uVar1;
 65765:     bVar3 = bVar3 + 1;
 65766:     puVar2 = puVar2 + 1;
 65767:   } while (bVar3 < 0x1e);
 65768:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65769:   return;
 65770: }
 65771: 
 65772: 
 65773: 
```

### W0590 - `FUN_140045a28` line 65879
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65874:           DbgPrint(
 65875:                   "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 65876:                   );
 65877:         }
 65878:         *(undefined1 *)(param_1 + 0x194) = 0;
 65879:         thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65880:         FUN_1400444d0(param_1,0x81,0x40,0);
 65881:         FUN_14003ee30(param_1,'\x01');
 65882:       }
 65883:     }
 65884:     if ((DAT_1400a04f8 & 0x10) == 0) {
```

### W0591 - `FUN_140045a28` line 65880
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 65875:                   "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 65876:                   );
 65877:         }
 65878:         *(undefined1 *)(param_1 + 0x194) = 0;
 65879:         thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65880:         FUN_1400444d0(param_1,0x81,0x40,0);
 65881:         FUN_14003ee30(param_1,'\x01');
 65882:       }
 65883:     }
 65884:     if ((DAT_1400a04f8 & 0x10) == 0) {
 65885:       return;
```

### W0592 - `FUN_140045b30` line 65926
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65921:   bVar1 = FUN_14003ce9c(param_1);
 65922:   if (*(byte *)(param_1 + 0x1a5) != bVar1) {
 65923:     bVar1 = FUN_14003ce9c(param_1);
 65924:     *(byte *)(param_1 + 0x1a5) = bVar1;
 65925:     if (bVar1 != 0) {
 65926:       thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4);
 65927:       uVar5 = 0;
 65928:       uVar3 = 3;
 65929:       FUN_1400444d0(param_1,0x40,3,0);
 65930:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65931:         DbgPrint("# SCDT ON #\n");
```

### W0593 - `FUN_140045b30` line 65929
- Register: `0x40 `
- Kind: `rmw_write`
- Mask: `3`
- Value: `0`
```c
 65924:     *(byte *)(param_1 + 0x1a5) = bVar1;
 65925:     if (bVar1 != 0) {
 65926:       thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4);
 65927:       uVar5 = 0;
 65928:       uVar3 = 3;
 65929:       FUN_1400444d0(param_1,0x40,3,0);
 65930:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65931:         DbgPrint("# SCDT ON #\n");
 65932:       }
 65933:       FUN_1400480e0(param_1,3,CONCAT71(uVar4,uVar3),uVar5);
 65934:       return;
```

### W0594 - `FUN_140045b30` line 65936
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 65931:         DbgPrint("# SCDT ON #\n");
 65932:       }
 65933:       FUN_1400480e0(param_1,3,CONCAT71(uVar4,uVar3),uVar5);
 65934:       return;
 65935:     }
 65936:     thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4);
 65937:     uVar5 = CONCAT71((int7)((ulonglong)param_4 >> 8),2);
 65938:     bVar1 = 3;
 65939:     FUN_1400444d0(param_1,0x40,3,2);
 65940:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65941:       DbgPrint("# SCDT OFF #\n");
```

### W0595 - `FUN_140045b30` line 65939
- Register: `0x40 `
- Kind: `rmw_write`
- Mask: `3`
- Value: `2`
```c
 65934:       return;
 65935:     }
 65936:     thunk_FUN_1400444d0(param_1,0,bVar2,(byte)param_4);
 65937:     uVar5 = CONCAT71((int7)((ulonglong)param_4 >> 8),2);
 65938:     bVar1 = 3;
 65939:     FUN_1400444d0(param_1,0x40,3,2);
 65940:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65941:       DbgPrint("# SCDT OFF #\n");
 65942:     }
 65943:     FUN_1400480e0(param_1,2,CONCAT71(uVar4,bVar1),uVar5);
 65944:     bVar2 = (byte)uVar5;
```

### W0596 - `FUN_140045eec` line 66106
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66101:     if ((DAT_1400a04f8 & 0x10) != 0) {
 66102:       DbgPrint("\n\nCustomer Setting Current Port = %d ! \n\n");
 66103:     }
 66104:     *(undefined1 *)(param_1 + 0x204) = 0;
 66105:     if (*(char *)(param_1 + 0x1ec) != param_2) {
 66106:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66107:       if (param_2 == '\0') {
 66108:         bVar3 = 0;
 66109:         FUN_1400444d0(param_1,0x35,1,0);
 66110:         sVar1 = 1;
 66111:       }
```

### W0597 - `FUN_140045eec` line 66109
- Register: `0x35 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 66104:     *(undefined1 *)(param_1 + 0x204) = 0;
 66105:     if (*(char *)(param_1 + 0x1ec) != param_2) {
 66106:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66107:       if (param_2 == '\0') {
 66108:         bVar3 = 0;
 66109:         FUN_1400444d0(param_1,0x35,1,0);
 66110:         sVar1 = 1;
 66111:       }
 66112:       else {
 66113:         bVar3 = 1;
 66114:         FUN_1400444d0(param_1,0x35,1,1);
```

### W0598 - `FUN_140045eec` line 66114
- Register: `0x35 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 66109:         FUN_1400444d0(param_1,0x35,1,0);
 66110:         sVar1 = 1;
 66111:       }
 66112:       else {
 66113:         bVar3 = 1;
 66114:         FUN_1400444d0(param_1,0x35,1,1);
 66115:         if (*(char *)(param_1 + 0x1d4) == -0x60) {
 66116:           bVar3 = 0;
 66117:           FUN_1400444d0(param_1,0xe2,1,0);
 66118:         }
 66119:         sVar1 = 0;
```

### W0599 - `FUN_140045eec` line 66117
- Register: `0xE2 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `0`
```c
 66112:       else {
 66113:         bVar3 = 1;
 66114:         FUN_1400444d0(param_1,0x35,1,1);
 66115:         if (*(char *)(param_1 + 0x1d4) == -0x60) {
 66116:           bVar3 = 0;
 66117:           FUN_1400444d0(param_1,0xe2,1,0);
 66118:         }
 66119:         sVar1 = 0;
 66120:       }
 66121:       uVar4 = 0;
 66122:       FUN_14003f500(param_1,sVar1,0);
```

### W0600 - `FUN_140045eec` line 66124
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66119:         sVar1 = 0;
 66120:       }
 66121:       uVar4 = 0;
 66122:       FUN_14003f500(param_1,sVar1,0);
 66123:       uVar2 = 0;
 66124:       thunk_FUN_1400444d0(param_1,0,(byte)uVar4,bVar3);
 66125:       *(char *)(param_1 + 0x1ec) = param_2;
 66126:       if (*(char *)(param_1 + 0x201) != *(char *)(param_1 + 0x202)) {
 66127:         FUN_140040960(param_1,uVar2,uVar4,bVar3);
 66128:       }
 66129:       thunk_FUN_1400444d0(param_1,3,(byte)uVar4,bVar3);
```

### W0601 - `FUN_140045eec` line 66129
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66124:       thunk_FUN_1400444d0(param_1,0,(byte)uVar4,bVar3);
 66125:       *(char *)(param_1 + 0x1ec) = param_2;
 66126:       if (*(char *)(param_1 + 0x201) != *(char *)(param_1 + 0x202)) {
 66127:         FUN_140040960(param_1,uVar2,uVar4,bVar3);
 66128:       }
 66129:       thunk_FUN_1400444d0(param_1,3,(byte)uVar4,bVar3);
 66130:       bVar7 = 0;
 66131:       bVar3 = 0x1c;
 66132:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
```

### W0602 - `FUN_140045eec` line 66132
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0`
```c
 66127:         FUN_140040960(param_1,uVar2,uVar4,bVar3);
 66128:       }
 66129:       thunk_FUN_1400444d0(param_1,3,(byte)uVar4,bVar3);
 66130:       bVar7 = 0;
 66131:       bVar3 = 0x1c;
 66132:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
```

### W0603 - `FUN_140045eec` line 66133
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66128:       }
 66129:       thunk_FUN_1400444d0(param_1,3,(byte)uVar4,bVar3);
 66130:       bVar7 = 0;
 66131:       bVar3 = 0x1c;
 66132:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
```

### W0604 - `FUN_140045eec` line 66136
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0`
```c
 66131:       bVar3 = 0x1c;
 66132:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
```

### W0605 - `FUN_140045eec` line 66137
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66132:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
```

### W0606 - `FUN_140045eec` line 66138
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66133:       thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
```

### W0607 - `FUN_140045eec` line 66139
- Register: `0x25 `
- Kind: `direct_write`
- Value: `0`
```c
 66134:       bVar7 = 0;
 66135:       bVar3 = 0x1c;
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
```

### W0608 - `FUN_140045eec` line 66141
- Register: `0x26 `
- Kind: `direct_write`
- Value: `0`
```c
 66136:       FUN_1400444d0(param_1,0xe5,0x1c,0);
 66137:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
```

### W0609 - `FUN_140045eec` line 66143
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0`
```c
 66138:       thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66139:       FUN_140044528(param_1,0x25,0);
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
```

### W0610 - `FUN_140045eec` line 66145
- Register: `0x2A `
- Kind: `direct_write`
- Value: `1`
```c
 66140:       if (*(char *)(param_1 + 0x1ec) == '\0') {
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
```

### W0611 - `FUN_140045eec` line 66146
- Register: `0x2D `
- Kind: `direct_write`
- Value: `0xff`
```c
 66141:         FUN_140044528(param_1,0x26,0);
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
```

### W0612 - `FUN_140045eec` line 66147
- Register: `0x2E `
- Kind: `direct_write`
- Value: `0xff`
```c
 66142:         uVar2 = 0;
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
```

### W0613 - `FUN_140045eec` line 66148
- Register: `0x2F `
- Kind: `direct_write`
- Value: `0xff`
```c
 66143:         FUN_140044528(param_1,0x27,0);
 66144:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
 66153:         bVar3 = 8;
```

### W0614 - `FUN_140045eec` line 66150
- Register: `0x32 `
- Kind: `direct_write`
- Value: `0x3e`
```c
 66145:         FUN_140044528(param_1,0x2a,1);
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
 66153:         bVar3 = 8;
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
```

### W0615 - `FUN_140045eec` line 66151
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66146:         FUN_140044528(param_1,0x2d,0xff);
 66147:         FUN_140044528(param_1,0x2e,0xff);
 66148:         FUN_140044528(param_1,0x2f,0xff);
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
 66153:         bVar3 = 8;
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
```

### W0616 - `FUN_140045eec` line 66154
- Register: `0xA8 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 66149:         bVar3 = 0x3e;
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
 66153:         bVar3 = 8;
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
 66157:         bVar3 = 8;
 66158:         FUN_1400444d0(param_1,0xa8,8,0);
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
```

### W0617 - `FUN_140045eec` line 66155
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66150:         FUN_140044528(param_1,0x32,0x3e);
 66151:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66152:         bVar7 = 8;
 66153:         bVar3 = 8;
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
 66157:         bVar3 = 8;
 66158:         FUN_1400444d0(param_1,0xa8,8,0);
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66160:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
```

### W0618 - `FUN_140045eec` line 66158
- Register: `0xA8 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `0`
```c
 66153:         bVar3 = 8;
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
 66157:         bVar3 = 8;
 66158:         FUN_1400444d0(param_1,0xa8,8,0);
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66160:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66161:         FUN_140048644(param_1,1);
 66162:         uVar8 = 0;
 66163:         uVar5 = CONCAT71(uVar6,0x10);
```

### W0619 - `FUN_140045eec` line 66159
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66154:         FUN_1400444d0(param_1,0xa8,8,8);
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
 66157:         bVar3 = 8;
 66158:         FUN_1400444d0(param_1,0xa8,8,0);
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66160:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66161:         FUN_140048644(param_1,1);
 66162:         uVar8 = 0;
 66163:         uVar5 = CONCAT71(uVar6,0x10);
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0620 - `FUN_140045eec` line 66160
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 66155:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66156:         bVar7 = 0;
 66157:         bVar3 = 8;
 66158:         FUN_1400444d0(param_1,0xa8,8,0);
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66160:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66161:         FUN_140048644(param_1,1);
 66162:         uVar8 = 0;
 66163:         uVar5 = CONCAT71(uVar6,0x10);
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66165:       }
```

### W0621 - `FUN_140045eec` line 66164
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 66159:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66160:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66161:         FUN_140048644(param_1,1);
 66162:         uVar8 = 0;
 66163:         uVar5 = CONCAT71(uVar6,0x10);
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66165:       }
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
```

### W0622 - `FUN_140045eec` line 66167
- Register: `0x26 `
- Kind: `direct_write`
- Value: `0xff`
```c
 66162:         uVar8 = 0;
 66163:         uVar5 = CONCAT71(uVar6,0x10);
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66165:       }
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
```

### W0623 - `FUN_140045eec` line 66168
- Register: `0x27 `
- Kind: `direct_write`
- Value: `0xff`
```c
 66163:         uVar5 = CONCAT71(uVar6,0x10);
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66165:       }
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
```

### W0624 - `FUN_140045eec` line 66169
- Register: `0x2A `
- Kind: `direct_write`
- Value: `0x3a`
```c
 66164:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66165:       }
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
```

### W0625 - `FUN_140045eec` line 66170
- Register: `0x2D `
- Kind: `direct_write`
- Value: `0`
```c
 66165:       }
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66175:         bVar3 = 1;
```

### W0626 - `FUN_140045eec` line 66171
- Register: `0x2E `
- Kind: `direct_write`
- Value: `0`
```c
 66166:       else {
 66167:         FUN_140044528(param_1,0x26,0xff);
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66175:         bVar3 = 1;
 66176:         FUN_140044528(param_1,0x32,1);
```

### W0627 - `FUN_140045eec` line 66173
- Register: `0x2F `
- Kind: `direct_write`
- Value: `0`
```c
 66168:         FUN_140044528(param_1,0x27,0xff);
 66169:         FUN_140044528(param_1,0x2a,0x3a);
 66170:         FUN_140044528(param_1,0x2d,0);
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66175:         bVar3 = 1;
 66176:         FUN_140044528(param_1,0x32,1);
 66177:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66178:         bVar7 = 0;
```

### W0628 - `FUN_140045eec` line 66176
- Register: `0x32 `
- Kind: `direct_write`
- Value: `1`
```c
 66171:         FUN_140044528(param_1,0x2e,0);
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66175:         bVar3 = 1;
 66176:         FUN_140044528(param_1,0x32,1);
 66177:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66178:         bVar7 = 0;
 66179:         bVar3 = 8;
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
```

### W0629 - `FUN_140045eec` line 66177
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66172:         uVar2 = 0;
 66173:         FUN_140044528(param_1,0x2f,0);
 66174:         uVar6 = (undefined7)((ulonglong)uVar2 >> 8);
 66175:         bVar3 = 1;
 66176:         FUN_140044528(param_1,0x32,1);
 66177:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66178:         bVar7 = 0;
 66179:         bVar3 = 8;
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
```

### W0630 - `FUN_140045eec` line 66180
- Register: `0xA8 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `0`
```c
 66175:         bVar3 = 1;
 66176:         FUN_140044528(param_1,0x32,1);
 66177:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66178:         bVar7 = 0;
 66179:         bVar3 = 8;
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
```

### W0631 - `FUN_140045eec` line 66181
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66176:         FUN_140044528(param_1,0x32,1);
 66177:         thunk_FUN_1400444d0(param_1,3,bVar3,bVar7);
 66178:         bVar7 = 0;
 66179:         bVar3 = 8;
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
```

### W0632 - `FUN_140045eec` line 66184
- Register: `0xA8 `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 66179:         bVar3 = 8;
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
```

### W0633 - `FUN_140045eec` line 66185
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66180:         FUN_1400444d0(param_1,0xa8,8,0);
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
 66190:         uVar5 = CONCAT71(uVar6,0x10);
```

### W0634 - `FUN_140045eec` line 66186
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66181:         thunk_FUN_1400444d0(param_1,7,bVar3,bVar7);
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
 66190:         uVar5 = CONCAT71(uVar6,0x10);
 66191:         FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0635 - `FUN_140045eec` line 66187
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 66182:         bVar7 = 8;
 66183:         bVar3 = 8;
 66184:         FUN_1400444d0(param_1,0xa8,8,8);
 66185:         thunk_FUN_1400444d0(param_1,0,bVar3,bVar7);
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
 66190:         uVar5 = CONCAT71(uVar6,0x10);
 66191:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66192:         thunk_FUN_1400444d0(param_1,0,(byte)uVar5,(byte)uVar8);
```

### W0636 - `FUN_140045eec` line 66191
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 66186:         thunk_FUN_1400444d0(param_1,4,bVar3,bVar7);
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
 66190:         uVar5 = CONCAT71(uVar6,0x10);
 66191:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66192:         thunk_FUN_1400444d0(param_1,0,(byte)uVar5,(byte)uVar8);
 66193:       }
 66194:       *(undefined4 *)(param_1 + 0x1e4) = 0;
 66195:       FUN_140041b38(param_1,0,uVar5,uVar8);
 66196:       FUN_1400458b4(param_1,*(byte *)(param_1 + 0x1ec),uVar5,uVar8);
```

### W0637 - `FUN_140045eec` line 66192
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66187:         FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66188:         FUN_140048644(param_1,1);
 66189:         uVar8 = 0;
 66190:         uVar5 = CONCAT71(uVar6,0x10);
 66191:         FUN_1400444d0(param_1,0xc5,0x10,0);
 66192:         thunk_FUN_1400444d0(param_1,0,(byte)uVar5,(byte)uVar8);
 66193:       }
 66194:       *(undefined4 *)(param_1 + 0x1e4) = 0;
 66195:       FUN_140041b38(param_1,0,uVar5,uVar8);
 66196:       FUN_1400458b4(param_1,*(byte *)(param_1 + 0x1ec),uVar5,uVar8);
 66197:     }
```

### W0638 - `FUN_1400461f4` line 66228
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66223:       }
 66224:       else {
 66225:         *(byte *)((param_2 & 0xff) + 0x201 + param_1) = bVar2;
 66226:         if ((char)param_2 == *(char *)(param_1 + 0x1ec)) {
 66227:           FUN_140040960(param_1,param_2,param_3,param_4);
 66228:           thunk_FUN_1400444d0(param_1,0,bVar2,param_4);
 66229:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66230:           bVar4 = 0;
 66231:           bVar2 = 0x10;
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
```

### W0639 - `FUN_1400461f4` line 66229
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 66224:       else {
 66225:         *(byte *)((param_2 & 0xff) + 0x201 + param_1) = bVar2;
 66226:         if ((char)param_2 == *(char *)(param_1 + 0x1ec)) {
 66227:           FUN_140040960(param_1,param_2,param_3,param_4);
 66228:           thunk_FUN_1400444d0(param_1,0,bVar2,param_4);
 66229:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66230:           bVar4 = 0;
 66231:           bVar2 = 0x10;
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
```

### W0640 - `FUN_1400461f4` line 66232
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 66227:           FUN_140040960(param_1,param_2,param_3,param_4);
 66228:           thunk_FUN_1400444d0(param_1,0,bVar2,param_4);
 66229:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66230:           bVar4 = 0;
 66231:           bVar2 = 0x10;
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66235:           uVar5 = 0;
 66236:           bVar2 = 0x10;
 66237:           FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0641 - `FUN_1400461f4` line 66233
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66228:           thunk_FUN_1400444d0(param_1,0,bVar2,param_4);
 66229:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66230:           bVar4 = 0;
 66231:           bVar2 = 0x10;
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66235:           uVar5 = 0;
 66236:           bVar2 = 0x10;
 66237:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66238:           thunk_FUN_1400444d0(param_1,0,bVar2,(byte)uVar5);
```

### W0642 - `FUN_1400461f4` line 66234
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 66229:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66230:           bVar4 = 0;
 66231:           bVar2 = 0x10;
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66235:           uVar5 = 0;
 66236:           bVar2 = 0x10;
 66237:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66238:           thunk_FUN_1400444d0(param_1,0,bVar2,(byte)uVar5);
 66239:           FUN_1400458b4(param_1,*(byte *)(param_1 + 0x1ec),CONCAT71(uVar3,bVar2),uVar5);
```

### W0643 - `FUN_1400461f4` line 66237
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 66232:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66235:           uVar5 = 0;
 66236:           bVar2 = 0x10;
 66237:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66238:           thunk_FUN_1400444d0(param_1,0,bVar2,(byte)uVar5);
 66239:           FUN_1400458b4(param_1,*(byte *)(param_1 + 0x1ec),CONCAT71(uVar3,bVar2),uVar5);
 66240:         }
 66241:       }
 66242:     }
```

### W0644 - `FUN_1400461f4` line 66238
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66233:           thunk_FUN_1400444d0(param_1,4,bVar2,bVar4);
 66234:           FUN_1400444d0(param_1,0xc5,0x10,0x10);
 66235:           uVar5 = 0;
 66236:           bVar2 = 0x10;
 66237:           FUN_1400444d0(param_1,0xc5,0x10,0);
 66238:           thunk_FUN_1400444d0(param_1,0,bVar2,(byte)uVar5);
 66239:           FUN_1400458b4(param_1,*(byte *)(param_1 + 0x1ec),CONCAT71(uVar3,bVar2),uVar5);
 66240:         }
 66241:       }
 66242:     }
 66243:     else if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0645 - `FUN_1400462e4` line 66270
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66265:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66266:         DbgPrint("[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n")
 66267:         ;
 66268:       }
 66269:       *(undefined1 *)(param_1 + 0x194) = 0;
 66270:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66271:       FUN_1400444d0(param_1,0x81,0x40,0);
 66272:       cVar1 = '\x01';
 66273:     }
 66274:     else {
 66275:       uVar2 = param_2 - 2;
```

### W0646 - `FUN_1400462e4` line 66271
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 66266:         DbgPrint("[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n")
 66267:         ;
 66268:       }
 66269:       *(undefined1 *)(param_1 + 0x194) = 0;
 66270:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66271:       FUN_1400444d0(param_1,0x81,0x40,0);
 66272:       cVar1 = '\x01';
 66273:     }
 66274:     else {
 66275:       uVar2 = param_2 - 2;
 66276:       uVar3 = (ulonglong)uVar2;
```

### W0647 - `FUN_1400462e4` line 66283
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66278:         if ((DAT_1400a04f8 & 0x10) != 0) {
 66279:           DbgPrint(
 66280:                   "[xAud] 68051 AudState change to STATEA_WaitForReady state !!!, iTE6805_aud_chg\n"
 66281:                   );
 66282:         }
 66283:         thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66284:         FUN_1400444d0(param_1,0x8c,0x10,0x10);
 66285:         FUN_1400444d0(param_1,0x8c,0x10,0);
 66286:         *(undefined1 *)(param_1 + 0x18e) = 4;
 66287:         return;
 66288:       }
```

### W0648 - `FUN_1400462e4` line 66284
- Register: `0x8C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 66279:           DbgPrint(
 66280:                   "[xAud] 68051 AudState change to STATEA_WaitForReady state !!!, iTE6805_aud_chg\n"
 66281:                   );
 66282:         }
 66283:         thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66284:         FUN_1400444d0(param_1,0x8c,0x10,0x10);
 66285:         FUN_1400444d0(param_1,0x8c,0x10,0);
 66286:         *(undefined1 *)(param_1 + 0x18e) = 4;
 66287:         return;
 66288:       }
 66289:       if (uVar2 != 1) {
```

### W0649 - `FUN_1400462e4` line 66285
- Register: `0x8C `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 66280:                   "[xAud] 68051 AudState change to STATEA_WaitForReady state !!!, iTE6805_aud_chg\n"
 66281:                   );
 66282:         }
 66283:         thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66284:         FUN_1400444d0(param_1,0x8c,0x10,0x10);
 66285:         FUN_1400444d0(param_1,0x8c,0x10,0);
 66286:         *(undefined1 *)(param_1 + 0x18e) = 4;
 66287:         return;
 66288:       }
 66289:       if (uVar2 != 1) {
 66290:         if ((DAT_1400a04f8 & 0x10) == 0) {
```

### W0650 - `FUN_140046400` line 66354
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66349:     goto LAB_140046809;
 66350:   }
 66351:   if (iVar1 == 2) {
 66352:     if (*(char *)(param_1 + 0x18e) == '\0') {
 66353:       cVar7 = '\x04';
 66354:       thunk_FUN_1400444d0(param_1,-(*(char *)(param_1 + 0x1ec) != '\0') & 4,param_3,param_4);
 66355:       bVar2 = FUN_140044484(param_1,0xb9);
 66356:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66357:       if ((bVar2 & 0xc0) != 0) {
 66358:         FUN_14003ea04(param_1);
 66359:       }
```

### W0651 - `FUN_140046400` line 66356
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66351:   if (iVar1 == 2) {
 66352:     if (*(char *)(param_1 + 0x18e) == '\0') {
 66353:       cVar7 = '\x04';
 66354:       thunk_FUN_1400444d0(param_1,-(*(char *)(param_1 + 0x1ec) != '\0') & 4,param_3,param_4);
 66355:       bVar2 = FUN_140044484(param_1,0xb9);
 66356:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66357:       if ((bVar2 & 0xc0) != 0) {
 66358:         FUN_14003ea04(param_1);
 66359:       }
 66360:       bVar2 = FUN_14003ce9c(param_1);
 66361:       if ((bVar2 != 0) && (bVar2 = FUN_14003ccac(param_1), bVar2 != 0)) {
```

### W0652 - `FUN_140046400` line 66390
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66385:   }
 66386:   cVar7 = *(char *)(param_1 + 399);
 66387:   *(char *)(param_1 + 399) = cVar7 + -1;
 66388:   if (cVar7 == '\0') {
 66389:     *(undefined1 *)(param_1 + 399) = 3;
 66390:     thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66391:     if (*(char *)(param_1 + 0x19e) == '\x01') {
 66392:       bVar2 = FUN_140044484(param_1,0xb0);
 66393:       *(byte *)(param_1 + 0x19a) = bVar2 & 0xf0;
 66394:       uVar3 = FUN_140044484(param_1,0xb1);
 66395:       *(undefined1 *)(param_1 + 0x19b) = uVar3;
```

### W0653 - `FUN_140046400` line 66400
- Register: `0x8C `
- Kind: `rmw_write`
- Mask: `8`
- Value: `0`
```c
 66395:       *(undefined1 *)(param_1 + 0x19b) = uVar3;
 66396:       bVar2 = FUN_140044484(param_1,0xb2);
 66397:       *(undefined1 *)(param_1 + 0x19e) = 0;
 66398:       *(byte *)(param_1 + 0x19c) = bVar2 & 2;
 66399:       if ((bVar2 & 2) == 0) {
 66400:         FUN_1400444d0(param_1,0x8c,8,0);
 66401:         if ((DAT_1400a04f8 & 0x10) != 0) {
 66402:           pcVar8 = "[xAud-fsm] 68051 STATEA_AudioOn -> disable HW auto mute \n";
 66403: LAB_140046513:
 66404:           DbgPrint(pcVar8);
 66405:         }
```

### W0654 - `FUN_140046400` line 66408
- Register: `0x8C `
- Kind: `rmw_write`
- Mask: `8`
- Value: `8`
```c
 66403: LAB_140046513:
 66404:           DbgPrint(pcVar8);
 66405:         }
 66406:       }
 66407:       else {
 66408:         FUN_1400444d0(param_1,0x8c,8,8);
 66409:         if ((DAT_1400a04f8 & 0x10) != 0) {
 66410:           pcVar8 = "[xAud-fsm] 68051 STATEA_AudioOn -> Enable HW auto mute \n";
 66411:           goto LAB_140046513;
 66412:         }
 66413:       }
```

### W0655 - `FUN_140046400` line 66418
- Register: `0x86 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 66413:       }
 66414:     }
 66415:     else {
 66416:       bVar2 = 1;
 66417:       bVar10 = 1;
 66418:       FUN_1400444d0(param_1,0x86,1,1);
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
```

### W0656 - `FUN_140046400` line 66419
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66414:     }
 66415:     else {
 66416:       bVar2 = 1;
 66417:       bVar10 = 1;
 66418:       FUN_1400444d0(param_1,0x86,1,1);
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
```

### W0657 - `FUN_140046400` line 66428
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
 66427:       uVar13 = (uint)bVar6 * 0x10 + (uint)(bVar4 >> 4) + (uint)bVar5 * 0x1000;
 66428:       thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66429:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66430:         uVar12 = (ulonglong)*(uint *)(param_1 + 600);
 66431:         uVar11 = (ulonglong)uVar9;
 66432:         DbgPrint("[xAud-fsm]  68051 N = %lu->%lu,  CTS = %lu->%lu \n",
 66433:                  *(undefined4 *)(param_1 + 0x254),uVar11,uVar12,uVar13);
```

### W0658 - `FUN_140046400` line 66463
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66458:             DbgPrint(
 66459:                     "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 66460:                     );
 66461:           }
 66462:           *(undefined1 *)(param_1 + 0x194) = 0;
 66463:           thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66464:           FUN_1400444d0(param_1,0x81,0x40,0);
 66465:           FUN_14003ee30(param_1,'\x01');
 66466:         }
 66467:       }
 66468:     }
```

### W0659 - `FUN_140046400` line 66464
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 66459:                     "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 66460:                     );
 66461:           }
 66462:           *(undefined1 *)(param_1 + 0x194) = 0;
 66463:           thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66464:           FUN_1400444d0(param_1,0x81,0x40,0);
 66465:           FUN_14003ee30(param_1,'\x01');
 66466:         }
 66467:       }
 66468:     }
 66469:   }
```

### W0660 - `FUN_140046828` line 66509
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66504:   undefined7 uVar20;
 66505:   ulonglong uVar19;
 66506:   uint uVar21;
 66507: 
 66508:   uVar13 = 0;
 66509:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 66510:   uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66511:   bVar2 = FUN_140044484(param_1,9);
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
```

### W0661 - `FUN_140046828` line 66513
- Register: `0x09 `
- Kind: `direct_write`
- Value: `bVar2 & 4`
```c
 66508:   uVar13 = 0;
 66509:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 66510:   uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66511:   bVar2 = FUN_140044484(param_1,9);
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
```

### W0662 - `FUN_140046828` line 66516
- Register: `0x10 `
- Kind: `direct_write`
- Value: `bVar3`
```c
 66511:   bVar2 = FUN_140044484(param_1,9);
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
```

### W0663 - `FUN_140046828` line 66519
- Register: `0x11 `
- Kind: `direct_write`
- Value: `(char)uVar14`
```c
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
```

### W0664 - `FUN_140046828` line 66522
- Register: `0x12 `
- Kind: `direct_write`
- Value: `(char)uVar14`
```c
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
```

### W0665 - `FUN_140046828` line 66525
- Register: `0x1D `
- Kind: `direct_write`
- Value: `bVar6`
```c
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
```

### W0666 - `FUN_140046828` line 66527
- Register: `0xD4 `
- Kind: `direct_write`
- Value: `bVar7`
```c
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
 66531:   FUN_140044528(param_1,0xd5,bVar8);
 66532:   if ((bVar2 & 4) != 0) {
```

### W0667 - `FUN_140046828` line 66531
- Register: `0xD5 `
- Kind: `direct_write`
- Value: `bVar8`
```c
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
 66531:   FUN_140044528(param_1,0xd5,bVar8);
 66532:   if ((bVar2 & 4) != 0) {
 66533:     uVar14 = CONCAT71((int7)(uVar14 >> 8),0xcf);
 66534:     bVar2 = FUN_140044484(param_1,0xcf);
 66535:     if ((bVar2 & 0x20) == 0) {
 66536:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0668 - `FUN_140046828` line 66546
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66541:     else {
 66542:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66543:         DbgPrint("HDCP Encryption ON! \n");
 66544:       }
 66545:       uVar13 = 0;
 66546:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66547:       uVar14 = CONCAT71((int7)((ulonglong)uVar13 >> 8),0xd6);
 66548:       bVar2 = FUN_140044484(param_1,0xd6);
 66549:       *(bool *)(param_1 + 0x1d0) = (bVar2 & 0xc0) != 0;
 66550:       *(bool *)(param_1 + 0x1cf) = (bVar2 & 0xc0) == 0;
 66551:     }
```

### W0669 - `FUN_140046828` line 66562
- Register: `0x86 `
- Kind: `rmw_write`
- Mask: `1`
- Value: `1`
```c
 66557:     }
 66558:     if ((((char)bVar3 < '\0') && (*(int *)(param_1 + 0x1dc) == 4)) &&
 66559:        (*(int *)(param_1 + 0x1e0) == 3)) {
 66560:       param_4 = CONCAT71((int7)(param_4 >> 8),1);
 66561:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66562:       FUN_1400444d0(param_1,0x86,1,1);
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
```

### W0670 - `FUN_140046828` line 66563
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66558:     if ((((char)bVar3 < '\0') && (*(int *)(param_1 + 0x1dc) == 4)) &&
 66559:        (*(int *)(param_1 + 0x1e0) == 3)) {
 66560:       param_4 = CONCAT71((int7)(param_4 >> 8),1);
 66561:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66562:       FUN_1400444d0(param_1,0x86,1,1);
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
```

### W0671 - `FUN_140046828` line 66573
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
 66571:       uVar14 = 0;
 66572:       uVar18 = (uint)bVar10 * 0x10 + (uint)(bVar2 >> 4) + (uint)bVar9 * 0x1000;
 66573:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66574:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66575:         param_4 = (ulonglong)*(uint *)(param_1 + 600);
 66576:         uVar14 = (ulonglong)*(uint *)(param_1 + 0x254);
 66577:         uVar19 = (ulonglong)uVar21;
 66578:         DbgPrint("[xAud-On] 68051 N = %lu->%lu,  CTS = %lu->%lu \n");
```

### W0672 - `FUN_140046828` line 66596
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66591:                     "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 66592:                     );
 66593:           }
 66594:           uVar13 = 0;
 66595:           *(undefined1 *)(param_1 + 0x194) = 0;
 66596:           thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66597:           param_4 = 0;
 66598:           uVar19 = CONCAT71((int7)(uVar19 >> 8),0x40);
 66599:           uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66600:           FUN_1400444d0(param_1,0x81,0x40,0);
 66601:           uVar14 = CONCAT71(uVar17,1);
```

### W0673 - `FUN_140046828` line 66600
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 66595:           *(undefined1 *)(param_1 + 0x194) = 0;
 66596:           thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66597:           param_4 = 0;
 66598:           uVar19 = CONCAT71((int7)(uVar19 >> 8),0x40);
 66599:           uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66600:           FUN_1400444d0(param_1,0x81,0x40,0);
 66601:           uVar14 = CONCAT71(uVar17,1);
 66602:           FUN_14003ee30(param_1,'\x01');
 66603:         }
 66604:       }
 66605:       else {
```

### W0674 - `FUN_140046828` line 66661
- Register: `0x11 `
- Kind: `direct_write`
- Value: `2`
```c
 66656:       DbgPrint("# CD Detect #\n");
 66657:     }
 66658:     if ((bVar4 & 2) != 0) {
 66659:       uVar19 = CONCAT71((int7)(uVar19 >> 8),2);
 66660:       uVar14 = CONCAT71((int7)(uVar14 >> 8),0x11);
 66661:       FUN_140044528(param_1,0x11,2);
 66662:     }
 66663:     if ((bVar4 & 1) != 0) {
 66664:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66665:       uVar14 = CONCAT71((int7)(uVar14 >> 8),0x11);
 66666:       FUN_140044528(param_1,0x11,1);
```

### W0675 - `FUN_140046828` line 66666
- Register: `0x11 `
- Kind: `direct_write`
- Value: `1`
```c
 66661:       FUN_140044528(param_1,0x11,2);
 66662:     }
 66663:     if ((bVar4 & 1) != 0) {
 66664:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66665:       uVar14 = CONCAT71((int7)(uVar14 >> 8),0x11);
 66666:       FUN_140044528(param_1,0x11,1);
 66667:     }
 66668:   }
 66669:   if (bVar5 != 0) {
 66670:     if ((char)bVar5 < '\0') {
 66671:       uVar13 = 0;
```

### W0676 - `FUN_140046828` line 66672
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66667:     }
 66668:   }
 66669:   if (bVar5 != 0) {
 66670:     if ((char)bVar5 < '\0') {
 66671:       uVar13 = 0;
 66672:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66673:       uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66674:       bVar2 = FUN_140044484(param_1,0x1a);
 66675:       uVar14 = CONCAT71(uVar17,0x1b);
 66676:       bVar3 = FUN_140044484(param_1,0x1b);
 66677:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0677 - `FUN_140046828` line 66687
- Register: `0x12 `
- Kind: `direct_write`
- Value: `0x80`
```c
 66682:         uVar14 = (ulonglong)(bVar3 & 7);
 66683:         DbgPrint("# VidParaChange_Sts=Reg1Bh=0x%02X Reg1Ah=0x%02X\n",uVar14,uVar19);
 66684:       }
 66685:       uVar19 = CONCAT71((int7)(uVar19 >> 8),0x80);
 66686:       uVar14 = CONCAT71((int7)(uVar14 >> 8),0x12);
 66687:       FUN_140044528(param_1,0x12,0x80);
 66688:     }
 66689:     if (((bVar5 & 0x40) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 66690:       DbgPrint("# 3D audio Valie Change #\n");
 66691:     }
 66692:     if ((bVar5 & 0x20) != 0) {
```

### W0678 - `FUN_140046828` line 66714
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66709:     }
 66710:     if ((bVar5 & 1) != 0) {
 66711:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66712:         DbgPrint("# New AVI InfoFrame Received #\n");
 66713:       }
 66714:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66715:       bVar2 = FUN_140044484(param_1,0x14);
 66716:       uVar15 = (ulonglong)bVar2;
 66717:       bVar3 = FUN_140044484(param_1,0x15);
 66718:       uVar14 = 0;
 66719:       uVar16 = (ulonglong)bVar3;
```

### W0679 - `FUN_140046828` line 66720
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66715:       bVar2 = FUN_140044484(param_1,0x14);
 66716:       uVar15 = (ulonglong)bVar2;
 66717:       bVar3 = FUN_140044484(param_1,0x15);
 66718:       uVar14 = 0;
 66719:       uVar16 = (ulonglong)bVar3;
 66720:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66721:       if (*(char *)(param_1 + 0x222) == '\x01') {
 66722:         FUN_1400449a0(param_1);
 66723:         if ((bVar2 != *(byte *)(param_1 + 0x198)) || (bVar3 != *(byte *)(param_1 + 0x199))) {
 66724:           *(byte *)(param_1 + 0x198) = bVar2;
 66725:           *(byte *)(param_1 + 0x199) = bVar3;
```

### W0680 - `FUN_140047318` line 66900
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66895: 
 66896:   bVar15 = (byte)param_4;
 66897:   uVar16 = (undefined7)((ulonglong)param_4 >> 8);
 66898:   uVar14 = (undefined7)((ulonglong)param_3 >> 8);
 66899:   uVar11 = 0;
 66900:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15);
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
```

### W0681 - `FUN_140047318` line 66903
- Register: `0x05 `
- Kind: `direct_write`
- Value: `bVar1`
```c
 66898:   uVar14 = (undefined7)((ulonglong)param_3 >> 8);
 66899:   uVar11 = 0;
 66900:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15);
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
```

### W0682 - `FUN_140047318` line 66905
- Register: `0x06 `
- Kind: `direct_write`
- Value: `bVar2`
```c
 66900:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15);
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
```

### W0683 - `FUN_140047318` line 66907
- Register: `0x08 `
- Kind: `direct_write`
- Value: `bVar3`
```c
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
```

### W0684 - `FUN_140047318` line 66910
- Register: `0x09 `
- Kind: `direct_write`
- Value: `bVar13`
```c
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
 66913:   uVar11 = CONCAT71(uVar12,0x15);
 66914:   bVar7 = FUN_140044484(param_1,0x15);
 66915:   if (bVar1 != 0) {
```

### W0685 - `FUN_140047318` line 67003
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 66998:           (0x14 < *(byte *)(param_1 + 0x193))) &&
 66999:          ((*(int *)(param_1 + 0x1e4) == 0 && (cVar9 = FUN_14003cdc4(param_1), cVar9 == '\0')))) {
 67000:         if ((DAT_1400a04f8 & 0x10) != 0) {
 67001:           DbgPrint("# Force set SCDC to 1/40#\n");
 67002:         }
 67003:         thunk_FUN_1400444d0(param_1,3,bVar13,bVar15);
 67004:         bVar15 = 0x1c;
 67005:         bVar13 = 0x1c;
 67006:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67007:         thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67008:       }
```

### W0686 - `FUN_140047318` line 67006
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0x1c`
```c
 67001:           DbgPrint("# Force set SCDC to 1/40#\n");
 67002:         }
 67003:         thunk_FUN_1400444d0(param_1,3,bVar13,bVar15);
 67004:         bVar15 = 0x1c;
 67005:         bVar13 = 0x1c;
 67006:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67007:         thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67008:       }
 67009:     }
 67010:     if (((bVar2 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67011:       DbgPrint("# CH2 CDR FIFO Aut0-Rst #\n");
```

### W0687 - `FUN_140047318` line 67007
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67002:         }
 67003:         thunk_FUN_1400444d0(param_1,3,bVar13,bVar15);
 67004:         bVar15 = 0x1c;
 67005:         bVar13 = 0x1c;
 67006:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67007:         thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67008:       }
 67009:     }
 67010:     if (((bVar2 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67011:       DbgPrint("# CH2 CDR FIFO Aut0-Rst #\n");
 67012:     }
```

### W0688 - `FUN_140047318` line 67027
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67022:       }
 67023:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67024:         FUN_140044484(param_1,0x14);
 67025:         DbgPrint(" 0x14 =  %x \n");
 67026:       }
 67027:       thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67028:       bVar1 = FUN_140044484(param_1,0x14);
 67029:       if (((bVar1 & 0x38) != 0) && (*(int *)(param_1 + 0x1e4) == 0)) {
 67030:         FUN_140041b38(param_1,0,CONCAT71(uVar14,bVar13),CONCAT71(uVar16,bVar15));
 67031:         FUN_140041b38(param_1,1,CONCAT71(uVar14,bVar13),CONCAT71(uVar16,bVar15));
 67032:       }
```

### W0689 - `FUN_140047318` line 67117
- Register: `0x4C `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 67112:   if ((char)bVar3 < '\0') {
 67113:     if ((DAT_1400a04f8 & 0x10) != 0) {
 67114:       bVar1 = FUN_140044484(param_1,0x15);
 67115:       DbgPrint("HDMI2 Det State=%x \n",bVar1 >> 2 & 0xf);
 67116:     }
 67117:     FUN_1400444d0(param_1,0x4c,0x80,0x80);
 67118:   }
 67119: LAB_14004796f:
 67120:   if (bVar4 != 0) {
 67121:     if (((bVar4 & 1) != 0) && (*(undefined1 *)(param_1 + 0x18c) = 0, (DAT_1400a04f8 & 0x10) != 0)) {
 67122:       DbgPrint("# Port 0 HDCP Authentication Start #\n");
```

### W0690 - `FUN_1400479f8` line 67161
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67156: 
 67157:   bVar16 = (byte)param_4;
 67158:   uVar17 = (undefined7)((ulonglong)param_4 >> 8);
 67159:   uVar15 = (undefined7)((ulonglong)param_3 >> 8);
 67160:   uVar11 = 0;
 67161:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16);
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
```

### W0691 - `FUN_1400479f8` line 67164
- Register: `0x0A `
- Kind: `direct_write`
- Value: `bVar1`
```c
 67159:   uVar15 = (undefined7)((ulonglong)param_3 >> 8);
 67160:   uVar11 = 0;
 67161:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16);
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
```

### W0692 - `FUN_1400479f8` line 67166
- Register: `0x0B `
- Kind: `direct_write`
- Value: `bVar2`
```c
 67161:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16);
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
```

### W0693 - `FUN_1400479f8` line 67168
- Register: `0x0D `
- Kind: `direct_write`
- Value: `bVar3`
```c
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
```

### W0694 - `FUN_1400479f8` line 67171
- Register: `0x0E `
- Kind: `direct_write`
- Value: `bVar4`
```c
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
 67174:   uVar12 = CONCAT71(uVar13,0x18);
 67175:   bVar7 = FUN_140044484(param_1,0x18);
 67176:   if (bVar1 != 0) {
```

### W0695 - `FUN_1400479f8` line 67263
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67258:           (0x14 < *(byte *)(param_1 + 0x193))) &&
 67259:          ((*(int *)(param_1 + 0x1e4) == 0 && (cVar9 = FUN_14003cdc4(param_1), cVar9 == '\0')))) {
 67260:         if ((DAT_1400a04f8 & 0x10) != 0) {
 67261:           DbgPrint("# Force set SCDC to 1/40#\n");
 67262:         }
 67263:         thunk_FUN_1400444d0(param_1,7,bVar14,bVar16);
 67264:         bVar16 = 0x1c;
 67265:         bVar14 = 0x1c;
 67266:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67267:         thunk_FUN_1400444d0(param_1,0,bVar14,bVar16);
 67268:       }
```

### W0696 - `FUN_1400479f8` line 67266
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0x1c`
```c
 67261:           DbgPrint("# Force set SCDC to 1/40#\n");
 67262:         }
 67263:         thunk_FUN_1400444d0(param_1,7,bVar14,bVar16);
 67264:         bVar16 = 0x1c;
 67265:         bVar14 = 0x1c;
 67266:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67267:         thunk_FUN_1400444d0(param_1,0,bVar14,bVar16);
 67268:       }
 67269:     }
 67270:     if (((bVar2 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67271:       DbgPrint("# CH2 CDR FIFO Aut0-Rst #\n");
```

### W0697 - `FUN_1400479f8` line 67267
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67262:         }
 67263:         thunk_FUN_1400444d0(param_1,7,bVar14,bVar16);
 67264:         bVar16 = 0x1c;
 67265:         bVar14 = 0x1c;
 67266:         FUN_1400444d0(param_1,0xe5,0x1c,0x1c);
 67267:         thunk_FUN_1400444d0(param_1,0,bVar14,bVar16);
 67268:       }
 67269:     }
 67270:     if (((bVar2 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67271:       DbgPrint("# CH2 CDR FIFO Aut0-Rst #\n");
 67272:     }
```

### W0698 - `FUN_1400479f8` line 67283
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67278:     }
 67279:     if ((bVar2 & 1) != 0) {
 67280:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67281:         DbgPrint("# Symbol Lock State Change #\n");
 67282:       }
 67283:       thunk_FUN_1400444d0(param_1,0,bVar14,bVar16);
 67284:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67285:         bVar1 = FUN_140044484(param_1,0x17);
 67286:         DbgPrint(" 0x17 =  %x \n",bVar1 & 0x38);
 67287:       }
 67288:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### W0699 - `FUN_1400479f8` line 67380
- Register: `0x4C `
- Kind: `rmw_write`
- Mask: `0x80`
- Value: `0x80`
```c
 67375:   if ((char)bVar3 < '\0') {
 67376:     if ((DAT_1400a04f8 & 0x10) != 0) {
 67377:       bVar14 = FUN_140044484(param_1,0x15);
 67378:       DbgPrint("HDMI2 Det State=%x \n",bVar14 >> 2 & 0xf);
 67379:     }
 67380:     FUN_1400444d0(param_1,0x4c,0x80,0x80);
 67381:   }
 67382: LAB_140048057:
 67383:   if (bVar4 != 0) {
 67384:     if (((bVar4 & 1) != 0) && (*(undefined1 *)(param_1 + 0x18c) = 0, (DAT_1400a04f8 & 0x10) != 0)) {
 67385:       DbgPrint("# Port 1 HDCP Authentication Start #\n");
```

### W0700 - `FUN_1400480e0` line 67416
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67411:     FUN_14003f500(param_1,1,0);
 67412:     return;
 67413:   }
 67414:   if (param_2 == 1) {
 67415:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 67416:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67417:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67418:       bVar3 = 0x10;
 67419:       bVar5 = 0;
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
```

### W0701 - `FUN_1400480e0` line 67417
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 67412:     return;
 67413:   }
 67414:   if (param_2 == 1) {
 67415:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 67416:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67417:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67418:       bVar3 = 0x10;
 67419:       bVar5 = 0;
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
```

### W0702 - `FUN_1400480e0` line 67420
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 67415:     if ((*(char *)(param_1 + 0x1d4) == -0x60) || (*(char *)(param_1 + 0x1d4) == -0x5f)) {
 67416:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67417:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67418:       bVar3 = 0x10;
 67419:       bVar5 = 0;
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67423:       param_4 = 0;
 67424:       param_3 = 0x10;
 67425:       FUN_1400444d0(param_1,0xc5,0x10,0);
```

### W0703 - `FUN_1400480e0` line 67421
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67416:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67417:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67418:       bVar3 = 0x10;
 67419:       bVar5 = 0;
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67423:       param_4 = 0;
 67424:       param_3 = 0x10;
 67425:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67426:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
```

### W0704 - `FUN_1400480e0` line 67422
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0x10`
```c
 67417:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67418:       bVar3 = 0x10;
 67419:       bVar5 = 0;
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67423:       param_4 = 0;
 67424:       param_3 = 0x10;
 67425:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67426:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67427:     }
```

### W0705 - `FUN_1400480e0` line 67425
- Register: `0xC5 `
- Kind: `rmw_write`
- Mask: `0x10`
- Value: `0`
```c
 67420:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67423:       param_4 = 0;
 67424:       param_3 = 0x10;
 67425:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67426:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67427:     }
 67428:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 67429:       bVar3 = 3;
 67430: LAB_14004834c:
```

### W0706 - `FUN_1400480e0` line 67426
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67421:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar5);
 67422:       FUN_1400444d0(param_1,0xc5,0x10,0x10);
 67423:       param_4 = 0;
 67424:       param_3 = 0x10;
 67425:       FUN_1400444d0(param_1,0xc5,0x10,0);
 67426:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67427:     }
 67428:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 67429:       bVar3 = 3;
 67430: LAB_14004834c:
 67431:       thunk_FUN_1400444d0(param_1,bVar3,(byte)param_3,(byte)param_4);
```

### W0707 - `FUN_1400480e0` line 67431
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67426:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67427:     }
 67428:     if (*(char *)(param_1 + 0x1ec) == '\0') {
 67429:       bVar3 = 3;
 67430: LAB_14004834c:
 67431:       thunk_FUN_1400444d0(param_1,bVar3,(byte)param_3,(byte)param_4);
 67432:     }
 67433:     else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 67434:       bVar3 = 7;
 67435:       goto LAB_14004834c;
 67436:     }
```

### W0708 - `FUN_1400480e0` line 67439
- Register: `0xE5 `
- Kind: `rmw_write`
- Mask: `0x1c`
- Value: `0`
```c
 67434:       bVar3 = 7;
 67435:       goto LAB_14004834c;
 67436:     }
 67437:     bVar3 = 0x1c;
 67438:     bVar5 = 0;
 67439:     FUN_1400444d0(param_1,0xe5,0x1c,0);
 67440:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 67441:     *(undefined2 *)(param_1 + 0x222) = 0;
 67442:     *(undefined2 *)(param_1 + 0x1ac) = 0;
 67443:     *(undefined4 *)(param_1 + 0x198) = 0;
 67444:     *(undefined1 *)(param_1 + 0x195) = 0;
```

### W0709 - `FUN_1400480e0` line 67440
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67435:       goto LAB_14004834c;
 67436:     }
 67437:     bVar3 = 0x1c;
 67438:     bVar5 = 0;
 67439:     FUN_1400444d0(param_1,0xe5,0x1c,0);
 67440:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar5);
 67441:     *(undefined2 *)(param_1 + 0x222) = 0;
 67442:     *(undefined2 *)(param_1 + 0x1ac) = 0;
 67443:     *(undefined4 *)(param_1 + 0x198) = 0;
 67444:     *(undefined1 *)(param_1 + 0x195) = 0;
 67445:     *(undefined1 *)(param_1 + 0x197) = 0;
```

### W0710 - `FUN_1400480e0` line 67452
- Register: `0x08 `
- Kind: `direct_write`
- Value: `4`
```c
 67447:     *(undefined1 *)(param_1 + 0x224) = 0;
 67448:     *(undefined1 *)(param_1 + 0x227) = 0;
 67449:     *(undefined1 *)(param_1 + 0x19c) = 0;
 67450:     *(undefined2 *)(param_1 + 0x191) = 0;
 67451:     *(undefined1 *)(param_1 + 0x193) = 0;
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
```

### W0711 - `FUN_1400480e0` line 67453
- Register: `0x0D `
- Kind: `direct_write`
- Value: `4`
```c
 67448:     *(undefined1 *)(param_1 + 0x227) = 0;
 67449:     *(undefined1 *)(param_1 + 0x19c) = 0;
 67450:     *(undefined2 *)(param_1 + 0x191) = 0;
 67451:     *(undefined1 *)(param_1 + 0x193) = 0;
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
```

### W0712 - `FUN_1400480e0` line 67454
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x12`
```c
 67449:     *(undefined1 *)(param_1 + 0x19c) = 0;
 67450:     *(undefined2 *)(param_1 + 0x191) = 0;
 67451:     *(undefined1 *)(param_1 + 0x193) = 0;
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
```

### W0713 - `FUN_1400480e0` line 67455
- Register: `0x22 `
- Kind: `direct_write`
- Value: `0x10`
```c
 67450:     *(undefined2 *)(param_1 + 0x191) = 0;
 67451:     *(undefined1 *)(param_1 + 0x193) = 0;
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 67460:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140048447;
```

### W0714 - `FUN_1400480e0` line 67456
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xac`
```c
 67451:     *(undefined1 *)(param_1 + 0x193) = 0;
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 67460:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140048447;
 67461:     pcVar1 = "VidState change to STATEV_Unplug state\n";
```

### W0715 - `FUN_1400480e0` line 67457
- Register: `0x23 `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xa0`
```c
 67452:     FUN_140044528(param_1,8,4);
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 67460:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140048447;
 67461:     pcVar1 = "VidState change to STATEV_Unplug state\n";
 67462:   }
```

### W0716 - `FUN_1400480e0` line 67458
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xac`
```c
 67453:     FUN_140044528(param_1,0xd,4);
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 67460:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140048447;
 67461:     pcVar1 = "VidState change to STATEV_Unplug state\n";
 67462:   }
 67463:   else {
```

### W0717 - `FUN_1400480e0` line 67459
- Register: `0x2B `
- Kind: `rmw_write`
- Mask: `0xfd`
- Value: `0xa0`
```c
 67454:     FUN_140044528(param_1,0x22,0x12);
 67455:     FUN_140044528(param_1,0x22,0x10);
 67456:     FUN_1400444d0(param_1,0x23,0xfd,0xac);
 67457:     FUN_1400444d0(param_1,0x23,0xfd,0xa0);
 67458:     FUN_1400444d0(param_1,0x2b,0xfd,0xac);
 67459:     FUN_1400444d0(param_1,0x2b,0xfd,0xa0);
 67460:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140048447;
 67461:     pcVar1 = "VidState change to STATEV_Unplug state\n";
 67462:   }
 67463:   else {
 67464:     if (param_2 != 2) {
```

### W0718 - `FUN_1400480e0` line 67483
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67478:       }
 67479:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67480:         DbgPrint("VidState change to STATEV_VidStable state\n");
 67481:       }
 67482:       uVar2 = 0;
 67483:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67484:       uVar4 = CONCAT71((int7)((ulonglong)param_3 >> 8),0x8f);
 67485:       uVar2 = CONCAT71((int7)((ulonglong)uVar2 >> 8),0x90);
 67486:       FUN_140044528(param_1,0x90,0x8f);
 67487:       FUN_14003d0a0(param_1);
 67488:       FUN_14003d388(param_1);
```

### W0719 - `FUN_1400480e0` line 67486
- Register: `0x90 `
- Kind: `direct_write`
- Value: `0x8f`
```c
 67481:       }
 67482:       uVar2 = 0;
 67483:       thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 67484:       uVar4 = CONCAT71((int7)((ulonglong)param_3 >> 8),0x8f);
 67485:       uVar2 = CONCAT71((int7)((ulonglong)uVar2 >> 8),0x90);
 67486:       FUN_140044528(param_1,0x90,0x8f);
 67487:       FUN_14003d0a0(param_1);
 67488:       FUN_14003d388(param_1);
 67489:       FUN_1400449a0(param_1);
 67490:       FUN_1400445c0(param_1,uVar2,(byte)uVar4,(byte)param_4);
 67491:       FUN_14003eb0c(param_1);
```

### W0720 - `FUN_1400480e0` line 67507
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67502:           DbgPrint(
 67503:                   "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
 67504:                   );
 67505:         }
 67506:         *(undefined1 *)(param_1 + 0x194) = 0;
 67507:         thunk_FUN_1400444d0(param_1,0,(byte)uVar4,(byte)param_4);
 67508:         param_4 = 0;
 67509:         uVar4 = CONCAT71((int7)(uVar4 >> 8),0x40);
 67510:         FUN_1400444d0(param_1,0x81,0x40,0);
 67511:         FUN_14003ee30(param_1,'\x01');
 67512:       }
```

### W0721 - `FUN_1400480e0` line 67510
- Register: `0x81 `
- Kind: `rmw_write`
- Mask: `0x40`
- Value: `0`
```c
 67505:         }
 67506:         *(undefined1 *)(param_1 + 0x194) = 0;
 67507:         thunk_FUN_1400444d0(param_1,0,(byte)uVar4,(byte)param_4);
 67508:         param_4 = 0;
 67509:         uVar4 = CONCAT71((int7)(uVar4 >> 8),0x40);
 67510:         FUN_1400444d0(param_1,0x81,0x40,0);
 67511:         FUN_14003ee30(param_1,'\x01');
 67512:       }
 67513:       FUN_14003eaa4(param_1);
 67514:       FUN_14003f774(param_1,'\0');
 67515:       *(undefined1 *)(param_1 + 0x222) = 1;
```

### W0722 - `FUN_140048478` line 67613
- Register: `0x0F `
- Kind: `bank_select`
- Mask: `0x07`
- Value: `param_1 & 7`
```c
 67608:           FUN_14003eaa4(param_1);
 67609:           *(undefined1 *)(param_1 + 0x19d) = 0;
 67610:           *(undefined1 *)(param_1 + 0x1a0) = 1;
 67611:           FUN_14003f774(param_1,'\0');
 67612:         }
 67613:         thunk_FUN_1400444d0(param_1,0,bVar1,bVar5);
 67614:         bVar1 = FUN_140044484(param_1,0x4f);
 67615:         if ((bVar1 & 0x20) != 0) {
 67616:           bVar1 = FUN_14003ccf4(param_1);
 67617:           if (bVar1 == 0) {
 67618:             FUN_14003ecf4(param_1,'\0');
```

## Appendix B - All Direct Register Read Hits (constant register arguments)
Each entry contains the requested metadata plus 5 lines of context before and after.

### R0001 - `FUN_140039640` line 57926
- Register: `0x6E `
- Kind: `direct_read`
```c
 57921:     if (((int)param_1[0x87] == 1) || ((uint)uVar3 < uVar2)) {
 57922:       cVar4 = '\x01';
 57923:     }
 57924:   }
 57925:   if (*(char *)((longlong)param_1 + 0x22b) != cVar4) {
 57926:     bVar1 = FUN_140044484((longlong)param_1,0x6e);
 57927:     bVar5 = bVar1 | 2;
 57928:     if (cVar4 == '\0') {
 57929:       bVar5 = bVar1 & 0xfd;
 57930:     }
 57931:     FUN_140044528((longlong)param_1,0x6e,bVar5);
```

### R0002 - `FUN_14003b090` line 59668
- Register: `0x9E `
- Kind: `direct_read`
```c
 59663:     bVar14 = (byte)iVar16;
 59664:     bVar15 = 0;
 59665:     DbgPrint("(_d:%d)  xxDevice:%d, repetition: 0x%02x [68051 vfmt]");
 59666:   }
 59667:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59668:   bVar12 = FUN_140044484(param_1,0x9e);
 59669:   bVar13 = FUN_140044484(param_1,0x9d);
 59670:   local_88 = (uint)bVar13 + (bVar12 & 0x3f) * 0x100;
 59671:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59672:     bVar14 = (byte)local_88;
 59673:     bVar15 = 0;
```

### R0003 - `FUN_14003b090` line 59669
- Register: `0x9D `
- Kind: `direct_read`
```c
 59664:     bVar15 = 0;
 59665:     DbgPrint("(_d:%d)  xxDevice:%d, repetition: 0x%02x [68051 vfmt]");
 59666:   }
 59667:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59668:   bVar12 = FUN_140044484(param_1,0x9e);
 59669:   bVar13 = FUN_140044484(param_1,0x9d);
 59670:   local_88 = (uint)bVar13 + (bVar12 & 0x3f) * 0x100;
 59671:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59672:     bVar14 = (byte)local_88;
 59673:     bVar15 = 0;
 59674:     DbgPrint("(_d:%d)  xxDevice:%d, packet_width: 0x%04x(%lu) [68051 vfmt]");
```

### R0004 - `FUN_14003b090` line 59677
- Register: `0xA5 `
- Kind: `direct_read`
```c
 59672:     bVar14 = (byte)local_88;
 59673:     bVar15 = 0;
 59674:     DbgPrint("(_d:%d)  xxDevice:%d, packet_width: 0x%04x(%lu) [68051 vfmt]");
 59675:   }
 59676:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59677:   bVar14 = FUN_140044484(param_1,0xa5);
 59678:   bVar15 = FUN_140044484(param_1,0xa4);
 59679:   uVar24 = (uint)bVar15 + (bVar14 & 0x3f) * 0x100;
 59680:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59681:     DbgPrint("(_d:%d)  xxDevice:%d, packet_height: 0x%04x(%lu) [68051 vfmt]",
 59682:              *(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8),0,uVar24);
```

### R0005 - `FUN_14003b090` line 59678
- Register: `0xA4 `
- Kind: `direct_read`
```c
 59673:     bVar15 = 0;
 59674:     DbgPrint("(_d:%d)  xxDevice:%d, packet_width: 0x%04x(%lu) [68051 vfmt]");
 59675:   }
 59676:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar14);
 59677:   bVar14 = FUN_140044484(param_1,0xa5);
 59678:   bVar15 = FUN_140044484(param_1,0xa4);
 59679:   uVar24 = (uint)bVar15 + (bVar14 & 0x3f) * 0x100;
 59680:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59681:     DbgPrint("(_d:%d)  xxDevice:%d, packet_height: 0x%04x(%lu) [68051 vfmt]",
 59682:              *(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8),0,uVar24);
 59683:   }
```

### R0006 - `FUN_14003b090` line 59761
- Register: `0x98 `
- Kind: `direct_read`
```c
 59756:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59757:     bVar15 = 0;
 59758:     DbgPrint("(_d:%d)  xxDevice:%d, iTE6805_DATA.AVIInfoFrame_Colorimetry,  0x%02x %s  [68051 vfmt]"
 59759:              ,*(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8));
 59760:   }
 59761:   FUN_140044484(param_1,0x98);
 59762:   bVar12 = FUN_140044484(param_1,0x6b);
 59763:   uVar17 = (uint)bVar12;
 59764:   local_68 = uVar17 & 3;
 59765:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59766:     uVar25 = 1;
```

### R0007 - `FUN_14003b090` line 59762
- Register: `0x6B `
- Kind: `direct_read`
```c
 59757:     bVar15 = 0;
 59758:     DbgPrint("(_d:%d)  xxDevice:%d, iTE6805_DATA.AVIInfoFrame_Colorimetry,  0x%02x %s  [68051 vfmt]"
 59759:              ,*(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8));
 59760:   }
 59761:   FUN_140044484(param_1,0x98);
 59762:   bVar12 = FUN_140044484(param_1,0x6b);
 59763:   uVar17 = (uint)bVar12;
 59764:   local_68 = uVar17 & 3;
 59765:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59766:     uVar25 = 1;
 59767:     bVar15 = 0xf0;
```

### R0008 - `FUN_14003b090` line 59787
- Register: `0xC0 `
- Kind: `direct_read`
```c
 59782:     bVar15 = 0;
 59783:     DbgPrint("(_d:%d)  xxDevice:%d, 68051(0x6b,98) In CS: %s, In ColorDepth: %s, Out CS: %s, Out ColorDepth: %s,  PP: %s, InForce: %s[68051 vfmt]"
 59784:              ,*(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8));
 59785:   }
 59786:   thunk_FUN_1400444d0(param_1,1,bVar15,bVar12);
 59787:   bVar13 = FUN_140044484(param_1,0xc0);
 59788:   FUN_140044484(param_1,0xc1);
 59789:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59790:     bVar12 = 0x10;
 59791:     uVar25 = 1;
 59792:     if ((bVar13 & 1) != 0) {
```

### R0009 - `FUN_14003b090` line 59788
- Register: `0xC1 `
- Kind: `direct_read`
```c
 59783:     DbgPrint("(_d:%d)  xxDevice:%d, 68051(0x6b,98) In CS: %s, In ColorDepth: %s, Out CS: %s, Out ColorDepth: %s,  PP: %s, InForce: %s[68051 vfmt]"
 59784:              ,*(undefined1 *)(*(longlong *)(param_1 + 0x2f0) + 8));
 59785:   }
 59786:   thunk_FUN_1400444d0(param_1,1,bVar15,bVar12);
 59787:   bVar13 = FUN_140044484(param_1,0xc0);
 59788:   FUN_140044484(param_1,0xc1);
 59789:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59790:     bVar12 = 0x10;
 59791:     uVar25 = 1;
 59792:     if ((bVar13 & 1) != 0) {
 59793:       bVar12 = 0;
```

### R0010 - `FUN_14003b090` line 59803
- Register: `0xB0 `
- Kind: `direct_read`
```c
 59798:             );
 59799:   }
 59800:   uVar20 = 0;
 59801:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar12);
 59802:   uVar22 = (undefined7)((ulonglong)uVar20 >> 8);
 59803:   bVar12 = FUN_140044484(param_1,0xb0);
 59804:   uVar21 = CONCAT71(uVar22,0xb1);
 59805:   bVar15 = FUN_140044484(param_1,0xb1);
 59806:   for (bVar15 = bVar15 & 0x3f; bVar15 != 0; bVar15 = bVar15 >> 1) {
 59807:     local_7c = local_7c + 2;
 59808:   }
```

### R0011 - `FUN_14003b090` line 59805
- Register: `0xB1 `
- Kind: `direct_read`
```c
 59800:   uVar20 = 0;
 59801:   thunk_FUN_1400444d0(param_1,0,bVar15,bVar12);
 59802:   uVar22 = (undefined7)((ulonglong)uVar20 >> 8);
 59803:   bVar12 = FUN_140044484(param_1,0xb0);
 59804:   uVar21 = CONCAT71(uVar22,0xb1);
 59805:   bVar15 = FUN_140044484(param_1,0xb1);
 59806:   for (bVar15 = bVar15 & 0x3f; bVar15 != 0; bVar15 = bVar15 >> 1) {
 59807:     local_7c = local_7c + 2;
 59808:   }
 59809:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59810:     uVar21 = (ulonglong)*(byte *)(*(longlong *)(param_1 + 0x2f0) + 8);
```

### R0012 - `FUN_14003b090` line 59820
- Register: `0x6C `
- Kind: `direct_read`
```c
 59815:     uVar21 = (ulonglong)*(byte *)(*(longlong *)(param_1 + 0x2f0) + 8);
 59816:     DbgPrint("(_d:%d)  xxDevice:%d, vic:%lu, sampleing_mode:%s, audio_sampling_Freq:%s, audio_sampling_bits:%lu, packet_hdcp:%s [68051 vfmt-m]"
 59817:              ,uVar21,0,uVar19);
 59818:   }
 59819:   uVar22 = (undefined7)(uVar21 >> 8);
 59820:   FUN_140044484(param_1,0x6c);
 59821:   uVar21 = CONCAT71(uVar22,0x6e);
 59822:   FUN_140044484(param_1,0x6e);
 59823:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59824:     uVar25 = 1;
 59825:     uVar21 = (ulonglong)*(byte *)(*(longlong *)(param_1 + 0x2f0) + 8);
```

### R0013 - `FUN_14003b090` line 59822
- Register: `0x6E `
- Kind: `direct_read`
```c
 59817:              ,uVar21,0,uVar19);
 59818:   }
 59819:   uVar22 = (undefined7)(uVar21 >> 8);
 59820:   FUN_140044484(param_1,0x6c);
 59821:   uVar21 = CONCAT71(uVar22,0x6e);
 59822:   FUN_140044484(param_1,0x6e);
 59823:   if ((DAT_1400a04f8 & 0x10) != 0) {
 59824:     uVar25 = 1;
 59825:     uVar21 = (ulonglong)*(byte *)(*(longlong *)(param_1 + 0x2f0) + 8);
 59826:     DbgPrint("(_d:%d)  xxDevice:%d, 68051(0x6c,6e) 0x%02x-CSC Mode: %s, 0x%02x-CSC Sel: %s, CbCr: %s, ColorClip: %s [68051 vfmt]"
 59827:              ,uVar21,0);
```

### R0014 - `FUN_14003ccac` line 60272
- Register: `0x10 `
- Kind: `direct_read`
```c
 60267: {
 60268:   byte bVar1;
 60269:   byte bVar2;
 60270: 
 60271:   FUN_1400444d0(param_1,0xf,7,0);
 60272:   bVar1 = FUN_140044484(param_1,0x10);
 60273:   bVar2 = FUN_140044484(param_1,0xb1);
 60274:   return (bVar2 & ~bVar1) >> 7;
 60275: }
 60276: 
 60277: 
```

### R0015 - `FUN_14003ccac` line 60273
- Register: `0xB1 `
- Kind: `direct_read`
```c
 60268:   byte bVar1;
 60269:   byte bVar2;
 60270: 
 60271:   FUN_1400444d0(param_1,0xf,7,0);
 60272:   bVar1 = FUN_140044484(param_1,0x10);
 60273:   bVar2 = FUN_140044484(param_1,0xb1);
 60274:   return (bVar2 & ~bVar1) >> 7;
 60275: }
 60276: 
 60277: 
 60278: 
```

### R0016 - `FUN_14003ccf4` line 60286
- Register: `0xAA `
- Kind: `direct_read`
```c
 60281: {
 60282:   byte bVar1;
 60283: 
 60284:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 60285:     FUN_1400444d0(param_1,0xf,7,0);
 60286:     bVar1 = FUN_140044484(param_1,0xaa);
 60287:   }
 60288:   else {
 60289:     FUN_1400444d0(param_1,0xf,7,4);
 60290:     bVar1 = FUN_140044484(param_1,0xaa);
 60291:     FUN_1400444d0(param_1,0xf,7,0);
```

### R0017 - `FUN_14003ccf4` line 60290
- Register: `0xAA `
- Kind: `direct_read`
```c
 60285:     FUN_1400444d0(param_1,0xf,7,0);
 60286:     bVar1 = FUN_140044484(param_1,0xaa);
 60287:   }
 60288:   else {
 60289:     FUN_1400444d0(param_1,0xf,7,4);
 60290:     bVar1 = FUN_140044484(param_1,0xaa);
 60291:     FUN_1400444d0(param_1,0xf,7,0);
 60292:   }
 60293:   return bVar1 & 8;
 60294: }
 60295: 
```

### R0018 - `FUN_14003ce08` line 60360
- Register: `0x13 `
- Kind: `direct_read`
```c
 60355: 
 60356: {
 60357:   byte bVar1;
 60358: 
 60359:   FUN_1400444d0(param_1,0xf,7,0);
 60360:   if ((param_2 == '\0') && (bVar1 = FUN_140044484(param_1,0x13), (bVar1 & 2) != 0)) {
 60361:     return 0;
 60362:   }
 60363:   if (param_2 == '\x01') {
 60364:     bVar1 = FUN_140044484(param_1,0x16);
 60365:     bVar1 = (bVar1 ^ 0xff) & 2;
```

### R0019 - `FUN_14003ce08` line 60364
- Register: `0x16 `
- Kind: `direct_read`
```c
 60359:   FUN_1400444d0(param_1,0xf,7,0);
 60360:   if ((param_2 == '\0') && (bVar1 = FUN_140044484(param_1,0x13), (bVar1 & 2) != 0)) {
 60361:     return 0;
 60362:   }
 60363:   if (param_2 == '\x01') {
 60364:     bVar1 = FUN_140044484(param_1,0x16);
 60365:     bVar1 = (bVar1 ^ 0xff) & 2;
 60366:   }
 60367:   else {
 60368:     bVar1 = 2;
 60369:   }
```

### R0020 - `FUN_14003ce60` line 60382
- Register: `0x13 `
- Kind: `direct_read`
```c
 60377: {
 60378:   byte bVar1;
 60379: 
 60380:   FUN_1400444d0(param_1,0xf,7,0);
 60381:   if (param_2 == '\0') {
 60382:     bVar1 = FUN_140044484(param_1,0x13);
 60383:     bVar1 = bVar1 & 0x40;
 60384:   }
 60385:   else {
 60386:     bVar1 = 0;
 60387:   }
```

### R0021 - `FUN_14003ce9c` line 60399
- Register: `0x19 `
- Kind: `direct_read`
```c
 60394: 
 60395: {
 60396:   byte bVar1;
 60397: 
 60398:   FUN_1400444d0(param_1,0xf,7,0);
 60399:   bVar1 = FUN_140044484(param_1,0x19);
 60400:   return bVar1 & 0x80;
 60401: }
 60402: 
 60403: 
 60404: 
```

### R0022 - `FUN_14003cec4` line 60411
- Register: `0xC0 `
- Kind: `direct_read`
```c
 60406: 
 60407: {
 60408:   byte bVar1;
 60409: 
 60410:   FUN_1400444d0(param_1,0xf,7,1);
 60411:   bVar1 = FUN_140044484(param_1,0xc0);
 60412:   FUN_1400444d0(param_1,0xf,7,0);
 60413:   return (bVar1 & 1) != 0;
 60414: }
 60415: 
 60416: 
```

### R0023 - `FUN_14003d0a0` line 60521
- Register: `0x15 `
- Kind: `direct_read`
```c
 60516: 
 60517: {
 60518:   byte bVar1;
 60519: 
 60520:   FUN_1400444d0(param_1,0xf,7,2);
 60521:   bVar1 = FUN_140044484(param_1,0x15);
 60522:   *(uint *)(param_1 + 0x20c) = bVar1 >> 5 & 3;
 60523:   bVar1 = FUN_140044484(param_1,0x16);
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
```

### R0024 - `FUN_14003d0a0` line 60523
- Register: `0x16 `
- Kind: `direct_read`
```c
 60518:   byte bVar1;
 60519: 
 60520:   FUN_1400444d0(param_1,0xf,7,2);
 60521:   bVar1 = FUN_140044484(param_1,0x15);
 60522:   *(uint *)(param_1 + 0x20c) = bVar1 >> 5 & 3;
 60523:   bVar1 = FUN_140044484(param_1,0x16);
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
 60527:   bVar1 = FUN_140044484(param_1,0x17);
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
```

### R0025 - `FUN_14003d0a0` line 60525
- Register: `0x17 `
- Kind: `direct_read`
```c
 60520:   FUN_1400444d0(param_1,0xf,7,2);
 60521:   bVar1 = FUN_140044484(param_1,0x15);
 60522:   *(uint *)(param_1 + 0x20c) = bVar1 >> 5 & 3;
 60523:   bVar1 = FUN_140044484(param_1,0x16);
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
 60527:   bVar1 = FUN_140044484(param_1,0x17);
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
 60529:   bVar1 = FUN_140044484(param_1,0x18);
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
```

### R0026 - `FUN_14003d0a0` line 60527
- Register: `0x17 `
- Kind: `direct_read`
```c
 60522:   *(uint *)(param_1 + 0x20c) = bVar1 >> 5 & 3;
 60523:   bVar1 = FUN_140044484(param_1,0x16);
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
 60527:   bVar1 = FUN_140044484(param_1,0x17);
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
 60529:   bVar1 = FUN_140044484(param_1,0x18);
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
 60531:   bVar1 = FUN_140044484(param_1,0x19);
 60532:   *(uint *)(param_1 + 0x218) = (uint)(bVar1 >> 6);
```

### R0027 - `FUN_14003d0a0` line 60529
- Register: `0x18 `
- Kind: `direct_read`
```c
 60524:   *(uint *)(param_1 + 0x210) = (uint)(bVar1 >> 6);
 60525:   bVar1 = FUN_140044484(param_1,0x17);
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
 60527:   bVar1 = FUN_140044484(param_1,0x17);
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
 60529:   bVar1 = FUN_140044484(param_1,0x18);
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
 60531:   bVar1 = FUN_140044484(param_1,0x19);
 60532:   *(uint *)(param_1 + 0x218) = (uint)(bVar1 >> 6);
 60533:   bVar1 = FUN_140044484(param_1,0x15);
 60534:   *(byte *)(param_1 + 0x21f) = bVar1 & 3;
```

### R0028 - `FUN_14003d0a0` line 60531
- Register: `0x19 `
- Kind: `direct_read`
```c
 60526:   *(byte *)(param_1 + 0x21e) = bVar1 >> 4 & 7;
 60527:   bVar1 = FUN_140044484(param_1,0x17);
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
 60529:   bVar1 = FUN_140044484(param_1,0x18);
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
 60531:   bVar1 = FUN_140044484(param_1,0x19);
 60532:   *(uint *)(param_1 + 0x218) = (uint)(bVar1 >> 6);
 60533:   bVar1 = FUN_140044484(param_1,0x15);
 60534:   *(byte *)(param_1 + 0x21f) = bVar1 & 3;
 60535:   FUN_1400444d0(param_1,0xf,7,0);
 60536:   if (*(int *)(param_1 + 0x214) == 0) {
```

### R0029 - `FUN_14003d0a0` line 60533
- Register: `0x15 `
- Kind: `direct_read`
```c
 60528:   *(uint *)(param_1 + 0x214) = bVar1 >> 2 & 3;
 60529:   bVar1 = FUN_140044484(param_1,0x18);
 60530:   *(byte *)(param_1 + 0x21c) = bVar1 & 0x7f;
 60531:   bVar1 = FUN_140044484(param_1,0x19);
 60532:   *(uint *)(param_1 + 0x218) = (uint)(bVar1 >> 6);
 60533:   bVar1 = FUN_140044484(param_1,0x15);
 60534:   *(byte *)(param_1 + 0x21f) = bVar1 & 3;
 60535:   FUN_1400444d0(param_1,0xf,7,0);
 60536:   if (*(int *)(param_1 + 0x214) == 0) {
 60537:     *(uint *)(param_1 + 0x214) = (*(byte *)(param_1 + 0x21c) < 2) + 1;
 60538:   }
```

### R0030 - `FUN_14003d18c` line 60561
- Register: `0x9A `
- Kind: `direct_read`
```c
 60556:   uVar4 = 1;
 60557:   lVar5 = 5;
 60558:   do {
 60559:     FUN_140048644(param_1,3);
 60560:     FUN_1400444d0(param_1,0x9a,0x80,0);
 60561:     bVar1 = FUN_140044484(param_1,0x9a);
 60562:     bVar2 = FUN_140044484(param_1,0x99);
 60563:     FUN_1400444d0(param_1,0x9a,0x80,0x80);
 60564:     uVar3 = uVar3 + (bVar1 & 7) * 0x100 + (uint)bVar2;
 60565:     lVar5 = lVar5 + -1;
 60566:   } while (lVar5 != 0);
```

### R0031 - `FUN_14003d18c` line 60562
- Register: `0x99 `
- Kind: `direct_read`
```c
 60557:   lVar5 = 5;
 60558:   do {
 60559:     FUN_140048644(param_1,3);
 60560:     FUN_1400444d0(param_1,0x9a,0x80,0);
 60561:     bVar1 = FUN_140044484(param_1,0x9a);
 60562:     bVar2 = FUN_140044484(param_1,0x99);
 60563:     FUN_1400444d0(param_1,0x9a,0x80,0x80);
 60564:     uVar3 = uVar3 + (bVar1 & 7) * 0x100 + (uint)bVar2;
 60565:     lVar5 = lVar5 + -1;
 60566:   } while (lVar5 != 0);
 60567:   if (uVar3 != 0) {
```

### R0032 - `FUN_14003d24c` line 60593
- Register: `0x48 `
- Kind: `direct_read`
```c
 60588:   uVar4 = 0;
 60589:   if (param_2 != 0) {
 60590:     uVar6 = (ulonglong)param_2;
 60591:     do {
 60592:       FUN_140048644(param_1,3);
 60593:       cVar1 = FUN_140044484(param_1,0x48);
 60594:       uVar5 = uVar5 + (byte)(cVar1 + 1);
 60595:       uVar6 = uVar6 - 1;
 60596:       uVar4 = (uint)param_2;
 60597:     } while (uVar6 != 0);
 60598:   }
```

### R0033 - `FUN_14003d24c` line 60599
- Register: `0x43 `
- Kind: `direct_read`
```c
 60594:       uVar5 = uVar5 + (byte)(cVar1 + 1);
 60595:       uVar6 = uVar6 - 1;
 60596:       uVar4 = (uint)param_2;
 60597:     } while (uVar6 != 0);
 60598:   }
 60599:   bVar2 = FUN_140044484(param_1,0x43);
 60600:   FUN_1400444d0(param_1,0xf,7,0);
 60601:   if ((char)(bVar2 & 0xe0) < '\0') {
 60602:     uVar3 = uVar4 * *(int *)(param_1 + 0x98) * 0x400;
 60603:   }
 60604:   else if ((bVar2 & 0x40) == 0) {
```

### R0034 - `FUN_14003d388` line 60657
- Register: `0x48 `
- Kind: `direct_read`
```c
 60652:   uVar2 = 1;
 60653:   uVar15 = 1;
 60654:   lVar19 = 10;
 60655:   do {
 60656:     FUN_140048644(param_1,10);
 60657:     bVar3 = FUN_140044484(param_1,0x48);
 60658:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60659:       DbgPrint("0x48 TMDSCLKSpeed = 0x%02x \n",bVar3 + 1);
 60660:     }
 60661:     uVar14 = uVar14 + bVar3 + 1;
 60662:     lVar19 = lVar19 + -1;
```

### R0035 - `FUN_14003d388` line 60667
- Register: `0x43 `
- Kind: `direct_read`
```c
 60662:     lVar19 = lVar19 + -1;
 60663:   } while (lVar19 != 0);
 60664:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60665:     DbgPrint("sumt = %lu \n",uVar14);
 60666:   }
 60667:   bVar3 = FUN_140044484(param_1,0x43);
 60668:   FUN_1400444d0(param_1,0xf,7,0);
 60669:   puVar1 = (uint *)(param_1 + 0x98);
 60670:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60671:     DbgPrint("RCLKVALUE=%lu.%02luMHz\n",*puVar1 / 1000,(ulonglong)(*puVar1 % 1000) / 10);
 60672:   }
```

### R0036 - `FUN_14003d388` line 60710
- Register: `0x98 `
- Kind: `direct_read`
```c
 60705:       uVar2 = uVar14;
 60706:     }
 60707:     uVar12 = (uint)(*(int *)(param_1 + 0x98) * 0x500) / uVar2;
 60708:   }
 60709:   FUN_14003d18c(param_1);
 60710:   bVar3 = FUN_140044484(param_1,0x98);
 60711:   bVar4 = FUN_140044484(param_1,0x9c);
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
```

### R0037 - `FUN_14003d388` line 60711
- Register: `0x9C `
- Kind: `direct_read`
```c
 60706:     }
 60707:     uVar12 = (uint)(*(int *)(param_1 + 0x98) * 0x500) / uVar2;
 60708:   }
 60709:   FUN_14003d18c(param_1);
 60710:   bVar3 = FUN_140044484(param_1,0x98);
 60711:   bVar4 = FUN_140044484(param_1,0x9c);
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
```

### R0038 - `FUN_14003d388` line 60712
- Register: `0x9B `
- Kind: `direct_read`
```c
 60707:     uVar12 = (uint)(*(int *)(param_1 + 0x98) * 0x500) / uVar2;
 60708:   }
 60709:   FUN_14003d18c(param_1);
 60710:   bVar3 = FUN_140044484(param_1,0x98);
 60711:   bVar4 = FUN_140044484(param_1,0x9c);
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
```

### R0039 - `FUN_14003d388` line 60714
- Register: `0x9E `
- Kind: `direct_read`
```c
 60709:   FUN_14003d18c(param_1);
 60710:   bVar3 = FUN_140044484(param_1,0x98);
 60711:   bVar4 = FUN_140044484(param_1,0x9c);
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
```

### R0040 - `FUN_14003d388` line 60715
- Register: `0x9D `
- Kind: `direct_read`
```c
 60710:   bVar3 = FUN_140044484(param_1,0x98);
 60711:   bVar4 = FUN_140044484(param_1,0x9c);
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
```

### R0041 - `FUN_14003d388` line 60717
- Register: `0xA1 `
- Kind: `direct_read`
```c
 60712:   bVar5 = FUN_140044484(param_1,0x9b);
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
```

### R0042 - `FUN_14003d388` line 60718
- Register: `0xA0 `
- Kind: `direct_read`
```c
 60713:   uVar16 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60714:   bVar4 = FUN_140044484(param_1,0x9e);
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
```

### R0043 - `FUN_14003d388` line 60720
- Register: `0xA1 `
- Kind: `direct_read`
```c
 60715:   bVar5 = FUN_140044484(param_1,0x9d);
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
```

### R0044 - `FUN_14003d388` line 60721
- Register: `0x9F `
- Kind: `direct_read`
```c
 60716:   sVar17 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60717:   bVar4 = FUN_140044484(param_1,0xa1);
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
```

### R0045 - `FUN_14003d388` line 60723
- Register: `0xA3 `
- Kind: `direct_read`
```c
 60718:   bVar5 = FUN_140044484(param_1,0xa0);
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
```

### R0046 - `FUN_14003d388` line 60724
- Register: `0xA2 `
- Kind: `direct_read`
```c
 60719:   sVar7 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60720:   bVar4 = FUN_140044484(param_1,0xa1);
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
```

### R0047 - `FUN_14003d388` line 60726
- Register: `0xA5 `
- Kind: `direct_read`
```c
 60721:   bVar5 = FUN_140044484(param_1,0x9f);
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
```

### R0048 - `FUN_14003d388` line 60727
- Register: `0xA4 `
- Kind: `direct_read`
```c
 60722:   sVar8 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60723:   bVar4 = FUN_140044484(param_1,0xa3);
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
```

### R0049 - `FUN_14003d388` line 60729
- Register: `0xA8 `
- Kind: `direct_read`
```c
 60724:   bVar5 = FUN_140044484(param_1,0xa2);
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
```

### R0050 - `FUN_14003d388` line 60730
- Register: `0xA7 `
- Kind: `direct_read`
```c
 60725:   uVar18 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60726:   bVar4 = FUN_140044484(param_1,0xa5);
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60735:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
```

### R0051 - `FUN_14003d388` line 60732
- Register: `0xA8 `
- Kind: `direct_read`
```c
 60727:   bVar5 = FUN_140044484(param_1,0xa4);
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60735:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 60736:     FUN_1400444d0(param_1,0xf,7,4);
 60737:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0052 - `FUN_14003d388` line 60733
- Register: `0xA6 `
- Kind: `direct_read`
```c
 60728:   sVar9 = (ushort)bVar5 + (ushort)(bVar4 & 0x3f) * 0x100;
 60729:   bVar4 = FUN_140044484(param_1,0xa8);
 60730:   bVar5 = FUN_140044484(param_1,0xa7);
 60731:   sVar10 = (ushort)bVar5 + (ushort)(bVar4 & 0xf0) * 0x10;
 60732:   bVar4 = FUN_140044484(param_1,0xa8);
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60735:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 60736:     FUN_1400444d0(param_1,0xf,7,4);
 60737:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60738:       uVar6 = FUN_140044484(param_1,0xaa);
```

### R0053 - `FUN_14003d388` line 60738
- Register: `0xAA `
- Kind: `direct_read`
```c
 60733:   bVar5 = FUN_140044484(param_1,0xa6);
 60734:   sVar11 = (ushort)bVar5 + (ushort)(bVar4 & 1) * 0x100;
 60735:   if (*(char *)(param_1 + 0x1ec) == '\x01') {
 60736:     FUN_1400444d0(param_1,0xf,7,4);
 60737:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60738:       uVar6 = FUN_140044484(param_1,0xaa);
 60739:       DbgPrint("hdmirxrd(0xAA) = 0x%02X \n",uVar6);
 60740:     }
 60741:   }
 60742:   bVar4 = FUN_140044484(param_1,0xaa);
 60743:   bVar5 = FUN_140044484(param_1,0xaa);
```

### R0054 - `FUN_14003d388` line 60742
- Register: `0xAA `
- Kind: `direct_read`
```c
 60737:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60738:       uVar6 = FUN_140044484(param_1,0xaa);
 60739:       DbgPrint("hdmirxrd(0xAA) = 0x%02X \n",uVar6);
 60740:     }
 60741:   }
 60742:   bVar4 = FUN_140044484(param_1,0xaa);
 60743:   bVar5 = FUN_140044484(param_1,0xaa);
 60744:   FUN_1400444d0(param_1,0xf,7,0);
 60745:   *(short *)(param_1 + 0x240) = sVar7;
 60746:   *(ushort *)(param_1 + 0x244) = ((uVar16 - sVar8) - sVar7) - sVar17;
 60747:   *(short *)(param_1 + 0x242) = sVar8;
```

### R0055 - `FUN_14003d388` line 60743
- Register: `0xAA `
- Kind: `direct_read`
```c
 60738:       uVar6 = FUN_140044484(param_1,0xaa);
 60739:       DbgPrint("hdmirxrd(0xAA) = 0x%02X \n",uVar6);
 60740:     }
 60741:   }
 60742:   bVar4 = FUN_140044484(param_1,0xaa);
 60743:   bVar5 = FUN_140044484(param_1,0xaa);
 60744:   FUN_1400444d0(param_1,0xf,7,0);
 60745:   *(short *)(param_1 + 0x240) = sVar7;
 60746:   *(ushort *)(param_1 + 0x244) = ((uVar16 - sVar8) - sVar7) - sVar17;
 60747:   *(short *)(param_1 + 0x242) = sVar8;
 60748:   *(short *)(param_1 + 0x22e) = sVar9;
```

### R0056 - `FUN_14003d7fc` line 60833
- Register: `0x08 `
- Kind: `direct_read`
```c
 60828:   FUN_1400444d0(param_1,0xf,7,3);
 60829:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
 60835:   bVar2 = FUN_140044484(param_1,0xd);
 60836:   bVar7 = 0;
 60837:   bVar2 = bVar2 & 0x30;
 60838:   bVar8 = 0;
```

### R0057 - `FUN_14003d7fc` line 60835
- Register: `0x0D `
- Kind: `direct_read`
```c
 60830:   FUN_1400444d0(param_1,0xf,7,7);
 60831:   FUN_1400444d0(param_1,0x3a,0x80,0x80);
 60832:   FUN_1400444d0(param_1,0xf,7,0);
 60833:   bVar1 = FUN_140044484(param_1,8);
 60834:   bVar1 = bVar1 & 0x30;
 60835:   bVar2 = FUN_140044484(param_1,0xd);
 60836:   bVar7 = 0;
 60837:   bVar2 = bVar2 & 0x30;
 60838:   bVar8 = 0;
 60839:   while( true ) {
 60840:     if ((bVar1 != 0) && (bVar2 != 0)) goto LAB_14003dcf9;
```

### R0058 - `FUN_14003d7fc` line 60841
- Register: `0x08 `
- Kind: `direct_read`
```c
 60836:   bVar7 = 0;
 60837:   bVar2 = bVar2 & 0x30;
 60838:   bVar8 = 0;
 60839:   while( true ) {
 60840:     if ((bVar1 != 0) && (bVar2 != 0)) goto LAB_14003dcf9;
 60841:     bVar1 = FUN_140044484(param_1,8);
 60842:     bVar1 = bVar1 & 0x30;
 60843:     bVar2 = FUN_140044484(param_1,0xd);
 60844:     bVar2 = bVar2 & 0x30;
 60845:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60846:       DbgPrint("Wait for CAOF Done!!!!!!\n");
```

### R0059 - `FUN_14003d7fc` line 60843
- Register: `0x0D `
- Kind: `direct_read`
```c
 60838:   bVar8 = 0;
 60839:   while( true ) {
 60840:     if ((bVar1 != 0) && (bVar2 != 0)) goto LAB_14003dcf9;
 60841:     bVar1 = FUN_140044484(param_1,8);
 60842:     bVar1 = bVar1 & 0x30;
 60843:     bVar2 = FUN_140044484(param_1,0xd);
 60844:     bVar2 = bVar2 & 0x30;
 60845:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60846:       DbgPrint("Wait for CAOF Done!!!!!!\n");
 60847:     }
 60848:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0060 - `FUN_14003d7fc` line 60849
- Register: `0x0D `
- Kind: `direct_read`
```c
 60844:     bVar2 = bVar2 & 0x30;
 60845:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60846:       DbgPrint("Wait for CAOF Done!!!!!!\n");
 60847:     }
 60848:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60849:       uVar3 = FUN_140044484(param_1,0xd);
 60850:       uVar4 = FUN_140044484(param_1,8);
 60851:       DbgPrint(" Reg08h= %x,  Reg0Dh=%x .......................\n",uVar4,uVar3);
 60852:     }
 60853:     if (4 < bVar7) {
 60854:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0061 - `FUN_14003d7fc` line 60850
- Register: `0x08 `
- Kind: `direct_read`
```c
 60845:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60846:       DbgPrint("Wait for CAOF Done!!!!!!\n");
 60847:     }
 60848:     if ((DAT_1400a04f8 & 0x10) != 0) {
 60849:       uVar3 = FUN_140044484(param_1,0xd);
 60850:       uVar4 = FUN_140044484(param_1,8);
 60851:       DbgPrint(" Reg08h= %x,  Reg0Dh=%x .......................\n",uVar4,uVar3);
 60852:     }
 60853:     if (4 < bVar7) {
 60854:       if ((DAT_1400a04f8 & 0x10) != 0) {
 60855:         DbgPrint(&LAB_14006bc90);
```

### R0062 - `FUN_14003d7fc` line 60898
- Register: `0x5A `
- Kind: `direct_read`
```c
 60893:     FUN_140048644(param_1,10);
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
```

### R0063 - `FUN_14003d7fc` line 60899
- Register: `0x59 `
- Kind: `direct_read`
```c
 60894:     FUN_1400444d0(param_1,0x32,0x40,0);
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
```

### R0064 - `FUN_14003d7fc` line 60900
- Register: `0x59 `
- Kind: `direct_read`
```c
 60895:   }
 60896: LAB_14003dcf9:
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
 60905:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0065 - `FUN_14003d7fc` line 60902
- Register: `0x5A `
- Kind: `direct_read`
```c
 60897:   FUN_1400444d0(param_1,0xf,7,3);
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
 60905:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60906:     DbgPrint("CAOF     CAOF    CAOF     CAOF    CAOF     CAOF\n");
 60907:   }
```

### R0066 - `FUN_14003d7fc` line 60903
- Register: `0x59 `
- Kind: `direct_read`
```c
 60898:   cVar5 = FUN_140044484(param_1,0x5a);
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
 60905:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60906:     DbgPrint("CAOF     CAOF    CAOF     CAOF    CAOF     CAOF\n");
 60907:   }
 60908:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0067 - `FUN_14003d7fc` line 60904
- Register: `0x59 `
- Kind: `direct_read`
```c
 60899:   bVar1 = FUN_140044484(param_1,0x59);
 60900:   bVar2 = FUN_140044484(param_1,0x59);
 60901:   FUN_1400444d0(param_1,0xf,7,7);
 60902:   cVar6 = FUN_140044484(param_1,0x5a);
 60903:   bVar7 = FUN_140044484(param_1,0x59);
 60904:   bVar8 = FUN_140044484(param_1,0x59);
 60905:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60906:     DbgPrint("CAOF     CAOF    CAOF     CAOF    CAOF     CAOF\n");
 60907:   }
 60908:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60909:     DbgPrint("Port 0 CAOF Int =%x , CAOF Status=%3x\n",bVar2 & 0xc0,(bVar1 & 0xf) + cVar5 * '\x10');
```

### R0068 - `FUN_14003df30` line 60954
- Register: `0x04 `
- Kind: `direct_read`
```c
 60949:   byte bVar8;
 60950:   undefined7 uVar9;
 60951:   ulonglong uVar10;
 60952: 
 60953:   uVar9 = (undefined7)((ulonglong)param_3 >> 8);
 60954:   uVar1 = FUN_140044484(param_1,4);
 60955:   *(undefined1 *)(param_1 + 0x1d4) = uVar1;
 60956:   if ((DAT_1400a04f8 & 0x10) != 0) {
 60957:     DbgPrint("iTE6805_DATA.ChipID = %X !!!\n",uVar1);
 60958:   }
 60959:   *(undefined1 *)(param_1 + 0x1d5) = 1;
```

### R0069 - `FUN_14003df30` line 60986
- Register: `0x13 `
- Kind: `direct_read`
```c
 60981:   *(undefined8 *)(param_1 + 500) = 1;
 60982:   *(undefined4 *)(param_1 + 0x1fc) = 4;
 60983:   *(undefined1 *)(param_1 + 0x200) = 1;
 60984:   FUN_14003f690(param_1);
 60985:   *(undefined1 *)(param_1 + 0x204) = 1;
 60986:   bVar2 = FUN_140044484(param_1,0x13);
 60987:   if ((bVar2 & 1) == 0) {
 60988:     bVar2 = FUN_140044484(param_1,0x16);
 60989:     if ((bVar2 & 1) != 0) {
 60990:       cVar3 = cVar7;
 60991:       cVar7 = '\x01';
```

### R0070 - `FUN_14003df30` line 60988
- Register: `0x16 `
- Kind: `direct_read`
```c
 60983:   *(undefined1 *)(param_1 + 0x200) = 1;
 60984:   FUN_14003f690(param_1);
 60985:   *(undefined1 *)(param_1 + 0x204) = 1;
 60986:   bVar2 = FUN_140044484(param_1,0x13);
 60987:   if ((bVar2 & 1) == 0) {
 60988:     bVar2 = FUN_140044484(param_1,0x16);
 60989:     if ((bVar2 & 1) != 0) {
 60990:       cVar3 = cVar7;
 60991:       cVar7 = '\x01';
 60992:       goto LAB_14003e049;
 60993:     }
```

### R0071 - `FUN_14003e090` line 61049
- Register: `0x92 `
- Kind: `direct_read`
```c
 61044:   if (0xff < uVar5) {
 61045:     uVar2 = 0xff;
 61046:   }
 61047:   FUN_140044528(param_1,0x92,uVar2);
 61048:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61049:     uVar2 = FUN_140044484(param_1,0x92);
 61050:     uVar3 = FUN_140044484(param_1,0x91);
 61051:     DbgPrint("1us Timer reg91=%02x, reg92=%02x \n",uVar3,uVar2);
 61052:   }
 61053:   *(uint *)(param_1 + 0x98) = uVar6;
 61054:   if (uVar6 < 0x6400) {
```

### R0072 - `FUN_14003e090` line 61050
- Register: `0x91 `
- Kind: `direct_read`
```c
 61045:     uVar2 = 0xff;
 61046:   }
 61047:   FUN_140044528(param_1,0x92,uVar2);
 61048:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61049:     uVar2 = FUN_140044484(param_1,0x92);
 61050:     uVar3 = FUN_140044484(param_1,0x91);
 61051:     DbgPrint("1us Timer reg91=%02x, reg92=%02x \n",uVar3,uVar2);
 61052:   }
 61053:   *(uint *)(param_1 + 0x98) = uVar6;
 61054:   if (uVar6 < 0x6400) {
 61055:     uVar2 = (undefined1)(uVar6 / 100);
```

### R0073 - `FUN_14003e090` line 61098
- Register: `0x91 `
- Kind: `direct_read`
```c
 61093:     uVar2 = 0xff;
 61094:   }
 61095:   FUN_140044528(param_1,0x47,uVar2);
 61096:   FUN_1400444d0(param_1,0xf,7,0);
 61097:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61098:     uVar2 = FUN_140044484(param_1,0x91);
 61099:     DbgPrint("read 0x91=0x%02X, \n",uVar2);
 61100:   }
 61101:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61102:     uVar2 = FUN_140044484(param_1,0x92);
 61103:     DbgPrint("read 0x92=0x%02X, \n",uVar2);
```

### R0074 - `FUN_14003e090` line 61102
- Register: `0x92 `
- Kind: `direct_read`
```c
 61097:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61098:     uVar2 = FUN_140044484(param_1,0x91);
 61099:     DbgPrint("read 0x91=0x%02X, \n",uVar2);
 61100:   }
 61101:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61102:     uVar2 = FUN_140044484(param_1,0x92);
 61103:     DbgPrint("read 0x92=0x%02X, \n",uVar2);
 61104:   }
 61105:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61106:     uVar2 = FUN_140044484(param_1,0x44);
 61107:     DbgPrint("read 0x44=0x%02X, \n",uVar2);
```

### R0075 - `FUN_14003e090` line 61106
- Register: `0x44 `
- Kind: `direct_read`
```c
 61101:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61102:     uVar2 = FUN_140044484(param_1,0x92);
 61103:     DbgPrint("read 0x92=0x%02X, \n",uVar2);
 61104:   }
 61105:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61106:     uVar2 = FUN_140044484(param_1,0x44);
 61107:     DbgPrint("read 0x44=0x%02X, \n",uVar2);
 61108:   }
 61109:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61110:     uVar2 = FUN_140044484(param_1,0x45);
 61111:     DbgPrint("read 0x45=0x%02X, \n",uVar2);
```

### R0076 - `FUN_14003e090` line 61110
- Register: `0x45 `
- Kind: `direct_read`
```c
 61105:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61106:     uVar2 = FUN_140044484(param_1,0x44);
 61107:     DbgPrint("read 0x44=0x%02X, \n",uVar2);
 61108:   }
 61109:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61110:     uVar2 = FUN_140044484(param_1,0x45);
 61111:     DbgPrint("read 0x45=0x%02X, \n",uVar2);
 61112:   }
 61113:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61114:     uVar2 = FUN_140044484(param_1,0x46);
 61115:     DbgPrint("read 0x46=0x%02X, \n",uVar2);
```

### R0077 - `FUN_14003e090` line 61114
- Register: `0x46 `
- Kind: `direct_read`
```c
 61109:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61110:     uVar2 = FUN_140044484(param_1,0x45);
 61111:     DbgPrint("read 0x45=0x%02X, \n",uVar2);
 61112:   }
 61113:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61114:     uVar2 = FUN_140044484(param_1,0x46);
 61115:     DbgPrint("read 0x46=0x%02X, \n",uVar2);
 61116:   }
 61117:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61118:     uVar2 = FUN_140044484(param_1,0x47);
 61119:     DbgPrint("read 0x47=0x%02X, \n",uVar2);
```

### R0078 - `FUN_14003e090` line 61118
- Register: `0x47 `
- Kind: `direct_read`
```c
 61113:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61114:     uVar2 = FUN_140044484(param_1,0x46);
 61115:     DbgPrint("read 0x46=0x%02X, \n",uVar2);
 61116:   }
 61117:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61118:     uVar2 = FUN_140044484(param_1,0x47);
 61119:     DbgPrint("read 0x47=0x%02X, \n",uVar2);
 61120:   }
 61121:   return;
 61122: }
 61123: 
```

### R0079 - `FUN_14003e4b8` line 61149
- Register: `0x60 `
- Kind: `direct_read`
```c
 61144:   FUN_1400444d0(param_1,0xf,7,1);
 61145:   FUN_140044528(param_1,0x5f,4);
 61146:   FUN_140044528(param_1,0x5f,5);
 61147:   FUN_140044528(param_1,0x58,0x12);
 61148:   FUN_140044528(param_1,0x58,2);
 61149:   cVar1 = FUN_140044484(param_1,0x60);
 61150:   uVar10 = 0;
 61151:   if (cVar1 != '\x19') {
 61152:     FUN_140044528(param_1,0xf8,0xc3);
 61153:     FUN_140044528(param_1,0xf8,0xa5);
 61154:     FUN_140044528(param_1,0x5f,4);
```

### R0080 - `FUN_14003e4b8` line 61159
- Register: `0x60 `
- Kind: `direct_read`
```c
 61154:     FUN_140044528(param_1,0x5f,4);
 61155:     FUN_140044528(param_1,0x58,0x12);
 61156:     FUN_140044528(param_1,0x58,2);
 61157:     cVar1 = '2';
 61158:     do {
 61159:       cVar2 = FUN_140044484(param_1,0x60);
 61160:       FUN_140048644(param_1,1);
 61161:       cVar1 = cVar1 + -1;
 61162:       if (cVar2 == '\x19') break;
 61163:     } while (cVar1 != '\0');
 61164:     FUN_140048644(param_1,10);
```

### R0081 - `FUN_14003e4b8` line 61173
- Register: `0x61 `
- Kind: `direct_read`
```c
 61168:   }
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
```

### R0082 - `FUN_14003e4b8` line 61174
- Register: `0x62 `
- Kind: `direct_read`
```c
 61169:   FUN_140044528(param_1,0x57,1);
 61170:   FUN_140044528(param_1,0x50,0);
 61171:   FUN_140044528(param_1,0x51,0);
 61172:   FUN_140044528(param_1,0x54,4);
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
```

### R0083 - `FUN_14003e4b8` line 61178
- Register: `0x61 `
- Kind: `direct_read`
```c
 61173:   cVar1 = FUN_140044484(param_1,0x61);
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
```

### R0084 - `FUN_14003e4b8` line 61179
- Register: `0x62 `
- Kind: `direct_read`
```c
 61174:   cVar2 = FUN_140044484(param_1,0x62);
 61175:   FUN_140044528(param_1,0x50,0);
 61176:   FUN_140044528(param_1,0x51,1);
 61177:   FUN_140044528(param_1,0x54,4);
 61178:   cVar3 = FUN_140044484(param_1,0x61);
 61179:   cVar4 = FUN_140044484(param_1,0x62);
 61180:   if ((((cVar1 == -1) && (cVar2 == -1)) && (cVar3 == '\0')) && (cVar4 == '\0')) {
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
```

### R0085 - `FUN_14003e4b8` line 61186
- Register: `0x61 `
- Kind: `direct_read`
```c
 61181:     uVar10 = 4;
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
```

### R0086 - `FUN_14003e4b8` line 61187
- Register: `0x62 `
- Kind: `direct_read`
```c
 61182:   }
 61183:   FUN_140044528(param_1,0x50,uVar10);
 61184:   FUN_140044528(param_1,0x51,0xb0);
 61185:   FUN_140044528(param_1,0x54,4);
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
```

### R0087 - `FUN_14003e4b8` line 61191
- Register: `0x61 `
- Kind: `direct_read`
```c
 61186:   bVar5 = FUN_140044484(param_1,0x61);
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
 61193:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61194:     DbgPrint("read 0x61=0x%02X, \n",bVar5);
 61195:   }
 61196:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0088 - `FUN_14003e4b8` line 61192
- Register: `0x62 `
- Kind: `direct_read`
```c
 61187:   bVar6 = FUN_140044484(param_1,0x62);
 61188:   FUN_140044528(param_1,0x50,uVar10);
 61189:   FUN_140044528(param_1,0x51,0xb1);
 61190:   FUN_140044528(param_1,0x54,4);
 61191:   bVar7 = FUN_140044484(param_1,0x61);
 61192:   bVar8 = FUN_140044484(param_1,0x62);
 61193:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61194:     DbgPrint("read 0x61=0x%02X, \n",bVar5);
 61195:   }
 61196:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61197:     DbgPrint("read 0x61=0x%02X, \n",bVar6);
```

### R0089 - `FUN_14003ea04` line 61305
- Register: `0x8A `
- Kind: `direct_read`
```c
 61300:   undefined1 uVar1;
 61301: 
 61302:   FUN_1400444d0(param_1,0xf,7,0);
 61303:   FUN_1400444d0(param_1,0x22,2,2);
 61304:   FUN_1400444d0(param_1,0x22,2,0);
 61305:   uVar1 = FUN_140044484(param_1,0x8a);
 61306:   FUN_140044528(param_1,0x8a,uVar1);
 61307:   FUN_140044528(param_1,0x8a,uVar1);
 61308:   FUN_140044528(param_1,0x8a,uVar1);
 61309:   FUN_140044528(param_1,0x8a,uVar1);
 61310:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0090 - `FUN_14003eb0c` line 61340
- Register: `0x1B `
- Kind: `direct_read`
```c
 61335:   byte bVar1;
 61336:   byte bVar2;
 61337:   bool bVar3;
 61338: 
 61339:   FUN_1400444d0(param_1,0xf,7,0);
 61340:   bVar1 = FUN_140044484(param_1,0x1b);
 61341:   bVar2 = bVar1 >> 4 & 3;
 61342:   if ((bVar1 & 0x30) != 0) {
 61343:     if (bVar2 == 1) {
 61344:       *(undefined1 *)(param_1 + 0x228) = 2;
 61345:       goto LAB_14003eb57;
```

### R0091 - `FUN_14003ebd0` line 61378
- Register: `0x43 `
- Kind: `direct_read`
```c
 61373:   byte bVar3;
 61374:   undefined1 uVar4;
 61375:   ulonglong uVar5;
 61376: 
 61377:   FUN_1400444d0(param_1,0xf,7,0);
 61378:   bVar2 = FUN_140044484(param_1,0x43);
 61379:   FUN_140044484(param_1,0x47);
 61380:   bVar3 = FUN_140044484(param_1,0x48);
 61381:   if ((char)bVar2 < '\0') {
 61382:     bVar3 = bVar3 >> 3;
 61383:   }
```

### R0092 - `FUN_14003ebd0` line 61379
- Register: `0x47 `
- Kind: `direct_read`
```c
 61374:   undefined1 uVar4;
 61375:   ulonglong uVar5;
 61376: 
 61377:   FUN_1400444d0(param_1,0xf,7,0);
 61378:   bVar2 = FUN_140044484(param_1,0x43);
 61379:   FUN_140044484(param_1,0x47);
 61380:   bVar3 = FUN_140044484(param_1,0x48);
 61381:   if ((char)bVar2 < '\0') {
 61382:     bVar3 = bVar3 >> 3;
 61383:   }
 61384:   else if ((bVar2 & 0x40) == 0) {
```

### R0093 - `FUN_14003ebd0` line 61380
- Register: `0x48 `
- Kind: `direct_read`
```c
 61375:   ulonglong uVar5;
 61376: 
 61377:   FUN_1400444d0(param_1,0xf,7,0);
 61378:   bVar2 = FUN_140044484(param_1,0x43);
 61379:   FUN_140044484(param_1,0x47);
 61380:   bVar3 = FUN_140044484(param_1,0x48);
 61381:   if ((char)bVar2 < '\0') {
 61382:     bVar3 = bVar3 >> 3;
 61383:   }
 61384:   else if ((bVar2 & 0x40) == 0) {
 61385:     if ((bVar2 & 0x20) != 0) {
```

### R0094 - `FUN_14003ebd0` line 61408
- Register: `0x47 `
- Kind: `direct_read`
```c
 61403:   }
 61404:   if ((bVar3 < (byte)uVar5) && ((bVar2 & 2) == 0)) {
 61405:     if ((DAT_1400a04f8 & 0x10) != 0) {
 61406:       DbgPrint("OverWrite IPLL/OPLL \n");
 61407:     }
 61408:     uVar4 = FUN_140044484(param_1,0x47);
 61409:     FUN_140044528(param_1,0x47,uVar4);
 61410:   }
 61411:   return;
 61412: }
 61413: 
```

### R0095 - `FUN_14003ee7c` line 61481
- Register: `0x98 `
- Kind: `direct_read`
```c
 61476: {
 61477:   ushort uVar1;
 61478:   byte bVar2;
 61479: 
 61480:   FUN_1400444d0(param_1,0xf,7,0);
 61481:   bVar2 = FUN_140044484(param_1,0x98);
 61482:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar2 & 0xf0);
 61483:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61484:     DbgPrint("\n Input ColorDepth = ");
 61485:   }
 61486:   uVar1 = *(ushort *)(param_1 + 0x252);
```

### R0096 - `FUN_14003ef68` line 61549
- Register: `0x98 `
- Kind: `direct_read`
```c
 61544:   FUN_1400444d0(param_1,0xf,7,0);
 61545:   FUN_1400444d0(param_1,0x33,8,8);
 61546:   uVar2 = *(undefined2 *)(param_1 + 0x22e);
 61547:   bVar18 = *(int *)(param_1 + 0x20c) == 3;
 61548:   uVar15 = *(short *)(param_1 + 0x22c) << bVar18;
 61549:   FUN_140044484(param_1,0x98);
 61550:   cVar1 = (char)*(undefined2 *)(param_1 + 0x250);
 61551:   uVar3 = *(ushort *)(param_1 + 0x246);
 61552:   uVar7 = uVar3 >> 1;
 61553:   uVar4 = *(ushort *)(param_1 + 0x248);
 61554:   uVar8 = uVar4 >> 1;
```

### R0097 - `FUN_14003f500` line 61685
- Register: `0x13 `
- Kind: `direct_read`
```c
 61680: 
 61681:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61682:     DbgPrint("iTE6805_Set_HPD_Ctrl PORT%d_HPD = %d \r\n",param_2,param_3);
 61683:   }
 61684:   if (param_2 == 0) {
 61685:     bVar1 = FUN_140044484(param_1,0x13);
 61686:     if ((bVar1 & 1) == 0) {
 61687:       FUN_14003b050(param_1,param_3);
 61688:     }
 61689:     bVar1 = FUN_14003ce60(param_1,'\0');
 61690:     if (bVar1 == 0) {
```

### R0098 - `FUN_14003f774` line 61818
- Register: `0x98 `
- Kind: `direct_read`
```c
 61813:   bool bVar2;
 61814:   char *pcVar3;
 61815:   undefined1 uVar4;
 61816: 
 61817:   FUN_1400444d0(param_1,0xf,7,0);
 61818:   bVar1 = FUN_140044484(param_1,0x98);
 61819:   *(ushort *)(param_1 + 0x252) = (ushort)(bVar1 & 0xf0);
 61820:   FUN_1400444d0(param_1,0xf,7,1);
 61821:   if (param_2 == '\0') {
 61822:     FUN_1400444d0(param_1,0xc5,0x80,0);
 61823:     FUN_1400444d0(param_1,0xc6,0x80,0);
```

### R0099 - `FUN_14003f774` line 61839
- Register: `0xC0 `
- Kind: `direct_read`
```c
 61834: LAB_14003f881:
 61835:       FUN_140044528(param_1,bVar1,uVar4);
 61836:     }
 61837:     else {
 61838:       FUN_1400444d0(param_1,0xf,7,1);
 61839:       bVar1 = FUN_140044484(param_1,0xc0);
 61840:       if (((bVar1 & 0xc0) == 0) || ((bVar1 & 0xc0) == 0x40)) {
 61841:         if (*(short *)(param_1 + 0x252) == 0x10) {
 61842:           uVar4 = 0x78;
 61843: LAB_14003f83a:
 61844:           bVar1 = 0xc5;
```

### R0100 - `FUN_14003f8e0` line 61901
- Register: `0xB0 `
- Kind: `direct_read`
```c
 61896:   byte bVar5;
 61897:   byte bVar6;
 61898:   byte bVar7;
 61899:   char *pcVar8;
 61900: 
 61901:   bVar1 = FUN_140044484(param_1,0xb0);
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
```

### R0101 - `FUN_14003f8e0` line 61902
- Register: `0xB0 `
- Kind: `direct_read`
```c
 61897:   byte bVar6;
 61898:   byte bVar7;
 61899:   char *pcVar8;
 61900: 
 61901:   bVar1 = FUN_140044484(param_1,0xb0);
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
```

### R0102 - `FUN_14003f8e0` line 61903
- Register: `0xB0 `
- Kind: `direct_read`
```c
 61898:   byte bVar7;
 61899:   char *pcVar8;
 61900: 
 61901:   bVar1 = FUN_140044484(param_1,0xb0);
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
```

### R0103 - `FUN_14003f8e0` line 61904
- Register: `0xB0 `
- Kind: `direct_read`
```c
 61899:   char *pcVar8;
 61900: 
 61901:   bVar1 = FUN_140044484(param_1,0xb0);
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
```

### R0104 - `FUN_14003f8e0` line 61905
- Register: `0xB1 `
- Kind: `direct_read`
```c
 61900: 
 61901:   bVar1 = FUN_140044484(param_1,0xb0);
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
```

### R0105 - `FUN_14003f8e0` line 61907
- Register: `0xB5 `
- Kind: `direct_read`
```c
 61902:   bVar2 = FUN_140044484(param_1,0xb0);
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
 61911:   bVar6 = FUN_140044484(param_1,0x46);
 61912:   bVar6 = bVar6 >> 2 & 7;
```

### R0106 - `FUN_14003f8e0` line 61908
- Register: `0xB5 `
- Kind: `direct_read`
```c
 61903:   bVar3 = FUN_140044484(param_1,0xb0);
 61904:   cVar4 = FUN_140044484(param_1,0xb0);
 61905:   bVar5 = FUN_140044484(param_1,0xb1);
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
 61911:   bVar6 = FUN_140044484(param_1,0x46);
 61912:   bVar6 = bVar6 >> 2 & 7;
 61913:   FUN_1400444d0(param_1,0xf,7,0);
```

### R0107 - `FUN_14003f8e0` line 61911
- Register: `0x46 `
- Kind: `direct_read`
```c
 61906:   bVar5 = bVar5 & 0x3f;
 61907:   bVar6 = FUN_140044484(param_1,0xb5);
 61908:   bVar7 = FUN_140044484(param_1,0xb5);
 61909:   bVar7 = (bVar7 & 0xf) + (bVar6 >> 2 & 0x30);
 61910:   FUN_1400444d0(param_1,0xf,7,2);
 61911:   bVar6 = FUN_140044484(param_1,0x46);
 61912:   bVar6 = bVar6 >> 2 & 7;
 61913:   FUN_1400444d0(param_1,0xf,7,0);
 61914:   if ((DAT_1400a04f8 & 0x10) != 0) {
 61915:     DbgPrint("Audio Format: ");
 61916:   }
```

### R0108 - `FUN_140040140` line 62335
- Register: `0x15 `
- Kind: `direct_read`
```c
 62330: 
 62331:   FUN_1400444d0(param_1,0xf,7,0);
 62332:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62333:     DbgPrint("\n\n -------Video Timing-------\n");
 62334:   }
 62335:   bVar2 = FUN_140044484(param_1,0x15);
 62336:   if ((bVar2 & 2) == 0) {
 62337:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62338:       pcVar4 = "HDMI2 Scramble Disable !! \n";
 62339:       goto LAB_1400401ac;
 62340:     }
```

### R0109 - `FUN_140040140` line 62347
- Register: `0x14 `
- Kind: `direct_read`
```c
 62342:   else if ((DAT_1400a04f8 & 0x10) != 0) {
 62343:     pcVar4 = "HDMI2 Scramble Enable !! \n";
 62344: LAB_1400401ac:
 62345:     DbgPrint(pcVar4);
 62346:   }
 62347:   bVar2 = FUN_140044484(param_1,0x14);
 62348:   if ((bVar2 & 0x40) == 0) {
 62349:     if ((DAT_1400a04f8 & 0x10) != 0) {
 62350:       pcVar4 = "HDMI CLK Ratio 1/10  !! \n";
 62351:       goto LAB_1400401df;
 62352:     }
```

### R0110 - `FUN_140040140` line 62359
- Register: `0x19 `
- Kind: `direct_read`
```c
 62354:   else if ((DAT_1400a04f8 & 0x10) != 0) {
 62355:     pcVar4 = "HDMI2 CLK Ratio 1/40 !! \n";
 62356: LAB_1400401df:
 62357:     DbgPrint(pcVar4);
 62358:   }
 62359:   bVar2 = FUN_140044484(param_1,0x19);
 62360:   bVar3 = FUN_140044484(param_1,0x36);
 62361:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62362:     uVar1 = (ulonglong)*(uint *)(param_1 + 0x238) / 1000;
 62363:     DbgPrint("TMDSCLK = %lu.%03luMHz\n",uVar1,*(uint *)(param_1 + 0x238) + (int)uVar1 * -1000);
 62364:   }
```

### R0111 - `FUN_140040140` line 62360
- Register: `0x36 `
- Kind: `direct_read`
```c
 62355:     pcVar4 = "HDMI2 CLK Ratio 1/40 !! \n";
 62356: LAB_1400401df:
 62357:     DbgPrint(pcVar4);
 62358:   }
 62359:   bVar2 = FUN_140044484(param_1,0x19);
 62360:   bVar3 = FUN_140044484(param_1,0x36);
 62361:   if ((DAT_1400a04f8 & 0x10) != 0) {
 62362:     uVar1 = (ulonglong)*(uint *)(param_1 + 0x238) / 1000;
 62363:     DbgPrint("TMDSCLK = %lu.%03luMHz\n",uVar1,*(uint *)(param_1 + 0x238) + (int)uVar1 * -1000);
 62364:   }
 62365:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0112 - `FUN_14004165c` line 63026
- Register: `0x19 `
- Kind: `direct_read`
```c
 63021:   char cVar5;
 63022:   byte bVar6;
 63023:   bool bVar7;
 63024: 
 63025:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
```

### R0113 - `FUN_14004165c` line 63028
- Register: `0x13 `
- Kind: `direct_read`
```c
 63023:   bool bVar7;
 63024: 
 63025:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
```

### R0114 - `FUN_14004165c` line 63029
- Register: `0x14 `
- Kind: `direct_read`
```c
 63024: 
 63025:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
```

### R0115 - `FUN_14004165c` line 63031
- Register: `0xB9 `
- Kind: `direct_read`
```c
 63026:   cVar1 = FUN_140044484(param_1,0x19);
 63027:   if (*(char *)(param_1 + 0x1ec) == '\0') {
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
```

### R0116 - `FUN_14004165c` line 63033
- Register: `0xBE `
- Kind: `direct_read`
```c
 63028:     cVar2 = FUN_140044484(param_1,0x13);
 63029:     bVar3 = FUN_140044484(param_1,0x14);
 63030:     FUN_140044528(param_1,0xb9,0xff);
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
```

### R0117 - `FUN_14004165c` line 63036
- Register: `0x16 `
- Kind: `direct_read`
```c
 63031:     cVar4 = FUN_140044484(param_1,0xb9);
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
```

### R0118 - `FUN_14004165c` line 63037
- Register: `0x17 `
- Kind: `direct_read`
```c
 63032:     FUN_140044528(param_1,0xbe,0xff);
 63033:     cVar5 = FUN_140044484(param_1,0xbe);
 63034:   }
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
```

### R0119 - `FUN_14004165c` line 63040
- Register: `0xB9 `
- Kind: `direct_read`
```c
 63035:   else {
 63036:     cVar2 = FUN_140044484(param_1,0x16);
 63037:     bVar3 = FUN_140044484(param_1,0x17);
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
 63044:     thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
 63045:   }
```

### R0120 - `FUN_14004165c` line 63043
- Register: `0xBE `
- Kind: `direct_read`
```c
 63038:     thunk_FUN_1400444d0(param_1,4,param_3,param_4);
 63039:     FUN_140044528(param_1,0xb9,0xff);
 63040:     cVar4 = FUN_140044484(param_1,0xb9);
 63041:     bVar6 = 0xff;
 63042:     FUN_140044528(param_1,0xbe,0xff);
 63043:     cVar5 = FUN_140044484(param_1,0xbe);
 63044:     thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
 63045:   }
 63046:   if ((((cVar2 == -0x41) && ((bVar3 & 0x38) == 0x38)) && (cVar1 < '\0')) && (cVar4 == '\0')) {
 63047:     bVar7 = cVar5 == '\0';
 63048:   }
```

### R0121 - `FUN_140041784` line 63095
- Register: `0xEA `
- Kind: `direct_read`
```c
 63090:   FUN_1400444d0(param_1,0x55,0x80,0);
 63091:   FUN_140044528(param_1,0xe9,0x80);
 63092:   FUN_140048644(param_1,100);
 63093:   FUN_140044528(param_1,0xe9,0);
 63094:   cVar1 = *(char *)(param_1 + 0x17c);
 63095:   uVar2 = FUN_140044484(param_1,0xea);
 63096:   *(undefined1 *)(param_1 + ((longlong)cVar1 + 0x34) * 6) = uVar2;
 63097:   cVar1 = *(char *)(param_1 + 0x17c);
 63098:   uVar2 = FUN_140044484(param_1,0xeb);
 63099:   *(undefined1 *)(param_1 + 0x139 + (longlong)cVar1 * 6) = uVar2;
 63100:   FUN_140044528(param_1,0xe9,0x20);
```

### R0122 - `FUN_140041784` line 63098
- Register: `0xEB `
- Kind: `direct_read`
```c
 63093:   FUN_140044528(param_1,0xe9,0);
 63094:   cVar1 = *(char *)(param_1 + 0x17c);
 63095:   uVar2 = FUN_140044484(param_1,0xea);
 63096:   *(undefined1 *)(param_1 + ((longlong)cVar1 + 0x34) * 6) = uVar2;
 63097:   cVar1 = *(char *)(param_1 + 0x17c);
 63098:   uVar2 = FUN_140044484(param_1,0xeb);
 63099:   *(undefined1 *)(param_1 + 0x139 + (longlong)cVar1 * 6) = uVar2;
 63100:   FUN_140044528(param_1,0xe9,0x20);
 63101:   cVar1 = *(char *)(param_1 + 0x17c);
 63102:   uVar2 = FUN_140044484(param_1,0xea);
 63103:   *(undefined1 *)(param_1 + 0x13a + (longlong)cVar1 * 6) = uVar2;
```

### R0123 - `FUN_140041784` line 63102
- Register: `0xEA `
- Kind: `direct_read`
```c
 63097:   cVar1 = *(char *)(param_1 + 0x17c);
 63098:   uVar2 = FUN_140044484(param_1,0xeb);
 63099:   *(undefined1 *)(param_1 + 0x139 + (longlong)cVar1 * 6) = uVar2;
 63100:   FUN_140044528(param_1,0xe9,0x20);
 63101:   cVar1 = *(char *)(param_1 + 0x17c);
 63102:   uVar2 = FUN_140044484(param_1,0xea);
 63103:   *(undefined1 *)(param_1 + 0x13a + (longlong)cVar1 * 6) = uVar2;
 63104:   cVar1 = *(char *)(param_1 + 0x17c);
 63105:   uVar2 = FUN_140044484(param_1,0xeb);
 63106:   bVar4 = 0x40;
 63107:   *(undefined1 *)(param_1 + 0x13b + (longlong)cVar1 * 6) = uVar2;
```

### R0124 - `FUN_140041784` line 63105
- Register: `0xEB `
- Kind: `direct_read`
```c
 63100:   FUN_140044528(param_1,0xe9,0x20);
 63101:   cVar1 = *(char *)(param_1 + 0x17c);
 63102:   uVar2 = FUN_140044484(param_1,0xea);
 63103:   *(undefined1 *)(param_1 + 0x13a + (longlong)cVar1 * 6) = uVar2;
 63104:   cVar1 = *(char *)(param_1 + 0x17c);
 63105:   uVar2 = FUN_140044484(param_1,0xeb);
 63106:   bVar4 = 0x40;
 63107:   *(undefined1 *)(param_1 + 0x13b + (longlong)cVar1 * 6) = uVar2;
 63108:   FUN_140044528(param_1,0xe9,0x40);
 63109:   cVar1 = *(char *)(param_1 + 0x17c);
 63110:   uVar2 = FUN_140044484(param_1,0xea);
```

### R0125 - `FUN_140041784` line 63110
- Register: `0xEA `
- Kind: `direct_read`
```c
 63105:   uVar2 = FUN_140044484(param_1,0xeb);
 63106:   bVar4 = 0x40;
 63107:   *(undefined1 *)(param_1 + 0x13b + (longlong)cVar1 * 6) = uVar2;
 63108:   FUN_140044528(param_1,0xe9,0x40);
 63109:   cVar1 = *(char *)(param_1 + 0x17c);
 63110:   uVar2 = FUN_140044484(param_1,0xea);
 63111:   *(undefined1 *)(param_1 + 0x13c + (longlong)cVar1 * 6) = uVar2;
 63112:   cVar1 = *(char *)(param_1 + 0x17c);
 63113:   uVar2 = FUN_140044484(param_1,0xeb);
 63114:   *(undefined1 *)(param_1 + 0x13d + (longlong)cVar1 * 6) = uVar2;
 63115:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0126 - `FUN_140041784` line 63113
- Register: `0xEB `
- Kind: `direct_read`
```c
 63108:   FUN_140044528(param_1,0xe9,0x40);
 63109:   cVar1 = *(char *)(param_1 + 0x17c);
 63110:   uVar2 = FUN_140044484(param_1,0xea);
 63111:   *(undefined1 *)(param_1 + 0x13c + (longlong)cVar1 * 6) = uVar2;
 63112:   cVar1 = *(char *)(param_1 + 0x17c);
 63113:   uVar2 = FUN_140044484(param_1,0xeb);
 63114:   *(undefined1 *)(param_1 + 0x13d + (longlong)cVar1 * 6) = uVar2;
 63115:   if ((DAT_1400a04f8 & 0x10) != 0) {
 63116:     DbgPrint("......................................................\n");
 63117:   }
 63118:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0127 - `FUN_140041b38` line 63344
- Register: `0xE5 `
- Kind: `direct_read`
```c
 63339:         }
 63340:         else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 63341:           bVar8 = 7;
 63342:           goto LAB_14004231a;
 63343:         }
 63344:         bVar8 = FUN_140044484(param_1,0xe5);
 63345:         if ((bVar8 & 0x10) == 0) {
 63346:           cVar2 = FUN_14003cdc4(param_1);
 63347:           cVar1 = *(char *)(param_1 + 0x1ec);
 63348:           if (cVar2 != '\0') {
 63349:             if (cVar1 == '\0') {
```

### R0128 - `FUN_1400428d8` line 63823
- Register: `0xD5 `
- Kind: `direct_read`
```c
 63818:   if ((DAT_1400a04f8 & 0x10) != 0) {
 63819:     DbgPrint("!!!!!!!!!!!!\n");
 63820:   }
 63821:   if (param_2 == 0) {
 63822:     FUN_1400444d0(param_1,0x37,0xc0,0);
 63823:     cVar2 = FUN_140044484(param_1,0xd5);
 63824:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63825:       pcVar9 = "!! Report Channel B DFE Value , EQ TargetRS = 0x%x!!\n";
 63826: LAB_140042a1a:
 63827:       DbgPrint(pcVar9,cVar2);
 63828:     }
```

### R0129 - `FUN_1400428d8` line 63832
- Register: `0xD6 `
- Kind: `direct_read`
```c
 63827:       DbgPrint(pcVar9,cVar2);
 63828:     }
 63829:   }
 63830:   else if (param_2 == 1) {
 63831:     FUN_1400444d0(param_1,0x37,0xc0,0x40);
 63832:     cVar2 = FUN_140044484(param_1,0xd6);
 63833:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63834:       pcVar9 = "!! Report Channel G DFE Value , EQ TargetRS = 0x%x!!\n";
 63835:       goto LAB_140042a1a;
 63836:     }
 63837:   }
```

### R0130 - `FUN_1400428d8` line 63840
- Register: `0xD7 `
- Kind: `direct_read`
```c
 63835:       goto LAB_140042a1a;
 63836:     }
 63837:   }
 63838:   else if (param_2 == 2) {
 63839:     FUN_1400444d0(param_1,0x37,0xc0,0x80);
 63840:     cVar2 = FUN_140044484(param_1,0xd7);
 63841:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63842:       pcVar9 = "!! Report Channel R DFE Value , EQ TargetRS = 0x%x!!\n";
 63843:       goto LAB_140042a1a;
 63844:     }
 63845:   }
```

### R0131 - `FUN_1400428d8` line 63859
- Register: `0x5D `
- Kind: `direct_read`
```c
 63854:   local_58 = 0;
 63855:   pcVar9 = "\x7f~?>\x1f\x1e\x0f\x0e\a\x06\x03\x02\x01";
 63856:   cVar13 = cVar2;
 63857:   do {
 63858:     FUN_1400444d0(param_1,0x36,0xf,local_58);
 63859:     bVar3 = FUN_140044484(param_1,0x5d);
 63860:     uVar17 = (uint)bVar3;
 63861:     bVar4 = FUN_140044484(param_1,0x5e);
 63862:     bVar5 = FUN_140044484(param_1,0x5f);
 63863:     bVar6 = FUN_140044484(param_1,0x60);
 63864:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0132 - `FUN_1400428d8` line 63861
- Register: `0x5E `
- Kind: `direct_read`
```c
 63856:   cVar13 = cVar2;
 63857:   do {
 63858:     FUN_1400444d0(param_1,0x36,0xf,local_58);
 63859:     bVar3 = FUN_140044484(param_1,0x5d);
 63860:     uVar17 = (uint)bVar3;
 63861:     bVar4 = FUN_140044484(param_1,0x5e);
 63862:     bVar5 = FUN_140044484(param_1,0x5f);
 63863:     bVar6 = FUN_140044484(param_1,0x60);
 63864:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63865:       DbgPrint("RS =%02x ",*pcVar9);
 63866:     }
```

### R0133 - `FUN_1400428d8` line 63862
- Register: `0x5F `
- Kind: `direct_read`
```c
 63857:   do {
 63858:     FUN_1400444d0(param_1,0x36,0xf,local_58);
 63859:     bVar3 = FUN_140044484(param_1,0x5d);
 63860:     uVar17 = (uint)bVar3;
 63861:     bVar4 = FUN_140044484(param_1,0x5e);
 63862:     bVar5 = FUN_140044484(param_1,0x5f);
 63863:     bVar6 = FUN_140044484(param_1,0x60);
 63864:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63865:       DbgPrint("RS =%02x ",*pcVar9);
 63866:     }
 63867:     if (bVar4 < bVar3) {
```

### R0134 - `FUN_1400428d8` line 63863
- Register: `0x60 `
- Kind: `direct_read`
```c
 63858:     FUN_1400444d0(param_1,0x36,0xf,local_58);
 63859:     bVar3 = FUN_140044484(param_1,0x5d);
 63860:     uVar17 = (uint)bVar3;
 63861:     bVar4 = FUN_140044484(param_1,0x5e);
 63862:     bVar5 = FUN_140044484(param_1,0x5f);
 63863:     bVar6 = FUN_140044484(param_1,0x60);
 63864:     if ((DAT_1400a04f8 & 0x10) != 0) {
 63865:       DbgPrint("RS =%02x ",*pcVar9);
 63866:     }
 63867:     if (bVar4 < bVar3) {
 63868:       iVar8 = (uint)bVar3 - (uint)bVar4;
```

### R0135 - `FUN_140042e78` line 64025
- Register: `0xD5 `
- Kind: `direct_read`
```c
 64020:     if (*(char *)(param_1 + 0x1ec) != '\x01') goto LAB_140042ea0;
 64021:     bVar1 = 7;
 64022:   }
 64023:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64024: LAB_140042ea0:
 64025:   bVar1 = FUN_140044484(param_1,0xd5);
 64026:   *(byte *)(param_1 + 0x164) = bVar1 & 0x7f;
 64027:   bVar1 = FUN_140044484(param_1,0xd6);
 64028:   *(byte *)(param_1 + 0x165) = bVar1 & 0x7f;
 64029:   bVar1 = FUN_140044484(param_1,0xd7);
 64030:   *(byte *)(param_1 + 0x166) = bVar1 & 0x7f;
```

### R0136 - `FUN_140042e78` line 64027
- Register: `0xD6 `
- Kind: `direct_read`
```c
 64022:   }
 64023:   thunk_FUN_1400444d0(param_1,bVar1,param_3,param_4);
 64024: LAB_140042ea0:
 64025:   bVar1 = FUN_140044484(param_1,0xd5);
 64026:   *(byte *)(param_1 + 0x164) = bVar1 & 0x7f;
 64027:   bVar1 = FUN_140044484(param_1,0xd6);
 64028:   *(byte *)(param_1 + 0x165) = bVar1 & 0x7f;
 64029:   bVar1 = FUN_140044484(param_1,0xd7);
 64030:   *(byte *)(param_1 + 0x166) = bVar1 & 0x7f;
 64031:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64032:     DbgPrint(&LAB_14006b500);
```

### R0137 - `FUN_140042e78` line 64029
- Register: `0xD7 `
- Kind: `direct_read`
```c
 64024: LAB_140042ea0:
 64025:   bVar1 = FUN_140044484(param_1,0xd5);
 64026:   *(byte *)(param_1 + 0x164) = bVar1 & 0x7f;
 64027:   bVar1 = FUN_140044484(param_1,0xd6);
 64028:   *(byte *)(param_1 + 0x165) = bVar1 & 0x7f;
 64029:   bVar1 = FUN_140044484(param_1,0xd7);
 64030:   *(byte *)(param_1 + 0x166) = bVar1 & 0x7f;
 64031:   if ((DAT_1400a04f8 & 0x10) != 0) {
 64032:     DbgPrint(&LAB_14006b500);
 64033:   }
 64034:   if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0138 - `FUN_140042f8c` line 64075
- Register: `0x74 `
- Kind: `direct_read`
```c
 64070:   }
 64071:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64072:     bVar1 = 7;
 64073:     goto LAB_140042fc3;
 64074:   }
 64075:   bVar1 = FUN_140044484(param_1,0x74);
 64076:   if ((bVar1 & 3) == 2) {
 64077:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64078:       pcVar4 = "CHB SKEW : ENSKEW =1, PSKEW= 0 \n";
 64079:       goto LAB_140043011;
 64080:     }
```

### R0139 - `FUN_140042f8c` line 64128
- Register: `0x75 `
- Kind: `direct_read`
```c
 64123: LAB_140043099:
 64124:   do {
 64125:     bVar1 = 0xc0;
 64126:     bVar6 = bVar5 << 6;
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
```

### R0140 - `FUN_140042f8c` line 64129
- Register: `0x76 `
- Kind: `direct_read`
```c
 64124:   do {
 64125:     bVar1 = 0xc0;
 64126:     bVar6 = bVar5 << 6;
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
 64134:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0141 - `FUN_140042f8c` line 64130
- Register: `0x77 `
- Kind: `direct_read`
```c
 64125:     bVar1 = 0xc0;
 64126:     bVar6 = bVar5 << 6;
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
 64134:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64135:       DbgPrint(" SKEW info of Selected channel: %d : \n");
```

### R0142 - `FUN_140042f8c` line 64131
- Register: `0x78 `
- Kind: `direct_read`
```c
 64126:     bVar6 = bVar5 << 6;
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
 64134:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64135:       DbgPrint(" SKEW info of Selected channel: %d : \n");
 64136:     }
```

### R0143 - `FUN_140042f8c` line 64132
- Register: `0x79 `
- Kind: `direct_read`
```c
 64127:     FUN_1400444d0(param_1,0x37,0xc0,bVar6);
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
 64134:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64135:       DbgPrint(" SKEW info of Selected channel: %d : \n");
 64136:     }
 64137:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0144 - `FUN_140042f8c` line 64133
- Register: `0x7A `
- Kind: `direct_read`
```c
 64128:     FUN_140044484(param_1,0x75);
 64129:     bVar2 = FUN_140044484(param_1,0x76);
 64130:     bVar3 = FUN_140044484(param_1,0x77);
 64131:     FUN_140044484(param_1,0x78);
 64132:     FUN_140044484(param_1,0x79);
 64133:     FUN_140044484(param_1,0x7a);
 64134:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64135:       DbgPrint(" SKEW info of Selected channel: %d : \n");
 64136:     }
 64137:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64138:       bVar1 = bVar2;
```

### R0145 - `FUN_140043370` line 64257
- Register: `0x4B `
- Kind: `direct_read`
```c
 64252:     FUN_140044528(param_1,0x4d,bVar6);
 64253:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64254:       DbgPrint("--------------setting Channel B DFE--------------\n");
 64255:     }
 64256:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64257:       FUN_140044484(param_1,0x4b);
 64258:       DbgPrint("-setting 4B to 0x%x -\n");
 64259:     }
 64260:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64261:       FUN_140044484(param_1,0x4c);
 64262:       DbgPrint("-setting 4C to 0x%x -\n");
```

### R0146 - `FUN_140043370` line 64261
- Register: `0x4C `
- Kind: `direct_read`
```c
 64256:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64257:       FUN_140044484(param_1,0x4b);
 64258:       DbgPrint("-setting 4B to 0x%x -\n");
 64259:     }
 64260:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64261:       FUN_140044484(param_1,0x4c);
 64262:       DbgPrint("-setting 4C to 0x%x -\n");
 64263:     }
 64264:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140043646;
 64265:     FUN_140044484(param_1,0x4d);
 64266:     pcVar2 = "-setting 4D to 0x%x -\n";
```

### R0147 - `FUN_140043370` line 64265
- Register: `0x4D `
- Kind: `direct_read`
```c
 64260:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64261:       FUN_140044484(param_1,0x4c);
 64262:       DbgPrint("-setting 4C to 0x%x -\n");
 64263:     }
 64264:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140043646;
 64265:     FUN_140044484(param_1,0x4d);
 64266:     pcVar2 = "-setting 4D to 0x%x -\n";
 64267:   }
 64268:   else {
 64269:     if (param_3 == 1) {
 64270:       uVar5 = (ulonglong)bVar4;
```

### R0148 - `FUN_140043370` line 64282
- Register: `0x4E `
- Kind: `direct_read`
```c
 64277:       FUN_140044528(param_1,0x50,bVar6);
 64278:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64279:         DbgPrint("--------------setting Channel G DFE--------------\n");
 64280:       }
 64281:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64282:         FUN_140044484(param_1,0x4e);
 64283:         DbgPrint("-setting 4E to 0x%x -\n");
 64284:       }
 64285:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64286:         FUN_140044484(param_1,0x4f);
 64287:         DbgPrint("-setting 4F to 0x%x -\n");
```

### R0149 - `FUN_140043370` line 64286
- Register: `0x4F `
- Kind: `direct_read`
```c
 64281:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64282:         FUN_140044484(param_1,0x4e);
 64283:         DbgPrint("-setting 4E to 0x%x -\n");
 64284:       }
 64285:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64286:         FUN_140044484(param_1,0x4f);
 64287:         DbgPrint("-setting 4F to 0x%x -\n");
 64288:       }
 64289:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64290:         FUN_140044484(param_1,0x50);
 64291:         DbgPrint("-setting 50 to 0x%x -\n");
```

### R0150 - `FUN_140043370` line 64290
- Register: `0x50 `
- Kind: `direct_read`
```c
 64285:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64286:         FUN_140044484(param_1,0x4f);
 64287:         DbgPrint("-setting 4F to 0x%x -\n");
 64288:       }
 64289:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64290:         FUN_140044484(param_1,0x50);
 64291:         DbgPrint("-setting 50 to 0x%x -\n");
 64292:       }
 64293:       if ((DAT_1400a04f8 & 0x10) != 0) {
 64294:         DbgPrint("-------------------------------------------------\n");
 64295:       }
```

### R0151 - `FUN_140043370` line 64309
- Register: `0x51 `
- Kind: `direct_read`
```c
 64304:     FUN_140044528(param_1,0x53,bVar6);
 64305:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64306:       DbgPrint("--------------setting Channel R DFE--------------\n");
 64307:     }
 64308:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64309:       FUN_140044484(param_1,0x51);
 64310:       DbgPrint("-setting 51 to 0x%x -\n");
 64311:     }
 64312:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64313:       FUN_140044484(param_1,0x52);
 64314:       DbgPrint("-setting 52 to 0x%x -\n");
```

### R0152 - `FUN_140043370` line 64313
- Register: `0x52 `
- Kind: `direct_read`
```c
 64308:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64309:       FUN_140044484(param_1,0x51);
 64310:       DbgPrint("-setting 51 to 0x%x -\n");
 64311:     }
 64312:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64313:       FUN_140044484(param_1,0x52);
 64314:       DbgPrint("-setting 52 to 0x%x -\n");
 64315:     }
 64316:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140043646;
 64317:     FUN_140044484(param_1,0x53);
 64318:     pcVar2 = "-setting 53 to 0x%x -\n";
```

### R0153 - `FUN_140043370` line 64317
- Register: `0x53 `
- Kind: `direct_read`
```c
 64312:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64313:       FUN_140044484(param_1,0x52);
 64314:       DbgPrint("-setting 52 to 0x%x -\n");
 64315:     }
 64316:     if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_140043646;
 64317:     FUN_140044484(param_1,0x53);
 64318:     pcVar2 = "-setting 53 to 0x%x -\n";
 64319:   }
 64320:   DbgPrint(pcVar2);
 64321: LAB_140043646:
 64322:   thunk_FUN_1400444d0(param_1,0,bVar6,param_4);
```

### R0154 - `FUN_140043660` line 64356
- Register: `0x63 `
- Kind: `direct_read`
```c
 64351: LAB_1400436ab:
 64352:   bVar9 = 0;
 64353:   psVar8 = (short *)(param_1 + 0x17e);
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
 64356:     bVar2 = FUN_140044484(param_1,99);
 64357:     bVar3 = FUN_140044484(param_1,100);
 64358:     bVar4 = FUN_140044484(param_1,0x6d);
 64359:     bVar5 = FUN_140044484(param_1,0x6e);
 64360:     sVar7 = (ushort)bVar5 * 0x100 + (ushort)bVar4;
 64361:     sVar6 = (ushort)bVar3 * 0x100 + (ushort)bVar2;
```

### R0155 - `FUN_140043660` line 64357
- Register: `0x64 `
- Kind: `direct_read`
```c
 64352:   bVar9 = 0;
 64353:   psVar8 = (short *)(param_1 + 0x17e);
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
 64356:     bVar2 = FUN_140044484(param_1,99);
 64357:     bVar3 = FUN_140044484(param_1,100);
 64358:     bVar4 = FUN_140044484(param_1,0x6d);
 64359:     bVar5 = FUN_140044484(param_1,0x6e);
 64360:     sVar7 = (ushort)bVar5 * 0x100 + (ushort)bVar4;
 64361:     sVar6 = (ushort)bVar3 * 0x100 + (ushort)bVar2;
 64362:     *psVar8 = sVar7;
```

### R0156 - `FUN_140043660` line 64358
- Register: `0x6D `
- Kind: `direct_read`
```c
 64353:   psVar8 = (short *)(param_1 + 0x17e);
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
 64356:     bVar2 = FUN_140044484(param_1,99);
 64357:     bVar3 = FUN_140044484(param_1,100);
 64358:     bVar4 = FUN_140044484(param_1,0x6d);
 64359:     bVar5 = FUN_140044484(param_1,0x6e);
 64360:     sVar7 = (ushort)bVar5 * 0x100 + (ushort)bVar4;
 64361:     sVar6 = (ushort)bVar3 * 0x100 + (ushort)bVar2;
 64362:     *psVar8 = sVar7;
 64363:     psVar8[3] = sVar6;
```

### R0157 - `FUN_140043660` line 64359
- Register: `0x6E `
- Kind: `direct_read`
```c
 64354:   do {
 64355:     FUN_1400444d0(param_1,0x37,0xc0,bVar9 << 6);
 64356:     bVar2 = FUN_140044484(param_1,99);
 64357:     bVar3 = FUN_140044484(param_1,100);
 64358:     bVar4 = FUN_140044484(param_1,0x6d);
 64359:     bVar5 = FUN_140044484(param_1,0x6e);
 64360:     sVar7 = (ushort)bVar5 * 0x100 + (ushort)bVar4;
 64361:     sVar6 = (ushort)bVar3 * 0x100 + (ushort)bVar2;
 64362:     *psVar8 = sVar7;
 64363:     psVar8[3] = sVar6;
 64364:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0158 - `FUN_140043810` line 64416
- Register: `0x14 `
- Kind: `direct_read`
```c
 64411:       param_4 = 2;
 64412:       param_3 = 2;
 64413:       FUN_1400444d0(param_1,0x23,2,2);
 64414:     }
 64415:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64416:       uVar1 = FUN_140044484(param_1,0x14);
 64417:       DbgPrint("******hdmirxrd(0x14) = 0x%X ******\n",uVar1);
 64418:     }
 64419:     bVar2 = FUN_140044484(param_1,0x14);
 64420:     bVar2 = bVar2 & 1;
 64421:   }
```

### R0159 - `FUN_140043810` line 64419
- Register: `0x14 `
- Kind: `direct_read`
```c
 64414:     }
 64415:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64416:       uVar1 = FUN_140044484(param_1,0x14);
 64417:       DbgPrint("******hdmirxrd(0x14) = 0x%X ******\n",uVar1);
 64418:     }
 64419:     bVar2 = FUN_140044484(param_1,0x14);
 64420:     bVar2 = bVar2 & 1;
 64421:   }
 64422:   else if (*(char *)(param_1 + 0x1ec) == '\x01') {
 64423:     FUN_140044528(param_1,0xc,0xff);
 64424:     FUN_140044528(param_1,0x2b,0xb0);
```

### R0160 - `FUN_140043810` line 64434
- Register: `0x17 `
- Kind: `direct_read`
```c
 64429:       param_4 = 2;
 64430:       param_3 = 2;
 64431:       FUN_1400444d0(param_1,0x2b,2,2);
 64432:     }
 64433:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64434:       uVar1 = FUN_140044484(param_1,0x17);
 64435:       DbgPrint("******hdmirxrd(0x17) = 0x%X ******\n",uVar1);
 64436:     }
 64437:     bVar3 = FUN_140044484(param_1,0x17);
 64438:     bVar2 = 0;
 64439:     if ((bVar3 & 1) != 0) {
```

### R0161 - `FUN_140043810` line 64437
- Register: `0x17 `
- Kind: `direct_read`
```c
 64432:     }
 64433:     if ((DAT_1400a04f8 & 0x10) != 0) {
 64434:       uVar1 = FUN_140044484(param_1,0x17);
 64435:       DbgPrint("******hdmirxrd(0x17) = 0x%X ******\n",uVar1);
 64436:     }
 64437:     bVar3 = FUN_140044484(param_1,0x17);
 64438:     bVar2 = 0;
 64439:     if ((bVar3 & 1) != 0) {
 64440:       bVar2 = 1;
 64441:     }
 64442:   }
```

### R0162 - `FUN_140043f14` line 64681
- Register: `0x07 `
- Kind: `direct_read`
```c
 64676:   undefined7 uVar5;
 64677:   byte bVar6;
 64678: 
 64679:   uVar5 = (undefined7)((ulonglong)param_3 >> 8);
 64680:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 64681:   bVar1 = FUN_140044484(param_1,7);
 64682:   bVar4 = bVar1 & 0xf7;
 64683:   FUN_140044528(param_1,7,bVar4);
 64684:   if (bVar1 == 0) {
 64685:     return;
 64686:   }
```

### R0163 - `FUN_14004419c` line 64791
- Register: `0x0C `
- Kind: `direct_read`
```c
 64786: 
 64787: {
 64788:   byte bVar1;
 64789: 
 64790:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 64791:   bVar1 = FUN_140044484(param_1,0xc);
 64792:   FUN_140044528(param_1,0xc,bVar1);
 64793:   if (bVar1 != 0) {
 64794:     if (((bVar1 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 64795:       DbgPrint("# Port 1 CH0 Lag Err#\n");
 64796:     }
```

### R0164 - `FUN_1400445c0` line 65075
- Register: `0x9E `
- Kind: `direct_read`
```c
 65070:   bVar8 = 0;
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65078:     bVar1 = FUN_140044484(param_1,0xa5);
 65079:     bVar2 = FUN_140044484(param_1,0xa4);
 65080:     *(ushort *)(param_1 + 0x22e) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
```

### R0165 - `FUN_1400445c0` line 65076
- Register: `0x9D `
- Kind: `direct_read`
```c
 65071:   bVar7 = 2;
 65072:   FUN_1400444d0(param_1,100,2,0);
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65078:     bVar1 = FUN_140044484(param_1,0xa5);
 65079:     bVar2 = FUN_140044484(param_1,0xa4);
 65080:     *(ushort *)(param_1 + 0x22e) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65081:     uVar3 = FUN_14003cb50(param_1);
```

### R0166 - `FUN_1400445c0` line 65078
- Register: `0xA5 `
- Kind: `direct_read`
```c
 65073:   if ((*(char *)(param_1 + 0x1d5) == '\x02') || (*(char *)(param_1 + 0x1d5) == '\x03')) {
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65078:     bVar1 = FUN_140044484(param_1,0xa5);
 65079:     bVar2 = FUN_140044484(param_1,0xa4);
 65080:     *(ushort *)(param_1 + 0x22e) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65081:     uVar3 = FUN_14003cb50(param_1);
 65082:     if ((((char)uVar3 != '\0') ||
 65083:         ((2999 < *(ushort *)(param_1 + 0x22c) && (1999 < *(ushort *)(param_1 + 0x22e))))) &&
```

### R0167 - `FUN_1400445c0` line 65079
- Register: `0xA4 `
- Kind: `direct_read`
```c
 65074:     thunk_FUN_1400444d0(param_1,0,bVar7,bVar8);
 65075:     bVar1 = FUN_140044484(param_1,0x9e);
 65076:     bVar2 = FUN_140044484(param_1,0x9d);
 65077:     *(ushort *)(param_1 + 0x22c) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65078:     bVar1 = FUN_140044484(param_1,0xa5);
 65079:     bVar2 = FUN_140044484(param_1,0xa4);
 65080:     *(ushort *)(param_1 + 0x22e) = (ushort)bVar2 + (ushort)(bVar1 & 0x3f) * 0x100;
 65081:     uVar3 = FUN_14003cb50(param_1);
 65082:     if ((((char)uVar3 != '\0') ||
 65083:         ((2999 < *(ushort *)(param_1 + 0x22c) && (1999 < *(ushort *)(param_1 + 0x22e))))) &&
 65084:        (uVar4 = FUN_14003cf10(param_1), (char)uVar4 != '\0')) {
```

### R0168 - `FUN_140044a88` line 65164
- Register: `0x11 `
- Kind: `direct_read`
```c
 65159:   byte bVar4;
 65160:   ulonglong uVar5;
 65161:   byte *pbVar6;
 65162: 
 65163:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65164:   bVar1 = FUN_140044484(param_1,0x11);
 65165:   bVar1 = bVar1 & 0x40;
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
 65169:     thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
```

### R0169 - `FUN_140044a88` line 65170
- Register: `0x24 `
- Kind: `direct_read`
```c
 65165:   bVar1 = bVar1 & 0x40;
 65166:   uVar5 = (ulonglong)bVar1;
 65167:   FUN_140044528(param_1,0x11,bVar1);
 65168:   if (*(int *)(param_1 + 0x1dc) == 4) {
 65169:     thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65170:     cVar2 = FUN_140044484(param_1,0x24);
 65171:     thunk_FUN_1400444d0(param_1,0,(byte)uVar5,param_4);
 65172:     bVar4 = 0;
 65173:     if ((bVar1 == 0) && (cVar2 != '\0')) {
 65174:       if (*(char *)(param_1 + 0x1ac) == '\0') {
 65175:         *(char *)(param_1 + 0x1a6) = *(char *)(param_1 + 0x1a6) + '\x01';
```

### R0170 - `FUN_140044a88` line 65195
- Register: `0x24 `
- Kind: `direct_read`
```c
 65190:           *(undefined1 *)(param_1 + 0x1a6) = 0;
 65191:           *(undefined1 *)(param_1 + 0x1ac) = 1;
 65192:         }
 65193:       }
 65194:       thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65195:       uVar3 = FUN_140044484(param_1,0x24);
 65196:       *(undefined1 *)(param_1 + 0x1ae) = uVar3;
 65197:       uVar3 = FUN_140044484(param_1,0x25);
 65198:       *(undefined1 *)(param_1 + 0x1af) = uVar3;
 65199:       uVar3 = FUN_140044484(param_1,0x26);
 65200:       *(undefined1 *)(param_1 + 0x1b0) = uVar3;
```

### R0171 - `FUN_140044a88` line 65197
- Register: `0x25 `
- Kind: `direct_read`
```c
 65192:         }
 65193:       }
 65194:       thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65195:       uVar3 = FUN_140044484(param_1,0x24);
 65196:       *(undefined1 *)(param_1 + 0x1ae) = uVar3;
 65197:       uVar3 = FUN_140044484(param_1,0x25);
 65198:       *(undefined1 *)(param_1 + 0x1af) = uVar3;
 65199:       uVar3 = FUN_140044484(param_1,0x26);
 65200:       *(undefined1 *)(param_1 + 0x1b0) = uVar3;
 65201:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65202:         DbgPrint("DRM PKT RECEIVE !!\n");
```

### R0172 - `FUN_140044a88` line 65199
- Register: `0x26 `
- Kind: `direct_read`
```c
 65194:       thunk_FUN_1400444d0(param_1,2,(byte)uVar5,param_4);
 65195:       uVar3 = FUN_140044484(param_1,0x24);
 65196:       *(undefined1 *)(param_1 + 0x1ae) = uVar3;
 65197:       uVar3 = FUN_140044484(param_1,0x25);
 65198:       *(undefined1 *)(param_1 + 0x1af) = uVar3;
 65199:       uVar3 = FUN_140044484(param_1,0x26);
 65200:       *(undefined1 *)(param_1 + 0x1b0) = uVar3;
 65201:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65202:         DbgPrint("DRM PKT RECEIVE !!\n");
 65203:       }
 65204:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0173 - `FUN_140044cf8` line 65269
- Register: `0xBE `
- Kind: `direct_read`
```c
 65264:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65265:   bVar11 = 1;
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
```

### R0174 - `FUN_140044cf8` line 65270
- Register: `0xBF `
- Kind: `direct_read`
```c
 65265:   bVar11 = 1;
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
 65275:   bVar4 = FUN_140044484(param_1,0xc1);
```

### R0175 - `FUN_140044cf8` line 65271
- Register: `0xC0 `
- Kind: `direct_read`
```c
 65266:   bVar9 = 1;
 65267:   FUN_1400444d0(param_1,0x86,1,1);
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
 65275:   bVar4 = FUN_140044484(param_1,0xc1);
 65276:   iVar7 = (uint)(bVar3 >> 4) + (uint)bVar4 * 0x1000;
```

### R0176 - `FUN_140044cf8` line 65273
- Register: `0xC0 `
- Kind: `direct_read`
```c
 65268:   thunk_FUN_1400444d0(param_1,2,bVar9,bVar11);
 65269:   bVar3 = FUN_140044484(param_1,0xbe);
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
 65275:   bVar4 = FUN_140044484(param_1,0xc1);
 65276:   iVar7 = (uint)(bVar3 >> 4) + (uint)bVar4 * 0x1000;
 65277:   *(int *)(param_1 + 600) = iVar7;
 65278:   bVar3 = FUN_140044484(param_1,0xc2);
```

### R0177 - `FUN_140044cf8` line 65275
- Register: `0xC1 `
- Kind: `direct_read`
```c
 65270:   bVar4 = FUN_140044484(param_1,0xbf);
 65271:   bVar5 = FUN_140044484(param_1,0xc0);
 65272:   *(uint *)(param_1 + 0x254) = (bVar5 & 0xf) + ((uint)bVar4 + (uint)bVar3 * 0x100) * 0x10;
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
 65275:   bVar4 = FUN_140044484(param_1,0xc1);
 65276:   iVar7 = (uint)(bVar3 >> 4) + (uint)bVar4 * 0x1000;
 65277:   *(int *)(param_1 + 600) = iVar7;
 65278:   bVar3 = FUN_140044484(param_1,0xc2);
 65279:   *(uint *)(param_1 + 600) = (uint)bVar3 * 0x10 + iVar7;
 65280:   FUN_14003d24c(param_1,10);
```

### R0178 - `FUN_140044cf8` line 65278
- Register: `0xC2 `
- Kind: `direct_read`
```c
 65273:   bVar3 = FUN_140044484(param_1,0xc0);
 65274:   *(uint *)(param_1 + 600) = (uint)(bVar3 >> 4);
 65275:   bVar4 = FUN_140044484(param_1,0xc1);
 65276:   iVar7 = (uint)(bVar3 >> 4) + (uint)bVar4 * 0x1000;
 65277:   *(int *)(param_1 + 600) = iVar7;
 65278:   bVar3 = FUN_140044484(param_1,0xc2);
 65279:   *(uint *)(param_1 + 600) = (uint)bVar3 * 0x10 + iVar7;
 65280:   FUN_14003d24c(param_1,10);
 65281:   if ((DAT_1400a04f8 & 0x10) != 0) {
 65282:     DbgPrint("[xAud] 68051 N = %lu \n");
 65283:   }
```

### R0179 - `FUN_140044cf8` line 65353
- Register: `0xB5 `
- Kind: `direct_read`
```c
 65348:       }
 65349:     }
 65350:     bVar5 = (byte)sVar8;
 65351:     *(short *)(param_1 + 0x25e) = sVar8;
 65352:     thunk_FUN_1400444d0(param_1,0,bVar9,bVar11);
 65353:     bVar3 = FUN_140044484(param_1,0xb5);
 65354:     bVar4 = FUN_140044484(param_1,0xb6);
 65355:     bVar3 = bVar4 >> 2 & 0x30 | bVar3 & 0xf;
 65356:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65357:       DbgPrint("[xAud] 68051 Audio_CH_Status[24:27 - 30:31][bit0~bit5] = %x ,iTE6805_Enable_Audio_Output\n"
 65358:                ,bVar3);
```

### R0180 - `FUN_140044cf8` line 65354
- Register: `0xB6 `
- Kind: `direct_read`
```c
 65349:     }
 65350:     bVar5 = (byte)sVar8;
 65351:     *(short *)(param_1 + 0x25e) = sVar8;
 65352:     thunk_FUN_1400444d0(param_1,0,bVar9,bVar11);
 65353:     bVar3 = FUN_140044484(param_1,0xb5);
 65354:     bVar4 = FUN_140044484(param_1,0xb6);
 65355:     bVar3 = bVar4 >> 2 & 0x30 | bVar3 & 0xf;
 65356:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65357:       DbgPrint("[xAud] 68051 Audio_CH_Status[24:27 - 30:31][bit0~bit5] = %x ,iTE6805_Enable_Audio_Output\n"
 65358:                ,bVar3);
 65359:     }
```

### R0181 - `FUN_140044cf8` line 65410
- Register: `0x81 `
- Kind: `direct_read`
```c
 65405:       if ((DAT_1400a04f8 & 0x10) != 0) {
 65406:         DbgPrint(
 65407:                 "[xAud] 68051 Audio_CH_Status == Force_Sampling_Frequency reset Audio, iTE6805_Enable_Audio_Output \n"
 65408:                 );
 65409:       }
 65410:       bVar3 = FUN_140044484(param_1,0x81);
 65411:       FUN_1400444d0(param_1,0x81,0x40,0);
 65412:       if ((bVar3 & 0x40) != 0) {
 65413:         FUN_14003ea04(param_1);
 65414:       }
 65415:       *(undefined1 *)(param_1 + 0x194) = 0;
```

### R0182 - `FUN_140045168` line 65479
- Register: `0x48 `
- Kind: `direct_read`
```c
 65474:     bVar3 = 1;
 65475:     FUN_1400444d0(param_1,0x6b,1,1);
 65476:     if (*(char *)(param_1 + 0x1ec) == '\x01') {
 65477:       thunk_FUN_1400444d0(param_1,4,bVar3,bVar8);
 65478:     }
 65479:     cVar4 = FUN_140044484(param_1,0x48);
 65480:     thunk_FUN_1400444d0(param_1,0,bVar3,bVar8);
 65481:     *(uint *)(param_1 + 0x210) = (cVar4 < '4') + 1;
 65482:   }
 65483:   uVar5 = 1;
 65484:   if (*(char *)(param_1 + 0x200) == '\x01') {
```

### R0183 - `FUN_140045654` line 65666
- Register: `0x13 `
- Kind: `direct_read`
```c
 65661:   undefined1 *puVar2;
 65662:   byte bVar3;
 65663: 
 65664:   bVar3 = 2;
 65665:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65666:   uVar1 = FUN_140044484(param_1,0x13);
 65667:   *param_2 = uVar1;
 65668:   uVar1 = FUN_140044484(param_1,0x12);
 65669:   param_2[1] = uVar1;
 65670:   puVar2 = param_2 + 2;
 65671:   do {
```

### R0184 - `FUN_140045654` line 65668
- Register: `0x12 `
- Kind: `direct_read`
```c
 65663: 
 65664:   bVar3 = 2;
 65665:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65666:   uVar1 = FUN_140044484(param_1,0x13);
 65667:   *param_2 = uVar1;
 65668:   uVar1 = FUN_140044484(param_1,0x12);
 65669:   param_2[1] = uVar1;
 65670:   puVar2 = param_2 + 2;
 65671:   do {
 65672:     uVar1 = FUN_140044484(param_1,bVar3 + 0x12);
 65673:     *puVar2 = uVar1;
```

### R0185 - `FUN_1400456c4` line 65691
- Register: `0x43 `
- Kind: `direct_read`
```c
 65686:   undefined1 uVar1;
 65687:   byte bVar2;
 65688:   undefined1 *puVar3;
 65689: 
 65690:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65691:   uVar1 = FUN_140044484(param_1,0x43);
 65692:   *param_2 = uVar1;
 65693:   uVar1 = FUN_140044484(param_1,0x4a);
 65694:   param_2[1] = uVar1;
 65695:   puVar3 = param_2 + 2;
 65696:   bVar2 = 0;
```

### R0186 - `FUN_1400456c4` line 65693
- Register: `0x4A `
- Kind: `direct_read`
```c
 65688:   undefined1 *puVar3;
 65689: 
 65690:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65691:   uVar1 = FUN_140044484(param_1,0x43);
 65692:   *param_2 = uVar1;
 65693:   uVar1 = FUN_140044484(param_1,0x4a);
 65694:   param_2[1] = uVar1;
 65695:   puVar3 = param_2 + 2;
 65696:   bVar2 = 0;
 65697:   do {
 65698:     uVar1 = FUN_140044484(param_1,bVar2 + 0x44);
```

### R0187 - `FUN_140045788` line 65738
- Register: `0x10 `
- Kind: `direct_read`
```c
 65733: 
 65734: {
 65735:   undefined1 uVar1;
 65736: 
 65737:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65738:   uVar1 = FUN_140044484(param_1,0x10);
 65739:   *param_2 = uVar1;
 65740:   uVar1 = FUN_140044484(param_1,0x11);
 65741:   param_2[1] = uVar1;
 65742:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65743:   return;
```

### R0188 - `FUN_140045788` line 65740
- Register: `0x11 `
- Kind: `direct_read`
```c
 65735:   undefined1 uVar1;
 65736: 
 65737:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65738:   uVar1 = FUN_140044484(param_1,0x10);
 65739:   *param_2 = uVar1;
 65740:   uVar1 = FUN_140044484(param_1,0x11);
 65741:   param_2[1] = uVar1;
 65742:   thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 65743:   return;
 65744: }
 65745: 
```

### R0189 - `FUN_1400457d0` line 65757
- Register: `0xDA `
- Kind: `direct_read`
```c
 65752:   undefined1 *puVar2;
 65753:   byte bVar3;
 65754: 
 65755:   bVar3 = 2;
 65756:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65757:   uVar1 = FUN_140044484(param_1,0xda);
 65758:   *param_2 = uVar1;
 65759:   uVar1 = FUN_140044484(param_1,0xdb);
 65760:   param_2[1] = uVar1;
 65761:   puVar2 = param_2 + 2;
 65762:   do {
```

### R0190 - `FUN_1400457d0` line 65759
- Register: `0xDB `
- Kind: `direct_read`
```c
 65754: 
 65755:   bVar3 = 2;
 65756:   thunk_FUN_1400444d0(param_1,2,param_3,param_4);
 65757:   uVar1 = FUN_140044484(param_1,0xda);
 65758:   *param_2 = uVar1;
 65759:   uVar1 = FUN_140044484(param_1,0xdb);
 65760:   param_2[1] = uVar1;
 65761:   puVar2 = param_2 + 2;
 65762:   do {
 65763:     uVar1 = FUN_140044484(param_1,bVar3 - 0x26);
 65764:     *puVar2 = uVar1;
```

### R0191 - `FUN_140045bfc` line 65964
- Register: `0x00 `
- Kind: `direct_read`
```c
 65959:   char cVar2;
 65960:   char cVar3;
 65961:   char cVar4;
 65962:   ulonglong uVar5;
 65963: 
 65964:   cVar1 = FUN_140044484(param_1,0);
 65965:   cVar2 = FUN_140044484(param_1,1);
 65966:   cVar3 = FUN_140044484(param_1,2);
 65967:   cVar4 = FUN_140044484(param_1,3);
 65968:   if ((((cVar1 == 'T') && (cVar2 == 'I')) && ((cVar3 - 5U & 0xfd) == 0)) && (cVar4 == 'h')) {
 65969:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0192 - `FUN_140045bfc` line 65965
- Register: `0x01 `
- Kind: `direct_read`
```c
 65960:   char cVar3;
 65961:   char cVar4;
 65962:   ulonglong uVar5;
 65963: 
 65964:   cVar1 = FUN_140044484(param_1,0);
 65965:   cVar2 = FUN_140044484(param_1,1);
 65966:   cVar3 = FUN_140044484(param_1,2);
 65967:   cVar4 = FUN_140044484(param_1,3);
 65968:   if ((((cVar1 == 'T') && (cVar2 == 'I')) && ((cVar3 - 5U & 0xfd) == 0)) && (cVar4 == 'h')) {
 65969:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65970:       DbgPrint("REG00 = %X !!!\n");
```

### R0193 - `FUN_140045bfc` line 65966
- Register: `0x02 `
- Kind: `direct_read`
```c
 65961:   char cVar4;
 65962:   ulonglong uVar5;
 65963: 
 65964:   cVar1 = FUN_140044484(param_1,0);
 65965:   cVar2 = FUN_140044484(param_1,1);
 65966:   cVar3 = FUN_140044484(param_1,2);
 65967:   cVar4 = FUN_140044484(param_1,3);
 65968:   if ((((cVar1 == 'T') && (cVar2 == 'I')) && ((cVar3 - 5U & 0xfd) == 0)) && (cVar4 == 'h')) {
 65969:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65970:       DbgPrint("REG00 = %X !!!\n");
 65971:     }
```

### R0194 - `FUN_140045bfc` line 65967
- Register: `0x03 `
- Kind: `direct_read`
```c
 65962:   ulonglong uVar5;
 65963: 
 65964:   cVar1 = FUN_140044484(param_1,0);
 65965:   cVar2 = FUN_140044484(param_1,1);
 65966:   cVar3 = FUN_140044484(param_1,2);
 65967:   cVar4 = FUN_140044484(param_1,3);
 65968:   if ((((cVar1 == 'T') && (cVar2 == 'I')) && ((cVar3 - 5U & 0xfd) == 0)) && (cVar4 == 'h')) {
 65969:     if ((DAT_1400a04f8 & 0x10) != 0) {
 65970:       DbgPrint("REG00 = %X !!!\n");
 65971:     }
 65972:     if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0195 - `FUN_140046400` line 66355
- Register: `0xB9 `
- Kind: `direct_read`
```c
 66350:   }
 66351:   if (iVar1 == 2) {
 66352:     if (*(char *)(param_1 + 0x18e) == '\0') {
 66353:       cVar7 = '\x04';
 66354:       thunk_FUN_1400444d0(param_1,-(*(char *)(param_1 + 0x1ec) != '\0') & 4,param_3,param_4);
 66355:       bVar2 = FUN_140044484(param_1,0xb9);
 66356:       thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66357:       if ((bVar2 & 0xc0) != 0) {
 66358:         FUN_14003ea04(param_1);
 66359:       }
 66360:       bVar2 = FUN_14003ce9c(param_1);
```

### R0196 - `FUN_140046400` line 66392
- Register: `0xB0 `
- Kind: `direct_read`
```c
 66387:   *(char *)(param_1 + 399) = cVar7 + -1;
 66388:   if (cVar7 == '\0') {
 66389:     *(undefined1 *)(param_1 + 399) = 3;
 66390:     thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66391:     if (*(char *)(param_1 + 0x19e) == '\x01') {
 66392:       bVar2 = FUN_140044484(param_1,0xb0);
 66393:       *(byte *)(param_1 + 0x19a) = bVar2 & 0xf0;
 66394:       uVar3 = FUN_140044484(param_1,0xb1);
 66395:       *(undefined1 *)(param_1 + 0x19b) = uVar3;
 66396:       bVar2 = FUN_140044484(param_1,0xb2);
 66397:       *(undefined1 *)(param_1 + 0x19e) = 0;
```

### R0197 - `FUN_140046400` line 66394
- Register: `0xB1 `
- Kind: `direct_read`
```c
 66389:     *(undefined1 *)(param_1 + 399) = 3;
 66390:     thunk_FUN_1400444d0(param_1,0,param_3,param_4);
 66391:     if (*(char *)(param_1 + 0x19e) == '\x01') {
 66392:       bVar2 = FUN_140044484(param_1,0xb0);
 66393:       *(byte *)(param_1 + 0x19a) = bVar2 & 0xf0;
 66394:       uVar3 = FUN_140044484(param_1,0xb1);
 66395:       *(undefined1 *)(param_1 + 0x19b) = uVar3;
 66396:       bVar2 = FUN_140044484(param_1,0xb2);
 66397:       *(undefined1 *)(param_1 + 0x19e) = 0;
 66398:       *(byte *)(param_1 + 0x19c) = bVar2 & 2;
 66399:       if ((bVar2 & 2) == 0) {
```

### R0198 - `FUN_140046400` line 66396
- Register: `0xB2 `
- Kind: `direct_read`
```c
 66391:     if (*(char *)(param_1 + 0x19e) == '\x01') {
 66392:       bVar2 = FUN_140044484(param_1,0xb0);
 66393:       *(byte *)(param_1 + 0x19a) = bVar2 & 0xf0;
 66394:       uVar3 = FUN_140044484(param_1,0xb1);
 66395:       *(undefined1 *)(param_1 + 0x19b) = uVar3;
 66396:       bVar2 = FUN_140044484(param_1,0xb2);
 66397:       *(undefined1 *)(param_1 + 0x19e) = 0;
 66398:       *(byte *)(param_1 + 0x19c) = bVar2 & 2;
 66399:       if ((bVar2 & 2) == 0) {
 66400:         FUN_1400444d0(param_1,0x8c,8,0);
 66401:         if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0199 - `FUN_140046400` line 66420
- Register: `0xBE `
- Kind: `direct_read`
```c
 66415:     else {
 66416:       bVar2 = 1;
 66417:       bVar10 = 1;
 66418:       FUN_1400444d0(param_1,0x86,1,1);
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
```

### R0200 - `FUN_140046400` line 66421
- Register: `0xBF `
- Kind: `direct_read`
```c
 66416:       bVar2 = 1;
 66417:       bVar10 = 1;
 66418:       FUN_1400444d0(param_1,0x86,1,1);
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
```

### R0201 - `FUN_140046400` line 66422
- Register: `0xC0 `
- Kind: `direct_read`
```c
 66417:       bVar10 = 1;
 66418:       FUN_1400444d0(param_1,0x86,1,1);
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
 66427:       uVar13 = (uint)bVar6 * 0x10 + (uint)(bVar4 >> 4) + (uint)bVar5 * 0x1000;
```

### R0202 - `FUN_140046400` line 66424
- Register: `0xC0 `
- Kind: `direct_read`
```c
 66419:       thunk_FUN_1400444d0(param_1,2,bVar10,bVar2);
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
 66427:       uVar13 = (uint)bVar6 * 0x10 + (uint)(bVar4 >> 4) + (uint)bVar5 * 0x1000;
 66428:       thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66429:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0203 - `FUN_140046400` line 66425
- Register: `0xC1 `
- Kind: `direct_read`
```c
 66420:       bVar4 = FUN_140044484(param_1,0xbe);
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
 66427:       uVar13 = (uint)bVar6 * 0x10 + (uint)(bVar4 >> 4) + (uint)bVar5 * 0x1000;
 66428:       thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66429:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66430:         uVar12 = (ulonglong)*(uint *)(param_1 + 600);
```

### R0204 - `FUN_140046400` line 66426
- Register: `0xC2 `
- Kind: `direct_read`
```c
 66421:       bVar5 = FUN_140044484(param_1,0xbf);
 66422:       bVar6 = FUN_140044484(param_1,0xc0);
 66423:       uVar9 = (bVar6 & 0xf) + ((uint)bVar5 + (uint)bVar4 * 0x100) * 0x10;
 66424:       bVar4 = FUN_140044484(param_1,0xc0);
 66425:       bVar5 = FUN_140044484(param_1,0xc1);
 66426:       bVar6 = FUN_140044484(param_1,0xc2);
 66427:       uVar13 = (uint)bVar6 * 0x10 + (uint)(bVar4 >> 4) + (uint)bVar5 * 0x1000;
 66428:       thunk_FUN_1400444d0(param_1,0,bVar10,bVar2);
 66429:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66430:         uVar12 = (ulonglong)*(uint *)(param_1 + 600);
 66431:         uVar11 = (ulonglong)uVar9;
```

### R0205 - `FUN_140046400` line 66437
- Register: `0xB0 `
- Kind: `direct_read`
```c
 66432:         DbgPrint("[xAud-fsm]  68051 N = %lu->%lu,  CTS = %lu->%lu \n",
 66433:                  *(undefined4 *)(param_1 + 0x254),uVar11,uVar12,uVar13);
 66434:         bVar2 = (byte)uVar12;
 66435:         bVar10 = (byte)uVar11;
 66436:       }
 66437:       bVar4 = FUN_140044484(param_1,0xb0);
 66438:       cVar7 = FUN_140044484(param_1,0xb1);
 66439:       bVar5 = FUN_140044484(param_1,0xb2);
 66440:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66441:         bVar2 = *(byte *)(param_1 + 0x19b);
 66442:         bVar10 = bVar4 & 0xf0;
```

### R0206 - `FUN_140046400` line 66438
- Register: `0xB1 `
- Kind: `direct_read`
```c
 66433:                  *(undefined4 *)(param_1 + 0x254),uVar11,uVar12,uVar13);
 66434:         bVar2 = (byte)uVar12;
 66435:         bVar10 = (byte)uVar11;
 66436:       }
 66437:       bVar4 = FUN_140044484(param_1,0xb0);
 66438:       cVar7 = FUN_140044484(param_1,0xb1);
 66439:       bVar5 = FUN_140044484(param_1,0xb2);
 66440:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66441:         bVar2 = *(byte *)(param_1 + 0x19b);
 66442:         bVar10 = bVar4 & 0xf0;
 66443:         DbgPrint(
```

### R0207 - `FUN_140046400` line 66439
- Register: `0xB2 `
- Kind: `direct_read`
```c
 66434:         bVar2 = (byte)uVar12;
 66435:         bVar10 = (byte)uVar11;
 66436:       }
 66437:       bVar4 = FUN_140044484(param_1,0xb0);
 66438:       cVar7 = FUN_140044484(param_1,0xb1);
 66439:       bVar5 = FUN_140044484(param_1,0xb2);
 66440:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66441:         bVar2 = *(byte *)(param_1 + 0x19b);
 66442:         bVar10 = bVar4 & 0xf0;
 66443:         DbgPrint(
 66444:                 "[xAud-fsm]  68051 0xB0=0x%02x->%02x, 0xB1=0x%02x->%02x, 0xB2=-0x%02x->%02x (old->new)\n"
```

### R0208 - `FUN_140046828` line 66511
- Register: `0x09 `
- Kind: `direct_read`
```c
 66506:   uint uVar21;
 66507: 
 66508:   uVar13 = 0;
 66509:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 66510:   uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66511:   bVar2 = FUN_140044484(param_1,9);
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
```

### R0209 - `FUN_140046828` line 66514
- Register: `0x10 `
- Kind: `direct_read`
```c
 66509:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
 66510:   uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66511:   bVar2 = FUN_140044484(param_1,9);
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
```

### R0210 - `FUN_140046828` line 66517
- Register: `0x11 `
- Kind: `direct_read`
```c
 66512:   uVar20 = (undefined7)((ulonglong)param_3 >> 8);
 66513:   FUN_140044528(param_1,9,bVar2 & 4);
 66514:   bVar3 = FUN_140044484(param_1,0x10);
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
```

### R0211 - `FUN_140046828` line 66520
- Register: `0x12 `
- Kind: `direct_read`
```c
 66515:   uVar15 = (ulonglong)bVar3;
 66516:   FUN_140044528(param_1,0x10,bVar3);
 66517:   bVar4 = FUN_140044484(param_1,0x11);
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
```

### R0212 - `FUN_140046828` line 66523
- Register: `0x1D `
- Kind: `direct_read`
```c
 66518:   uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
 66519:   FUN_140044528(param_1,0x11,(char)uVar14);
 66520:   bVar5 = FUN_140044484(param_1,0x12);
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
```

### R0213 - `FUN_140046828` line 66526
- Register: `0xD4 `
- Kind: `direct_read`
```c
 66521:   uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
 66522:   FUN_140044528(param_1,0x12,(char)uVar14);
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
 66531:   FUN_140044528(param_1,0xd5,bVar8);
```

### R0214 - `FUN_140046828` line 66528
- Register: `0xD5 `
- Kind: `direct_read`
```c
 66523:   bVar6 = FUN_140044484(param_1,0x1d);
 66524:   uVar20 = (undefined7)(uVar14 >> 8);
 66525:   FUN_140044528(param_1,0x1d,bVar6);
 66526:   bVar7 = FUN_140044484(param_1,0xd4);
 66527:   FUN_140044528(param_1,0xd4,bVar7);
 66528:   bVar8 = FUN_140044484(param_1,0xd5);
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
 66531:   FUN_140044528(param_1,0xd5,bVar8);
 66532:   if ((bVar2 & 4) != 0) {
 66533:     uVar14 = CONCAT71((int7)(uVar14 >> 8),0xcf);
```

### R0215 - `FUN_140046828` line 66534
- Register: `0xCF `
- Kind: `direct_read`
```c
 66529:   uVar19 = CONCAT71(uVar20,bVar8);
 66530:   uVar14 = CONCAT71(uVar17,0xd5);
 66531:   FUN_140044528(param_1,0xd5,bVar8);
 66532:   if ((bVar2 & 4) != 0) {
 66533:     uVar14 = CONCAT71((int7)(uVar14 >> 8),0xcf);
 66534:     bVar2 = FUN_140044484(param_1,0xcf);
 66535:     if ((bVar2 & 0x20) == 0) {
 66536:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66537:         DbgPrint("HDCP Encryption OFF !\n");
 66538:       }
 66539:       *(undefined2 *)(param_1 + 0x1cf) = 0;
```

### R0216 - `FUN_140046828` line 66548
- Register: `0xD6 `
- Kind: `direct_read`
```c
 66543:         DbgPrint("HDCP Encryption ON! \n");
 66544:       }
 66545:       uVar13 = 0;
 66546:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66547:       uVar14 = CONCAT71((int7)((ulonglong)uVar13 >> 8),0xd6);
 66548:       bVar2 = FUN_140044484(param_1,0xd6);
 66549:       *(bool *)(param_1 + 0x1d0) = (bVar2 & 0xc0) != 0;
 66550:       *(bool *)(param_1 + 0x1cf) = (bVar2 & 0xc0) == 0;
 66551:     }
 66552:   }
 66553:   if (bVar3 != 0) {
```

### R0217 - `FUN_140046828` line 66564
- Register: `0xBE `
- Kind: `direct_read`
```c
 66559:        (*(int *)(param_1 + 0x1e0) == 3)) {
 66560:       param_4 = CONCAT71((int7)(param_4 >> 8),1);
 66561:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66562:       FUN_1400444d0(param_1,0x86,1,1);
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
```

### R0218 - `FUN_140046828` line 66565
- Register: `0xBF `
- Kind: `direct_read`
```c
 66560:       param_4 = CONCAT71((int7)(param_4 >> 8),1);
 66561:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66562:       FUN_1400444d0(param_1,0x86,1,1);
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
```

### R0219 - `FUN_140046828` line 66566
- Register: `0xC0 `
- Kind: `direct_read`
```c
 66561:       uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
 66562:       FUN_1400444d0(param_1,0x86,1,1);
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
 66571:       uVar14 = 0;
```

### R0220 - `FUN_140046828` line 66568
- Register: `0xC0 `
- Kind: `direct_read`
```c
 66563:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
 66571:       uVar14 = 0;
 66572:       uVar18 = (uint)bVar10 * 0x10 + (uint)(bVar2 >> 4) + (uint)bVar9 * 0x1000;
 66573:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
```

### R0221 - `FUN_140046828` line 66569
- Register: `0xC1 `
- Kind: `direct_read`
```c
 66564:       bVar2 = FUN_140044484(param_1,0xbe);
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
 66571:       uVar14 = 0;
 66572:       uVar18 = (uint)bVar10 * 0x10 + (uint)(bVar2 >> 4) + (uint)bVar9 * 0x1000;
 66573:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66574:       if ((DAT_1400a04f8 & 0x10) != 0) {
```

### R0222 - `FUN_140046828` line 66570
- Register: `0xC2 `
- Kind: `direct_read`
```c
 66565:       bVar9 = FUN_140044484(param_1,0xbf);
 66566:       bVar10 = FUN_140044484(param_1,0xc0);
 66567:       uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
 66568:       bVar2 = FUN_140044484(param_1,0xc0);
 66569:       bVar9 = FUN_140044484(param_1,0xc1);
 66570:       bVar10 = FUN_140044484(param_1,0xc2);
 66571:       uVar14 = 0;
 66572:       uVar18 = (uint)bVar10 * 0x10 + (uint)(bVar2 >> 4) + (uint)bVar9 * 0x1000;
 66573:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66574:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66575:         param_4 = (ulonglong)*(uint *)(param_1 + 600);
```

### R0223 - `FUN_140046828` line 66674
- Register: `0x1A `
- Kind: `direct_read`
```c
 66669:   if (bVar5 != 0) {
 66670:     if ((char)bVar5 < '\0') {
 66671:       uVar13 = 0;
 66672:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66673:       uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66674:       bVar2 = FUN_140044484(param_1,0x1a);
 66675:       uVar14 = CONCAT71(uVar17,0x1b);
 66676:       bVar3 = FUN_140044484(param_1,0x1b);
 66677:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66678:         DbgPrint("# Video Parameters Change #\n");
 66679:       }
```

### R0224 - `FUN_140046828` line 66676
- Register: `0x1B `
- Kind: `direct_read`
```c
 66671:       uVar13 = 0;
 66672:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66673:       uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
 66674:       bVar2 = FUN_140044484(param_1,0x1a);
 66675:       uVar14 = CONCAT71(uVar17,0x1b);
 66676:       bVar3 = FUN_140044484(param_1,0x1b);
 66677:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66678:         DbgPrint("# Video Parameters Change #\n");
 66679:       }
 66680:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66681:         uVar19 = (ulonglong)bVar2;
```

### R0225 - `FUN_140046828` line 66715
- Register: `0x14 `
- Kind: `direct_read`
```c
 66710:     if ((bVar5 & 1) != 0) {
 66711:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66712:         DbgPrint("# New AVI InfoFrame Received #\n");
 66713:       }
 66714:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66715:       bVar2 = FUN_140044484(param_1,0x14);
 66716:       uVar15 = (ulonglong)bVar2;
 66717:       bVar3 = FUN_140044484(param_1,0x15);
 66718:       uVar14 = 0;
 66719:       uVar16 = (ulonglong)bVar3;
 66720:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
```

### R0226 - `FUN_140046828` line 66717
- Register: `0x15 `
- Kind: `direct_read`
```c
 66712:         DbgPrint("# New AVI InfoFrame Received #\n");
 66713:       }
 66714:       thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
 66715:       bVar2 = FUN_140044484(param_1,0x14);
 66716:       uVar15 = (ulonglong)bVar2;
 66717:       bVar3 = FUN_140044484(param_1,0x15);
 66718:       uVar14 = 0;
 66719:       uVar16 = (ulonglong)bVar3;
 66720:       thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
 66721:       if (*(char *)(param_1 + 0x222) == '\x01') {
 66722:         FUN_1400449a0(param_1);
```

### R0227 - `FUN_140046828` line 66809
- Register: `0xD6 `
- Kind: `direct_read`
```c
 66804:                   );
 66805:         }
 66806:         FUN_140041b38(param_1,1,uVar19,param_4);
 66807:       }
 66808:     }
 66809:     bVar3 = FUN_140044484(param_1,0xd6);
 66810:     if ((bVar2 & bVar3) == 0) {
 66811:       if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14004712d;
 66812:       pcVar12 = "HDCP Encryption OFF !\n";
 66813:     }
 66814:     else {
```

### R0228 - `FUN_140047318` line 66902
- Register: `0x05 `
- Kind: `direct_read`
```c
 66897:   uVar16 = (undefined7)((ulonglong)param_4 >> 8);
 66898:   uVar14 = (undefined7)((ulonglong)param_3 >> 8);
 66899:   uVar11 = 0;
 66900:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15);
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
```

### R0229 - `FUN_140047318` line 66904
- Register: `0x06 `
- Kind: `direct_read`
```c
 66899:   uVar11 = 0;
 66900:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar15);
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
```

### R0230 - `FUN_140047318` line 66906
- Register: `0x08 `
- Kind: `direct_read`
```c
 66901:   uVar12 = (undefined7)((ulonglong)uVar11 >> 8);
 66902:   bVar1 = FUN_140044484(param_1,5);
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
```

### R0231 - `FUN_140047318` line 66908
- Register: `0x09 `
- Kind: `direct_read`
```c
 66903:   FUN_140044528(param_1,5,bVar1);
 66904:   bVar2 = FUN_140044484(param_1,6);
 66905:   FUN_140044528(param_1,6,bVar2);
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
 66913:   uVar11 = CONCAT71(uVar12,0x15);
```

### R0232 - `FUN_140047318` line 66911
- Register: `0x13 `
- Kind: `direct_read`
```c
 66906:   bVar3 = FUN_140044484(param_1,8);
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
 66913:   uVar11 = CONCAT71(uVar12,0x15);
 66914:   bVar7 = FUN_140044484(param_1,0x15);
 66915:   if (bVar1 != 0) {
 66916:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
```

### R0233 - `FUN_140047318` line 66912
- Register: `0x14 `
- Kind: `direct_read`
```c
 66907:   FUN_140044528(param_1,8,bVar3);
 66908:   bVar4 = FUN_140044484(param_1,9);
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
 66913:   uVar11 = CONCAT71(uVar12,0x15);
 66914:   bVar7 = FUN_140044484(param_1,0x15);
 66915:   if (bVar1 != 0) {
 66916:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
 66917:       uVar11 = 0;
```

### R0234 - `FUN_140047318` line 66914
- Register: `0x15 `
- Kind: `direct_read`
```c
 66909:   bVar13 = bVar4 & 0xfb;
 66910:   FUN_140044528(param_1,9,bVar13);
 66911:   cVar5 = FUN_140044484(param_1,0x13);
 66912:   bVar6 = FUN_140044484(param_1,0x14);
 66913:   uVar11 = CONCAT71(uVar12,0x15);
 66914:   bVar7 = FUN_140044484(param_1,0x15);
 66915:   if (bVar1 != 0) {
 66916:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
 66917:       uVar11 = 0;
 66918:       FUN_140045a28(param_1,0,bVar13,bVar15);
 66919:     }
```

### R0235 - `FUN_140047318` line 66953
- Register: `0x13 `
- Kind: `direct_read`
```c
 66948:       if ((DAT_1400a04f8 & 0x10) != 0) {
 66949:         DbgPrint("# Port 0 Input Clock Change Detect #\n");
 66950:       }
 66951:       *(undefined2 *)(param_1 + 0x191) = 0;
 66952:       *(undefined1 *)(param_1 + 0x193) = 0;
 66953:       bVar8 = FUN_140044484(param_1,0x13);
 66954:       if ((bVar8 & 0x10) == 0) {
 66955:         if ((DAT_1400a04f8 & 0x10) != 0) {
 66956:           DbgPrint("# Clock NOT Stable\t#\n");
 66957:         }
 66958:         *(undefined2 *)(param_1 + 0x1cf) = 0;
```

### R0236 - `FUN_140047318` line 67024
- Register: `0x14 `
- Kind: `direct_read`
```c
 67019:     if ((bVar2 & 1) != 0) {
 67020:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67021:         DbgPrint("# Symbol Lock State Change # ");
 67022:       }
 67023:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67024:         FUN_140044484(param_1,0x14);
 67025:         DbgPrint(" 0x14 =  %x \n");
 67026:       }
 67027:       thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67028:       bVar1 = FUN_140044484(param_1,0x14);
 67029:       if (((bVar1 & 0x38) != 0) && (*(int *)(param_1 + 0x1e4) == 0)) {
```

### R0237 - `FUN_140047318` line 67028
- Register: `0x14 `
- Kind: `direct_read`
```c
 67023:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67024:         FUN_140044484(param_1,0x14);
 67025:         DbgPrint(" 0x14 =  %x \n");
 67026:       }
 67027:       thunk_FUN_1400444d0(param_1,0,bVar13,bVar15);
 67028:       bVar1 = FUN_140044484(param_1,0x14);
 67029:       if (((bVar1 & 0x38) != 0) && (*(int *)(param_1 + 0x1e4) == 0)) {
 67030:         FUN_140041b38(param_1,0,CONCAT71(uVar14,bVar13),CONCAT71(uVar16,bVar15));
 67031:         FUN_140041b38(param_1,1,CONCAT71(uVar14,bVar13),CONCAT71(uVar16,bVar15));
 67032:       }
 67033:       if (cVar5 < '\0') {
```

### R0238 - `FUN_140047318` line 67114
- Register: `0x15 `
- Kind: `direct_read`
```c
 67109:   if (((bVar3 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67110:     DbgPrint("# EDID Bus Hange #\n");
 67111:   }
 67112:   if ((char)bVar3 < '\0') {
 67113:     if ((DAT_1400a04f8 & 0x10) != 0) {
 67114:       bVar1 = FUN_140044484(param_1,0x15);
 67115:       DbgPrint("HDMI2 Det State=%x \n",bVar1 >> 2 & 0xf);
 67116:     }
 67117:     FUN_1400444d0(param_1,0x4c,0x80,0x80);
 67118:   }
 67119: LAB_14004796f:
```

### R0239 - `FUN_1400479f8` line 67163
- Register: `0x0A `
- Kind: `direct_read`
```c
 67158:   uVar17 = (undefined7)((ulonglong)param_4 >> 8);
 67159:   uVar15 = (undefined7)((ulonglong)param_3 >> 8);
 67160:   uVar11 = 0;
 67161:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16);
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
```

### R0240 - `FUN_1400479f8` line 67165
- Register: `0x0B `
- Kind: `direct_read`
```c
 67160:   uVar11 = 0;
 67161:   thunk_FUN_1400444d0(param_1,0,(byte)param_3,bVar16);
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
```

### R0241 - `FUN_1400479f8` line 67167
- Register: `0x0D `
- Kind: `direct_read`
```c
 67162:   uVar13 = (undefined7)((ulonglong)uVar11 >> 8);
 67163:   bVar1 = FUN_140044484(param_1,10);
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
```

### R0242 - `FUN_1400479f8` line 67169
- Register: `0x0E `
- Kind: `direct_read`
```c
 67164:   FUN_140044528(param_1,10,bVar1);
 67165:   bVar2 = FUN_140044484(param_1,0xb);
 67166:   FUN_140044528(param_1,0xb,bVar2);
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
 67174:   uVar12 = CONCAT71(uVar13,0x18);
```

### R0243 - `FUN_1400479f8` line 67172
- Register: `0x16 `
- Kind: `direct_read`
```c
 67167:   bVar3 = FUN_140044484(param_1,0xd);
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
 67174:   uVar12 = CONCAT71(uVar13,0x18);
 67175:   bVar7 = FUN_140044484(param_1,0x18);
 67176:   if (bVar1 != 0) {
 67177:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
```

### R0244 - `FUN_1400479f8` line 67173
- Register: `0x17 `
- Kind: `direct_read`
```c
 67168:   FUN_140044528(param_1,0xd,bVar3);
 67169:   bVar4 = FUN_140044484(param_1,0xe);
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
 67174:   uVar12 = CONCAT71(uVar13,0x18);
 67175:   bVar7 = FUN_140044484(param_1,0x18);
 67176:   if (bVar1 != 0) {
 67177:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
 67178:       uVar12 = CONCAT71((int7)(uVar12 >> 8),1);
```

### R0245 - `FUN_1400479f8` line 67175
- Register: `0x18 `
- Kind: `direct_read`
```c
 67170:   bVar14 = bVar4;
 67171:   FUN_140044528(param_1,0xe,bVar4);
 67172:   cVar5 = FUN_140044484(param_1,0x16);
 67173:   bVar6 = FUN_140044484(param_1,0x17);
 67174:   uVar12 = CONCAT71(uVar13,0x18);
 67175:   bVar7 = FUN_140044484(param_1,0x18);
 67176:   if (bVar1 != 0) {
 67177:     if (((bVar1 & 0x10) != 0) && (bVar8 = FUN_14003cd90(param_1), bVar8 != 0)) {
 67178:       uVar12 = CONCAT71((int7)(uVar12 >> 8),1);
 67179:       FUN_140045a28(param_1,uVar12,bVar14,bVar16);
 67180:     }
```

### R0246 - `FUN_1400479f8` line 67214
- Register: `0x16 `
- Kind: `direct_read`
```c
 67209:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67210:         DbgPrint("# Port 1 Input Clock Change Detect #\n");
 67211:       }
 67212:       *(undefined2 *)(param_1 + 0x191) = 0;
 67213:       *(undefined1 *)(param_1 + 0x193) = 0;
 67214:       bVar8 = FUN_140044484(param_1,0x16);
 67215:       if ((bVar8 & 0x10) == 0) {
 67216:         if ((DAT_1400a04f8 & 0x10) != 0) {
 67217:           DbgPrint("# Clock NOT Stable\t#\n");
 67218:         }
 67219:         FUN_140041b38(param_1,0,CONCAT71(uVar15,bVar14),CONCAT71(uVar17,bVar16));
```

### R0247 - `FUN_1400479f8` line 67285
- Register: `0x17 `
- Kind: `direct_read`
```c
 67280:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67281:         DbgPrint("# Symbol Lock State Change #\n");
 67282:       }
 67283:       thunk_FUN_1400444d0(param_1,0,bVar14,bVar16);
 67284:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67285:         bVar1 = FUN_140044484(param_1,0x17);
 67286:         DbgPrint(" 0x17 =  %x \n",bVar1 & 0x38);
 67287:       }
 67288:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67289:         DbgPrint(" STATEEQ = %d \n",*(int *)(param_1 + 0x1e4));
 67290:       }
```

### R0248 - `FUN_1400479f8` line 67291
- Register: `0x17 `
- Kind: `direct_read`
```c
 67286:         DbgPrint(" 0x17 =  %x \n",bVar1 & 0x38);
 67287:       }
 67288:       if ((DAT_1400a04f8 & 0x10) != 0) {
 67289:         DbgPrint(" STATEEQ = %d \n",*(int *)(param_1 + 0x1e4));
 67290:       }
 67291:       bVar1 = FUN_140044484(param_1,0x17);
 67292:       if (((bVar1 & 0x38) != 0) && (*(int *)(param_1 + 0x1e4) == 0)) {
 67293:         FUN_140041b38(param_1,0,CONCAT71(uVar15,bVar14),CONCAT71(uVar17,bVar16));
 67294:         FUN_140041b38(param_1,1,CONCAT71(uVar15,bVar14),CONCAT71(uVar17,bVar16));
 67295:       }
 67296:       if (cVar5 < '\0') {
```

### R0249 - `FUN_1400479f8` line 67377
- Register: `0x15 `
- Kind: `direct_read`
```c
 67372:   if (((bVar3 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
 67373:     DbgPrint("# EDID Bus Hange #\n");
 67374:   }
 67375:   if ((char)bVar3 < '\0') {
 67376:     if ((DAT_1400a04f8 & 0x10) != 0) {
 67377:       bVar14 = FUN_140044484(param_1,0x15);
 67378:       DbgPrint("HDMI2 Det State=%x \n",bVar14 >> 2 & 0xf);
 67379:     }
 67380:     FUN_1400444d0(param_1,0x4c,0x80,0x80);
 67381:   }
 67382: LAB_140048057:
```

### R0250 - `FUN_140048478` line 67614
- Register: `0x4F `
- Kind: `direct_read`
```c
 67609:           *(undefined1 *)(param_1 + 0x19d) = 0;
 67610:           *(undefined1 *)(param_1 + 0x1a0) = 1;
 67611:           FUN_14003f774(param_1,'\0');
 67612:         }
 67613:         thunk_FUN_1400444d0(param_1,0,bVar1,bVar5);
 67614:         bVar1 = FUN_140044484(param_1,0x4f);
 67615:         if ((bVar1 & 0x20) != 0) {
 67616:           bVar1 = FUN_14003ccf4(param_1);
 67617:           if (bVar1 == 0) {
 67618:             FUN_14003ecf4(param_1,'\0');
 67619:           }
```

## Appendix C - Interrupt / DPC Function Definitions
Full definitions extracted from `linuxextract.c`.

### `FUN_140036e30` (55868-55879)
```c
ulonglong FUN_140036e30(undefined8 param_1,longlong param_2)

{
  ulonglong in_RAX;
  ulonglong uVar1;
  
  if ((param_2 != 0) && (*(longlong **)(param_2 + 0x18) != (longlong *)0x0)) {
    uVar1 = (**(code **)(**(longlong **)(param_2 + 0x18) + 0x28))();
    return uVar1;
  }
  return in_RAX & 0xffffffffffffff00;
}
```

### `FUN_140019d50` (22414-22504)
```c
ulonglong FUN_140019d50(longlong *param_1)

{
  undefined1 uVar1;
  uint uVar2;
  ulonglong uVar3;
  uint uVar4;
  
  uVar3 = param_1[3];
  uVar2 = *(uint *)(uVar3 + 0x10);
  if ((uVar2 & 0x1fff) == 0) {
    uVar3 = uVar3 & 0xffffffffffffff00;
  }
  else {
    if ((uVar2 & 2) != 0) {
      uVar4 = *(uint *)(param_1[3] + 0x300) & 7;
      uVar3 = (ulonglong)uVar4;
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) V-Desc index %lu occurs, with 0x304: 0x%x",0,uVar3,
                 *(undefined4 *)(param_1[3] + 0x304));
      }
      if (uVar4 - 1 < 4) {
        if ((int)param_1[0x1a] == 0) {
          if ((DAT_1400a04f8 & 1) != 0) {
            DbgPrint("(ch %d) V-Desc index %lu Err DMAState = 0",0,uVar3);
          }
        }
        else {
          *(undefined4 *)(param_1 + 0x126) = *(undefined4 *)(param_1[3] + 0x358);
          *(undefined4 *)(param_1 + 0x127) = *(undefined4 *)(param_1[3] + 0x35c);
          KeInsertQueueDpc(param_1 + 0x59,&DAT_14009f090 + uVar3 * 4,0);
        }
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,2);
    }
    if ((uVar2 & 1) != 0) {
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) video termination",0);
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,1);
      *(undefined4 *)(param_1 + 0xcc) = 1;
    }
    if ((uVar2 & 0x20) != 0) {
      uVar4 = *(uint *)(param_1[3] + 0x14) & 3;
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) A-Desc index %lu complete +",0,uVar4);
      }
      if ((uVar4 == 1 || uVar4 == 2) && (*(int *)((longlong)param_1 + 0xd4) != 0)) {
        KeInsertQueueDpc(param_1 + 0x81,&DAT_14009f090 + (ulonglong)uVar4 * 4,0);
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,0x20);
    }
    if ((uVar2 & 0x10) != 0) {
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) audio termination",0);
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,0x10);
      *(undefined4 *)((longlong)param_1 + 0x664) = 1;
    }
    if ((uVar2 & 0x200) != 0) {
      uVar4 = *(uint *)(param_1[3] + 0x14) >> 2 & 3;
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) AM-Desc index %lu complete +",0,uVar4);
      }
      if ((uVar4 == 1 || uVar4 == 2) && ((int)param_1[0x1b] != 0)) {
        KeInsertQueueDpc(param_1 + 0x99,&DAT_14009f090 + (ulonglong)uVar4 * 4,0);
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,0x200);
    }
    if ((uVar2 & 0x100) != 0) {
      if ((DAT_1400a04f8 & 1) != 0) {
        DbgPrint("(ch %d) audio termination",0);
      }
      uVar3 = (**(code **)(*param_1 + 0x58))(param_1,0x10,0x100);
      *(undefined4 *)(param_1 + 0xcd) = 1;
    }
    if ((uVar2 & 0x800) != 0) {
      uVar1 = *(undefined1 *)(param_1[3] + 0x1a4);
      *(undefined1 *)((longlong)param_1 + 0x679) = uVar1;
      *(undefined1 *)(param_1 + 0xcf) = 1;
      (**(code **)(*param_1 + 0x58))(param_1,0x10);
      uVar3 = KeInsertQueueDpc(param_1 + 0xb3,0,0);
      if (*(char *)((longlong)param_1 + 0x679) == '\0') {
        uVar3 = DbgPrint("*** FATAL ERROR *** : Bus(0) Status(0x%x) slave(0x%x) sub(0x%x)",uVar1,
                         *(undefined4 *)(param_1[3] + 0x184),*(undefined4 *)(param_1[3] + 400));
      }
    }
    uVar3 = CONCAT71((int7)(uVar3 >> 8),1);
  }
  return uVar3;
}
```

### `FUN_1400179d0` (21041-21121)
```c
void FUN_1400179d0(undefined8 param_1,longlong param_2,uint *param_3)

{
  longlong lVar1;
  ushort uVar2;
  undefined8 *puVar3;
  longlong lVar4;
  undefined8 *puVar5;
  longlong lVar6;
  undefined8 uVar7;
  ulonglong uVar8;
  longlong *plVar9;
  
  if ((param_2 != 0) && (param_3 != (uint *)0x0)) {
    uVar2 = *(ushort *)((longlong)param_3 + 2);
    uVar8 = (ulonglong)uVar2;
    lVar1 = param_2 + 0x448 + uVar8 * 8;
    lVar6 = (ulonglong)(*param_3 & 0xffff) + uVar8 * 2;
    if (*(int *)(param_2 + 0x664 + uVar8 * 4) == 0) {
      if (*(int *)(*(longlong *)(param_2 + 0x10) + 0x88 + uVar8 * 4) == 0) {
        KeAcquireSpinLockAtDpcLevel();
        puVar5 = (undefined8 *)FUN_14005f05c(0xb408,0x41475046);
        if (puVar5 != (undefined8 *)0x0) {
          puVar3 = *(undefined8 **)(param_2 + 1000 + lVar6 * 0x10);
          *(undefined4 *)(puVar5 + 0x1680) = 0xb400;
          *(undefined1 *)((longlong)puVar5 + 0xb404) = 0;
          FUN_140066d80(puVar5,puVar3,0xb400);
          plVar9 = (longlong *)(param_2 + 0x830 + uVar8 * 0x20);
          lVar6 = FUN_14005f05c(0x18,0x41475046);
          if (lVar6 != 0) {
            *(undefined8 **)(lVar6 + 0x10) = puVar5;
            ExInterlockedInsertTailList(plVar9,lVar6,plVar9 + 2);
            *(char *)(plVar9 + 3) = (char)plVar9[3] + '\x01';
          }
          if (((2 < *(byte *)(uVar8 * 0x20 + 0x848 + param_2)) && ((longlong *)*plVar9 != plVar9))
             && (lVar6 = ((longlong *)*plVar9)[2], lVar6 != 0)) {
            uVar7 = ExInterlockedRemoveHeadList(plVar9,plVar9 + 2);
            *(char *)(plVar9 + 3) = (char)plVar9[3] + -1;
            ExFreePoolWithTag(uVar7,0);
            ExFreePoolWithTag(lVar6,0x41475046);
          }
        }
      }
      else {
        KeAcquireSpinLockAtDpcLevel(lVar1);
        puVar5 = (undefined8 *)FUN_14005f05c(0xb408,0x41475046);
        if (puVar5 == (undefined8 *)0x0) {
          return;
        }
        puVar3 = *(undefined8 **)(param_2 + 1000 + lVar6 * 0x10);
        *(undefined4 *)(puVar5 + 0x1680) = 0xb400;
        *(undefined1 *)((longlong)puVar5 + 0xb404) = 0;
        FUN_140066d80(puVar5,puVar3,0xb400);
        lVar4 = uVar8 * 0x20 + param_2;
        lVar6 = FUN_14005f05c(0x18,0x41475046);
        if (lVar6 != 0) {
          *(undefined8 **)(lVar6 + 0x10) = puVar5;
          ExInterlockedInsertTailList(lVar4 + 0x830,lVar6,lVar4 + 0x840);
          *(char *)(lVar4 + 0x848) = *(char *)(lVar4 + 0x848) + '\x01';
        }
      }
    }
    else {
      KeAcquireSpinLockAtDpcLevel();
      plVar9 = (longlong *)((ulonglong)(uint)uVar2 * 0x20 + 0x3c8 + param_2);
      if ((longlong *)*plVar9 != plVar9) {
        lVar4 = ExInterlockedRemoveHeadList(plVar9,plVar9 + 2);
        *(char *)(plVar9 + 3) = (char)plVar9[3] + -1;
        lVar6 = *(longlong *)(lVar4 + 0x10);
        ExFreePoolWithTag(lVar4,0);
        if (lVar6 != 0) {
          *(undefined4 *)(*(longlong *)(lVar6 + 0x10) + 0x24) = 0;
          FUN_14003479c(*(longlong *)(param_2 + 0x10),(uint)uVar2,(undefined8 *)0x0,
                        *(uint *)(param_2 + 0x8a8 + uVar8 * 4));
        }
      }
    }
    KeReleaseSpinLockFromDpcLevel(lVar1);
  }
  return;
}
```

### `FUN_140017c30` (21125-21177)
```c
void FUN_140017c30(undefined8 param_1,longlong param_2,uint *param_3)

{
  ushort uVar1;
  uint uVar2;
  undefined4 uVar3;
  longlong lVar4;
  undefined8 *puVar5;
  undefined1 *puVar6;
  undefined4 uVar7;
  longlong lVar8;
  longlong lVar9;
  longlong *plVar10;
  ulonglong uVar11;
  
  if ((param_2 != 0) && (param_3 != (uint *)0x0)) {
    uVar1 = *(ushort *)((longlong)param_3 + 2);
    uVar11 = (ulonglong)uVar1;
    uVar2 = *param_3;
    lVar9 = param_2 + 0x448 + uVar11 * 8;
    KeAcquireSpinLockAtDpcLevel(lVar9);
    plVar10 = (longlong *)(uVar11 * 0x20 + 0x3c8 + param_2);
    if ((longlong *)*plVar10 != plVar10) {
      lVar8 = ExInterlockedRemoveHeadList(plVar10,plVar10 + 2);
      *(char *)(plVar10 + 3) = (char)plVar10[3] + -1;
      lVar4 = *(longlong *)(lVar8 + 0x10);
      ExFreePoolWithTag(lVar8,0);
      if (lVar4 != 0) {
        puVar5 = *(undefined8 **)
                  (param_2 + 1000 + ((ulonglong)(uVar2 & 0xffff) + uVar11 * 2) * 0x10);
        lVar4 = *(longlong *)(lVar4 + 0x10);
        uVar3 = *(undefined4 *)(param_2 + 0x8b0 + uVar11 * 4);
        uVar7 = uVar3;
        if (*(int *)(param_2 + 0x664 + uVar11 * 4) != 0) {
          uVar7 = 0;
        }
        puVar6 = *(undefined1 **)(lVar4 + 0x28);
        *(undefined4 *)(lVar4 + 0x24) = uVar7;
        if ((DAT_1400a04f8 & 4) != 0) {
          DbgPrint("[a2-SP(Audio) @ is 0x%p size=%lu, AudioStop= %d] %02x %02x %02x %02x %02x %02x %02x %02x, %02x %02x %02x %02x %02x %02x %02x %02x"
                   ,puVar6,uVar3,*(undefined4 *)(param_2 + 0x664 + uVar11 * 4),*puVar6,puVar6[1],
                   puVar6[2],puVar6[3],puVar6[4],puVar6[5],puVar6[6],puVar6[7],puVar6[8],puVar6[9],
                   puVar6[10],puVar6[0xb],puVar6[0xc],puVar6[0xd],puVar6[0xe],puVar6[0xf]);
          lVar9 = param_2 + (uVar11 + 0x89) * 8;
        }
        FUN_14003479c(*(longlong *)(param_2 + 0x10),(uint)uVar1,puVar5,
                      *(uint *)(param_2 + 0x8a8 + uVar11 * 4));
      }
    }
    KeReleaseSpinLockFromDpcLevel(lVar9);
  }
  return;
}
```

### `FUN_140017e70` (21181-21261)
```c
void FUN_140017e70(undefined8 param_1,longlong param_2,uint *param_3)

{
  longlong lVar1;
  ushort uVar2;
  undefined8 *puVar3;
  longlong lVar4;
  undefined8 *puVar5;
  longlong lVar6;
  undefined8 uVar7;
  ulonglong uVar8;
  longlong *plVar9;
  
  if ((param_2 != 0) && (param_3 != (uint *)0x0)) {
    uVar2 = *(ushort *)((longlong)param_3 + 2);
    uVar8 = (ulonglong)uVar2;
    lVar1 = param_2 + 0x508 + uVar8 * 8;
    lVar6 = (ulonglong)(*param_3 & 0xffff) + uVar8 * 2;
    if (*(int *)(param_2 + 0x668 + uVar8 * 4) == 0) {
      if (*(int *)(*(longlong *)(param_2 + 0x10) + 0x88 + uVar8 * 4) == 0) {
        KeAcquireSpinLockAtDpcLevel();
        puVar5 = (undefined8 *)FUN_14005f05c(0xb408,0x41475046);
        if (puVar5 != (undefined8 *)0x0) {
          puVar3 = *(undefined8 **)(param_2 + 0x4a8 + lVar6 * 0x10);
          *(undefined4 *)(puVar5 + 0x1680) = 0x2d00;
          *(undefined1 *)((longlong)puVar5 + 0xb404) = 0;
          FUN_140066d80(puVar5,puVar3,0x2d00);
          plVar9 = (longlong *)(param_2 + 0x850 + uVar8 * 0x20);
          lVar6 = FUN_14005f05c(0x18,0x41475046);
          if (lVar6 != 0) {
            *(undefined8 **)(lVar6 + 0x10) = puVar5;
            ExInterlockedInsertTailList(plVar9,lVar6,plVar9 + 2);
            *(char *)(plVar9 + 3) = (char)plVar9[3] + '\x01';
          }
          if (((2 < *(byte *)(uVar8 * 0x20 + 0x868 + param_2)) && ((longlong *)*plVar9 != plVar9))
             && (lVar6 = ((longlong *)*plVar9)[2], lVar6 != 0)) {
            uVar7 = ExInterlockedRemoveHeadList(plVar9,plVar9 + 2);
            *(char *)(plVar9 + 3) = (char)plVar9[3] + -1;
            ExFreePoolWithTag(uVar7,0);
            ExFreePoolWithTag(lVar6,0x41475046);
          }
        }
      }
      else {
        KeAcquireSpinLockAtDpcLevel(lVar1);
        puVar5 = (undefined8 *)FUN_14005f05c(0xb408,0x41475046);
        if (puVar5 == (undefined8 *)0x0) {
          return;
        }
        puVar3 = *(undefined8 **)(param_2 + 0x4a8 + lVar6 * 0x10);
        *(undefined4 *)(puVar5 + 0x1680) = 0x2d00;
        *(undefined1 *)((longlong)puVar5 + 0xb404) = 0;
        FUN_140066d80(puVar5,puVar3,0x2d00);
        lVar4 = uVar8 * 0x20 + param_2;
        lVar6 = FUN_14005f05c(0x18,0x41475046);
        if (lVar6 != 0) {
          *(undefined8 **)(lVar6 + 0x10) = puVar5;
          ExInterlockedInsertTailList(lVar4 + 0x850,lVar6,lVar4 + 0x860);
          *(char *)(lVar4 + 0x868) = *(char *)(lVar4 + 0x868) + '\x01';
        }
      }
    }
    else {
      KeAcquireSpinLockAtDpcLevel();
      plVar9 = (longlong *)((ulonglong)(uint)uVar2 * 0x20 + 0x488 + param_2);
      if ((longlong *)*plVar9 != plVar9) {
        lVar4 = ExInterlockedRemoveHeadList(plVar9,plVar9 + 2);
        *(char *)(plVar9 + 3) = (char)plVar9[3] + -1;
        lVar6 = *(longlong *)(lVar4 + 0x10);
        ExFreePoolWithTag(lVar4,0);
        if (lVar6 != 0) {
          *(undefined4 *)(*(longlong *)(lVar6 + 0x10) + 0x24) = 0;
          FUN_1400347c4(*(longlong *)(param_2 + 0x10),(uint)uVar2,(undefined8 *)0x0,
                        *(uint *)(param_2 + 0x8ac + uVar8 * 4));
        }
      }
    }
    KeReleaseSpinLockFromDpcLevel(lVar1);
  }
  return;
}
```

### `FUN_1400180d0` (21265-21317)
```c
void FUN_1400180d0(undefined8 param_1,longlong param_2,uint *param_3)

{
  ushort uVar1;
  uint uVar2;
  undefined4 uVar3;
  longlong lVar4;
  undefined8 *puVar5;
  undefined1 *puVar6;
  undefined4 uVar7;
  longlong lVar8;
  longlong lVar9;
  longlong *plVar10;
  ulonglong uVar11;
  
  if ((param_2 != 0) && (param_3 != (uint *)0x0)) {
    uVar1 = *(ushort *)((longlong)param_3 + 2);
    uVar11 = (ulonglong)uVar1;
    uVar2 = *param_3;
    lVar9 = param_2 + 0x508 + uVar11 * 8;
    KeAcquireSpinLockAtDpcLevel(lVar9);
    plVar10 = (longlong *)(uVar11 * 0x20 + 0x488 + param_2);
    if ((longlong *)*plVar10 != plVar10) {
      lVar8 = ExInterlockedRemoveHeadList(plVar10,plVar10 + 2);
      *(char *)(plVar10 + 3) = (char)plVar10[3] + -1;
      lVar4 = *(longlong *)(lVar8 + 0x10);
      ExFreePoolWithTag(lVar8,0);
      if (lVar4 != 0) {
        puVar5 = *(undefined8 **)
                  (param_2 + 0x4a8 + ((ulonglong)(uVar2 & 0xffff) + uVar11 * 2) * 0x10);
        lVar4 = *(longlong *)(lVar4 + 0x10);
        uVar3 = *(undefined4 *)(param_2 + 0x8b4 + uVar11 * 4);
        uVar7 = uVar3;
        if (*(int *)(param_2 + 0x668 + uVar11 * 4) != 0) {
          uVar7 = 0;
        }
        puVar6 = *(undefined1 **)(lVar4 + 0x28);
        *(undefined4 *)(lVar4 + 0x24) = uVar7;
        if ((DAT_1400a04f8 & 4) != 0) {
          DbgPrint("[a2-SP(Mixer) @ is 0x%p size=%lu, AudioStop=%d] %02x %02x %02x %02x %02x %02x %02x %02x, %02x %02x %02x %02x %02x %02x %02x %02x"
                   ,puVar6,uVar3,*(undefined4 *)(param_2 + 0x668 + uVar11 * 4),*puVar6,puVar6[1],
                   puVar6[2],puVar6[3],puVar6[4],puVar6[5],puVar6[6],puVar6[7],puVar6[8],puVar6[9],
                   puVar6[10],puVar6[0xb],puVar6[0xc],puVar6[0xd],puVar6[0xe],puVar6[0xf]);
          lVar9 = param_2 + (uVar11 + 0xa1) * 8;
        }
        FUN_1400347c4(*(longlong *)(param_2 + 0x10),(uint)uVar1,puVar5,
                      *(uint *)(param_2 + 0x8ac + uVar11 * 4));
      }
    }
    KeReleaseSpinLockFromDpcLevel(lVar9);
  }
  return;
}
```

### `FUN_140018310` (21321-21328)
```c
void FUN_140018310(undefined8 param_1,longlong param_2)

{
  if (param_2 != 0) {
    KeSetEvent(param_2 + 0x580,2);
  }
  return;
}
```

### `FUN_140018340` (21334-21409)
```c
void FUN_140018340(undefined8 param_1,longlong *param_2,uint *param_3)

{
  longlong *plVar1;
  ushort uVar2;
  uint uVar3;
  longlong lVar4;
  undefined8 uVar5;
  int iVar6;
  uint uVar7;
  longlong lVar8;
  longlong *plVar9;
  ulonglong uVar10;
  int local_48;
  int iStack_44;
  
  if ((param_2 != (longlong *)0x0) && (param_3 != (uint *)0x0)) {
    uVar7 = *(uint *)(param_2 + 0x126);
    if (*(uint *)((longlong)param_2 + 0x92c) < uVar7) {
      DbgPrint("Error: Video is too less. Drop(%lu)\n",*(int *)((longlong)param_2 + 0x934) + uVar7);
      uVar7 = *(uint *)(param_2 + 0x126);
      *(uint *)((longlong)param_2 + 0x92c) = uVar7;
    }
    if (*(uint *)((longlong)param_2 + 0x934) < *(uint *)(param_2 + 0x127)) {
      DbgPrint("Error: Video is too much. Drop(%lu)\n",*(uint *)(param_2 + 0x127) + uVar7);
      *(int *)((longlong)param_2 + 0x934) = (int)param_2[0x127];
    }
    uVar7 = *param_3;
    uVar2 = *(ushort *)((longlong)param_3 + 2);
    uVar10 = (ulonglong)uVar2;
    if (*(int *)((longlong)param_2 + uVar10 * 0xc + 0xd0) != 0) {
      plVar1 = param_2 + uVar10 + 0x61;
      KeAcquireSpinLockAtDpcLevel(plVar1);
      plVar9 = param_2 + uVar10 * 4 + 0x1d;
      if ((longlong *)*plVar9 != plVar9) {
        lVar8 = ExInterlockedRemoveHeadList(plVar9,plVar9 + 2);
        *(char *)(plVar9 + 3) = (char)plVar9[3] + -1;
        lVar4 = *(longlong *)(lVar8 + 0x10);
        ExFreePoolWithTag(lVar8,0);
        if (lVar4 != 0) {
          FUN_140016a24(param_2,(uint)uVar2);
          *(undefined4 *)(param_2 + 0x10f) = 1;
          uVar3 = *(uint *)(*(longlong *)(lVar4 + 0x10) + 0x20);
          *(uint *)(*(longlong *)(lVar4 + 0x10) + 0x24) = uVar3;
          if (*(char *)(param_2[2] + 0xd4) == '\0') {
            FUN_140066d80(*(undefined8 **)(*(longlong *)(lVar4 + 0x10) + 0x28),
                          (undefined8 *)
                          param_2[((ulonglong)(uVar7 & 0xffff) + uVar10 * 4) * 2 + 0x69],
                          (ulonglong)uVar3);
          }
          lVar8 = *(longlong *)(param_2[2] + 0xf8);
          uVar5 = *(undefined8 *)(lVar8 + 0x88);
          if ((*(int *)(lVar8 + 0x90) == 0x30313050) && (iVar6 = FUN_140063ae4(lVar8), iVar6 != 0))
          {
            local_48 = (int)uVar5;
            lVar4 = *(longlong *)(*(longlong *)(lVar4 + 0x10) + 0x28);
            iStack_44 = (int)((ulonglong)uVar5 >> 0x20);
            FUN_1400168d4(lVar4,lVar4,local_48,iStack_44,(local_48 * 3) / 2,local_48 * 2);
          }
          uVar7 = (**(code **)(*param_2 + 0x138))(param_2);
          if ((0x20180718 < uVar7) &&
             (iVar6 = (**(code **)(*param_2 + 0x1f0))(param_2,(uint)uVar2), iVar6 != 0)) {
            *(undefined4 *)((longlong)param_2 + uVar10 * 0xc + 0x820) = 0;
          }
          FUN_1400352d8(param_2[2],(uint)uVar2,
                        *(undefined4 *)((longlong)param_2 + uVar10 * 0xc + 0x820));
          KeReleaseSpinLockFromDpcLevel(plVar1);
          *(undefined4 *)(param_2 + 0x10f) = 0;
          return;
        }
      }
      KeReleaseSpinLockFromDpcLevel(plVar1);
    }
  }
  return;
}
```

### `FUN_140046828` (66482-66872)
```c
void FUN_140046828(longlong param_1,undefined8 param_2,undefined8 param_3,ulonglong param_4)

{
  uint uVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  byte bVar5;
  byte bVar6;
  byte bVar7;
  byte bVar8;
  byte bVar9;
  byte bVar10;
  char cVar11;
  char extraout_AL;
  char *pcVar12;
  undefined8 uVar13;
  undefined7 uVar17;
  ulonglong uVar14;
  ulonglong uVar15;
  ulonglong uVar16;
  uint uVar18;
  undefined7 uVar20;
  ulonglong uVar19;
  uint uVar21;
  
  uVar13 = 0;
  thunk_FUN_1400444d0(param_1,0,(byte)param_3,(byte)param_4);
  uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
  bVar2 = FUN_140044484(param_1,9);
  uVar20 = (undefined7)((ulonglong)param_3 >> 8);
  FUN_140044528(param_1,9,bVar2 & 4);
  bVar3 = FUN_140044484(param_1,0x10);
  uVar15 = (ulonglong)bVar3;
  FUN_140044528(param_1,0x10,bVar3);
  bVar4 = FUN_140044484(param_1,0x11);
  uVar14 = CONCAT71(uVar20,bVar4) & 0xffffffffffffffbf;
  FUN_140044528(param_1,0x11,(char)uVar14);
  bVar5 = FUN_140044484(param_1,0x12);
  uVar14 = CONCAT71((int7)(uVar14 >> 8),bVar5) & 0xffffffffffffff7f;
  FUN_140044528(param_1,0x12,(char)uVar14);
  bVar6 = FUN_140044484(param_1,0x1d);
  uVar20 = (undefined7)(uVar14 >> 8);
  FUN_140044528(param_1,0x1d,bVar6);
  bVar7 = FUN_140044484(param_1,0xd4);
  FUN_140044528(param_1,0xd4,bVar7);
  bVar8 = FUN_140044484(param_1,0xd5);
  uVar19 = CONCAT71(uVar20,bVar8);
  uVar14 = CONCAT71(uVar17,0xd5);
  FUN_140044528(param_1,0xd5,bVar8);
  if ((bVar2 & 4) != 0) {
    uVar14 = CONCAT71((int7)(uVar14 >> 8),0xcf);
    bVar2 = FUN_140044484(param_1,0xcf);
    if ((bVar2 & 0x20) == 0) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("HDCP Encryption OFF !\n");
      }
      *(undefined2 *)(param_1 + 0x1cf) = 0;
    }
    else {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("HDCP Encryption ON! \n");
      }
      uVar13 = 0;
      thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
      uVar14 = CONCAT71((int7)((ulonglong)uVar13 >> 8),0xd6);
      bVar2 = FUN_140044484(param_1,0xd6);
      *(bool *)(param_1 + 0x1d0) = (bVar2 & 0xc0) != 0;
      *(bool *)(param_1 + 0x1cf) = (bVar2 & 0xc0) == 0;
    }
  }
  if (bVar3 != 0) {
    if ((DAT_1400a04f8 & 0x10) != 0) {
      DbgPrint("[xAud-On] 68051 # 0x10 reg = 0x%02x # (iTE6805_hdmirx_common_irq)\n");
      uVar14 = uVar15;
    }
    if ((((char)bVar3 < '\0') && (*(int *)(param_1 + 0x1dc) == 4)) &&
       (*(int *)(param_1 + 0x1e0) == 3)) {
      param_4 = CONCAT71((int7)(param_4 >> 8),1);
      uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
      FUN_1400444d0(param_1,0x86,1,1);
      thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
      bVar2 = FUN_140044484(param_1,0xbe);
      bVar9 = FUN_140044484(param_1,0xbf);
      bVar10 = FUN_140044484(param_1,0xc0);
      uVar21 = (bVar10 & 0xf) + ((uint)bVar9 + (uint)bVar2 * 0x100) * 0x10;
      bVar2 = FUN_140044484(param_1,0xc0);
      bVar9 = FUN_140044484(param_1,0xc1);
      bVar10 = FUN_140044484(param_1,0xc2);
      uVar14 = 0;
      uVar18 = (uint)bVar10 * 0x10 + (uint)(bVar2 >> 4) + (uint)bVar9 * 0x1000;
      thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
      if ((DAT_1400a04f8 & 0x10) != 0) {
        param_4 = (ulonglong)*(uint *)(param_1 + 600);
        uVar14 = (ulonglong)*(uint *)(param_1 + 0x254);
        uVar19 = (ulonglong)uVar21;
        DbgPrint("[xAud-On] 68051 N = %lu->%lu,  CTS = %lu->%lu \n");
      }
      uVar1 = *(uint *)(param_1 + 0x254);
      if (((uVar1 + 2 < uVar21) || (uVar21 < uVar1 - 2)) ||
         ((uVar21 = *(uint *)(param_1 + 600), uVar21 + 0x14 < uVar18 || (uVar18 < uVar21 - 0x14))))
      {
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint("[xAud-On] 68051 Irq check Video/Audio Stable now => STATEA_RequestAudio\n");
        }
        if (*(int *)(param_1 + 0x1e0) != 1) {
          *(int *)(param_1 + 0x1e0) = 1;
          if ((DAT_1400a04f8 & 0x10) != 0) {
            DbgPrint(
                    "[xAud] 68051 AudState change to STATEA_RequestAudio state !!!, iTE6805_aud_chg\n"
                    );
          }
          uVar13 = 0;
          *(undefined1 *)(param_1 + 0x194) = 0;
          thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
          param_4 = 0;
          uVar19 = CONCAT71((int7)(uVar19 >> 8),0x40);
          uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
          FUN_1400444d0(param_1,0x81,0x40,0);
          uVar14 = CONCAT71(uVar17,1);
          FUN_14003ee30(param_1,'\x01');
        }
      }
      else {
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint(
                  "[xAud-On] 68051 Irq check Video/Audio Stable now => >> Enable-Audio-Output <<  #\n"
                  );
        }
        FUN_14003ea04(param_1);
        FUN_140044cf8(param_1,uVar14,(byte)uVar19,(byte)param_4);
      }
    }
    if (((bVar3 & 0x40) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("[xAud] 68051 # Audio Auto Mute #\n");
    }
    if (((bVar3 & 0x20) != 0) && (*(char *)(param_1 + 0x222) != '\0')) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("[xAud] 68051 # PKT Left Mute #\n");
      }
      uVar14 = 0;
      FUN_14003ecf4(param_1,'\0');
    }
    if (((bVar3 & 0x10) != 0) && (*(char *)(param_1 + 0x222) != '\0')) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("[xAud] 68051 # Set Mute PKT Received #\n");
      }
      uVar14 = CONCAT71((int7)(uVar14 >> 8),1);
      FUN_14003ecf4(param_1,'\x01');
    }
    if (((bVar3 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# Timer Counter Tntterrupt #\n");
    }
    if (((bVar3 & 4) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# Video Mode Changed #\n");
    }
    if ((bVar3 & 2) != 0) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("# SCDT Change #\n");
      }
      FUN_140045b30(param_1,uVar14,uVar19,param_4);
    }
    if (((bVar3 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# Video Abnormal Interrup #\n");
    }
  }
  if (bVar4 != 0) {
    if (((bVar4 & 0x20) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("[xAud] 68051 # No Audio InfoFrame Received #\n");
    }
    if (((bVar4 & 0x10) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# No AVI InfoFrame Received #\n");
    }
    if (((bVar4 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# CD Detect #\n");
    }
    if ((bVar4 & 2) != 0) {
      uVar19 = CONCAT71((int7)(uVar19 >> 8),2);
      uVar14 = CONCAT71((int7)(uVar14 >> 8),0x11);
      FUN_140044528(param_1,0x11,2);
    }
    if ((bVar4 & 1) != 0) {
      uVar19 = CONCAT71((int7)(uVar19 >> 8),1);
      uVar14 = CONCAT71((int7)(uVar14 >> 8),0x11);
      FUN_140044528(param_1,0x11,1);
    }
  }
  if (bVar5 != 0) {
    if ((char)bVar5 < '\0') {
      uVar13 = 0;
      thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
      uVar17 = (undefined7)((ulonglong)uVar13 >> 8);
      bVar2 = FUN_140044484(param_1,0x1a);
      uVar14 = CONCAT71(uVar17,0x1b);
      bVar3 = FUN_140044484(param_1,0x1b);
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("# Video Parameters Change #\n");
      }
      if ((DAT_1400a04f8 & 0x10) != 0) {
        uVar19 = (ulonglong)bVar2;
        uVar14 = (ulonglong)(bVar3 & 7);
        DbgPrint("# VidParaChange_Sts=Reg1Bh=0x%02X Reg1Ah=0x%02X\n",uVar14,uVar19);
      }
      uVar19 = CONCAT71((int7)(uVar19 >> 8),0x80);
      uVar14 = CONCAT71((int7)(uVar14 >> 8),0x12);
      FUN_140044528(param_1,0x12,0x80);
    }
    if (((bVar5 & 0x40) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# 3D audio Valie Change #\n");
    }
    if ((bVar5 & 0x20) != 0) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("# New DRM PKT Received #\n");
      }
      *(undefined1 *)(param_1 + 0x1ad) = 1;
    }
    if (((bVar5 & 0x10) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# New Audio PKT Received #\n");
    }
    if (((bVar5 & 8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# New ACP PKT Received #\n");
    }
    if (((bVar5 & 4) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# New SPD PKT Received #\n");
    }
    if (((bVar5 & 2) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint("# New MPEG InfoFrame Received #\n");
    }
    if ((bVar5 & 1) != 0) {
      if ((DAT_1400a04f8 & 0x10) != 0) {
        DbgPrint("# New AVI InfoFrame Received #\n");
      }
      thunk_FUN_1400444d0(param_1,2,(byte)uVar19,(byte)param_4);
      bVar2 = FUN_140044484(param_1,0x14);
      uVar15 = (ulonglong)bVar2;
      bVar3 = FUN_140044484(param_1,0x15);
      uVar14 = 0;
      uVar16 = (ulonglong)bVar3;
      thunk_FUN_1400444d0(param_1,0,(byte)uVar19,(byte)param_4);
      if (*(char *)(param_1 + 0x222) == '\x01') {
        FUN_1400449a0(param_1);
        if ((bVar2 != *(byte *)(param_1 + 0x198)) || (bVar3 != *(byte *)(param_1 + 0x199))) {
          *(byte *)(param_1 + 0x198) = bVar2;
          *(byte *)(param_1 + 0x199) = bVar3;
          if (*(char *)(param_1 + 0x1a1) == '\x01') {
            *(undefined1 *)(param_1 + 0x1a1) = 0;
          }
          else {
            if ((DAT_1400a04f8 & 0x10) != 0) {
              DbgPrint("AVIDB[0] = %x \n");
              uVar14 = uVar15;
            }
            if ((DAT_1400a04f8 & 0x10) != 0) {
              DbgPrint("AVIDB[0] = %x \n");
              uVar14 = uVar16;
            }
            if ((DAT_1400a04f8 & 0x10) != 0) {
              uVar14 = (ulonglong)*(byte *)(param_1 + 0x198);
              DbgPrint("prevAVIDB[0] = %x \n");
            }
            if ((DAT_1400a04f8 & 0x10) != 0) {
              uVar14 = (ulonglong)*(byte *)(param_1 + 0x199);
              DbgPrint("prevAVIDB[0] = %x \n");
            }
            if ((DAT_1400a04f8 & 0x10) != 0) {
              DbgPrint("# New AVI InfoFrame Received #\n");
            }
          }
          *(undefined1 *)(param_1 + 0x19d) = 1;
        }
      }
      else {
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint("Flag_VidStable_Done!=TRUE, Save AVI Infoframe to variable. \n");
        }
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint("AVIDB[0] = %x \n");
          uVar14 = uVar15;
        }
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint("AVIDB[0] = %x \n");
          uVar14 = uVar16;
        }
        if ((DAT_1400a04f8 & 0x10) != 0) {
          uVar14 = (ulonglong)*(byte *)(param_1 + 0x198);
          DbgPrint("prevAVIDB[0] = %x \n");
        }
        if ((DAT_1400a04f8 & 0x10) != 0) {
          uVar14 = (ulonglong)*(byte *)(param_1 + 0x199);
          DbgPrint("prevAVIDB[0] = %x \n");
        }
        *(byte *)(param_1 + 0x198) = bVar2;
        *(byte *)(param_1 + 0x199) = bVar3;
        *(undefined1 *)(param_1 + 0x1a1) = 0;
      }
    }
  }
  if (bVar6 == 0) {
    return;
  }
  if (((bVar6 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("HDCP2 Authen Start \n");
  }
  if (((bVar6 & 2) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("HDCP2 Authen Done \n");
  }
  if (((bVar6 & 4) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("HDCP2 Off Detect \n");
  }
  bVar2 = 8;
  if ((bVar6 & 8) != 0) {
    if ((DAT_1400a04f8 & 0x10) != 0) {
      DbgPrint("HDCP Encryption Change \n");
    }
    cVar11 = FUN_14003cdc4(param_1);
    if ((cVar11 == '\0') && (*(char *)(param_1 + 0x19f) == '\x01')) {
      *(undefined1 *)(param_1 + 0x19f) = 0;
      bVar2 = FUN_140041558(param_1,uVar14,(byte)uVar19,(byte)param_4);
      if (extraout_AL == '\0') {
        if ((DAT_1400a04f8 & 0x10) != 0) {
          DbgPrint(
                  "******EQ Done But Find CED Error when Encryption change, Restart Manual EQ ******\n"
                  );
        }
        FUN_140041b38(param_1,1,uVar19,param_4);
      }
    }
    bVar3 = FUN_140044484(param_1,0xd6);
    if ((bVar2 & bVar3) == 0) {
      if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14004712d;
      pcVar12 = "HDCP Encryption OFF !\n";
    }
    else {
      if ((DAT_1400a04f8 & 0x10) == 0) goto LAB_14004712d;
      pcVar12 = "HDCP Encryption ON! \n";
    }
    DbgPrint(pcVar12);
  }
LAB_14004712d:
  if ((bVar6 & 0x10) != 0) {
    if (((bVar7 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" AKE_Init_Rcv \n");
    }
    if (((bVar7 & 2) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" AKE_NoStr_Km_Rcv \n");
    }
    if (((bVar7 & 4) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" AKE_Str_Km_Rcv \n");
    }
    if (((bVar2 & bVar7) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" LC_Init_Rcv \n");
    }
    if (((bVar7 & 0x10) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" SKE_Send_Eks_Rcv \n");
    }
    if (((bVar7 & 0x20) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" Rpt_Send_Ack_Rcv \n");
    }
    if (((bVar7 & 0x40) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" Rpt_Str_Manage_Rcv \n");
    }
    if (((char)bVar7 < '\0') && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" RSA_Fail_pulse \n");
    }
    if (((bVar8 & 1) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Read CERT Done \n");
    }
    if (((bVar8 & 2) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Read H Done \n");
    }
    if (((bVar8 & 4) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Read Pair Done \n");
    }
    if (((bVar2 & bVar8) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Read L\' Done \n");
    }
    if (((bVar8 & 0x10) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Read KSV and V\' \n");
    }
    if (((bVar8 & 0x20) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" Rpt_Send_Ack_Rcv \n");
    }
    if (((bVar8 & 0x40) != 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" HDCP2 Message Read Error !! \n");
    }
    if (((char)bVar8 < '\0') && ((DAT_1400a04f8 & 0x10) != 0)) {
      DbgPrint(" ECC Re-Authen !! \n");
    }
  }
  return;
}
```

## Appendix D - DMA / Stream Function Definitions
Full definitions extracted from `linuxextract.c`.

### `FUN_140016a24` (20455-20563)
```c
void FUN_140016a24(longlong *param_1,int param_2)

{
  longlong lVar1;
  longlong lVar2;
  uint uVar3;
  longlong *plVar4;
  int iVar5;
  longlong lVar6;
  
  iVar5 = 0;
  lVar6 = (longlong)param_2;
  if (*(longlong *)(param_1[2] + 0xf8) != 0) {
    iVar5 = *(int *)(*(longlong *)(param_1[2] + 0xf8) + 0x90);
  }
  if (*(byte *)(param_1 + (lVar6 + 8) * 4) < 4) {
    plVar4 = param_1 + lVar6 * 4 + 0x21;
    if ((longlong *)*plVar4 != plVar4) {
      lVar2 = ExInterlockedRemoveHeadList(plVar4,plVar4 + 2);
      *(char *)(plVar4 + 3) = (char)plVar4[3] + -1;
      lVar1 = *(longlong *)(lVar2 + 0x10);
      ExFreePoolWithTag(lVar2,0);
      if (lVar1 != 0) {
        if (*(int *)((longlong)param_1 + lVar6 * 0xc + 0xd0) != 0) {
          if (*(char *)(param_1[2] + 0xd4) != '\0') {
            lVar2 = lVar6 * 4;
            (**(code **)(*param_1 + 0x58))
                      (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x308,
                       (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) +
                                    lVar2) * 4 + 0x25]);
            (**(code **)(*param_1 + 0x58))
                      (param_1,(*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) + 0x41) * 0xc,
                       *(undefined4 *)
                        (((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) + lVar2) *
                         0x20 + 300 + (longlong)param_1));
            (**(code **)(*param_1 + 0x58))
                      (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x310,
                       (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) +
                                    lVar2) * 4 + 0x27]);
            if ((DAT_1400a04f8 & 2) != 0) {
              DbgPrint("(ch %d)S1 Insert DESC-v to FPGA DescList number2(%lu)",param_2,
                       *(undefined4 *)((longlong)param_1 + lVar6 * 4 + 0x65c));
            }
            if ((((iVar5 == 0x30323449) || (iVar5 == 0x32315659)) || (iVar5 == 0x3231564e)) ||
               ((iVar5 == 0x30313050 || (iVar5 == 0x30313250)))) {
              (**(code **)(*param_1 + 0x58))
                        (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x508,
                         (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) +
                                      lVar2) * 4 + 0x35]);
              (**(code **)(*param_1 + 0x58))
                        (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x50c,
                         *(undefined4 *)
                          (((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) + lVar2) *
                           0x20 + 0x1ac + (longlong)param_1));
              (**(code **)(*param_1 + 0x58))
                        (param_1,(*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) + 0x6c) * 0xc,
                         (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) +
                                      lVar2) * 4 + 0x37]);
              if ((DAT_1400a04f8 & 2) != 0) {
                DbgPrint("(ch %d)S2 Insert DESC-v to FPGA DescList number2(%lu)",param_2,
                         *(undefined4 *)((longlong)param_1 + lVar6 * 4 + 0x65c));
              }
              if ((iVar5 == 0x30323449) || (iVar5 == 0x32315659)) {
                (**(code **)(*param_1 + 0x58))
                          (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x608,
                           (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658)
                                        + lVar2) * 4 + 0x45]);
                (**(code **)(*param_1 + 0x58))
                          (param_1,(*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) + 0x81) * 0xc,
                           *(undefined4 *)
                            (((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) + lVar2) *
                             0x20 + 0x22c + (longlong)param_1));
                (**(code **)(*param_1 + 0x58))
                          (param_1,*(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) * 0xc + 0x610,
                           (int)param_1[((ulonglong)*(uint *)((longlong)param_1 + lVar6 * 4 + 0x658)
                                        + lVar2) * 4 + 0x47]);
                if ((DAT_1400a04f8 & 2) != 0) {
                  DbgPrint("(ch %d)S3 Insert DESC-v to FPGA DescList number2(%lu)",param_2,
                           *(undefined4 *)((longlong)param_1 + lVar6 * 4 + 0x65c));
                }
              }
            }
          }
          uVar3 = *(uint *)(param_1[3] + 0x304) |
                  1 << ((char)*(undefined4 *)((longlong)param_1 + lVar6 * 4 + 0x65c) + 1U & 0x1f);
          (**(code **)(*param_1 + 0x58))(param_1,0x304,uVar3);
          if ((((iVar5 == 0x30323449) || (iVar5 == 0x32315659)) || (iVar5 == 0x3231564e)) ||
             ((iVar5 == 0x30313050 || (iVar5 == 0x30313250)))) {
            (**(code **)(*param_1 + 0x58))(param_1,0x504,uVar3);
            if ((iVar5 == 0x30323449) || (iVar5 == 0x32315659)) {
              (**(code **)(*param_1 + 0x58))(param_1,0x604,uVar3);
            }
          }
          *(uint *)((longlong)param_1 + lVar6 * 4 + 0x658) =
               *(int *)((longlong)param_1 + lVar6 * 4 + 0x658) + 1U & 3;
          *(uint *)((longlong)param_1 + lVar6 * 4 + 0x65c) =
               *(int *)((longlong)param_1 + lVar6 * 4 + 0x65c) + 1U & 3;
        }
        lVar2 = FUN_14005f05c(0x18,0x41475046);
        if (lVar2 != 0) {
          *(longlong *)(lVar2 + 0x10) = lVar1;
          ExInterlockedInsertTailList(param_1 + lVar6 * 4 + 0x1d,lVar2,param_1 + lVar6 * 4 + 0x1f);
          *(char *)(param_1 + lVar6 * 4 + 0x20) = (char)param_1[lVar6 * 4 + 0x20] + '\x01';
        }
      }
    }
  }
  return;
}
```

### `FUN_1400173f4` (20830-21037)
```c
void FUN_1400173f4(longlong *param_1)

{
  byte *pbVar1;
  undefined1 uVar2;
  longlong lVar3;
  longlong lVar4;
  uint *puVar5;
  longlong *plVar6;
  byte *pbVar7;
  int iVar8;
  uint uVar9;
  undefined8 uVar10;
  longlong *plVar11;
  int iVar12;
  longlong *local_res18;
  uint *local_78;
  undefined8 ******local_60;
  undefined8 ******local_58;
  undefined1 local_50 [8];
  byte local_48;
  
  local_78 = (uint *)(param_1 + 0x73);
  local_res18 = param_1 + 0x61;
  puVar5 = (uint *)(param_1 + 0x104);
  plVar6 = param_1 + 0xcc;
  pbVar7 = (byte *)(param_1 + 0x24);
  iVar12 = 0;
  do {
    plVar11 = param_1 + ((longlong)iVar12 + 0xe) * 7;
    if (puVar5[-1] == 0) {
      ExAcquireFastMutex(plVar11);
      if (((int)*plVar6 == 0) || (puVar5[-2] == 0)) {
        ExReleaseFastMutex(plVar11);
      }
      else {
        *(undefined4 *)(param_1 + 0x71) = 0;
        ((int *)((longlong)plVar6 + -0xc))[0] = 0;
        ((int *)((longlong)plVar6 + -0xc))[1] = 0;
        *(int *)((longlong)plVar6 + -4) = 0;
        ExReleaseFastMutex(plVar11);
        local_58 = &local_60;
        local_48 = 0;
        local_60 = &local_60;
        KeInitializeSpinLock(local_50);
        uVar2 = KeAcquireSpinLockRaiseToDpc(local_res18);
        uVar9 = (uint)pbVar7[-0x20];
        if (pbVar7[-0x20] != 0) {
          pbVar1 = pbVar7 + -0x38;
          do {
            if (*(byte **)pbVar1 == pbVar1) {
              uVar10 = 0;
            }
            else {
              lVar4 = ExInterlockedRemoveHeadList(pbVar1,pbVar7 + -0x28);
              pbVar7[-0x20] = pbVar7[-0x20] - 1;
              uVar10 = *(undefined8 *)(lVar4 + 0x10);
              ExFreePoolWithTag(lVar4,0);
            }
            lVar4 = FUN_14005f05c(0x18,0x41475046);
            if (lVar4 != 0) {
              *(undefined8 *)(lVar4 + 0x10) = uVar10;
              ExInterlockedInsertTailList(&local_60,lVar4,local_50);
              local_48 = local_48 + 1;
            }
            uVar9 = uVar9 - 1;
          } while (uVar9 != 0);
        }
        uVar9 = (uint)*pbVar7;
        if (*pbVar7 != 0) {
          pbVar1 = pbVar7 + -0x18;
          do {
            if (*(byte **)pbVar1 == pbVar1) {
              uVar10 = 0;
            }
            else {
              lVar4 = ExInterlockedRemoveHeadList(pbVar1,pbVar7 + -8);
              *pbVar7 = *pbVar7 - 1;
              uVar10 = *(undefined8 *)(lVar4 + 0x10);
              ExFreePoolWithTag(lVar4,0);
            }
            lVar4 = FUN_14005f05c(0x18,0x41475046);
            if (lVar4 != 0) {
              *(undefined8 *)(lVar4 + 0x10) = uVar10;
              ExInterlockedInsertTailList(&local_60,lVar4,local_50);
              local_48 = local_48 + 1;
            }
            uVar9 = uVar9 - 1;
          } while (uVar9 != 0);
        }
        KeReleaseSpinLock(local_res18,uVar2);
        uVar9 = (uint)local_48;
        if (local_48 != 0) {
          do {
            if ((undefined8 *******)local_60 == &local_60) {
              uVar10 = 0;
            }
            else {
              lVar4 = ExInterlockedRemoveHeadList(&local_60,local_50);
              local_48 = local_48 - 1;
              uVar10 = *(undefined8 *)(lVar4 + 0x10);
              ExFreePoolWithTag(lVar4,0);
            }
            (**(code **)(*param_1 + 0xd8))(param_1,iVar12,uVar10);
            uVar9 = uVar9 - 1;
          } while (uVar9 != 0);
        }
        if (*(longlong **)(param_1[2] + 0x100) != (longlong *)0x0) {
          FUN_140062200(*(longlong **)(param_1[2] + 0x100),*local_78,local_78[1]);
        }
        (**(code **)(*param_1 + 0x1f8))(param_1,iVar12,0x1e);
        (**(code **)(*param_1 + 0xa8))(param_1,iVar12,1);
        (**(code **)(*param_1 + 0xb0))(param_1,iVar12,1,1);
        if ((int)param_1[0x124] != 0) {
          (**(code **)(*param_1 + 0xb8))(param_1,iVar12,1,1);
        }
        if ((param_1[2] != 0) &&
           (plVar11 = *(longlong **)
                       ((longlong)local_res18 + param_1[2] + (0x110 - (longlong)param_1)),
           plVar11 != (longlong *)0x0)) {
          (**(code **)(*plVar11 + 0x10))();
        }
      }
    }
    else {
      ExAcquireFastMutex(plVar11);
      if ((int)*plVar6 == 0) {
        puVar5[-0x1d4] = 0;
        (**(code **)(*param_1 + 0xb0))(param_1,iVar12,0,1);
        if ((int)param_1[0x124] != 0) {
          (**(code **)(*param_1 + 0xb8))(param_1,iVar12,0,1);
        }
        if (*(longlong *)(param_1[2] + 0xf8) != 0) {
          FUN_1400162a0();
        }
        iVar8 = 0;
        if ((int)*plVar6 == 0) {
          while (*(int *)((longlong)param_1 + 0xdc) != 0) {
            FUN_140036d84(10);
            iVar8 = iVar8 + 1;
            if ((iVar8 == 0x14) || ((int)*plVar6 != 0)) break;
          }
        }
        if ((int)*plVar6 != 1) {
          _DAT_1400a04fc = _DAT_1400a04fc + 1;
          *(int *)plVar6 = 1;
        }
        FUN_14001a2e0(param_1,1);
        ExReleaseFastMutex(plVar11);
      }
      else {
        ExReleaseFastMutex(plVar11);
        if (*(longlong *)(param_1[2] + 0xf8) != 0) {
          ExAcquireFastMutex(*(longlong *)(param_1[2] + 0xf8) + 8);
          **(undefined4 **)(param_1[2] + 0xf8) = 0;
          ExReleaseFastMutex(*(longlong *)(param_1[2] + 0xf8) + 8);
        }
        uVar9 = (**(code **)(*param_1 + 0x138))(param_1);
        if (uVar9 < 0x20180719) {
          FUN_14003479c(param_1[2],iVar12,(undefined8 *)0x0,*(uint *)(plVar6 + 0x49));
          uVar2 = KeAcquireSpinLockRaiseToDpc(local_res18 + 0x40);
          pbVar1 = pbVar7 + 0x368;
          if (*(byte **)pbVar1 != pbVar1) {
            lVar3 = ExInterlockedRemoveHeadList(pbVar1,pbVar7 + 0x378);
            pbVar7[0x380] = pbVar7[0x380] - 1;
            lVar4 = *(longlong *)(lVar3 + 0x10);
            ExFreePoolWithTag(lVar3,0);
            if (lVar4 != 0) {
              *(undefined4 *)(*(longlong *)(lVar4 + 0x10) + 0x24) = 0;
              FUN_1400347c4(param_1[2],iVar12,(undefined8 *)0x0,*(uint *)((longlong)plVar6 + 0x24c))
              ;
            }
          }
          KeReleaseSpinLock(local_res18 + 0x40,uVar2);
          uVar2 = KeAcquireSpinLockRaiseToDpc(local_res18);
          pbVar1 = pbVar7 + -0x38;
          if (*(byte **)pbVar1 == pbVar1) {
code_r0x000140017997:
            KeReleaseSpinLock(param_1 + (longlong)iVar12 + 0x61,uVar2);
            return;
          }
          lVar3 = ExInterlockedRemoveHeadList(pbVar1,pbVar7 + -0x28);
          pbVar7[-0x20] = pbVar7[-0x20] - 1;
          lVar4 = *(longlong *)(lVar3 + 0x10);
          ExFreePoolWithTag(lVar3,0);
          if (lVar4 == 0) goto code_r0x000140017997;
          FUN_140016a24(param_1,iVar12);
          *(undefined4 *)(param_1 + 0x10f) = 1;
          FUN_14000d734(*(longlong *)
                         ((longlong)local_res18 + (-0x2e0 - (longlong)param_1) + param_1[2]),lVar4,
                        *(uint *)(*(longlong *)(lVar4 + 0x10) + 0x20),*puVar5,0);
          FUN_1400352d8(param_1[2],iVar12,*puVar5);
          KeReleaseSpinLock(local_res18,uVar2);
          *(undefined4 *)(param_1 + 0x10f) = 0;
        }
      }
    }
    iVar12 = iVar12 + 1;
    local_res18 = local_res18 + 1;
    puVar5 = puVar5 + 3;
    local_78 = local_78 + 7;
    plVar6 = (longlong *)((longlong)plVar6 + 4);
    pbVar7 = pbVar7 + 0x20;
    if (0 < iVar12) {
      return;
    }
  } while( true );
}
```

### `FUN_14001c220` (23694-23721)
```c
void FUN_14001c220(longlong *param_1,int param_2,longlong param_3)

{
  undefined1 uVar1;
  undefined8 uVar2;
  longlong lVar3;
  longlong lVar4;
  
  if ((param_3 != 0) &&
     ((lVar4 = (longlong)param_2, *(char *)(param_1[2] + 0xd4) == '\0' ||
      (uVar2 = FUN_14001cbd4((longlong)param_1,param_2,param_3,
                             (ulonglong)*(uint *)((longlong)param_1 + lVar4 * 4 + 0x654)),
      (int)uVar2 != 0)))) {
    uVar1 = KeAcquireSpinLockRaiseToDpc(param_1 + lVar4 + 0x61);
    if ((DAT_1400a04f8 & 2) != 0) {
      DbgPrint("(_d:%d)  DESC-u Process() Q a SP to m_VideoSPList_sw",param_2);
    }
    lVar3 = FUN_14005f05c(0x18,0x41475046);
    if (lVar3 != 0) {
      *(longlong *)(lVar3 + 0x10) = param_3;
      ExInterlockedInsertTailList(param_1 + lVar4 * 4 + 0x21,lVar3,param_1 + lVar4 * 4 + 0x23);
      *(char *)(param_1 + lVar4 * 4 + 0x24) = (char)param_1[lVar4 * 4 + 0x24] + '\x01';
    }
    FUN_140016a24(param_1,param_2);
    KeReleaseSpinLock(param_1 + lVar4 + 0x61,uVar1);
  }
  return;
}
```

### `FUN_14001cbd4` (24077-24326)
```c
undefined8 FUN_14001cbd4(longlong param_1,int param_2,longlong param_3,ulonglong param_4)

{
  longlong *plVar1;
  bool bVar2;
  int iVar3;
  undefined8 uVar4;
  uint uVar5;
  ulonglong uVar6;
  uint uVar7;
  uint uVar8;
  longlong lVar9;
  uint uVar10;
  uint uVar11;
  longlong lVar12;
  ulonglong uVar13;
  uint uVar14;
  longlong lVar15;
  longlong lVar16;
  int local_108;
  uint local_fc;
  ulonglong local_f0;
  undefined8 local_d8;
  longlong lStack_d0;
  undefined8 local_c8;
  undefined8 uStack_c0;
  longlong local_b8;
  undefined8 uStack_b0;
  int local_a8;
  longlong local_a0;
  int local_98;
  uint local_88;
  ulonglong *local_80;
  undefined8 local_78;
  longlong lStack_70;
  undefined8 local_68;
  undefined8 uStack_60;
  
  uVar13 = param_4 & 0xffffffff;
  lVar9 = (longlong)param_2;
  if (((param_3 == 0) || (*(longlong *)(param_1 + 0x10) == 0)) ||
     (lVar12 = *(longlong *)(*(longlong *)(param_1 + 0x10) + 0xf8), lVar12 == 0)) {
    return 0;
  }
  local_d8 = *(undefined8 *)(lVar12 + 0x88);
  lStack_d0 = *(longlong *)(lVar12 + 0x90);
  local_c8 = *(undefined8 *)(lVar12 + 0x98);
  uStack_c0 = *(undefined8 *)(lVar12 + 0xa0);
  local_b8 = *(longlong *)(lVar12 + 0xa8);
  uStack_b0 = *(undefined8 *)(lVar12 + 0xb0);
  bVar2 = false;
  local_78 = local_d8;
  lStack_70 = lStack_d0;
  local_68 = local_c8;
  uStack_60 = uStack_c0;
  uVar4 = FUN_140064944(lVar12,*(int *)(lVar12 + 0xc0),*(uint *)(param_1 + 0x1140));
  if ((int)uVar4 == 0) {
    *(undefined4 *)(param_1 + 0x928 + lVar9 * 4) = 1;
  }
  FUN_140066b80(&lStack_d0,0,(undefined1 *)0x40);
  if ((int)lStack_70 == 0x30313050) {
    lVar12 = (uVar13 + lVar9 * 4) * 0x20;
    lVar15 = param_1 + 0x128 + lVar12;
    lVar16 = param_1 + 0x1a8 + lVar12;
    iVar3 = FUN_140063ae4(*(longlong *)(*(longlong *)(param_1 + 0x10) + 0xf8));
    lStack_d0 = lVar15;
    if (iVar3 != 0) {
      uVar10 = 3;
      iVar3 = local_78._4_4_ * (uint)local_78 * 6;
      local_d8 = CONCAT44(local_d8._4_4_,(int)(iVar3 + (iVar3 >> 0x1f & 3U)) >> 2);
      iVar3 = local_78._4_4_ * (uint)local_78 * 2;
      uStack_c0 = CONCAT44(uStack_c0._4_4_,(int)(iVar3 + (iVar3 >> 0x1f & 3U)) >> 2);
      local_a8 = ((local_78._4_4_ * 2) / 2) * (uint)local_78;
      local_b8 = param_1 + 0x228 + lVar12;
      local_c8 = CONCAT44(local_c8._4_4_,
                          (int)((uint)local_78 * 6 + ((int)((uint)local_78 * 6) >> 0x1f & 3U)) >> 2)
      ;
      uVar5 = (int)((uint)local_78 * 2) >> 0x1f & 3;
      uVar6 = (ulonglong)uVar5;
      uStack_b0 = CONCAT44(uStack_b0._4_4_,(int)((uint)local_78 * 2 + uVar5) >> 2);
      local_a0 = lVar16;
      goto LAB_14001ceec;
    }
    local_d8 = CONCAT44(local_d8._4_4_,local_78._4_4_ * (uint)local_78 * 2);
    uVar5 = local_78._4_4_ * 2 >> 0x1f;
    uStack_c0 = CONCAT44(uStack_c0._4_4_,((local_78._4_4_ * 2) / 2) * (uint)local_78);
    local_b8 = lVar16;
LAB_14001cee0:
    iVar3 = (uint)local_78 * 2;
    uVar6 = (ulonglong)uVar5;
    uStack_b0 = CONCAT44(uStack_b0._4_4_,iVar3);
    uVar10 = 2;
  }
  else {
    if ((int)lStack_70 == 0x30313250) {
      lVar12 = (uVar13 + lVar9 * 4) * 0x20;
      iVar3 = local_78._4_4_ * (uint)local_78 * 2;
      local_d8 = CONCAT44(local_d8._4_4_,iVar3);
      uStack_c0 = CONCAT44(uStack_c0._4_4_,iVar3);
      uVar5 = (uint)local_78;
      lStack_d0 = param_1 + 0x128 + lVar12;
      local_b8 = param_1 + 0x1a8 + lVar12;
      goto LAB_14001cee0;
    }
    if ((int)lStack_70 == 0x30323449) {
LAB_14001cd3f:
      uVar10 = 3;
      local_c8 = CONCAT44(local_c8._4_4_,(uint)local_78);
      local_98 = (int)(uint)local_78 / 2;
      uStack_b0 = CONCAT44(uStack_b0._4_4_,local_98);
      local_d8 = CONCAT44(local_d8._4_4_,local_78._4_4_ * (uint)local_78);
      local_a8 = (local_78._4_4_ / 2) * local_98;
      uVar6 = (uVar13 + lVar9 * 4) * 0x20;
      uStack_c0 = CONCAT44(uStack_c0._4_4_,local_a8);
      local_b8 = param_1 + 0x1a8 + uVar6;
      lStack_d0 = param_1 + 0x128 + uVar6;
      local_a0 = param_1 + 0x228 + uVar6;
      goto LAB_14001ceec;
    }
    if ((int)lStack_70 == 0x3231564e) {
      uVar10 = 2;
      uVar6 = (ulonglong)(uint)(local_78._4_4_ >> 0x1f);
      local_c8 = CONCAT44(local_c8._4_4_,(uint)local_78);
      uStack_b0 = CONCAT44(uStack_b0._4_4_,(uint)local_78);
      local_d8 = CONCAT44(local_d8._4_4_,local_78._4_4_ * (uint)local_78);
      lVar12 = (uVar13 + lVar9 * 4) * 0x20;
      uStack_c0 = CONCAT44(uStack_c0._4_4_,(local_78._4_4_ / 2) * (uint)local_78);
      local_b8 = param_1 + 0x1a8 + lVar12;
      lStack_d0 = param_1 + 0x128 + lVar12;
      goto LAB_14001ceec;
    }
    if ((int)lStack_70 == 0x32315659) goto LAB_14001cd3f;
    uVar10 = 1;
    local_d8 = CONCAT44(local_d8._4_4_,(int)local_68);
    lStack_d0 = (uVar13 + lVar9 * 4) * 0x20 + 0x128 + param_1;
    iVar3 = (int)local_68 / local_78._4_4_;
    uVar6 = (longlong)(int)local_68 % (longlong)local_78._4_4_ & 0xffffffff;
  }
  local_c8 = CONCAT44(local_c8._4_4_,iVar3);
LAB_14001ceec:
  ExAcquireFastMutex((lVar9 + 0xe) * 0x38 + param_1,uVar6);
  plVar1 = *(longlong **)(param_3 + 0x18);
  uVar7 = 0;
  uVar5 = 0;
  local_108 = 0;
  uVar6 = 0;
  local_fc = 0;
  local_80 = (ulonglong *)*plVar1;
  uVar13 = *local_80;
  uVar8 = (uint)local_80[1];
  if ((int)plVar1[1] != 0) {
    local_f0 = uVar13 & 0xffffffff;
    do {
      uVar14 = (uint)uVar6;
      if (uVar10 <= uVar14) break;
      lVar9 = (&lStack_d0)[uVar6 * 3];
      lVar12 = (ulonglong)local_fc * 0x10;
      *(int *)(lVar12 + *(longlong *)(lVar9 + 8)) = (int)local_f0;
      uVar11 = uVar8 + uVar5;
      *(int *)(lVar12 + 4 + *(longlong *)(lVar9 + 8)) = (int)(uVar13 >> 0x20);
      local_88 = *(uint *)(&local_d8 + uVar6 * 3);
      if (uVar11 == local_88) {
        if (((uVar8 & 0xfffffff8) != uVar8) && ((DAT_1400a04f8 & 8) != 0)) {
          DbgPrint("(_d:%lu) Size_Debug:= SP Buf index(Total:%lu) %lu, Data Block %lu contain %lu segment, Err BufferSize = 0x%x !!"
                   ,*(undefined1 *)(*(longlong *)(param_1 + 0x10) + 8),uVar7,(int)plVar1[1],uVar14,
                   *(undefined4 *)(lVar9 + 0x10),uVar8);
        }
        *(uint *)(*(longlong *)(lVar9 + 8) + 8 + lVar12) = uVar8 + 3 >> 2;
        *(undefined4 *)(*(longlong *)(lVar9 + 8) + 0xc + lVar12) = 0x80008000;
        *(longlong *)(lVar9 + 0x18) = param_3;
        *(int *)(lVar9 + 0x10) = local_108 + 1;
        if ((DAT_1400a04f8 & 8) != 0) {
          DbgPrint("(_d:%d) Size_Debug:= SP Buf index(Total:%lu) %lu, Data Block %lu contain %lu segment, BufferSize = %lu !!"
                   ,*(undefined1 *)(*(longlong *)(param_1 + 0x10) + 8),uVar7,(int)plVar1[1],uVar14,
                   local_108 + 1,uVar11);
        }
        local_108 = 0;
        uVar5 = 0;
        local_fc = 0;
        uVar6 = (ulonglong)(uVar14 + 1);
        uVar7 = uVar7 + 1;
        if (uVar7 < *(uint *)(plVar1 + 1)) {
          uVar13 = local_80[(ulonglong)uVar7 * 2];
          uVar8 = (uint)local_80[(ulonglong)uVar7 * 2 + 1];
          local_f0 = uVar13;
        }
        else {
LAB_14001d26d:
          local_fc = 0;
          local_108 = 0;
          uVar5 = 0;
        }
      }
      else {
        if (uVar11 < local_88) {
          if (((uVar8 & 0xfffffff8) != uVar8) && ((DAT_1400a04f8 & 8) != 0)) {
            DbgPrint("(_d:%lu) Size_Debug:< SP Buf index(Total:%lu) %lu, Data Block %lu contain %lu segment, Err BufferSize = 0x%x !!"
                     ,*(undefined1 *)(*(longlong *)(param_1 + 0x10) + 8),uVar7,(int)plVar1[1],uVar14
                     ,*(undefined4 *)(lVar9 + 0x10),uVar8);
          }
          *(uint *)(*(longlong *)(lVar9 + 8) + 8 + lVar12) = uVar8 + 3 >> 2;
          uVar7 = uVar7 + 1;
          local_108 = local_108 + 1;
          local_fc = local_fc + 1;
          *(undefined4 *)(*(longlong *)(lVar9 + 8) + 0xc + lVar12) = 0x80008000;
          uVar13 = local_80[(ulonglong)uVar7 * 2];
          uVar8 = (uint)(local_80 + (ulonglong)uVar7 * 2)[1];
          uVar5 = uVar11;
          if (bVar2) {
            uVar5 = uVar11 + 4;
            bVar2 = false;
          }
        }
        else {
          uVar11 = local_88 - uVar5;
          if ((uVar11 + 3 >> 2 & 1) != 0) {
            bVar2 = true;
            uVar11 = uVar11 + 4;
          }
          if (((uVar11 & 0xfffffff8) != uVar11) && ((DAT_1400a04f8 & 8) != 0)) {
            DbgPrint("(_d:%d) Size_Debug:(>) SP Buf index(Total:%lu) %lu, Data Block %lu contain %lu segment, Err BufferSize = 0x%x !!"
                     ,*(undefined1 *)(*(longlong *)(param_1 + 0x10) + 8),uVar7,(int)plVar1[1],uVar14
                     ,*(undefined4 *)(lVar9 + 0x10),uVar11);
          }
          *(uint *)(*(longlong *)(lVar9 + 8) + 8 + lVar12) = uVar11 + 3 >> 2;
          *(undefined4 *)(*(longlong *)(lVar9 + 8) + 0xc + lVar12) = 0x80008000;
          *(longlong *)(lVar9 + 0x18) = param_3;
          *(int *)(lVar9 + 0x10) = local_108 + 1;
          if ((DAT_1400a04f8 & 8) != 0) {
            DbgPrint("(_d:%d) Size_Debug:(>) SP Buf index(Total:%lu) %lu, Data Block %lu contain %lu segment, BufferSize = %lu !!"
                     ,*(undefined1 *)(*(longlong *)(param_1 + 0x10) + 8),uVar7,(int)plVar1[1],uVar14
                     ,local_108 + 1,local_88);
          }
          uVar5 = 0;
          local_108 = 0;
          local_fc = 0;
          uVar6 = (ulonglong)(uVar14 + 1);
          if (uVar10 <= uVar14 + 1) goto LAB_14001d26d;
          uVar13 = uVar13 + uVar11;
          uVar8 = uVar8 - uVar11;
        }
        local_f0 = uVar13 & 0xffffffff;
      }
    } while (uVar7 < *(uint *)(plVar1 + 1));
  }
  lVar9 = (longlong)param_2;
  *(uint *)(param_1 + 0x654 + lVar9 * 4) = *(int *)(param_1 + 0x654 + lVar9 * 4) + 1U & 3;
  ExReleaseFastMutex((lVar9 + 0xe) * 0x38 + param_1);
  return 1;
}
```

### `FUN_14001a480` (22675-22754)
```c
void FUN_14001a480(longlong *param_1,uint param_2,int param_3,ulonglong param_4)

{
  longlong lVar1;
  undefined1 uVar2;
  longlong lVar3;
  int iVar4;
  longlong *plVar5;
  longlong *plVar6;
  longlong lVar7;
  
  lVar7 = (longlong)(int)param_2;
  ExAcquireFastMutex(param_1 + lVar7 * 7 + 0x8a);
  iVar4 = 0;
  lVar1 = *(longlong *)(param_1[2] + 0x48 + lVar7 * 8);
  if (lVar1 != 0) {
    iVar4 = *(int *)(lVar1 + 0x80);
    *(undefined4 *)(lVar1 + 0x54) = 0;
  }
  param_1[lVar7 + 0x111] = 0;
  param_1[lVar7 + 0x112] = 0;
  if (param_3 == 0) {
    if (((int)param_1[0x19] != 0) &&
       (FUN_14001c584((longlong)param_1,param_2), (int)param_1[0x19] != 0)) {
      uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + lVar7 + 0x89);
      plVar5 = param_1 + lVar7 * 4 + 0x106;
      plVar6 = (longlong *)*plVar5;
      while (plVar6 != plVar5) {
        lVar3 = ExInterlockedRemoveHeadList(plVar5,plVar5 + 2);
        *(char *)(plVar5 + 3) = (char)plVar5[3] + -1;
        lVar1 = *(longlong *)(lVar3 + 0x10);
        ExFreePoolWithTag(lVar3,0);
        if (lVar1 == 0) break;
        ExFreePoolWithTag(lVar1,0x41475046);
        plVar6 = (longlong *)*plVar5;
      }
      KeReleaseSpinLock(param_1 + lVar7 + 0x89,uVar2);
    }
    iVar4 = 0;
    *(undefined4 *)((longlong)param_1 + lVar7 * 0xc + 0xd4) = 0;
    (**(code **)(*param_1 + 0x250))(param_1,param_2);
    do {
      if ((*(int *)((longlong)param_1 + lVar7 * 4 + 0x664) != 0) ||
         (*(int *)((longlong)param_1 + 0xdc) == 0)) break;
      FUN_140036d84(10);
      iVar4 = iVar4 + 1;
    } while (iVar4 != 0x14);
    FUN_14001a2e0(param_1,0x100);
    *(undefined4 *)((longlong)param_1 + lVar7 * 4 + 0x664) = 1;
    uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + lVar7 + 0x89);
    KeRemoveQueueDpc(param_1 + lVar7 * 8 + 0x81);
    if ((param_4 & 1) == 0) {
      plVar6 = param_1 + lVar7 * 4 + 0x79;
      do {
        if ((longlong *)*plVar6 == plVar6) break;
        lVar3 = ExInterlockedRemoveHeadList(plVar6,plVar6 + 2);
        *(char *)(plVar6 + 3) = (char)plVar6[3] + -1;
        lVar1 = *(longlong *)(lVar3 + 0x10);
        ExFreePoolWithTag(lVar3,0);
      } while (lVar1 != 0);
    }
    KeReleaseSpinLock(param_1 + lVar7 + 0x89,uVar2);
  }
  else if ((*(int *)((longlong)param_1 + lVar7 * 4 + 0x664) != 0) && (iVar4 != 0)) {
    (**(code **)(*param_1 + 0xf8))
              (param_1,param_2,*(undefined4 *)((longlong)param_1 + lVar7 * 4 + 0x8c0),
               *(undefined4 *)((longlong)param_1 + lVar7 * 4 + 0x8b8),
               *(undefined4 *)((longlong)param_1 + lVar7 * 4 + 0x8bc),0);
    (**(code **)(*param_1 + 0x248))(param_1,param_2);
    *(undefined4 *)((longlong)param_1 + lVar7 * 0xc + 0xd4) = 1;
    if ((int)param_1[0x19] != 0) {
      FUN_14001c3fc((longlong)param_1,(ulonglong)param_2);
    }
    *(undefined4 *)((longlong)param_1 + lVar7 * 4 + 0x664) = 0;
  }
                    // WARNING: Could not recover jumptable at 0x00014001a72a. Too many branches
                    // WARNING: Treating indirect jump as call
  ExReleaseFastMutex(param_1 + lVar7 * 7 + 0x8a);
  return;
}
```

### `FUN_14001a740` (22760-22839)
```c
void FUN_14001a740(longlong *param_1,uint param_2,int param_3,ulonglong param_4)

{
  longlong lVar1;
  undefined1 uVar2;
  longlong lVar3;
  longlong lVar4;
  int iVar5;
  longlong *plVar6;
  longlong *plVar7;
  
  lVar4 = (longlong)(int)param_2;
  ExAcquireFastMutex(param_1 + lVar4 * 7 + 0xa2);
  iVar5 = 0;
  lVar1 = *(longlong *)(param_1[2] + 0x68 + lVar4 * 8);
  if (lVar1 != 0) {
    iVar5 = *(int *)(lVar1 + 0x80);
    *(undefined4 *)(lVar1 + 0x54) = 0;
  }
  param_1[lVar4 + 0x113] = 0;
  param_1[lVar4 + 0x114] = 0;
  if (param_3 == 0) {
    if ((int)param_1[0x19] != 0) {
      FUN_14001c558((longlong)param_1,param_2);
      uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + lVar4 + 0xa1);
      plVar6 = param_1 + lVar4 * 4 + 0x10a;
      plVar7 = (longlong *)*plVar6;
      while (plVar7 != plVar6) {
        lVar3 = ExInterlockedRemoveHeadList(plVar6,plVar6 + 2);
        *(char *)(plVar6 + 3) = (char)plVar6[3] + -1;
        lVar1 = *(longlong *)(lVar3 + 0x10);
        ExFreePoolWithTag(lVar3,0);
        if (lVar1 == 0) break;
        ExFreePoolWithTag(lVar1,0x41475046);
        plVar7 = (longlong *)*plVar6;
      }
      KeReleaseSpinLock(param_1 + lVar4 + 0xa1,uVar2);
    }
    *(undefined4 *)((longlong)param_1 + lVar4 * 0xc + 0xd8) = 0;
    (**(code **)(*param_1 + 0x260))(param_1,param_2);
    iVar5 = 0;
    do {
      if ((*(int *)((longlong)param_1 + lVar4 * 4 + 0x668) != 0) ||
         (*(int *)((longlong)param_1 + 0xdc) == 0)) break;
      FUN_140036d84(10);
      iVar5 = iVar5 + 1;
    } while (iVar5 != 0x14);
    FUN_14001a2e0(param_1,0x200);
    *(undefined4 *)((longlong)param_1 + lVar4 * 4 + 0x668) = 1;
    uVar2 = KeAcquireSpinLockRaiseToDpc(param_1 + lVar4 + 0xa1);
    KeRemoveQueueDpc(param_1 + lVar4 * 8 + 0x99);
    if ((param_4 & 1) == 0) {
      plVar7 = param_1 + lVar4 * 4 + 0x91;
      do {
        if ((longlong *)*plVar7 == plVar7) break;
        lVar3 = ExInterlockedRemoveHeadList(plVar7,plVar7 + 2);
        *(char *)(plVar7 + 3) = (char)plVar7[3] + -1;
        lVar1 = *(longlong *)(lVar3 + 0x10);
        ExFreePoolWithTag(lVar3,0);
      } while (lVar1 != 0);
    }
    KeReleaseSpinLock(param_1 + lVar4 + 0xa1,uVar2);
  }
  else if ((*(int *)((longlong)param_1 + lVar4 * 4 + 0x668) != 0) && (iVar5 != 0)) {
    (**(code **)(*param_1 + 0x100))
              (param_1,param_2,*(undefined4 *)((longlong)param_1 + lVar4 * 4 + 0x8cc),
               *(undefined4 *)((longlong)param_1 + lVar4 * 4 + 0x8c4),
               *(undefined4 *)((longlong)param_1 + lVar4 * 4 + 0x8c8),0);
    (**(code **)(*param_1 + 600))(param_1,param_2);
    *(undefined4 *)((longlong)param_1 + lVar4 * 0xc + 0xd8) = 1;
    if ((int)param_1[0x19] != 0) {
      FUN_14001c3b8((longlong)param_1,(ulonglong)param_2);
    }
    *(undefined4 *)((longlong)param_1 + lVar4 * 4 + 0x668) = 0;
  }
                    // WARNING: Could not recover jumptable at 0x00014001a9e8. Too many branches
                    // WARNING: Treating indirect jump as call
  ExReleaseFastMutex(param_1 + lVar4 * 7 + 0xa2);
  return;
}
```

### `FUN_14001c310` (23725-23742)
```c
undefined8 FUN_14001c310(longlong *param_1)

{
  byte bVar1;
  uint uVar2;
  
  FUN_14001a2e0(param_1,0x301);
  FUN_14001a0b8(param_1);
  *(undefined4 *)((longlong)param_1 + 0x924) = 0;
  FUN_14001c440((longlong)param_1);
  *(undefined4 *)((longlong)param_1 + 0xdc) = 1;
  if (DAT_1400a0720 != 0) {
    bVar1 = *(byte *)(param_1[2] + 8);
    uVar2 = FUN_1400195f4(param_1);
    FUN_14005fbf8(DAT_1400a0720,bVar1,uVar2);
  }
  return 1;
}
```

### `FUN_14001c4a0` (23803-23817)
```c
void FUN_14001c4a0(longlong *param_1)

{
  *(undefined4 *)((longlong)param_1 + 0xdc) = 0;
  if (*(int *)(param_1[2] + 0x560) != 0) {
    *(undefined2 *)(param_1 + 0xcf) = 0;
    KeSetEvent(param_1 + 0xb0,2);
  }
  FUN_14001c5b0((longlong)param_1);
  (**(code **)(*param_1 + 0x1e8))(param_1,0,0,0);
                    // WARNING: Could not recover jumptable at 0x00014001c507. Too many branches
                    // WARNING: Treating indirect jump as call
  KeRemoveQueueDpc(param_1 + 0xb3);
  return;
}
```

### `FUN_14001a2e0` (22592-22637)
```c
void FUN_14001a2e0(longlong *param_1,uint param_2)

{
  uint uVar1;
  uint uVar2;
  bool bVar3;
  
  uVar2 = 0;
  if (*(int *)((longlong)param_1 + 0xdc) != 0) {
    if ((param_2 & 1) != 0) {
      (**(code **)(*param_1 + 0x58))(param_1,0xc,1);
      uVar1 = uVar2;
      do {
        FUN_140036d84(1);
        if ((*(uint *)(param_1[3] + 0xc) & 8) != 0) break;
        bVar3 = uVar1 < 10;
        uVar1 = uVar1 + 1;
      } while (bVar3);
      FUN_140036d84(0x1e);
      *(undefined8 *)((longlong)param_1 + 0x92c) = 0;
      *(undefined8 *)((longlong)param_1 + 0x934) = 0;
    }
    if ((param_2 >> 8 & 1) != 0) {
      (**(code **)(*param_1 + 0x58))(param_1,0xc,0x100);
      uVar1 = uVar2;
      do {
        FUN_140036d84(1);
        if ((*(uint *)(param_1[3] + 0xc) >> 8 & 1) != 0) break;
        bVar3 = uVar1 < 10;
        uVar1 = uVar1 + 1;
      } while (bVar3);
      FUN_140036d84(0x1e);
    }
    if ((param_2 & 0x200) != 0) {
      (**(code **)(*param_1 + 0x58))(param_1,0xc,0x200);
      do {
        FUN_140036d84(1);
        if ((*(uint *)(param_1[3] + 0xc) & 0x200) != 0) break;
        bVar3 = uVar2 < 10;
        uVar2 = uVar2 + 1;
      } while (bVar3);
      FUN_140036d84(0x1e);
    }
  }
  return;
}
```

### `FUN_14001a0b8` (22510-22570)
```c
void FUN_14001a0b8(longlong *param_1)

{
  undefined4 uVar1;
  code *pcVar2;
  uint uVar3;
  undefined8 uVar4;
  
  ExAcquireFastMutex();
  *(undefined4 *)(param_1 + 0x10e) = *(undefined4 *)param_1[3];
  *(undefined4 *)((longlong)param_1 + 0x874) = *(undefined4 *)(param_1[3] + 100);
  if ((*(int *)(param_1[2] + 0x24) == 0x5730) && (*(uint *)(param_1 + 0x10e) < 0x20190904)) {
    uVar3 = 1;
  }
  else {
    uVar3 = 0;
  }
  *(uint *)(param_1 + 0x19) = uVar3;
  pcVar2 = FUN_140017c30;
  *(uint *)(param_1 + 1) = uVar3 ^ 1;
  *(uint *)((longlong)param_1 + 0xcc) = (-(uint)(uVar3 != 0) & 3) + 1;
  if (uVar3 != 0) {
    pcVar2 = FUN_1400179d0;
  }
  KeInitializeDpc(param_1 + 0x81,pcVar2,param_1);
  pcVar2 = FUN_1400180d0;
  if ((int)param_1[0x19] != 0) {
    pcVar2 = FUN_140017e70;
  }
  KeInitializeDpc(param_1 + 0x99,pcVar2,param_1);
  (**(code **)(*param_1 + 0x58))(param_1,0x40,*(uint *)(param_1[3] + 0x40) | 8);
  FUN_140036d84(100);
  uVar1 = 0x3d;
  if (*(short *)((longlong)param_1 + 0xe2) == 100) {
    uVar1 = 0xf9;
  }
  (**(code **)(*param_1 + 0x58))(param_1,0x120,uVar1);
  (**(code **)(*param_1 + 0x58))(param_1,0x124,0);
  (**(code **)(*param_1 + 0x58))(param_1,0x128,0x80);
  FUN_140036d84(100);
  (**(code **)(*param_1 + 0x58))
            (param_1,0x180,0x1e848 / (ulonglong)*(ushort *)((longlong)param_1 + 0xe2));
  FUN_140036d84(10);
  (**(code **)(*param_1 + 0x58))(param_1,0x10,0x1fff);
  if (0x19081600 < *(uint *)(param_1 + 0x10e)) {
    if (*(int *)(param_1[2] + 0x24) == 0x5550) {
      uVar4 = 0x1b33;
    }
    else {
      if ((*(uint *)(param_1 + 0x10e) < 0x20190904) || (*(int *)(param_1[2] + 0x24) != 0x5730))
      goto LAB_14001a2a5;
      uVar4 = 0xb33;
    }
    (**(code **)(*param_1 + 0x58))(param_1,0x1c,uVar4);
  }
LAB_14001a2a5:
                    // WARNING: Could not recover jumptable at 0x00014001a2b6. Too many branches
                    // WARNING: Treating indirect jump as call
  ExReleaseFastMutex(param_1 + 0x62);
  return;
}
```

### `FUN_140044484` (64908-64920)
```c
undefined1 FUN_140044484(longlong param_1,byte param_2)

{
  bool bVar1;
  undefined7 extraout_var;
  undefined1 local_res18 [16];
  
  bVar1 = FUN_1400392c0(param_1,0x90,param_2,1,local_res18);
  if (((short)CONCAT71(extraout_var,bVar1) == 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("=====HDMI Read ERROR Read Reg0x%X=\n",param_2);
  }
  return local_res18[0];
}
```

### `FUN_1400444d0` (64924-64932)
```c
void FUN_1400444d0(longlong param_1,byte param_2,byte param_3,byte param_4)

{
  byte bVar1;
  
  bVar1 = FUN_140044484(param_1,param_2);
  FUN_140044528(param_1,param_2,(bVar1 & ~param_3) + (param_3 & param_4));
  return;
}
```

### `FUN_140044528` (64936-64954)
```c
bool FUN_140044528(longlong param_1,byte param_2,undefined1 param_3)

{
  bool bVar1;
  short sVar2;
  undefined7 extraout_var;
  undefined1 local_res18 [16];
  
  local_res18[0] = param_3;
  bVar1 = FUN_1400392f8(param_1,0x90,param_2,1,local_res18);
  sVar2 = (short)CONCAT71(extraout_var,bVar1);
  if ((*(char *)(param_1 + 0x1f0) == '\x01') && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("i2c_single_write(0x90, 0x%02X, 1, 0x%02X);\n",param_2,local_res18[0]);
  }
  if ((sVar2 == 0) && ((DAT_1400a04f8 & 0x10) != 0)) {
    DbgPrint("=====HDMI I2C ERROR Write Reg0x%X=%X =====\n",param_2,local_res18[0]);
  }
  return sVar2 == 0;
}
```

