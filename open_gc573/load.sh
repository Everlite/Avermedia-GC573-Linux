#!/usr/bin/env bash
# load.sh - Safely swap to open_gc573 driver for AVerMedia Live Gamer 4K GC573
set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
    echo "[!] Bitte mit sudo ausführen: sudo ./open_gc573/load.sh"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MODULE="${SCRIPT_DIR}/open_gc573.ko"

if [[ ! -f "${MODULE}" ]]; then
    echo "[*] open_gc573.ko nicht gefunden. Kompiliere..."
    make -C "${SCRIPT_DIR}" LLVM=1 CC=clang
fi

echo "[*] Prüfe und bereinige geladene Module..."
rmmod open_gc573 2>/dev/null || true

if grep -q "^cx511h " /proc/modules; then
    echo "[*] Entlade proprietären cx511h Treiber über unload.sh..."
    "${REPO_ROOT}/unload.sh"
fi

echo "[*] Lade Kernel-Abhängigkeiten (v4l2-dv-timings, videobuf2, etc.)..."
modprobe v4l2-dv-timings || true
modprobe videobuf2-dma-sg || true
modprobe videobuf2-v4l2 || true
modprobe led-class-multicolor || true
modprobe snd-pcm || true

echo "[*] Lade neuen open_gc573 Treiber (${MODULE})..."
insmod "${MODULE}"

# Sysfs RGB LED Zugriff für Benutzer ohne Root-Rechte freigeben
chmod 0666 /sys/bus/pci/drivers/open_gc573/*/led_mode 2>/dev/null || true

echo "[+] open_gc573 erfolgreich geladen!"

echo "[*] Warte auf HDMI-Signal-Lock (max. 5s)..."
SIGNAL_FOUND=0
for _ in $(seq 1 25); do
    VDEV="$(for d in /sys/class/video4linux/video*; do
        [[ "$(readlink -f "$d/device/driver")" == */open_gc573 ]] && echo "/dev/$(basename "$d")"
    done | head -n 1 || true)"
    if [[ -n "${VDEV}" ]] && v4l2-ctl -d "${VDEV}" --query-dv-timings >/dev/null 2>&1; then
        echo "[+] Signal erkannt auf ${VDEV}"
        SIGNAL_FOUND=1
        break
    fi
    sleep 0.2
done
if [[ "${SIGNAL_FOUND}" -eq 0 ]]; then
    echo "[!] Hinweis: Bisher kein HDMI-Signal-Lock erkannt (Quelle aus oder Handshake ausstehend)."
fi
echo ""
echo "=== PCI Treiber Zuordnung ==="
lspci -k -d 1461: || true

echo ""
echo "=== Kernel Meldungen (dmesg) ==="
dmesg | tail -n 25

echo ""
echo "=== V4L2 Geräte ==="
v4l2-ctl --list-devices || true
