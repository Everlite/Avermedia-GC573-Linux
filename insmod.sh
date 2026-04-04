#!/bin/bash
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module loader with smart audio handling ==="

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to find our ALSA card number
find_our_card() {
    if [ -d /proc/asound ]; then
        # Look for card with AVerMedia or CL511H in name
        OUR_CARD=$(grep -l "AVerMedia\|cx511h\|CL511H" /proc/asound/card*/id 2>/dev/null | head -1)
        if [ -n "$OUR_CARD" ]; then
            # Extract card number
            CARD_NUM=$(echo "$OUR_CARD" | grep -o 'card[0-9]*' | grep -o '[0-9]*')
            echo "$CARD_NUM"
            return 0
        fi
    fi
    echo ""
    return 1
}

# Function to kill blocking processes for a specific card
kill_blocking_processes() {
    local CARD_NUM=$1
    local KILLED_PROCESSES=""

    echo "Checking for processes blocking card $CARD_NUM..."

    # Kill ffplay if running (common video playback blocker)
    echo "Killing ffplay if running..."
    pkill -9 ffplay 2>/dev/null || true
    sleep 0.5

    # Check pcm devices for our card
    for pcm_dev in /dev/snd/pcmC${CARD_NUM}D*; do
        if [ -e "$pcm_dev" ]; then
            if command_exists fuser; then
                # Get processes using this device
                FUSER_OUTPUT=$(fuser -v "$pcm_dev" 2>/dev/null || true)
                if [ -n "$FUSER_OUTPUT" ]; then
                    echo "Processes using $pcm_dev:"
                    echo "$FUSER_OUTPUT"

                    # Kill pipewire/wireplumber processes only
                    while read -r line; do
                        if [[ "$line" =~ ^([a-zA-Z0-9_]+)[[:space:]]+([0-9]+) ]]; then
                            proc_name="${BASH_REMATCH[1]}"
                            pid="${BASH_REMATCH[2]}"

                            # Only kill audio service processes
                            if [[ "$proc_name" =~ pipewire|wireplumber ]]; then
                                echo "Killing blocking process: $proc_name (PID: $pid)"
                                kill -9 "$pid" 2>/dev/null || true
                                KILLED_PROCESSES="$KILLED_PROCESSES $proc_name"
                            fi
                        fi
                    done <<< "$FUSER_OUTPUT"
                fi
            fi
        fi
    done

    echo "$KILLED_PROCESSES"
}

# Function to restart audio services
restart_audio_services() {
    echo "Restarting audio services..."
    sleep 2  # Allow system to settle

    # Try systemctl --user first (for user services)
    if command_exists systemctl; then
        # Check if user systemd is available
        if systemctl --user list-units 2>/dev/null | grep -q pipewire; then
            echo "Restarting user audio services..."
            systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
            echo "✓ User audio services restarted."
        fi
    fi

    # Also ensure processes are running
    sleep 1
    if ! pgrep -x pipewire >/dev/null; then
        echo "Starting pipewire directly..."
        pipewire 2>/dev/null &
    fi
    if ! pgrep -x wireplumber >/dev/null; then
        echo "Starting wireplumber directly..."
        wireplumber 2>/dev/null &
    fi

    echo "✓ Audio services restored."
}

# Main script logic
MODULE_LOADED=$(lsmod | grep -q cx511h && echo "yes" || echo "no")
CARD_NUM=""
KILLED_AUDIO=""

if [ "$MODULE_LOADED" = "yes" ]; then
    echo "cx511h module is already loaded."

    # Find our ALSA card
    CARD_NUM=$(find_our_card)
    if [ -n "$CARD_NUM" ]; then
        echo "Found AVerMedia card at number $CARD_NUM"

        # Check and kill blocking processes
        KILLED_AUDIO=$(kill_blocking_processes "$CARD_NUM")

        # Try to remove the module
        echo "Removing existing module..."
        if rmmod cx511h 2>/dev/null; then
            echo "✓ Old module removed."
        else
            echo "Failed to remove module. It may still be in use."
            echo "Trying force remove..."
            rmmod -f cx511h 2>/dev/null || {
                echo "⚠️  Could not remove existing module. Attempting to load anyway..."
            }
        fi
        sleep 1
    else
        echo "Could not find AVerMedia ALSA card. Attempting to remove module..."
        rmmod cx511h 2>/dev/null || true
        sleep 1
    fi
else
    echo "cx511h module is not loaded."
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
cd "$(dirname "$0")/driver"
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

# Restart audio services if we killed them
if [ -n "$KILLED_AUDIO" ]; then
    restart_audio_services
fi

echo ""
echo "=== Module load complete ==="
echo "✓ cx511h loaded, audio restored."
echo ""

# Final status check
echo "Module status:"
if lsmod | grep -q cx511h; then
    echo "✓ cx511h is loaded"
else
    echo "✗ cx511h is NOT loaded"
fi

# Find and display our ALSA card
NEW_CARD_NUM=$(find_our_card)
if [ -n "$NEW_CARD_NUM" ]; then
    echo "✓ AVerMedia card found at number $NEW_CARD_NUM"
else
    echo "⚠️  AVerMedia card not detected (may need HDMI input)"
fi

echo "Audio services:"
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

exit 0
