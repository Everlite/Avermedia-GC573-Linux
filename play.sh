#!/usr/bin/env bash
# play.sh - One-click launcher for EverLive Console Gaming Screen
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VIEWER="${SCRIPT_DIR}/tools/everlive/everlive_viewer.py"

# Check if open_gc573 is loaded
if ! grep -q "^open_gc573 " /proc/modules; then
    echo "[!] open_gc573 Treiber ist nicht geladen."
    echo "[*] Lade Treiber..."
    if [ ! -t 0 ]; then
        # Launched from application launcher / desktop menu without a terminal
        if command -v pkexec >/dev/null 2>&1; then
            if ! pkexec "${SCRIPT_DIR}/open_gc573/load.sh"; then
                if command -v kdialog >/dev/null 2>&1; then
                    kdialog --error "Authentifizierung abgebrochen: open_gc573 Treiber konnte nicht geladen werden."
                elif command -v zenity >/dev/null 2>&1; then
                    zenity --error --text="Authentifizierung abgebrochen: open_gc573 Treiber konnte nicht geladen werden."
                fi
                exit 1
            fi
        elif command -v kdesu >/dev/null 2>&1; then
            kdesu "${SCRIPT_DIR}/open_gc573/load.sh"
        else
            if command -v kdialog >/dev/null 2>&1; then
                kdialog --error "Der open_gc573 Treiber ist nicht geladen. Bitte führe im Terminal einmal 'sudo ./open_gc573/load.sh' aus."
            elif command -v zenity >/dev/null 2>&1; then
                zenity --error --text="Der open_gc573 Treiber ist nicht geladen. Bitte führe im Terminal einmal 'sudo ./open_gc573/load.sh' aus."
            fi
            exit 1
        fi
    else
        sudo "${SCRIPT_DIR}/open_gc573/load.sh"
    fi
fi

if [ -x "${SCRIPT_DIR}/tools/everlive/.venv/bin/python3" ]; then
    exec "${SCRIPT_DIR}/tools/everlive/.venv/bin/python3" "${VIEWER}" "$@"
elif [ -x "${SCRIPT_DIR}/tools/everlive/.venv/bin/python" ]; then
    exec "${SCRIPT_DIR}/tools/everlive/.venv/bin/python" "${VIEWER}" "$@"
else
    exec "${VIEWER}" "$@"
fi
