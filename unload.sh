#!/bin/bash
# cx511h unload: release V4L2/ALSA users, unbind PCI (driver name CL511H), then rmmod.

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module unloader with audio restoration ==="

AVM_PCI_VENDOR="0x1461"
AVM_PCI_DEVICE="0x0054"
AVM_PCI_ID="1461:0054"
# Kernel module is cx511h; the registered pci_driver name is CL511H (see board_config.c).
PCI_DRIVER_NAMES=(CL511H cx511h)

KILLED_AUDIO_SERVICES=false

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

is_pci_bdf() {
    [[ "$1" =~ ^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]$ ]]
}

# Return ALSA card index for the capture card, or empty.
find_our_alsa_card() {
    local card_path card_num id name

    if [ ! -d /proc/asound ]; then
        return 1
    fi

    for card_path in /proc/asound/card[0-9]*; do
        [ -f "$card_path/id" ] || continue
        id=$(<"$card_path/id")
        if [[ "$id" =~ AVerMedia|cx511h|CL511H|cl511h ]]; then
            card_num=${card_path##*/card}
            echo "$card_num"
            return 0
        fi
    done

    # Fallback: match by card name in /proc/asound/cards
    if [ -f /proc/asound/cards ]; then
        while read -r _ card_num _ name _; do
            [[ "$card_num" =~ ^[0-9]+$ ]] || continue
            if [[ "$name" =~ AVerMedia|CL511H|cx511h ]]; then
                echo "$card_num"
                return 0
            fi
        done < /proc/asound/cards
    fi

    return 1
}

# Return sysfs paths of V4L2 nodes owned by this driver.
find_our_video_sysfs_nodes() {
    local sysfs name driver_link
    for sysfs in /sys/class/video4linux/video*; do
        [ -d "$sysfs" ] || continue
        name=$(<"$sysfs/name" 2>/dev/null) || name=""
        if [[ "$name" =~ AVerMedia|CL511H|cx511h ]]; then
            echo "$sysfs"
            continue
        fi
        if [ -L "$sysfs/device/driver" ]; then
            driver_link=$(basename "$(readlink -f "$sysfs/device/driver")" 2>/dev/null)
            if [[ "$driver_link" =~ ^(CL511H|cx511h)$ ]]; then
                echo "$sysfs"
            fi
        fi
    done
}

# Return PCI BDF (e.g. 0000:0a:00.0) for the AVerMedia GC573 / CL511H.
find_cx511h_pci_bdf() {
    local driver dir entry vendor device bdf

    # 1) Device already bound under the kernel PCI driver (name is CL511H, not cx511h).
    for driver in "${PCI_DRIVER_NAMES[@]}"; do
        dir="/sys/bus/pci/drivers/$driver"
        [ -d "$dir" ] || continue
        for entry in "$dir"/*; do
            [ -e "$entry" ] || continue
            bdf=$(basename "$entry")
            is_pci_bdf "$bdf" || continue
            echo "$bdf"
            return 0
        done
    done

    # 2) Match vendor:device in sysfs (works even if driver lookup by name failed).
    for entry in /sys/bus/pci/devices/*; do
        [ -f "$entry/vendor" ] || continue
        vendor=$(<"$entry/vendor")
        device=$(<"$entry/device")
        if [ "$vendor" = "$AVM_PCI_VENDOR" ] && [ "$device" = "$AVM_PCI_DEVICE" ]; then
            basename "$entry"
            return 0
        fi
    done

    # 3) lspci fallback.
    if command_exists lspci; then
        bdf=$(lspci -Dn 2>/dev/null | awk -v id="$AVM_PCI_ID" '$3 == id { print $1; exit }')
        if [ -n "$bdf" ]; then
            echo "$bdf"
            return 0
        fi
    fi

    return 1
}

# Return sysfs directory of the bound PCI driver for BDF, or empty.
pci_driver_sysfs_for_bdf() {
    local bdf=$1 driver_link
    [ -n "$bdf" ] || return 1
    [ -L "/sys/bus/pci/devices/$bdf/driver" ] || return 1
    driver_link=$(readlink -f "/sys/bus/pci/devices/$bdf/driver")
    dirname "$driver_link"
}

kill_processes_on_device() {
    local dev=$1
    [ -e "$dev" ] || return 0

    if command_exists fuser; then
        if fuser -s "$dev" 2>/dev/null; then
            echo "  Releasing $dev (SIGTERM)..."
            fuser -k -TERM "$dev" 2>/dev/null || true
            sleep 0.3
            if fuser -s "$dev" 2>/dev/null; then
                echo "  Forcing $dev (SIGKILL)..."
                fuser -k -KILL "$dev" 2>/dev/null || true
            fi
        fi
    elif command_exists lsof; then
        local pids
        pids=$(lsof -t "$dev" 2>/dev/null || true)
        if [ -n "$pids" ]; then
            echo "  Killing PIDs holding $dev: $pids"
            kill -TERM $pids 2>/dev/null || true
            sleep 0.3
            kill -KILL $pids 2>/dev/null || true
        fi
    fi
}

# Stop known userspace capture apps that keep module references open.
kill_known_capture_clients() {
    local sig proc
    for proc in ffplay ffmpeg obs gst-launch-1.0 gst-launch v4l2-ctl \
                parecord pw-record pw-cat arecord aplay mpv vlc; do
        if pgrep -x "$proc" >/dev/null 2>&1; then
            echo "  Stopping $proc..."
            pkill -TERM -x "$proc" 2>/dev/null || true
        fi
    done
    sleep 0.5
    for proc in ffplay ffmpeg obs gst-launch-1.0 gst-launch v4l2-ctl \
                parecord pw-record pw-cat arecord aplay mpv vlc; do
        pkill -KILL -x "$proc" 2>/dev/null || true
    done
}

# Best-effort STREAMOFF for our V4L2 nodes before killing holders.
v4l2_streamoff_our_devices() {
  local sysfs dev
  command_exists v4l2-ctl || return 0
  while IFS= read -r sysfs; do
    [ -n "$sysfs" ] || continue
    dev="/dev/$(basename "$sysfs")"
    [ -c "$dev" ] || continue
    echo "  v4l2-ctl --stream-off on $dev"
    v4l2-ctl -d "$dev" --stream-off 2>/dev/null || true
  done < <(find_our_video_sysfs_nodes)
}

# Release V4L2 and ALSA device nodes held by userspace.
release_cx511h_device_nodes() {
    local card_num dev sysfs

    echo "Releasing V4L2 and ALSA device nodes..."

    kill_known_capture_clients
    v4l2_streamoff_our_devices

    while IFS= read -r sysfs; do
        [ -n "$sysfs" ] || continue
        kill_processes_on_device "/dev/$(basename "$sysfs")"
    done < <(find_our_video_sysfs_nodes)

    card_num=$(find_our_alsa_card || true)
    if [ -n "$card_num" ]; then
        echo "  ALSA card $card_num: closing open PCM/control nodes..."
        for dev in \
            /dev/snd/controlC"${card_num}" \
            /dev/snd/pcmC"${card_num}"* \
            /dev/snd/timer; do
            [ -e "$dev" ] || continue
            kill_processes_on_device "$dev"
        done

        # PipeWire / WirePlumber often hold the capture PCM even when idle.
        if command_exists fuser; then
            for dev in /dev/snd/pcmC"${card_num}"*; do
                [ -e "$dev" ] || continue
                if fuser -s "$dev" 2>/dev/null; then
                    for sig in pipewire wireplumber pipewire-pulse; do
                        if pgrep -x "$sig" >/dev/null 2>&1; then
                            echo "  Restarting $sig (held ALSA PCM for card $card_num)..."
                            pkill -TERM -x "$sig" 2>/dev/null || true
                            KILLED_AUDIO_SERVICES=true
                        fi
                    done
                    break
                fi
            done
            sleep 1
            pkill -KILL -x pipewire wireplumber pipewire-pulse 2>/dev/null || true
            for dev in /dev/snd/pcmC"${card_num}"* /dev/snd/controlC"${card_num}"; do
                [ -e "$dev" ] || continue
                kill_processes_on_device "$dev"
            done
        fi
    else
        echo "  No AVerMedia ALSA card found in /proc/asound."
    fi

    sleep 0.5
}

unbind_cx511h_pci() {
    local bdf driver_sysfs

    bdf=$(find_cx511h_pci_bdf || true)
    if [ -z "$bdf" ]; then
        echo "⚠ Could not locate AVerMedia PCI device (${AVM_PCI_ID})."
        return 1
    fi

    echo "Found AVerMedia PCI device: $bdf"

    driver_sysfs=$(pci_driver_sysfs_for_bdf "$bdf" || true)
    if [ -z "$driver_sysfs" ] || [ ! -f "$driver_sysfs/unbind" ]; then
        echo "⚠ PCI device $bdf is not bound to a driver (already unbound?)."
        return 1
    fi

    echo "Unbinding via $driver_sysfs/unbind ..."
    if echo "$bdf" > "$driver_sysfs/unbind" 2>/dev/null; then
        echo "✓ PCI device $bdf unbound."
        sleep 2
        return 0
    fi

    echo "✗ PCI unbind failed for $bdf"
    return 1
}

get_cx511h_refcnt() {
    local rc
    [ -f /sys/module/cx511h/refcnt ] || return 1
    rc=$(< /sys/module/cx511h/refcnt)
    [[ "$rc" =~ ^-?[0-9]+$ ]] || return 1
    echo "$rc"
}

# True when refcnt is unreadable or negative (kernel underflow / stuck state).
refcnt_needs_radical_remove() {
    local rc=$1
    if [ -z "$rc" ]; then
        return 0
    fi
    if [ "$rc" -lt 0 ]; then
        return 0
    fi
    return 1
}

# Sever PCI device from the bus, then force-remove the module (refcnt -1 recovery).
radical_pci_remove_cx511h() {
    local bdf remove_path driver_sysfs

    bdf=$(find_cx511h_pci_bdf || true)
    if [ -z "$bdf" ]; then
        echo "✗ Radical remove: could not find PCI BDF for ${AVM_PCI_ID}."
        return 1
    fi

    remove_path="/sys/bus/pci/devices/$bdf/remove"
    if [ ! -f "$remove_path" ]; then
        echo "✗ Radical remove: $remove_path does not exist."
        return 1
    fi

    echo "=== RADICAL PCI REMOVE: $bdf ==="
    echo "  (sysfs remove severs the device from the kernel PCI bus)"

    driver_sysfs=$(pci_driver_sysfs_for_bdf "$bdf" || true)
    if [ -n "$driver_sysfs" ] && [ -f "$driver_sysfs/unbind" ]; then
        echo "  Pre-unbinding driver before remove..."
        echo "$bdf" > "$driver_sysfs/unbind" 2>/dev/null || true
        sleep 0.5
    fi

    if echo 1 > "$remove_path" 2>/dev/null; then
        echo "✓ Wrote 1 to $remove_path"
    else
        echo "✗ Failed to write 1 to $remove_path"
        return 1
    fi

    sleep 1

    if rmmod -f cx511h 2>/dev/null; then
        echo "✓ Module force-removed (rmmod -f) after PCI remove."
        return 0
    fi

    if ! lsmod | grep -q '^cx511h '; then
        echo "✓ Module no longer loaded after PCI remove."
        return 0
    fi

    echo "✗ PCI device removed but cx511h module is still loaded."
    return 1
}

pci_rescan() {
    if [ -w /sys/bus/pci/rescan ]; then
        echo 1 > /sys/bus/pci/rescan 2>/dev/null
        echo "✓ PCIe rescan triggered (/sys/bus/pci/rescan)."
    else
        echo "⚠ Cannot write /sys/bus/pci/rescan (run as root)."
    fi
}

restart_wireplumber_user() {
    local user="${1:-}"

    [ -n "$user" ] || return 1
    command_exists systemctl || return 1

    echo "Restarting wireplumber for user $user..."
    if systemctl --user -M "${user}@" restart wireplumber 2>/dev/null; then
        echo "✓ wireplumber restarted (systemctl -M ${user}@)."
        return 0
    fi
    if sudo -u "$user" systemctl --user restart wireplumber 2>/dev/null; then
        echo "✓ wireplumber restarted (sudo -u $user systemctl --user)."
        return 0
    fi
    return 1
}

restart_audio_services() {
    local session_user=""

    echo "Restoring audio services..."
    sleep 1

    # Prefer the user who invoked sudo, else the login owner of this session.
    if [ -n "${SUDO_USER:-}" ]; then
        session_user="$SUDO_USER"
    elif [ -n "${USER:-}" ] && [ "$USER" != "root" ]; then
        session_user="$USER"
    else
        session_user="everlite"
    fi

    restart_wireplumber_user "$session_user" || true

    if command_exists systemctl; then
        for user_dir in /run/user/*; do
            [ -d "$user_dir" ] || continue
            uid=${user_dir##*/}
            [[ "$uid" =~ ^[0-9]+$ ]] || continue
            if sudo -u "#$uid" systemctl --user list-units 2>/dev/null | grep -q pipewire; then
                sudo -u "#$uid" systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
            fi
        done
        if [ -n "${SUDO_UID:-}" ] && [ "$SUDO_UID" -gt 0 ]; then
            sudo -u "#$SUDO_UID" systemctl --user restart pipewire pipewire-pulse wireplumber 2>/dev/null || true
        fi
    fi

    if ! pgrep -x pipewire >/dev/null 2>&1; then
        echo "  pipewire not running — start it from your desktop session if needed."
    fi
}

# --- main ---

if ! lsmod | grep -q '^cx511h '; then
    echo "cx511h module is not loaded."
    exit 0
fi

echo "cx511h module is loaded."
REF_COUNT=$(get_cx511h_refcnt 2>/dev/null || echo "?")
echo "Initial module reference count: $REF_COUNT"

release_cx511h_device_nodes

MODULE_UNLOADED=false
USE_RADICAL=false

if refcnt_needs_radical_remove "$REF_COUNT"; then
    echo "⚠ refcnt=$REF_COUNT — negative or invalid; skipping normal rmmod."
    USE_RADICAL=true
fi

if [ "$USE_RADICAL" = false ]; then
    echo "Attempting normal rmmod cx511h..."
    if rmmod cx511h 2>/dev/null; then
        echo "✓ Module removed via normal rmmod."
        MODULE_UNLOADED=true
    else
        echo "Normal rmmod failed."
        REF_COUNT=$(get_cx511h_refcnt 2>/dev/null || echo "?")
        echo "Module reference count: $REF_COUNT"
        if refcnt_needs_radical_remove "$REF_COUNT"; then
            USE_RADICAL=true
        fi
    fi
fi

if [ "$MODULE_UNLOADED" = false ] && [ "$USE_RADICAL" = false ]; then
    echo "Trying PCI unbind (clean unload path)..."
    if unbind_cx511h_pci; then
        if rmmod cx511h 2>/dev/null || modprobe -r cx511h 2>/dev/null; then
            echo "✓ Module removed after PCI unbind."
            MODULE_UNLOADED=true
        fi
    fi
fi

if [ "$MODULE_UNLOADED" = false ] && [ "$USE_RADICAL" = false ]; then
    echo "Module still loaded — releasing devices and retrying unbind..."
    release_cx511h_device_nodes
    unbind_cx511h_pci || true
    if rmmod cx511h 2>/dev/null || modprobe -r cx511h 2>/dev/null; then
        echo "✓ Module removed after second unbind attempt."
        MODULE_UNLOADED=true
    else
        USE_RADICAL=true
    fi
fi

if [ "$MODULE_UNLOADED" = false ]; then
    echo "Escalating to radical PCI remove + rmmod -f..."
    if radical_pci_remove_cx511h; then
        MODULE_UNLOADED=true
    else
        echo "✗ Radical PCI remove path failed."
        echo "  Diagnostics:"
        echo "    refcnt=$(get_cx511h_refcnt 2>/dev/null || echo '?')"
        bdf=$(find_cx511h_pci_bdf || true)
        if [ -n "$bdf" ]; then
            echo "    PCI BDF: $bdf"
            if [ -L "/sys/bus/pci/devices/$bdf/driver" ]; then
                echo "    driver: $(basename "$(readlink -f "/sys/bus/pci/devices/$bdf/driver")")"
            elif [ -e "/sys/bus/pci/devices/$bdf" ]; then
                echo "    driver: (unbound)"
            else
                echo "    sysfs: device entry gone (remove may have succeeded)"
            fi
        fi
        echo "  Check: fuser -v /dev/video* /dev/snd/*"
    fi
fi

echo ""
echo "Triggering PCIe rescan so the card can be re-probed on next insmod..."
pci_rescan

echo ""
echo "Restoring user audio (wireplumber)..."
restart_audio_services

echo ""
echo "=== Module unload complete ==="
if lsmod | grep -q '^cx511h '; then
    echo "⚠️  Warning: cx511h module may still be loaded."
else
    echo "✓ cx511h module unloaded."
fi

echo ""
echo "Audio service status:"
if pgrep -x pipewire >/dev/null; then
    echo "✓ pipewire is running"
else
    echo "✗ pipewire is not running (restart from your user session if needed)"
fi
if pgrep -x wireplumber >/dev/null; then
    echo "✓ wireplumber is running"
else
    echo "✗ wireplumber is not running"
fi

exit 0
