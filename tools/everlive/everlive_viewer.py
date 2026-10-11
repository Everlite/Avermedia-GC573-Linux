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

# PySide6 is optional when run with --cli
try:
    from PySide6.QtWidgets import (
        QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
        QLabel, QPushButton, QComboBox, QCheckBox, QSlider, QFrame,
        QGroupBox, QMessageBox
    )
    from PySide6.QtCore import Qt, QTimer, QThread, Signal
    from PySide6.QtGui import QFont, QIcon, QColor, QPalette
    HAS_QT = True
except ImportError:
    HAS_QT = False


DEFAULT_VDEV = "/dev/video2"
DEFAULT_ADEV = "hw:CARD=A4K,DEV=0"

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
    except PermissionError:
        # Fallback to pkexec if user permissions not granted yet
        try:
            p = subprocess.run(
                ["pkexec", "tee", path],
                input=f"{mode}\n".encode(),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5.0
            )
            success = (p.returncode == 0)
        except Exception:
            success = False
    except Exception:
        success = False

    if success:
        save_settings({"led_mode": mode})
    return success


class GameStreamer:
    """Manages low-latency video and audio child processes."""
    def __init__(self, vdev: str = DEFAULT_VDEV, adev: str = DEFAULT_ADEV):
        self.vdev = vdev
        self.adev = adev
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
                fps = 60
                if clk_m and tot_w and tot_h:
                    clk = int(clk_m.group(1))
                    tw = int(tot_w.group(1))
                    th = int(tot_h.group(1))
                    if tw > 0 and th > 0:
                        fps = round(clk / (tw * th))
                info["locked"] = True
                info["resolution"] = f"{w}x{h}"
                info["framerate"] = f"{fps} Hz"
        except Exception:
            # Fallback check via current format
            try:
                fmt_out = subprocess.check_output(
                    ["v4l2-ctl", "-d", self.vdev, "-V"],
                    stderr=subprocess.DEVNULL, timeout=0.5
                ).decode()
                dim_m = re.search(r"Width/Height\s*:\s*(\d+)/(\d+)", fmt_out)
                if dim_m:
                    info["resolution"] = f"{dim_m.group(1)}x{dim_m.group(2)}"
                    info["locked"] = True
            except Exception:
                pass

        return info

    def start(self, preset_name: str = "Auto (HDMI Signal)", fullscreen: bool = False, volume: float = 1.0) -> bool:
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
            # Apply to V4L2 device
            try:
                subprocess.run(
                    ["v4l2-ctl", "-d", self.vdev, f"--set-fmt-video=width={width},height={height},pixelformat={fmt}"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1.0
                )
            except Exception as e:
                print(f"[*] Hinweis beim Setzen des Formats: {e}")

        script_dir = os.path.dirname(os.path.abspath(__file__))
        native_bin = os.path.join(script_dir, "everlive_display")
        if not os.path.isfile(native_bin):
            native_bin = os.path.join(script_dir, "gc573_display")

        if os.path.isfile(native_bin) and os.access(native_bin, os.X_OK):
            cmd = [native_bin, "-d", self.vdev, "-a", self.adev]
            if fullscreen:
                cmd.append("-f")
            print(f"[*] Starte nativen EverLive Direct-Renderer mit integriertem Audio ({native_bin})...")
            try:
                self.mpv_proc = subprocess.Popen(cmd)
                self.is_running = True
                return True
            except Exception as e:
                print(f"[!] Fehler beim Starten von everlive_display: {e}")
                self.stop()
                return False

        # Build optimized mpv command for zero-delay display
        mpv_cmd = [
            "mpv",
            f"av://v4l2:{self.vdev}",
            "--title=PS5 - AVerMedia GC573",
            "--profile=low-latency",
            "--untimed",
            "--no-cache",
            "--no-correct-pts",
            "--demuxer-lavf-probe-info=no",
            "--demuxer-lavf-analyzeduration=0",
            "--demuxer-readahead-secs=0",
            "--video-latency-hacks=yes",
            "--interpolation=no",
            "--framedrop=no",
            "--video-sync=display-desync",
            "--opengl-glfinish=yes",
            "--vo=gpu",
            "--gpu-context=wayland",
            "--cursor-autohide=1000",
            "--no-terminal",
            "--input-default-bindings=yes",
        ]

        # Video size options if preset selected
        lavf_opts = ["fflags=nobuffer", "flags=low_delay"]
        if width and height:
            lavf_opts.append(f"video_size={width}x{height}")
        if fps:
            lavf_opts.append(f"framerate={fps}")
            mpv_cmd.append(f"--fps={fps}")
        lavf_opts.append("input_format=yuyv422")
        mpv_cmd.append(f"--demuxer-lavf-o={','.join(lavf_opts)}")

        if fullscreen:
            mpv_cmd.append("--fullscreen")

        try:
            self.mpv_proc = subprocess.Popen(mpv_cmd)
            self.is_running = True
            if self.adev and self.adev != "none":
                try:
                    self.arecord_proc = subprocess.Popen(
                        ["arecord", "-D", self.adev, "-f", "S16_LE", "-c", "8", "-r", "48000",
                         "--buffer-size=8192", "--period-size=480", "-q"],
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
                    )
                    self.pwplay_proc = subprocess.Popen(
                        ["pw-play", "--latency=10ms", "-P", "application.name=gc573-audio node.name=gc573-audio", "-"],
                        stdin=self.arecord_proc.stdout, stderr=subprocess.DEVNULL
                    )
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
                self.mpv_proc.terminate()
                self.mpv_proc.wait(timeout=1.0)
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
    class MainWindow(QMainWindow):
        def __init__(self, streamer: GameStreamer):
            super().__init__()
            self.streamer = streamer
            self.init_ui()

            # Timer to check signal & player status
            self.timer = QTimer(self)
            self.timer.timeout.connect(self.update_status)
            self.timer.start(1000)

        def init_ui(self):
            self.setWindowTitle("🎮 GC573 PS5 Gaming Screen")
            self.setMinimumSize(480, 430)

            # Modern Dark Theme Stylesheet
            self.setStyleSheet("""
                QMainWindow {
                    background-color: #12141a;
                }
                QGroupBox {
                    border: 1px solid #2a2e3d;
                    border-radius: 8px;
                    margin-top: 14px;
                    font-weight: bold;
                    color: #8fa0c0;
                    padding-top: 15px;
                }
                QGroupBox::title {
                    subcontrol-origin: margin;
                    subcontrol-position: top left;
                    padding: 0 8px;
                    color: #58a6ff;
                }
                QLabel {
                    color: #e6edf3;
                    font-size: 13px;
                }
                QComboBox {
                    background-color: #1f2430;
                    color: #f0f6fc;
                    border: 1px solid #3b4252;
                    border-radius: 6px;
                    padding: 6px 12px;
                    min-height: 24px;
                }
                QComboBox::drop-down {
                    border: none;
                }
                QPushButton#btnPlay {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1f6feb, stop:1 #388bfd);
                    color: white;
                    font-size: 15px;
                    font-weight: bold;
                    border-radius: 8px;
                    padding: 12px;
                    border: none;
                }
                QPushButton#btnPlay:hover {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #388bfd, stop:1 #58a6ff);
                }
                QPushButton#btnStop {
                    background-color: #da3633;
                    color: white;
                    font-size: 15px;
                    font-weight: bold;
                    border-radius: 8px;
                    padding: 12px;
                    border: none;
                }
                QPushButton#btnStop:hover {
                    background-color: #f85149;
                }
                QCheckBox {
                    color: #c9d1d9;
                    font-size: 13px;
                }
                QSlider::groove:horizontal {
                    height: 6px;
                    background: #2a2e3d;
                    border-radius: 3px;
                }
                QSlider::sub-page:horizontal {
                    background: #58a6ff;
                    border-radius: 3px;
                }
                QSlider::handle:horizontal {
                    background: #f0f6fc;
                    width: 16px;
                    margin-top: -5px;
                    margin-bottom: -5px;
                    border-radius: 8px;
                }
            """)

            central_widget = QWidget()
            self.setCentralWidget(central_widget)
            layout = QVBoxLayout(central_widget)
            layout.setContentsMargins(20, 20, 20, 20)
            layout.setSpacing(15)

            # Signal Status Box
            status_box = QGroupBox("Signal-Status")
            status_layout = QVBoxLayout(status_box)
            self.lbl_status = QLabel("● Prüfe Signal...")
            self.lbl_status.setFont(QFont("Inter", 13, QFont.Bold))
            self.lbl_details = QLabel("HDMI: - | Audio: -")
            self.lbl_details.setStyleSheet("color: #8b949e; font-size: 12px;")
            status_layout.addWidget(self.lbl_status)
            status_layout.addWidget(self.lbl_details)
            layout.addWidget(status_box)

            # Settings Box
            settings_box = QGroupBox("Einstellungen")
            settings_layout = QVBoxLayout(settings_box)
            settings_layout.setSpacing(10)

            # Preset Combo
            res_layout = QHBoxLayout()
            lbl_res = QLabel("Auflösung:")
            self.combo_presets = QComboBox()
            self.combo_presets.addItems(list(PRESETS.keys()))
            res_layout.addWidget(lbl_res)
            res_layout.addWidget(self.combo_presets, 1)
            settings_layout.addLayout(res_layout)

            # Restore saved user preferences
            saved_cfg = load_settings()
            saved_led = saved_cfg.get("led_mode")
            if saved_led is not None and isinstance(saved_led, int):
                curr = read_led_mode()
                if curr >= 0 and curr != saved_led:
                    write_led_mode(saved_led)

            if "preset" in saved_cfg and saved_cfg["preset"] in PRESETS:
                self.combo_presets.setCurrentText(saved_cfg["preset"])
            self.combo_presets.currentIndexChanged.connect(
                lambda: save_settings({"preset": self.combo_presets.currentText()})
            )

            # LED Lighting Combo (RGB On-the-Fly Control)
            led_layout = QHBoxLayout()
            lbl_led = QLabel("LED Beleuchtung:")
            self.combo_led = QComboBox()
            for name, val in LED_MODES.items():
                self.combo_led.addItem(name, val)

            curr_mode = read_led_mode()
            if curr_mode >= 0:
                idx = self.combo_led.findData(curr_mode)
                if idx >= 0:
                    self.combo_led.setCurrentIndex(idx)
            else:
                self.combo_led.setEnabled(False)
                self.combo_led.setToolTip("Kein open_gc573 LED-Knoten in sysfs gefunden")

            self.combo_led.currentIndexChanged.connect(self.on_led_mode_changed)
            led_layout.addWidget(lbl_led)
            led_layout.addWidget(self.combo_led, 1)
            settings_layout.addLayout(led_layout)

            # Fullscreen Checkbox
            self.chk_fullscreen = QCheckBox("Direkt im Vollbildmodus starten (F11 zum Umschalten)")
            self.chk_fullscreen.setChecked(bool(saved_cfg.get("fullscreen", False)))
            self.chk_fullscreen.toggled.connect(
                lambda v: save_settings({"fullscreen": v})
            )
            settings_layout.addWidget(self.chk_fullscreen)

            # Volume Slider
            vol_layout = QHBoxLayout()
            lbl_vol = QLabel("Lautstärke:")
            self.slider_vol = QSlider(Qt.Horizontal)
            self.slider_vol.setRange(0, 150)
            initial_vol = int(saved_cfg.get("volume", 1.0) * 100)
            self.slider_vol.setValue(initial_vol)
            self.lbl_vol_val = QLabel(f"{initial_vol}%")
            self.lbl_vol_val.setFixedWidth(40)
            self.slider_vol.valueChanged.connect(lambda v: self.lbl_vol_val.setText(f"{v}%"))
            self.slider_vol.valueChanged.connect(
                lambda v: save_settings({"volume": v / 100.0})
            )
            vol_layout.addWidget(lbl_vol)
            vol_layout.addWidget(self.slider_vol, 1)
            vol_layout.addWidget(self.lbl_vol_val)
            settings_layout.addLayout(vol_layout)

            layout.addWidget(settings_box)

            # Action Button
            self.btn_toggle = QPushButton("🎮 PS5 Fenster starten")
            self.btn_toggle.setObjectName("btnPlay")
            self.btn_toggle.setCursor(Qt.PointingHandCursor)
            self.btn_toggle.clicked.connect(self.toggle_stream)
            layout.addWidget(self.btn_toggle)

            # Shortcut tips
            tips = QLabel("Tipp: Im Spielfenster mit 'f' oder 'F11' Vollbild umschalten, 'm' stummschalten, 'q' beenden.")
            tips.setStyleSheet("color: #6e7681; font-size: 11px;")
            tips.setAlignment(Qt.AlignCenter)
            layout.addWidget(tips)

            self.update_status()

        def update_status(self):
            info = self.streamer.get_signal_info()
            if info["locked"]:
                self.lbl_status.setText(f"● Verbunden: {info['resolution']} @ {info['framerate']}")
                self.lbl_status.setStyleSheet("color: #3fb950; font-weight: bold;")
                self.lbl_details.setText(f"HDMI 2.0 Lock aktiv | Audio: {info['audio']}")
            else:
                self.lbl_status.setText("○ Kein HDMI-Signal erkannt")
                self.lbl_status.setStyleSheet("color: #f85149; font-weight: bold;")
                self.lbl_details.setText("Bitte PS5 einschalten oder HDMI-Kabel prüfen")

            if self.streamer.is_running:
                if not self.streamer.poll():
                    self.set_ui_stopped()

        def toggle_stream(self):
            if not self.streamer.is_running:
                preset = self.combo_presets.currentText()
                fs = self.chk_fullscreen.isChecked()
                vol = self.slider_vol.value() / 100.0

                success = self.streamer.start(preset_name=preset, fullscreen=fs, volume=vol)
                if success:
                    self.btn_toggle.setText("🛑 Spielfenster schließen")
                    self.btn_toggle.setObjectName("btnStop")
                    self.btn_toggle.setStyle(self.btn_toggle.style())
                else:
                    QMessageBox.warning(self, "Fehler", "Spielfenster konnte nicht gestartet werden. Läuft OBS noch?")
            else:
                self.streamer.stop()
                self.set_ui_stopped()

        def on_led_mode_changed(self, index: int):
            mode = self.combo_led.currentData()
            if mode is not None:
                success = write_led_mode(mode)
                if not success:
                    print(f"[!] LED Modus {mode} konnte nicht gesetzt werden (Berechtigung?).")

        def set_ui_stopped(self):
            self.btn_toggle.setText("🎮 PS5 Fenster starten")
            self.btn_toggle.setObjectName("btnPlay")
            self.btn_toggle.setStyle(self.btn_toggle.style())

        def closeEvent(self, event):
            self.streamer.stop()
            event.accept()


def main():
    parser = argparse.ArgumentParser(description="GC573 PS5 Low-Latency Gaming Screen")
    parser.add_argument("-f", "--fullscreen", action="store_true", help="Direkt im Vollbild starten")
    parser.add_argument("-p", "--preset", default="Auto (HDMI Signal)", choices=list(PRESETS.keys()), help="Auflösungs-Preset")
    parser.add_argument("--cli", action="store_true", help="Startet direkt ohne Einstellungsfenster")
    parser.add_argument("-v", "--volume", type=float, default=1.0, help="Lautstärke (0.0 bis 1.5)")
    parser.add_argument("--led", type=str, default=None,
                        help="GC573 LED Beleuchtung sofort ändern (0-7 oder off/blue/green/red/cyan/magenta/yellow/white)")
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

    streamer = GameStreamer()

    # Graceful shutdown on Ctrl+C
    def sig_handler(sig, frame):
        streamer.stop()
        sys.exit(0)
    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    if args.cli or not HAS_QT:
        print("[*] Starte PS5 Gaming Screen im Low-Latency-Modus...")
        info = streamer.get_signal_info()
        print(f"[*] Signal: {info['resolution']} {info['framerate']}")
        if not streamer.start(preset_name=args.preset, fullscreen=args.fullscreen, volume=args.volume):
            sys.exit(1)
        print("[+] Fenster geöffnet. Drücke 'q' im Fenster oder Strg+C zum Beenden.")
        while streamer.poll():
            time.sleep(0.5)
        streamer.stop()
    else:
        app = QApplication(sys.argv)
        app.setApplicationName("GC573 PS5 Screen")
        window = MainWindow(streamer)
        window.show()
        if args.fullscreen:
            window.chk_fullscreen.setChecked(True)
        sys.exit(app.exec())


if __name__ == "__main__":
    main()
