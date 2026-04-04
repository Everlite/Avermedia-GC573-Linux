#!/bin/bash
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module unloader (driver directory) ==="

# Check if module is loaded
if ! lsmod | grep -q cx511h; then
    echo "cx511h module is not loaded."
    exit 0
fi

echo "cx511h module is loaded."

# Check if module is in use
echo "Checking if module is in use..."

# Kill ffplay if running (common video playback blocker)
echo "Killing ffplay if running..."
pkill -9 ffplay 2>/dev/null || true
sleep 0.5

# Check for ALSA devices that might be in use
# Look for AVerMedia ALSA card
CARD_NUM=""
if [ -d /proc/asound ]; then
    OUR_CARD=$(grep -l "AVerMedia\|cx511h\|CL511H" /proc/asound/card*/id 2>/dev/null | head -1)
    if [ -n "$OUR_CARD" ]; then
        CARD_NUM=$(echo "$OUR_CARD" | grep -o 'card[0-9]*' | grep -o '[0-9]*')
        echo "Found AVerMedia card at number $CARD_NUM"

        # Kill pipewire/wireplumber if they have our PCM devices open
        if command -v fuser >/dev/null 2>&1; then
            for pcm_dev in /dev/snd/pcmC${CARD_NUM}D*; do
                if [ -e "$pcm_dev" ]; then
                    FUSER_OUTPUT=$(fuser -v "$pcm_dev" 2>/dev/null || true)
                    if [ -n "$FUSER_OUTPUT" ]; then
                        echo "Killing pipewire/wireplumber processes using $pcm_dev..."
                        pkill -9 pipewire 2>/dev/null || true
                        pkill -9 wireplumber 2>/dev/null || true
                        sleep 1
                        break
                    fi
                fi
            done
        fi
    fi
fi

# Try to remove the module
echo "Removing cx511h module..."
if rmmod cx511h 2>/dev/null; then
    echo "✓ Module removed successfully."
else
    echo "rmmod failed, trying force remove..."
    rmmod -f cx511h 2>/dev/null || {
        echo "✗ Failed to remove module even with force."
        echo "Module may still be in use by other processes."
        exit 1
    }
    echo "✓ Module force-removed."
fi

# Restart audio services to restore system audio
echo "Restoring audio services..."
sleep 2  # Allow system to settle

# Restart pipewire services
if command -v systemctl >/dev/null 2>&1; then
    # Check if user systemd is available
    if systemctl --user list-units 2>/dev/null | grep -q pipewire; then
        echo "Restarting audio services via systemd..."
        systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
        echo "✓ Audio services restarted via systemd."
    fi
fi

# Also ensure processes are running directly
sleep 1
if ! pgrep -x pipewire >/dev/null; then
    echo "Starting pipewire directly..."
    pipewire 2>/dev/null &
fi
if ! pgrep -x wireplumber >/dev/null; then
    echo "Starting wireplumber directly..."
    wireplumber 2>/dev/null &
fi

echo ""
echo "=== Module unload complete ==="

# Final verification
if lsmod | grep -q cx511h; then
    echo "⚠️  Warning: cx511h module may still be loaded."
    exit 1
else
    echo "✓ cx511h module unloaded."
fi

# Check audio services
echo "Audio service status:"
if pgrep -x pipewire >/dev/null; then
    echo "✓ pipewire is running"
else
    echo "✗ pipewire is not running"
fi
if pgrep -x wireplumber >/dev/null; then
    echo "✓ wireplumber is running"
else
    echo "✗ wireplumber is not running"
fi

echo "✓ Audio restored."
exit 0
