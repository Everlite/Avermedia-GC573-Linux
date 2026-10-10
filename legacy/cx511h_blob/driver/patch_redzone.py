#!/usr/bin/env python3
"""
patch_redzone.py — Make the vendor blob safe to run with interrupts enabled.

AverMediaLib_64.a was built without -mno-red-zone. Its leaf functions keep
locals below %rsp (push %rbp; mov %rsp,%rbp; mov %rdi,-0x18(%rbp) ...). In
the kernel an interrupt pushes SS/RSP/RFLAGS/CS/RIP right there, so any IRQ
landing in such a function corrupts its locals (seen as a NULL dereference
of 0x297 + 0x58 in aver_xilinx_is_TX_available: 0x297 is RFLAGS).

Each affected function is patched in place, without changing any length:
  push %rbp; mov %rsp,%rbp  (55 48 89 e5)  ->  enter $0x100,$0  (c8 00 01 00)
  pop %rbp                  (5d)           ->  leave            (c9)
enter reserves the stack the function already uses, leave restores %rsp
before popping %rbp. Relocations never cover these bytes.
"""

import os
import re
import struct
import subprocess
import sys
import tempfile

PROLOGUE = bytes.fromhex("554889e5")
ENTER = bytes.fromhex("c8000100")
POP_RBP = 0x5D
LEAVE = 0xC9
MAX_FRAME = 0x100

FUNC_RE = re.compile(r"^([0-9a-f]+) <([^>]+)>:$")
INSN_RE = re.compile(r"^\s+([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)$")
SECTION_RE = re.compile(r"^Disassembly of section (\S+):$")
NEG_RBP_RE = re.compile(r"-0x([0-9a-f]+)\(%rbp\)")


def archive_members(data):
    if data[:8] != b"!<arch>\n":
        raise SystemExit("not an ar archive")
    off = 8
    while off + 60 <= len(data):
        hdr = data[off:off + 60]
        name = hdr[0:16].decode(errors="replace").rstrip(" ").rstrip("/")
        size = int(hdr[48:58].decode().strip())
        yield name, off + 60, size
        off += 60 + size + (size % 2)


def section_offsets(elf):
    shoff, = struct.unpack_from("<Q", elf, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x3A)
    headers = [struct.unpack_from("<IIQQQQIIQQ", elf, shoff + i * shentsize)
               for i in range(shnum)]
    strtab = headers[shstrndx][4]
    offsets = {}
    for h in headers:
        start = strtab + h[0]
        name = elf[start:elf.index(b"\0", start)].decode()
        offsets[name] = h[4]
    return offsets


def disassemble(elf):
    with tempfile.NamedTemporaryFile(suffix=".o", delete=False) as tmp:
        tmp.write(elf)
        path = tmp.name
    try:
        return subprocess.run(["objdump", "-d", "-w", path], check=True,
                              capture_output=True, text=True).stdout
    finally:
        os.unlink(path)


def functions(listing):
    section, func = None, None
    for line in listing.splitlines():
        m = SECTION_RE.match(line)
        if m:
            section = m.group(1)
            continue
        m = FUNC_RE.match(line)
        if m:
            if func:
                yield func
            func = {"section": section, "name": m.group(2), "insns": []}
            continue
        m = INSN_RE.match(line)
        if m and func:
            func["insns"].append((int(m.group(1), 16),
                                  bytes.fromhex(m.group(2).replace(" ", "")),
                                  m.group(3)))
    if func:
        yield func


def redzone_patch_sites(func):
    insns = func["insns"]
    if len(insns) < 2 or insns[0][1] + insns[1][1] != PROLOGUE:
        return None
    texts = [text for _, _, text in insns]
    if any(re.search(r"sub\s+\$0x[0-9a-f]+,%rsp", t) for t in texts):
        return None
    if any(t.startswith(("call", "leave")) for t in texts):
        return None
    offsets = [int(v, 16) for t in texts for v in NEG_RBP_RE.findall(t)]
    if not offsets or max(offsets) > MAX_FRAME:
        return None
    pops = [addr for addr, raw, _ in insns if raw == bytes([POP_RBP])]
    rets = [t for t in texts if t.startswith("ret")]
    if len(pops) != 1 or len(rets) != 1:
        return None
    return insns[0][0], pops[0]


def patch_member(elf):
    listing = disassemble(bytes(elf))
    offsets = section_offsets(elf)
    patched = []
    for func in functions(listing):
        sites = redzone_patch_sites(func)
        if not sites:
            continue
        base = offsets[func["section"]]
        start, pop = sites
        elf[base + start:base + start + 4] = ENTER
        elf[base + pop] = LEAVE
        patched.append(func["name"])
    return patched


def main(path):
    with open(path, "rb") as f:
        data = bytearray(f.read())

    total = 0
    for name, start, size in list(archive_members(data)):
        # GNU ar stores names longer than 15 chars as "/<offset>": match on
        # the ELF magic instead of the member name.
        if data[start:start + 4] != b"\x7fELF":
            continue
        elf = bytearray(data[start:start + size])
        patched = patch_member(elf)
        if patched:
            data[start:start + size] = elf
            total += len(patched)
            print("[patch-redzone] %s: %d functions" % (name, len(patched)))

    if total == 0:
        raise SystemExit("[patch-redzone] 0 leaf functions patched; refusing to link the blob")

    with open(path, "wb") as f:
        f.write(data)
    print("[patch-redzone] %d leaf functions now reserve their stack frame" % total)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_redzone.py AverMediaLib_64.o")
    main(sys.argv[1])
