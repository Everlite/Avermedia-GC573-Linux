#!/bin/bash
# reload.sh — Safely unload + reload the cx511h module WITHOUT a reboot.
#
# unload.sh is the authoritative, now-safe unloader (it force-kills the audio
# stack that pins the ALSA PCM, rmmods cleanly, and NO LONGER touches the PCI
# 'remove' file that used to wedge the card).  This script reuses that logic by
# delegating to unload.sh and only insmod's the freshly built module after the
# unload actually succeeded.
#
# Usage:  sudo ./reload.sh
set -u

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)." >&2
    exit 1
fi

cd "$(dirname "$0")" || exit 1

echo "=== cx511h reload (safe) ==="

# ---- 1) Unload via the safe unloader ----
# Exit code 3 means the module is wedged and a reboot is required; exit 0/other
# successful paths are fine.  unload.sh force-kills PipeWire/WirePlumber so the
# ALSA PCM is released and rmmod succeeds without a reboot in the normal case.
if lsmod | grep -q '^cx511h '; then
    echo "[*] Running safe unload..."
    ./unload.sh
    rc=$?
    if [ "$rc" -eq 3 ]; then
        echo "! Module could not be unloaded (wedged). A reboot is required."
        echo "  After reboot:  sudo ./reload.sh"
        exit 2
    fi
else
    echo "[*] Module not loaded; nothing to unload."
fi
sleep 1

# ---- 2) Load the freshly built module (insmod, no install/depmod) ----
if [ ! -f cx511h.ko ]; then
    echo "ERROR: cx511h.ko not found — run ./build.sh first." >&2
    exit 1
fi
echo "[*] Loading cx511h.ko..."
insmod ./cx511h.ko && echo "[+] Module loaded."
sleep 1
lsmod | grep -q '^cx511h ' || echo "! Warning: module not visible after insmod."

# ---- 3) Report status ----
echo ""
echo "refcnt:    $(cat /sys/module/cx511h/refcnt 2>/dev/null || echo '?')"
echo "initstate: $(cat /sys/module/cx511h/initstate 2>/dev/null || echo '?')"
echo "video node(s):"
for v in /sys/class/video4linux/video*; do
    [ -e "$v/device" ] || continue
    case "$(cat "$v/name" 2>/dev/null)" in *CL511H*|*AVerMedia*)
        echo "  /dev/${v##*/} -> $(cat "$v/name")";; esac
done
echo "=== done ==="
