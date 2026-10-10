#!/bin/bash
# cx511h unload: release V4L2/ALSA users, then rmmod.  NEVER touches the PCI bus,
# because writing to the device's sysfs "remove" file wedges the card in a
# MODULE_STATE_GOING / refcnt --1 state that only a reboot can clear.

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (use sudo)"
    exit 1
fi

echo "=== cx511h module unloader with audio restoration ==="

KILLED_AUDIO_SERVICES=false

command_exists() {
    command -v "$1" >/dev/null 2>&1
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
    echo "  v4l2-ctl -d $dev --stream-off (best effort)"
    # Some v4l2-ctl builds print the full usage help if `--stream-off` isn't
    # supported; redirect stdout to keep the unload output clean.
    v4l2-ctl -d "$dev" --stream-off >/dev/null 2>&1 || true
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

get_cx511h_refcnt() {
    local rc
    [ -f /sys/module/cx511h/refcnt ] || return 1
    rc=$(< /sys/module/cx511h/refcnt)
    [[ "$rc" =~ ^-?[0-9]+$ ]] || return 1
    echo "$rc"
}

# NOTE on the "radical PCI remove" technique that used to live here: writing 1
# to /sys/bus/pci/devices/<BDF>/remove yanks the card off the bus while the
# module's board_remove()/blob teardown is still talking to it over I2C, which
# hangs cx511h in MODULE_STATE_GOING with refcnt -1 — an unrecoverable state
# that forces a full reboot.  That path was removed on purpose; unload.sh now
# only ever releases users and runs a clean rmmod, and if the module is already
# wedged it plainly tells the user to reboot instead of wedging it harder.


# Aggressively stop the userspace that pins V4L2/ALSA nodes on the card.  A
# normal rmmod only succeeds once every open/reference on the card is released.
# PipeWire/wireplumber commonly hold the capture PCM and must be SIGKILLed, not
# just SIGTERMed.  Dies with status 0 if at least the capture/ALSA holders were
# stopped; callers then retry rmmod.
kill_everything_holding_card() {
    local card_num dev

    echo "[audio] Killing capture/playback clients..."
    for p in ffplay ffmpeg obs gst-launch-1.0 gst-launch v4l2-ctl \
             parecord pw-record pw-cat mpv vlc; do
        pkill -TERM -x "$p" 2>/dev/null || true
    done
    sleep 0.5

    # The sound server(s) are the usual culprit.  Kill them hard.
    echo "[audio] Stopping PipeWire/WirePlumber/Pulse/ALSA..."
    pkill -KILL -x pipewire-pulse 2>/dev/null || true
    pkill -KILL -x wireplumber 2>/dev/null || true
    pkill -KILL -x pipewire 2>/dev/null || true
    pkill -KILL -x pipewire-media-session 2>/dev/null || true
    pkill -KILL -x pulseaudio 2>/dev/null || true
    sleep 1

    card_num=$(find_our_alsa_card || true)

    # Fall back to fuser SIGKILL on any of our nodes / the card's PCMs.
    while IFS= read -r sysfs; do
        [ -n "$sysfs" ] || continue
        dev="/dev/$(basename "$sysfs")"
        [ -e "$dev" ] && fuser -k -KILL -v "$dev" 2>/dev/null || true
    done < <(find_our_video_sysfs_nodes)

    if [ -n "$card_num" ]; then
        for dev in /dev/snd/controlC${card_num} /dev/snd/pcmC${card_num}* /dev/snd/timer; do
            [ -e "$dev" ] && fuser -k -KILL -v "$dev" 2>/dev/null || true
        done
    fi
    sleep 1
    return 0
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

MODULE_UNLOADED=false

# 1) A normal rmmod works only when every user of the card's nodes is gone.
echo "Attempting normal rmmod cx511h..."
if rmmod cx511h 2>/dev/null; then
    echo "✓ Module removed via normal rmmod."
    MODULE_UNLOADED=true
fi

# 2) rmmod busy → gently release capture/audio holders (STREAMOFF, TERM clients),
#    then retry.  The PCI device is left untouched.
if [ "$MODULE_UNLOADED" = false ]; then
    echo "rmmod busy — releasing capture/audio holders and retrying..."
    release_cx511h_device_nodes
    rmmod cx511h 2>/dev/null && MODULE_UNLOADED=true
fi

# 3) Still busy → hard-kill the whole audio stack (PipeWire/WirePlumber often
#    pin the ALSA capture PCM even when idle) and retry again.
if [ "$MODULE_UNLOADED" = false ]; then
    echo "Still busy — hard-stopping audio daemons and retrying..."
    kill_everything_holding_card
    rmmod cx511h 2>/dev/null && MODULE_UNLOADED=true
fi

# 4) A final clean retry after everything has had time to fully exit.
if [ "$MODULE_UNLOADED" = false ]; then
    echo "Retrying rmmod after a short settle..."
    sleep 2
    rmmod cx511h 2>/dev/null && MODULE_UNLOADED=true
fi

# Refusal to unload despite releasing everyone usually means a wedged module
# (refcnt underflow / MODULE_STATE_GOING).  Do NOT touch the PCI 'remove' file:
# that is what wedges the card permanently.  Report and stop.
if [ "$MODULE_UNLOADED" = false ]; then
    echo ""
    echo "✗ Could not unload cx511h even after releasing all users."
    echo "  refcnt=$(get_cx511h_refcnt 2>/dev/null || echo '?')   " \
         "initstate=$(cat /sys/module/cx511h/initstate 2>/dev/null || echo '?')"
    if [ "$(cat /sys/module/cx511h/initstate 2>/dev/null)" = "going" ] \
       || { rc=$(get_cx511h_refcnt 2>/dev/null || echo 0); [ "$rc" -lt 0 ]; }; then
        echo "  The module is WEDGED (stuck in GOING / negative refcnt)."
        echo "  The card will NOT reach a clean state in software."
        echo "  ► REBOOT the machine, then just run: sudo ./insmod.sh"
        echo "  (reload.sh / unload.sh no longer wedge the card.)"
    else
        echo "  Check what still holds it:  fuser -v /dev/video* /dev/snd/*"
    fi
    exit 3
fi

echo ""
echo "The PCI device was never touched, so no rescan is needed."

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
