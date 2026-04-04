#!/bin/bash
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module loader (driver directory) ==="

# Kill blocking processes
echo "Killing blocking audio/video processes..."
pkill -9 ffplay 2>/dev/null || true
pkill -9 pipewire 2>/dev/null || true
pkill -9 wireplumber 2>/dev/null || true
sleep 1

# Check if module is already loaded
if lsmod | grep -q cx511h; then
    echo "Removing existing cx511h module..."
    rmmod cx511h 2>/dev/null || {
        echo "Module is in use, attempting force remove..."
        rmmod -f cx511h 2>/dev/null || true
    }
    sleep 1
fi

# Load dependencies IN ORDER (non-fatal)
echo "Loading kernel module dependencies..."
modprobe videobuf2-common 2>/dev/null || true
modprobe videobuf2-memops 2>/dev/null || true
modprobe videobuf2-v4l2 2>/dev/null || true
modprobe videobuf2-vmalloc 2>/dev/null || true
modprobe videobuf2-dma-contig 2>/dev/null || true
modprobe videobuf2-dma-sg 2>/dev/null || true
modprobe videodev 2>/dev/null || true
# media module not available on all kernels, skipping
modprobe snd 2>/dev/null || true
modprobe snd-pcm 2>/dev/null || true
echo "✓ Dependencies loaded."

# Load our module
echo "Loading cx511h module..."
if insmod cx511h.ko; then
    echo "✓ cx511h module loaded successfully."

    # Verify module is actually loaded
    if lsmod | grep -q cx511h; then
        echo "✓ Module verification: cx511h is in kernel module list"
    else
        echo "⚠️  Warning: cx511h not found in module list"
    fi

    # Check for video device
    if test -c /dev/video0; then
        echo "✓ Video device created: /dev/video0 exists"
    else
        echo "⚠️  Warning: /dev/video0 not created"
        # Check for any video devices
        VIDEO_DEVICES=$(ls /dev/video* 2>/dev/null | wc -l)
        if [ "$VIDEO_DEVICES" -gt 0 ]; then
            echo "  Found video devices: $(ls /dev/video* 2>/dev/null | tr '\n' ' ')"
        fi
    fi
else
    echo "✗ Failed to load cx511h module."
    exit 1
fi

# Restart audio services
echo "Restoring audio services..."
sleep 2

# Try to restart pipewire services
if command -v systemctl >/dev/null 2>&1 && systemctl --user list-units 2>/dev/null | grep -q pipewire; then
    systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
    echo "✓ Audio services restarted via systemd."
else
    # Start directly if systemctl not available
    if ! pgrep -x pipewire >/dev/null; then
        pipewire 2>/dev/null &
    fi
    if ! pgrep -x wireplumber >/dev/null; then
        wireplumber 2>/dev/null &
    fi
    echo "✓ Audio services started directly."
fi

echo ""
echo "=== Module load complete ==="
echo "✓ cx511h loaded, audio restored."

# Final check
if lsmod | grep -q cx511h; then
    echo "✓ Module verification: cx511h is loaded"
else
    echo "✗ Module verification: cx511h is NOT loaded"
    exit 1
fi

exit 0
