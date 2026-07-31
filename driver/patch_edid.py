#!/usr/bin/env python3
"""
patch_edid.py — Force the card to present a 1080p-max EDID to the HDMI source.

The ITE6805 front-end loads its EDID tables from the blob's
ite6805_EDID.o  (symbols Default_Edid_Block / Fix_Edid_Block).  Those tables
advertise 4K, so sources (e.g. a PS5 forced to its internal 4K pipeline) send
4K and the driver must downscale through the fragile dual-pixel path, which
currently yields a black picture.

Here we overwrite BOTH tables with a valid, checksum-correct 1080p-max EDID.
The chip then advertises 1080p/720p/480p only, so the source drops to native
1080p and the known-good single-pixel path is used.

The patch is byte-precise and recomputes the symbol offsets from the archive
every run so it survives re-extraction or rebuilds.
"""

import io
import sys

# 1080p-max EDID, block 0 (base) and block 1 (CEA-861, 1080p only).
BLK0 = bytes([
    0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x06,0xd8,0x42,0x00,0x00,0x00,0x00,0x00,
    0x24,0x22,0x01,0x03,0x80,0xa0,0x5a,0x78,0xea,0x08,0xa5,0xa2,0x57,0x4f,0xa2,0x28,
    0x0f,0x50,0x54,0x21,0x08,0x00,0xd1,0xc0,0x81,0xc0,0x81,0x80,0x81,0x00,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x3a,0x80,0x18,0x71,0x38,0x2d,0x40,0x58,0x2c,
    0x45,0x00,0x40,0x84,0x63,0x00,0x00,0x1e,0x01,0x1d,0x00,0x72,0x51,0xd0,0x1e,0x20,
    0x6e,0x28,0x55,0x00,0xc4,0x8e,0x21,0x00,0x00,0x1e,0x00,0x00,0x00,0xfc,0x00,0x41,
    0x56,0x54,0x20,0x43,0x4c,0x35,0x31,0x31,0x2d,0x48,0x4e,0x0a,0x00,0x00,0x00,0xfd,
    0x00,0x32,0xf0,0x1e,0xde,0x3c,0x00,0x0a,0x20,0x20,0x20,0x20,0x20,0x20,0x01,0x4a,
])

BLK1 = bytes([
    0x02,0x03,0x60,0x18,0x02,0x09,0x90,0x10,0x1f,0x04,0x13,0x05,0x14,0x03,0x01,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x02,0x3a,0x80,0x18,0x71,0x38,0x2d,0x40,0x58,0x2c,0x45,0x00,0x40,0x84,0x63,0x00,
    0x00,0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8d,
])

EDID = BLK0 + BLK1

# IT6664 HDMI2-RX default EDID tables.  This is the front-end chip the source
# (e.g. a PS5) negotiates with.  Default_Edid_table4k advertises 4K; we
# replace both with the same 1080p-max EDID so the source drops to native
# 1080p regardless of which table is selected.
IT6664_TABLE4K_SIG = bytes.fromhex("00ffffffffffff002685636607021720")
IT6664_TABLE2K_SIG = bytes.fromhex("00ffffffffffff002685636607021720")


def checksum_ok(block):
    return (sum(block) % 256) == 0


def find_all(data, needle):
    """Return list of byte offsets where needle occurs in data."""
    res = []
    start = 0
    while True:
        i = data.find(needle, start)
        if i < 0:
            break
        res.append(i)
        start = i + 1
    return res


def find_member(data, name_substr):
    """Return (data_offset, size) of the archive member whose name contains name_substr."""
    if data[:8] != b"!<arch>\n":
        return None
    off = len(b"!<arch>\n")
    while off + 60 <= len(data):
        hdr = data[off:off + 60]
        nm = hdr[0:16].decode(errors="replace").rstrip(" ")
        try:
            size = int(hdr[48:58].decode().strip())
        except ValueError:
            break
        if name_substr in nm:
            return (off + 60, size)
        off += 60 + size
        if size % 2:
            off += 1
    return None


def patch_archive(path, name_substr="ite6805_EDID.o", verbose=True):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"!<arch>\n":
        raise SystemExit("not a valid ar archive: " + path)

    changed = False

    # --- 1) ITE6805 EDID tables (Default_Edid_Block / Fix_Edid_Block) ---
    hit = find_member(data, name_substr)
    if not hit:
        raise SystemExit("member '%s' not found in %s" % (name_substr, path))
    mdata, msize = hit

    default_off = mdata + 0x980 + 0x000
    fix_offset = mdata + 0x980 + 0x120

    if data[default_off:default_off + 4] != b"\x00\xff\xff\xff":
        raise SystemExit("EDID magic not found at expected offset 0x%x" % default_off)

    buf = bytearray(data)
    buf[default_off:default_off + 256] = EDID
    buf[fix_offset:fix_offset + 256] = EDID
    buf[:0] = b""
    data = bytes(buf)

    # --- 2) IT6664 HDMI2-RX default EDID tables (4k and 2k) ---
    hits4k = [i for i in find_all(data, IT6664_TABLE4K_SIG)
              if i + 256 <= len(data) and (sum(data[i:i + 256]) % 256) == 0]
    # table4k and table2k share the same 16-byte base signature; the two are the
    # first two valid 256-byte tables found (they are adjacent in the member).
    if len(hits4k) < 2:
        if verbose:
            print("[patch-edid] WARN: expected 2 IT6664 EDID tables, found %d"
                  % len(hits4k), file=__import__("sys").stderr)
    buf = bytearray(data)
    for off in hits4k[:2]:
        buf[off:off + 256] = EDID
    data = bytes(buf)

    with open(path, "wb") as f:
        f.write(data)

    # --- Self-verify all patched regions + checksums ---
    v_ite0 = data[default_off:default_off + 256]
    v_iteF = data[fix_offset:fix_offset + 256]
    ok = (v_ite0 == EDID and v_iteF == EDID
          and checksum_ok(v_ite0[:128]) and checksum_ok(v_ite0[128:]))
    for off in hits4k[:2]:
        t = data[off:off + 256]
        if t != EDID or not checksum_ok(t[:128]) or not checksum_ok(t[128:]):
            ok = False

    if verbose:
        print("[patch-edid] patched %s: ite6805 Default/Fix @0x%x/0x%x, "
              "it6664 tables @[0x%x..0x%x] <- 1080p-max EDID, verify=%s"
              % (path, default_off, fix_offset,
                 hits4k[0] if hits4k else -1,
                 hits4k[-1] if hits4k else -1,
                 "OK" if ok else "FAILED"))
    if not ok:
        raise SystemExit("EDID patch self-verification failed")
    return default_off, hits4k[:2]


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: patch_edid.py <AverMediaLib_64.a|AverMediaLib_64.o>")
    patch_archive(sys.argv[1])
