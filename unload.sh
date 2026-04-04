#!/bin/bash
set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module unloader with audio restoration ==="

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check if module is loaded
if ! lsmod | grep -q cx511h; then
    echo "cx511h module is not loaded."
    exit 0
fi

echo "cx511h module is loaded."

# Check if module is in use
MODULE_IN_USE=false
BLOCKING_PROCESSES=""

# Method 1: Check lsof for module references
if command_exists lsof; then
    echo "Checking for processes using cx511h module..."
    # Get processes holding the module
    LSOF_OUTPUT=$(lsof 2>/dev/null | grep cx511h || true)
    if [ -n "$LSOF_OUTPUT" ]; then
        MODULE_IN_USE=true
        echo "Processes holding cx511h module:"
        echo "$LSOF_OUTPUT"
        # Extract PIDs
        BLOCKING_PIDS=$(echo "$LSOF_OUTPUT" | awk '{print $2}' | sort -u)
        for pid in $BLOCKING_PIDS; do
            if [ -n "$pid" ] && [ "$pid" -gt 0 ]; then
                proc_name=$(ps -p "$pid" -o comm= 2>/dev/null || echo "unknown")
                BLOCKING_PROCESSES="$BLOCKING_PROCESSES $proc_name($pid)"
            fi
        done
    fi
fi

# Method 2: Check ALSA devices for our card
echo "Checking ALSA devices..."
# Find our audio card (cx511h should create an ALSA card)
if [ -d /proc/asound ]; then
    # Look for card with AVerMedia in name
    OUR_CARD=$(grep -l "AVerMedia\|cx511h\|CL511H" /proc/asound/card*/id 2>/dev/null | head -1)
    if [ -n "$OUR_CARD" ]; then
        # Extract card number
        CARD_NUM=$(echo "$OUR_CARD" | grep -o 'card[0-9]*' | grep -o '[0-9]*')
        echo "Found AVerMedia card at number $CARD_NUM"

        # Check if any processes have pcm devices for our card open
        for pcm_dev in /dev/snd/pcmC${CARD_NUM}D*; do
            if [ -e "$pcm_dev" ]; then
                if command_exists fuser; then
                    FUSER_OUTPUT=$(fuser -v "$pcm_dev" 2>/dev/null || true)
                    if [ -n "$FUSER_OUTPUT" ]; then
                        MODULE_IN_USE=true
                        echo "Processes using $pcm_dev:"
                        echo "$FUSER_OUTPUT"
                        # Extract process names
                        while read -r line; do
                            if [[ "$line" =~ ^([a-zA-Z0-9_]+)[[:space:]]+([0-9]+) ]]; then
                                proc_name="${BASH_REMATCH[1]}"
                                pid="${BASH_REMATCH[2]}"
                                # Only track pipewire/wireplumber/ffplay
                                if [[ "$proc_name" =~ pipewire|wireplumber|ffplay ]]; then
                                    BLOCKING_PROCESSES="$BLOCKING_PROCESSES $proc_name($pid)"
                                fi
                            fi
                        done <<< "$FUSER_OUTPUT"
                    fi
                fi
            fi
        done
    fi
fi

# Kill ffplay if running (common blocker for video playback)
echo "Killing ffplay if running..."
pkill -9 ffplay 2>/dev/null || true
sleep 0.5

# If module is in use and we have blocking processes
if [ "$MODULE_IN_USE" = true ] && [ -n "$BLOCKING_PROCESSES" ]; then
    echo "Module is in use by: $BLOCKING_PROCESSES"

    # Kill only pipewire/wireplumber processes that are blocking
    for proc in $BLOCKING_PROCESSES; do
        if [[ "$proc" =~ pipewire\(([0-9]+)\) ]]; then
            pid="${BASH_REMATCH[1]}"
            echo "Killing pipewire process $pid..."
            kill -9 "$pid" 2>/dev/null || true
        elif [[ "$proc" =~ wireplumber\(([0-9]+)\) ]]; then
            pid="${BASH_REMATCH[1]}"
            echo "Killing wireplumber process $pid..."
            kill -9 "$pid" 2>/dev/null || true
        fi
    done

    sleep 1
else
    echo "Module does not appear to be actively used by critical processes."
fi

# Try to remove the module
echo "Attempting to remove cx511h module..."
if rmmod cx511h 2>/dev/null; then
    echo "✓ Module removed successfully."
else
    # If rmmod fails, check why
    echo "rmmod failed. Checking reference count..."
    REF_COUNT=$(cat /sys/module/cx511h/refcnt 2>/dev/null || echo "unknown")
    echo "Module reference count: $REF_COUNT"

    # Try force remove if reference count is low
    if [ "$REF_COUNT" = "0" ] || [ "$REF_COUNT" = "unknown" ]; then
        echo "Trying force remove..."
        rmmod -f cx511h 2>/dev/null || echo "Force remove also failed."
    fi
fi

# Restart audio services to restore system audio
echo "Restoring audio services..."
sleep 2  # Allow system to settle

# Restart pipewire services if they were killed
if [[ "$BLOCKING_PROCESSES" =~ pipewire ]]; then
    echo "Restarting pipewire services..."

    # Try systemctl --user first (for user services)
    if command_exists systemctl; then
        # Check if user systemd is available
        if systemctl --user list-units 2>/dev/null | grep -q pipewire; then
            systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
            echo "✓ User audio services restarted."
        fi
    fi

    # Also try direct restart if systemctl fails
    sleep 1
    # Launch pipewire in background if not running
    if ! pgrep -x pipewire >/dev/null; then
        echo "Starting pipewire directly..."
        pipewire 2>/dev/null &
    fi
    if ! pgrep -x wireplumber >/dev/null; then
        echo "Starting wireplumber directly..."
        wireplumber 2>/dev/null &
    fi
fi

echo ""
echo "=== Module unload complete ==="
if lsmod | grep -q cx511h; then
    echo "⚠️  Warning: cx511h module may still be loaded."
    echo "   Some processes may still be holding references."
else
    echo "✓ cx511h module unloaded, audio restored."
fi

# Final check for audio services
echo ""
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

exit 0
