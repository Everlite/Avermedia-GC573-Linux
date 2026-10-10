#!/usr/bin/env bash
# play.sh - One-click launcher for EverLive Console Gaming Screen
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIEWER="${SCRIPT_DIR}/tools/everlive/everlive_viewer.py"

# Check if open_gc573 is loaded
if ! grep -q "^open_gc573 " /proc/modules; then
    echo "[!] open_gc573 Treiber ist nicht geladen."
    echo "[*] Lade Treiber..."
    sudo "${SCRIPT_DIR}/open_gc573/load.sh"
fi

exec "${VIEWER}" "$@"
