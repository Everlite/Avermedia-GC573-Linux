#!/bin/bash
# Install script for Avermedia GC573 Driver
# Installs cx511h.ko into the running kernel's module tree and updates module dependencies.
set -euo pipefail

# Check for root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)." >&2
    exit 1
fi

# Verify the module exists before trying to install
if [ ! -f cx511h.ko ]; then
    echo "ERROR: cx511h.ko not found in current directory." >&2
    echo "       Run ./build.sh first to compile the module." >&2
    exit 1
fi

MODULE_INSTALL_DIR="/lib/modules/$(uname -r)/kernel/drivers/media/avermedia"

echo "==> Creating module directory: ${MODULE_INSTALL_DIR}"
install -d "${MODULE_INSTALL_DIR}"

echo "==> Installing cx511h.ko..."
install -m 644 cx511h.ko "${MODULE_INSTALL_DIR}"

echo "==> Running depmod..."
# depmod may be in /sbin or /usr/sbin depending on the distribution
if command -v depmod >/dev/null 2>&1; then
    depmod -a
elif [ -x /sbin/depmod ]; then
    /sbin/depmod -a
else
    echo "ERROR: depmod not found. Is kmod installed?" >&2
    exit 1
fi

echo "==> Install complete. You can now run insmod.sh to load the module."
