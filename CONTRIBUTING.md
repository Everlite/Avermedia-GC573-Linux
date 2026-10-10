# Contributing to EverLive

Contributions, bug reports, and hardware testing feedback are warmly welcomed! Whether you want to report a kernel issue, submit a patch for frame pacing, or add support for other distributions, here is how you can help.

---

## 🛠️ How to Report Issues

If you encounter an issue, please open an issue using our [Bug Report Template](.github/ISSUE_TEMPLATE/bug_report.md).
Please provide:
1. Your distribution and kernel version (`uname -r`).
2. PCI device detection (`lspci -nnk -d 1461:`).
3. The kernel log output (`sudo dmesg | grep -iE 'gc573|open_gc573'`).
4. Connected console and monitor model.

---

## 💻 Development & Submitting Pull Requests

1. **Clean-Room & Blob-Free Policy:**
   - Under no circumstances should proprietary binaries, firmware dumps, decompiler outputs, or proprietary SDK headers be committed to the repository.
   - All contributions to `open_gc573` must be original source code under the **GPL-2.0-only** license.
   - Companion user-space tools in `tools/everlive/` and root scripts are licensed under **MIT**.

2. **Code Standards:**
   - Kernel module code should follow standard Linux kernel coding conventions.
   - Avoid breaking backward compatibility with existing supported kernels (6.19+ through 7.x).
   - Test your changes locally before submitting a PR:
     ```bash
     make -C open_gc573 LLVM=1 CC=clang clean
     make -C open_gc573 LLVM=1 CC=clang
     ```

3. **AI-Assisted Contributions:**
   - In accordance with our [transparency policy](README.md#transparency--ai-assistance), AI-assisted code is welcomed as long as you have personally validated it on real hardware or through tests, and you understand the logic being submitted.

---

## 🎮 Hardware Testing

We are especially interested in test reports for:
- **AVerMedia Live Gamer BOLT (GC555):** Physical hardware testing over Thunderbolt on Linux.
- Other consoles (Nintendo Switch, Xbox Series X/S, Steam Deck dock).
- Different desktop compositors (GNOME Wayland, Hyprland, Sway, KDE X11).
