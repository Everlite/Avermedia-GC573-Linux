#!/usr/bin/env python3
"""
GC573 PS5 Gaming Screen - Ultra-Low-Latency Display & Audio Viewer
A standalone app to play PS5 (or any HDMI console) directly on the PC desktop
with sub-frame video rendering and synchronized PipeWire/ALSA audio.
"""

import sys
import os
import signal
import subprocess
import time
import argparse
import re
import json
from typing import Optional, Tuple, Dict

CONFIG_DIR = os.path.expanduser("~/.config/everlive")
CONFIG_FILE = os.path.join(CONFIG_DIR, "settings.json")


def load_settings() -> dict:
    if os.path.isfile(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, "r") as f:
                return json.load(f)
        except Exception:
            pass
    return {}


def save_settings(data: dict):
    try:
        os.makedirs(CONFIG_DIR, exist_ok=True)
        cur = load_settings()
        cur.update(data)
        with open(CONFIG_FILE, "w") as f:
            json.dump(cur, f, indent=2)
    except Exception:
        pass

# Force X11 / xcb on Wayland so QWidget.winId() provides an X11 Window ID for mpv --wid embedding
if "QT_QPA_PLATFORM" not in os.environ and os.environ.get("WAYLAND_DISPLAY"):
    os.environ["QT_QPA_PLATFORM"] = "xcb"

# PySide6 is optional when run with --cli
try:
    from PySide6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QLabel, QPushButton, QComboBox, QCheckBox, QSlider, QFrame,
        QGroupBox, QMessageBox, QStackedLayout, QSizePolicy
    )
    from PySide6.QtCore import Qt, QTimer, QThread, Signal, QEvent
    from PySide6.QtGui import QFont, QIcon, QColor, QPalette, QKeySequence, QShortcut
    HAS_QT = True
except ImportError:
    HAS_QT = False


def find_gc573_vdev() -> str:
    """Auto-detects the V4L2 device node for open_gc573."""
    import glob
    # 1. Check open_gc573 driver binding in sysfs
    for cand in sorted(glob.glob("/sys/bus/pci/drivers/open_gc573/*/video4linux/video*")):
        vdev_name = os.path.basename(cand)
        dev_path = f"/dev/{vdev_name}"
        if os.path.exists(dev_path):
            return dev_path

    # 2. Check /sys/class/video4linux/*/device/driver symlink
    for d in sorted(glob.glob("/sys/class/video4linux/video*")):
        try:
            driver_link = os.path.realpath(os.path.join(d, "device", "driver"))
            if os.path.basename(driver_link) == "open_gc573":
                dev_path = f"/dev/{os.path.basename(d)}"
                if os.path.exists(dev_path):
                    return dev_path
        except Exception:
            pass

    # 3. Check /sys/class/video4linux/*/name
    for name_file in sorted(glob.glob("/sys/class/video4linux/video*/name")):
        try:
            with open(name_file, "r") as f:
                name = f.read().lower()
                if "gc573" in name or "live gamer 4k" in name or "open_gc573" in name:
                    dev_path = f"/dev/{os.path.basename(os.path.dirname(name_file))}"
                    if os.path.exists(dev_path):
                        return dev_path
        except Exception:
            pass

    # 4. Fallback: check existing video devices
    for fallback in ["/dev/video2", "/dev/video0", "/dev/video1", "/dev/video3", "/dev/video4"]:
        if os.path.exists(fallback):
            return fallback

    return "/dev/video0"


def find_gc573_adev() -> str:
    """Auto-detects ALSA capture card for GC573."""
    try:
        with open("/proc/asound/cards", "r") as f:
            for line in f:
                if "GC573" in line or "Live Gamer 4K" in line or "A4K" in line:
                    m = re.search(r"^\s*(\d+)\s+\[([^\]]+)\]", line)
                    if m:
                        card_id = m.group(2).strip()
                        return f"hw:CARD={card_id},DEV=0"
    except Exception:
        pass
    return "hw:CARD=A4K,DEV=0"


DEFAULT_VDEV = find_gc573_vdev()
DEFAULT_ADEV = find_gc573_adev()

PRESETS = {
    "Auto (HDMI Signal)": None,
    "2560x1440 @ 120 FPS": {"width": 2560, "height": 1440, "fps": 120, "fmt": "YUYV"},
    "2560x1440 @ 60 FPS":  {"width": 2560, "height": 1440, "fps": 60,  "fmt": "YUYV"},
    "1920x1080 @ 120 FPS": {"width": 1920, "height": 1080, "fps": 120, "fmt": "YUYV"},
    "1920x1080 @ 60 FPS":  {"width": 1920, "height": 1080, "fps": 60,  "fmt": "YUYV"},
    "3840x2160 @ 60 FPS":  {"width": 3840, "height": 2160, "fps": 60,  "fmt": "YUYV"},
}

LED_MODES = {
    "🌑 Aus (Dunkel)": 0,
    "🔵 Blau (Standard)": 1,
    "🟢 Grün": 2,
    "🔴 Rot": 3,
    "🔷 Cyan / Türkis": 4,
    "🟣 Magenta / Lila": 5,
    "🟡 Gelb": 6,
    "⚪ Weiß": 7,
}

LED_NAME_MAP = {
    "0": 0, "off": 0, "aus": 0, "dark": 0,
    "1": 1, "blue": 1, "blau": 1,
    "2": 2, "green": 2, "gruen": 2, "grün": 2,
    "3": 3, "red": 3, "rot": 3,
    "4": 4, "cyan": 4, "tuerkis": 4, "türkis": 4,
    "5": 5, "magenta": 5, "lila": 5, "purple": 5,
    "6": 6, "yellow": 6, "gelb": 6,
    "7": 7, "white": 7, "weiss": 7, "weiß": 7,
}


def find_led_sysfs_path() -> Optional[str]:
    """Locates the open_gc573 sysfs led_mode attribute."""
    import glob
    candidates = glob.glob("/sys/bus/pci/drivers/open_gc573/*/led_mode")
    if candidates and os.path.exists(candidates[0]):
        return candidates[0]
    return None


def read_led_mode() -> int:
    """Reads current LED mode from kernel driver."""
    path = find_led_sysfs_path()
    if path:
        try:
            with open(path, "r") as f:
                return int(f.read().strip())
        except Exception:
            pass
    return -1


def write_led_mode(mode: int) -> bool:
    """Applies new LED mode to hardware in real-time and persists it."""
    path = find_led_sysfs_path()
    if not path:
        return False
    success = False
    try:
        with open(path, "w") as f:
            f.write(f"{mode}\n")
        success = True
    except Exception:
        success = False

    if success:
        save_settings({"led_mode": mode})
    return success


def set_audio_volume(vol_pct: int):
    """Sets stream volume for gc573-audio via wpctl / pactl (0-150%)."""
    try:
        out = subprocess.check_output(["wpctl", "status"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            if "output.gc573-audio" in line or "gc573-audio" in line:
                for part in line.strip().split():
                    clean = part.strip(".│*")
                    if clean.isdigit():
                        vol_float = vol_pct / 100.0
                        subprocess.run(["wpctl", "set-volume", clean, f"{vol_float:.2f}"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        return
    except Exception:
        pass

    try:
        out = subprocess.check_output(["pactl", "list", "sink-inputs"], text=True, stderr=subprocess.DEVNULL)
        curr_id = None
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("Sink-Eingabe #") or line.startswith("Sink Input #"):
                curr_id = line.split("#")[-1].strip()
            elif "gc573-audio" in line and curr_id:
                subprocess.run(["pactl", "set-sink-input-volume", curr_id, f"{vol_pct}%"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                break
    except Exception:
        pass


def set_audio_mute(mute: bool):
    """Mutes or unmutes stream for gc573-audio via wpctl / pactl."""
    try:
        out = subprocess.check_output(["wpctl", "status"], text=True, stderr=subprocess.DEVNULL)
        for line in out.splitlines():
            if "output.gc573-audio" in line or "gc573-audio" in line:
                for part in line.strip().split():
                    clean = part.strip(".│*")
                    if clean.isdigit():
                        subprocess.run(["wpctl", "set-mute", clean, "1" if mute else "0"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                        return
    except Exception:
        pass

    try:
        out = subprocess.check_output(["pactl", "list", "sink-inputs"], text=True, stderr=subprocess.DEVNULL)
        curr_id = None
        for line in out.splitlines():
            line = line.strip()
            if line.startswith("Sink-Eingabe #") or line.startswith("Sink Input #"):
                curr_id = line.split("#")[-1].strip()
            elif "gc573-audio" in line and curr_id:
                subprocess.run(["pactl", "set-sink-input-mute", curr_id, "1" if mute else "0"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                break
    except Exception:
        pass


class GameStreamer:
    """Manages low-latency video and audio child processes."""
    def __init__(self, vdev: Optional[str] = None, adev: Optional[str] = None, force_mpv: bool = False):
        self.vdev = vdev if vdev is not None else find_gc573_vdev()
        self.adev = adev if adev is not None else find_gc573_adev()
        self.force_mpv = force_mpv
        self.mpv_proc: Optional[subprocess.Popen] = None
        self.arecord_proc: Optional[subprocess.Popen] = None
        self.pwplay_proc: Optional[subprocess.Popen] = None
        self.is_running = False

    def get_signal_info(self) -> Dict[str, str]:
        """Queries current signal status via v4l2-ctl."""
        info = {
            "locked": False,
            "resolution": "Kein Signal",
            "framerate": "",
            "pixelformat": "",
            "audio": "8 Kanäle @ 48 kHz"
        }
        if not os.path.exists(self.vdev):
            info["resolution"] = f"{self.vdev} nicht gefunden"
            return info

        try:
            out = subprocess.check_output(
                ["v4l2-ctl", "-d", self.vdev, "--query-dv-timings"],
                stderr=subprocess.DEVNULL, timeout=1.0
            ).decode()
            
            # Example: Active width: 2560, Active height: 1440, Pixelclock: 489300000 Hz
            w_m = re.search(r"Active width:\s*(\d+)", out)
            h_m = re.search(r"Active height:\s*(\d+)", out)
            clk_m = re.search(r"Pixelclock:\s*(\d+)", out)
            tot_w = re.search(r"Total width:\s*(\d+)", out)
            tot_h = re.search(r"Total height:\s*(\d+)", out)

            if w_m and h_m:
                w, h = int(w_m.group(1)), int(h_m.group(1))
                fps_str = "60 FPS"
                if clk_m and tot_w and tot_h:
                    clk = int(clk_m.group(1))
                    tw = int(tot_w.group(1))
                    th = int(tot_h.group(1))
                    if tw > 0 and th > 0:
                        fps_calc = clk / (tw * th)
                        if abs(fps_calc - round(fps_calc)) < 0.01:
                            fps_str = f"{int(round(fps_calc))} FPS"
                        else:
                            fps_str = f"{fps_calc:.2f} FPS"
                info["locked"] = True
                info["resolution"] = f"{w}x{h}"
                info["framerate"] = fps_str

                try:
                    fmt_out = subprocess.check_output(
                        ["v4l2-ctl", "-d", self.vdev, "--get-fmt-video"],
                        stderr=subprocess.DEVNULL, timeout=0.8
                    ).decode()
                    if "SMPTE 2084" in fmt_out or "P010" in fmt_out:
                        info["hdr"] = "HDR10"
                    else:
                        info["hdr"] = "SDR"
                except Exception:
                    info["hdr"] = "SDR"
        except Exception:
            # Kein aktiver HDMI-Signal-Lock über DV-Timings vorhanden
            info["locked"] = False
            info["resolution"] = "Kein Signal"
            info["framerate"] = ""
            info["hdr"] = ""

        return info

    def resync(self) -> bool:
        """Forces HDMI handshake renegotiation via v4l2-ctl query."""
        if not os.path.exists(self.vdev):
            return False
        try:
            subprocess.run(
                ["v4l2-ctl", "-d", self.vdev, "--set-dv-bt-timings", "query"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2.0
            )
            return True
        except Exception:
            return False

    def start(self, preset_name: str = "Auto (HDMI Signal)", fullscreen: bool = False, volume: float = 1.0, wid: Optional[int] = None) -> bool:
        if self.is_running:
            return True

        if not os.path.exists(self.vdev):
            print(f"[!] Fehler: Videogerät {self.vdev} existiert nicht.")
            return False

        preset = PRESETS.get(preset_name)
        width, height, fps = None, None, None

        if preset:
            width = preset["width"]
            height = preset["height"]
            fps = preset["fps"]
            fmt = preset["fmt"]
            # Apply fixed preset to V4L2 device
            try:
                subprocess.run(
                    ["v4l2-ctl", "-d", self.vdev, f"--set-fmt-video=width={width},height={height},pixelformat={fmt}"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0
                )
            except Exception as e:
                print(f"[*] Hinweis beim Setzen des Formats: {e}")
        else:
            # Auto (HDMI Signal): Hardware DV-Timings vom HDMI-Signal abfragen und anwenden (z.B. 4K 3840x2160)
            try:
                subprocess.run(
                    ["v4l2-ctl", "-d", self.vdev, "--set-dv-bt-timings", "query"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2.0
                )
            except Exception:
                pass

        # Prefer native high-performance everlive_display binary (< 2 ms display latency)
        script_dir = os.path.dirname(os.path.abspath(__file__))
        native_bin = os.path.join(script_dir, "everlive_display")
        if not os.path.isfile(native_bin):
            native_bin = os.path.join(script_dir, "gc573_display")

        if not self.force_mpv and os.path.isfile(native_bin) and os.access(native_bin, os.X_OK):
            cmd = [native_bin, "-d", self.vdev, "-a", self.adev]
            if wid is not None:
                cmd.extend(["-w", str(wid)])
            elif fullscreen:
                cmd.append("-f")
            print(f"[*] Starte nativen EverLive Direct-Renderer (1-2 ms Hardware-Latenz): {' '.join(cmd)}")
            try:
                self.mpv_proc = subprocess.Popen(cmd, start_new_session=True)
                self.is_running = True
                return True
            except Exception as e:
                print(f"[!] Fehler beim Starten von everlive_display ({e}), weiche auf mpv aus...")

        # Fallback: optimized mpv command with zero-delay low-latency profile
        mpv_cmd = [
            "mpv",
            f"av://v4l2:{self.vdev}",
            "--title=EverLive - Live Gamer 4K",
            "--profile=low-latency",
            "--untimed",
            "--no-cache",
            "--no-correct-pts",
            "--demuxer-lavf-probe-info=no",
            "--demuxer-lavf-analyzeduration=0",
            "--demuxer-readahead-secs=0",
            "--demuxer-lavf-o=fflags=nobuffer+fastseek",
            "--demuxer-lavf-o-add=flags=low_delay",
            "--demuxer-lavf-o-add=probesize=32",
            "--video-latency-hacks=yes",
            "--interpolation=no",
            "--framedrop=vo",
            "--video-sync=display-desync",
            "--opengl-glfinish=no",
            "--vd-lavc-threads=1",
            "--vd-lavc-fast=yes",
            "--vo=gpu",
            "--cursor-autohide=1000",
            "--no-terminal",
            "--input-default-bindings=yes",
        ]

        mpv_env = dict(os.environ)
        if wid is not None:
            # Force X11 backend so mpv attaches directly to Qt XCB widget
            mpv_env.pop("WAYLAND_DISPLAY", None)
            mpv_cmd.extend([
                f"--wid={wid}",
                "--gpu-context=x11egl",
                "--x11-bypass-compositor=no",
                "--input-vo-keyboard=no",
                "--input-cursor-passthrough=yes",
                "--input-default-bindings=no",
            ])
        elif fullscreen:
            mpv_cmd.append("--fullscreen")

        try:
            self.mpv_proc = subprocess.Popen(mpv_cmd, env=mpv_env, start_new_session=True)
            self.is_running = True
            if self.adev and self.adev != "none":
                try:
                    cmd = (
                        f"{{ arecord -D '{self.adev}' -f S16_LE -c 8 -r 48000 -q 2>/dev/null || "
                        f"   arecord -D '{self.adev}' -f S16_LE -c 6 -r 48000 -q 2>/dev/null || "
                        f"   arecord -D '{self.adev}' -f S16_LE -c 2 -r 48000 -q 2>/dev/null; }} | "
                        f"pw-play --latency=20ms -P 'application.name=gc573-audio node.name=gc573-audio' - 2>/dev/null"
                    )
                    self.pwplay_proc = subprocess.Popen(cmd, shell=True, preexec_fn=os.setsid)
                except Exception as ae:
                    print(f"[*] Hinweis beim Starten des Audio-Streams: {ae}")
            return True
        except Exception as e:
            print(f"[!] mpv konnte nicht gestartet werden: {e}")
            self.stop()
            return False

    def stop(self):
        self.is_running = False
        if self.mpv_proc:
            try:
                pgid = os.getpgid(self.mpv_proc.pid)
                os.killpg(pgid, signal.SIGTERM)
            except Exception:
                try:
                    self.mpv_proc.terminate()
                except Exception:
                    pass
            try:
                self.mpv_proc.wait(timeout=1.0)
            except Exception:
                try:
                    pgid = os.getpgid(self.mpv_proc.pid)
                    os.killpg(pgid, signal.SIGKILL)
                except Exception:
                    try:
                        self.mpv_proc.kill()
                    except Exception:
                        pass
            self.mpv_proc = None

        if self.pwplay_proc:
            try:
                self.pwplay_proc.terminate()
                self.pwplay_proc.wait(timeout=0.5)
            except Exception:
                try:
                    self.pwplay_proc.kill()
                except Exception:
                    pass
            self.pwplay_proc = None

        if self.arecord_proc:
            try:
                self.arecord_proc.terminate()
                self.arecord_proc.wait(timeout=0.5)
            except Exception:
                try:
                    self.arecord_proc.kill()
                except Exception:
                    pass
            self.arecord_proc = None

    def poll(self) -> bool:
        """Returns True if mpv is still alive."""
        if self.mpv_proc:
            if self.mpv_proc.poll() is not None:
                self.stop()
                return False
            return True
        return False


if HAS_QT:
    class VideoViewport(QWidget):
        """Native X11 window widget for direct embedded mpv rendering."""
        def __init__(self, parent=None):
            super().__init__(parent)
            self.setAttribute(Qt.WA_NativeWindow, True)
            self.setAttribute(Qt.WA_OpaquePaintEvent, True)
            self.setStyleSheet("background-color: #050608;")
            self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        def resizeEvent(self, event):
            super().resizeEvent(event)
            p = self.window()
            if p and hasattr(p, "streamer") and p.streamer.mpv_proc:
                try:
                    p.streamer.mpv_proc.send_signal(signal.SIGWINCH)
                except Exception:
                    pass


    class MainWindow(QMainWindow):
        def __init__(self, streamer: GameStreamer):
            super().__init__()
            self.streamer = streamer
            self.is_fullscreen = False
            self.muted = False
            self.saved_cfg = load_settings()

            self.init_ui()

            # Completely hide default QMainWindow menuBar & statusBar to eliminate top black bar offset in fullscreen
            self.menuBar().hide()
            self.menuBar().setFixedHeight(0)
            self.statusBar().hide()
            self.statusBar().setFixedHeight(0)

            # Signal monitor timer (1s polling)
            self.timer = QTimer(self)
            self.timer.timeout.connect(self.update_status)
            self.timer.start(1000)

            # Auto-start stream inside viewport as soon as window is shown
            QTimer.singleShot(200, self.auto_start_stream)

        def init_ui(self):
            self.setWindowTitle("EverLive - Live Gamer 4K")
            self.resize(1280, 780)
            self.setMinimumSize(640, 420)

            # EverLive Dark Studio Theme
            self.setStyleSheet("""
                QMainWindow {
                    background-color: #0c0d12;
                }
                QFrame#headerDeck {
                    background-color: #141720;
                    border-bottom: 1px solid #232736;
                    padding: 4px 12px;
                }
                QFrame#bottomBar {
                    background-color: #0f1118;
                    border-top: 1px solid #1c202e;
                    padding: 2px 10px;
                }
                QLabel {
                    color: #e6edf3;
                    font-family: 'Inter', 'Segoe UI', system-ui, sans-serif;
                }
                QLabel#logoTitle {
                    color: #ffffff;
                    font-size: 15px;
                    font-weight: 800;
                    letter-spacing: 0.5px;
                }
                QLabel#badgeModel {
                    color: #ff3344;
                    background-color: #2b1115;
                    border: 1px solid #5a1e25;
                    border-radius: 4px;
                    padding: 2px 6px;
                    font-size: 10px;
                    font-weight: bold;
                    letter-spacing: 1px;
                }
                QLabel#pillLive {
                    border-radius: 12px;
                    padding: 4px 12px;
                    font-size: 12px;
                    font-weight: bold;
                }
                QComboBox {
                    background-color: #1b202c;
                    color: #f0f6fc;
                    border: 1px solid #303749;
                    border-radius: 6px;
                    padding: 4px 10px;
                    font-size: 12px;
                    min-height: 24px;
                }
                QComboBox:hover {
                    border-color: #58a6ff;
                }
                QComboBox::drop-down {
                    border: none;
                    width: 20px;
                }
                QPushButton {
                    background-color: #1f2533;
                    color: #e6edf3;
                    border: 1px solid #323b4e;
                    border-radius: 6px;
                    padding: 5px 12px;
                    font-size: 12px;
                    font-weight: 600;
                }
                QPushButton:hover {
                    background-color: #2b3346;
                    border-color: #58a6ff;
                }
                QPushButton#btnToggleLive {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #da3633, stop:1 #f85149);
                    color: white;
                    border: none;
                    font-weight: bold;
                }
                QPushButton#btnToggleLive:hover {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f85149, stop:1 #ff7b72);
                }
                QPushButton#btnToggleStart {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #238636, stop:1 #2ea043);
                    color: white;
                    border: none;
                    font-weight: bold;
                }
                QPushButton#btnToggleStart:hover {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2ea043, stop:1 #3fb950);
                }
                QSlider::groove:horizontal {
                    height: 5px;
                    background: #252c3c;
                    border-radius: 2px;
                }
                QSlider::sub-page:horizontal {
                    background: #ff3344;
                    border-radius: 2px;
                }
                QSlider::handle:horizontal {
                    background: #ffffff;
                    width: 14px;
                    margin-top: -5px;
                    margin-bottom: -5px;
                    border-radius: 7px;
                }
            """)

            central_widget = QWidget()
            self.setCentralWidget(central_widget)
            root_layout = QVBoxLayout(central_widget)
            root_layout.setContentsMargins(0, 0, 0, 0)
            root_layout.setSpacing(0)

            # -------------------------------------------------------------
            # Top Deck: EverLive Branding & Quick Controls
            # -------------------------------------------------------------
            self.header_deck = QFrame()
            self.header_deck.setObjectName("headerDeck")
            h_layout = QHBoxLayout(self.header_deck)
            h_layout.setContentsMargins(12, 6, 12, 6)
            h_layout.setSpacing(12)

            # Logo & Model
            lbl_logo = QLabel("🟣 EverLive")
            lbl_logo.setObjectName("logoTitle")
            h_layout.addWidget(lbl_logo)

            lbl_badge = QLabel("LIVE GAMER 4K")
            lbl_badge.setObjectName("badgeModel")
            h_layout.addWidget(lbl_badge)

            # Dynamic Signal Pill
            self.lbl_pill = QLabel("● PRÜFE SIGNAL...")
            self.lbl_pill.setObjectName("pillLive")
            self.lbl_pill.setStyleSheet("background-color: #2b2518; color: #d29922; border: 1px solid #7d5918;")
            h_layout.addWidget(self.lbl_pill)

            h_layout.addStretch(1)

            # Resolution Preset
            h_layout.addWidget(QLabel("Format:"))
            self.combo_presets = QComboBox()
            self.combo_presets.addItems(list(PRESETS.keys()))
            if "preset" in self.saved_cfg and self.saved_cfg["preset"] in PRESETS:
                self.combo_presets.setCurrentText(self.saved_cfg["preset"])
            self.combo_presets.currentIndexChanged.connect(self.on_preset_changed)
            h_layout.addWidget(self.combo_presets)

            # LED Lighting Combo
            h_layout.addWidget(QLabel("LED:"))
            self.combo_led = QComboBox()
            for name, val in LED_MODES.items():
                self.combo_led.addItem(name, val)

            saved_led = self.saved_cfg.get("led_mode", 1)
            if saved_led is not None and isinstance(saved_led, int) and 0 <= saved_led <= 7:
                write_led_mode(saved_led)
                idx = self.combo_led.findData(saved_led)
                if idx >= 0:
                    self.combo_led.setCurrentIndex(idx)
            else:
                curr_led = read_led_mode()
                if curr_led >= 0:
                    idx = self.combo_led.findData(curr_led)
                    if idx >= 0:
                        self.combo_led.setCurrentIndex(idx)
                else:
                    self.combo_led.setEnabled(False)
            self.combo_led.currentIndexChanged.connect(self.on_led_changed)
            h_layout.addWidget(self.combo_led)

            # Audio Mute & Volume
            self.btn_mute = QPushButton("🔊")
            self.btn_mute.setFixedWidth(36)
            self.btn_mute.setCursor(Qt.PointingHandCursor)
            self.btn_mute.clicked.connect(self.toggle_mute)
            h_layout.addWidget(self.btn_mute)

            self.slider_vol = QSlider(Qt.Horizontal)
            self.slider_vol.setRange(0, 150)
            initial_vol = int(self.saved_cfg.get("volume", 1.0) * 100)
            self.slider_vol.setValue(initial_vol)
            self.slider_vol.setFixedWidth(90)
            self.slider_vol.valueChanged.connect(self.on_volume_changed)
            h_layout.addWidget(self.slider_vol)

            self.lbl_vol = QLabel(f"{initial_vol}%")
            self.lbl_vol.setFixedWidth(36)
            self.lbl_vol.setStyleSheet("color: #8b949e; font-size: 11px;")
            h_layout.addWidget(self.lbl_vol)

            # Stream Restart/Toggle Button
            self.btn_toggle_stream = QPushButton("⏹ Stop")
            self.btn_toggle_stream.setObjectName("btnToggleLive")
            self.btn_toggle_stream.setCursor(Qt.PointingHandCursor)
            self.btn_toggle_stream.clicked.connect(self.toggle_stream)
            h_layout.addWidget(self.btn_toggle_stream)

            # HDMI Re-Sync Button
            self.btn_resync = QPushButton("🔄 Re-Sync")
            self.btn_resync.setCursor(Qt.PointingHandCursor)
            self.btn_resync.setToolTip("HDMI Handshake & Signal neu aushandeln (Taste: H oder R)")
            self.btn_resync.clicked.connect(self.force_resync)
            h_layout.addWidget(self.btn_resync)

            # Fullscreen Button
            self.btn_fs = QPushButton("⛶ Vollbild")
            self.btn_fs.setCursor(Qt.PointingHandCursor)
            self.btn_fs.clicked.connect(self.toggle_fullscreen)
            h_layout.addWidget(self.btn_fs)

            root_layout.addWidget(self.header_deck)

            # -------------------------------------------------------------
            # Central Video Viewport (mpv embeds directly here)
            # -------------------------------------------------------------
            self.viewport = VideoViewport(central_widget)
            self.viewport.installEventFilter(self)
            root_layout.addWidget(self.viewport, 1)

            # -------------------------------------------------------------
            # Bottom Status Bar
            # -------------------------------------------------------------
            self.bottom_bar = QFrame()
            self.bottom_bar.setObjectName("bottomBar")
            b_layout = QHBoxLayout(self.bottom_bar)
            b_layout.setContentsMargins(10, 4, 10, 4)

            self.lbl_vdev = QLabel(f"🎮 Video: {self.streamer.vdev}")
            self.lbl_vdev.setStyleSheet("color: #8b949e; font-size: 11px;")
            b_layout.addWidget(self.lbl_vdev)

            self.btn_stats = QPushButton("📊 HUD Stats")
            self.btn_stats.setCursor(Qt.PointingHandCursor)
            self.btn_stats.setStyleSheet("""
                QPushButton {
                    background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d;
                    border-radius: 4px; padding: 4px 10px; font-size: 11px; font-weight: bold;
                }
                QPushButton:hover { background-color: #30363d; color: #58a6ff; border-color: #58a6ff; }
            """)
            self.btn_stats.clicked.connect(self.toggle_stats)
            b_layout.addWidget(self.btn_stats)

            b_layout.addStretch(1)

            self.lbl_hint = QLabel("Tab: Stats • H/R: Re-Sync • Doppelklick / F11: Vollbild • M: Stumm • ESC: Fenster")
            self.lbl_hint.setStyleSheet("color: #58a6ff; font-size: 11px;")
            b_layout.addWidget(self.lbl_hint)

            b_layout.addStretch(1)

            self.lbl_engine = QLabel("⚡ EverLive Direct-DMA Engine (< 2 ms Hardware-Latenz)")
            self.lbl_engine.setStyleSheet("color: #3fb950; font-size: 11px; font-weight: bold;")
            b_layout.addWidget(self.lbl_engine)

            root_layout.addWidget(self.bottom_bar)

            # Setup global shortcuts
            QShortcut(QKeySequence("F11"), self, self.toggle_fullscreen)
            QShortcut(QKeySequence("f"), self, self.toggle_fullscreen)
            QShortcut(QKeySequence("Escape"), self, self.exit_fullscreen)
            QShortcut(QKeySequence("Tab"), self, self.toggle_stats)
            QShortcut(QKeySequence("i"), self, self.toggle_stats)
            QShortcut(QKeySequence("h"), self, self.force_resync)
            QShortcut(QKeySequence("r"), self, self.force_resync)
            QShortcut(QKeySequence("m"), self, self.toggle_mute)
            QShortcut(QKeySequence("q"), self, self.close)

            self.update_status()

        def toggle_stats(self):
            if self.streamer.mpv_proc and self.streamer.mpv_proc.poll() is None:
                try:
                    self.streamer.mpv_proc.send_signal(signal.SIGUSR1)
                except Exception:
                    pass

        def force_resync(self, *args):
            """Forces HDMI handshake renegotiation and restarts stream if active."""
            if hasattr(self, "_resyncing") and self._resyncing:
                return
            self._resyncing = True
            if hasattr(self, "btn_resync"):
                self.btn_resync.setEnabled(False)
            try:
                was_running = self.streamer.is_running
                if was_running:
                    self.streamer.stop()
                    self.btn_toggle_stream.setText("▶ Start")
                    self.btn_toggle_stream.setObjectName("btnToggleStart")
                    self.btn_toggle_stream.setStyle(self.btn_toggle_stream.style())
                self.streamer.resync()
                self.update_status()
                if was_running:
                    QTimer.singleShot(250, self.auto_start_stream)
            except Exception as e:
                print(f"[!] Fehler bei HDMI Re-Sync: {e}")
            finally:
                QTimer.singleShot(600, lambda: (setattr(self, "_resyncing", False), hasattr(self, "btn_resync") and self.btn_resync.setEnabled(True)))

        def auto_start_stream(self):
            if not self.streamer.is_running:
                preset = self.combo_presets.currentText()
                vol = self.slider_vol.value() / 100.0
                wid = int(self.viewport.winId())
                success = self.streamer.start(preset_name=preset, fullscreen=False, volume=vol, wid=wid)
                if success:
                    self.btn_toggle_stream.setText("⏹ Stop")
                    self.btn_toggle_stream.setObjectName("btnToggleLive")
                    self.btn_toggle_stream.setStyle(self.btn_toggle_stream.style())
                    set_audio_volume(self.slider_vol.value())

        def toggle_stream(self):
            if self.streamer.is_running:
                self.streamer.stop()
                self.btn_toggle_stream.setText("▶ Start")
                self.btn_toggle_stream.setObjectName("btnToggleStart")
                self.btn_toggle_stream.setStyle(self.btn_toggle_stream.style())
            else:
                self.auto_start_stream()

        def on_preset_changed(self):
            preset = self.combo_presets.currentText()
            save_settings({"preset": preset})
            if self.streamer.is_running:
                self.streamer.stop()
                QTimer.singleShot(150, self.auto_start_stream)

        def on_led_changed(self, index: int):
            mode = self.combo_led.currentData()
            if mode is not None:
                write_led_mode(mode)

        def toggle_mute(self):
            self.muted = not self.muted
            self.btn_mute.setText("🔇" if self.muted else "🔊")
            set_audio_mute(self.muted)

        def on_volume_changed(self, val: int):
            self.lbl_vol.setText(f"{val}%")
            save_settings({"volume": val / 100.0})
            if not self.muted:
                set_audio_volume(val)

        def toggle_fullscreen(self):
            if not self.is_fullscreen:
                self.is_fullscreen = True
                self.header_deck.hide()
                self.bottom_bar.hide()
                self.showFullScreen()
                self.btn_fs.setText("⛶ Fenster")
            else:
                self.is_fullscreen = False
                self.header_deck.show()
                self.bottom_bar.show()
                self.showNormal()
                self.btn_fs.setText("⛶ Vollbild")

            if hasattr(self, "streamer") and self.streamer.mpv_proc:
                try:
                    self.streamer.mpv_proc.send_signal(signal.SIGWINCH)
                except Exception:
                    pass
            QTimer.singleShot(100, self.notify_streamer_winch)

        def notify_streamer_winch(self):
            if hasattr(self, "streamer") and self.streamer.mpv_proc:
                try:
                    self.streamer.mpv_proc.send_signal(signal.SIGWINCH)
                except Exception:
                    pass

        def exit_fullscreen(self):
            if self.is_fullscreen:
                self.toggle_fullscreen()

        def keyPressEvent(self, event):
            if event.key() == Qt.Key_Escape:
                if self.is_fullscreen:
                    self.toggle_fullscreen()
                    event.accept()
                    return
            elif event.key() in (Qt.Key_F11, Qt.Key_F):
                self.toggle_fullscreen()
                event.accept()
                return
            elif event.key() in (Qt.Key_Tab, Qt.Key_I):
                self.toggle_stats()
                event.accept()
                return
            elif event.key() in (Qt.Key_H, Qt.Key_R):
                self.force_resync()
                event.accept()
                return
            elif event.key() == Qt.Key_M:
                self.toggle_mute()
                event.accept()
                return
            elif event.key() == Qt.Key_Q:
                self.close()
                event.accept()
                return
            super().keyPressEvent(event)

        def eventFilter(self, obj, event):
            if obj == self.viewport:
                if event.type() == QEvent.MouseButtonDblClick:
                    self.toggle_fullscreen()
                    return True
            return super().eventFilter(obj, event)

        def update_status(self):
            info = self.streamer.get_signal_info()
            if info["locked"]:
                hdr_str = f" • {info['hdr']}" if info.get("hdr") else ""
                self.lbl_pill.setText(f"● LIVE {info['resolution']} @ {info['framerate']}{hdr_str}")
                self.lbl_pill.setStyleSheet("background-color: #12281a; color: #3fb950; border: 1px solid #238636;")
            else:
                self.lbl_pill.setText("○ KEIN HDMI-SIGNAL")
                self.lbl_pill.setStyleSheet("background-color: #2b1313; color: #f85149; border: 1px solid #7d2323;")

            if self.streamer.is_running and not self.streamer.poll():
                self.btn_toggle_stream.setText("▶ Start")
                self.btn_toggle_stream.setObjectName("btnToggleStart")
                self.btn_toggle_stream.setStyle(self.btn_toggle_stream.style())

        def closeEvent(self, event):
            self.streamer.stop()
            event.accept()


def main():
    parser = argparse.ArgumentParser(description="EverLive - Live Gamer 4K Console Gaming Screen")
    parser.add_argument("-f", "--fullscreen", action="store_true", help="Direkt im Vollbild starten")
    parser.add_argument("-p", "--preset", default="Auto (HDMI Signal)", choices=list(PRESETS.keys()), help="Auflösungs-Preset")
    parser.add_argument("--cli", action="store_true", help="Startet direkt ohne GUI")
    parser.add_argument("--mpv", action="store_true", help="Erzwingt mpv als Video-Engine anstelle von everlive_display")
    parser.add_argument("--led", type=str, default=None,
                        help="GC573 LED Beleuchtung sofort ändern (0-7 oder off/blue/green/red/cyan/magenta/yellow/white)")
    parser.add_argument("-v", "--volume", type=float, default=1.0, help="Lautstärke (0.0 bis 1.5)")
    parser.add_argument("-d", "--device", default=None, help="V4L2 Video Device (z.B. /dev/video0, Standard: Auto-Erkennung)")
    parser.add_argument("-a", "--audio", default=None, help="ALSA Audio Device (z.B. hw:CARD=A4K,DEV=0, Standard: Auto-Erkennung)")
    args = parser.parse_args()

    if args.led is not None:
        key = args.led.lower().strip()
        val = LED_NAME_MAP.get(key)
        if val is None:
            try:
                val = int(key)
            except ValueError:
                pass
        if val is not None and 0 <= val <= 7:
            if write_led_mode(val):
                print(f"[+] GC573 LED Beleuchtung erfolgreich auf Modus {val} gesetzt.")
            else:
                print(f"[!] Fehler beim Setzen der GC573 LED auf Modus {val}.")
        else:
            print(f"[!] Ungültiger LED Modus '{args.led}'. Gültig: {list(LED_NAME_MAP.keys())}")
        if not args.cli and not any(arg in sys.argv for arg in ["-f", "--fullscreen", "-p", "--preset"]):
            sys.exit(0)

    streamer = GameStreamer(vdev=args.device, adev=args.audio, force_mpv=args.mpv)
    print(f"[*] GC573 Hardware: Video={streamer.vdev}, Audio={streamer.adev}")

    # Graceful shutdown on Ctrl+C
    def sig_handler(sig, frame):
        streamer.stop()
        sys.exit(0)
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    if args.cli or not HAS_QT:
        print("[*] Starte PS5 Gaming Screen im Low-Latency-Modus...")
        info = streamer.get_signal_info()
        print(f"[*] Signal: {info['resolution']} {info['framerate']}".strip())
        if not info["locked"]:
            print("[!] Warnung: Es wurde aktuell kein aktives HDMI-Signal auf der GC573 erkannt.")
            print("    Bitte prüfen, ob die Quelle eingeschaltet ist und ein Bild ausgibt.")
        if not streamer.start(preset_name=args.preset, fullscreen=args.fullscreen, volume=args.volume):
            sys.exit(1)
        print("[+] Fenster geöffnet. Drücke 'q' im Fenster oder Strg+C zum Beenden.")
        while streamer.poll():
            time.sleep(0.5)
        streamer.stop()
    else:
        app = QApplication(sys.argv)
        app.setApplicationName("EverLive")
        window = MainWindow(streamer)
        window.show()
        if args.fullscreen:
            window.toggle_fullscreen()
        sys.exit(app.exec())


if __name__ == "__main__":
    main()
