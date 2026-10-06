# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Structure
`Glass` is an application Window Decoration (KStyle engine) fork optimized for providing matching decorations to the `kwin-effects-glass` plugin.

- This repository primarily focuses on desktop application styles and KWin borders (`libglasscommon`, `kdecoration`).

### Deployment Environment
Because the host environment (Bazzite) uses `rpm-ostree`, system installations typically involve rendering to `~/.local` prefix directories or using `install.sh` scripts that manage containerized `cmake --install` locations locally.

For compilation testing, utilize the `fedora-kwin` distrobox image mapping if full system-level packages (`cpack`) are ever integrated, or map installations per the instructions found in `install.sh`.

### Modifications Guideline
Ensure your changes in border sizes/decorations do not negatively clip with KWin Effect's `glass` shading rules on corners. KWin's compositor heavily relies on window-provided `blurRegion()` bounding box measurements, which this engine supplies natively.

## Per-User Accent Fix Integration

### New files
- `tools/kde-glass-accent-fix.py` — Python helper that inotify-watches `~/.config/kdeglobals` and strips spurious 4-component RGBA values from `[Colors:Selection]` so that KDE accent colours render correctly.
- `tools/kde-glass-accent-fix.service` — Portable systemd user unit using `ExecStart=%h/.local/bin/kde-glass-accent-fix.py` (no hard-coded paths or usernames).
- `tools/glass-user-helper` — Bash install/remove script for the per-user helper. Handles root/user context correctly: if run under `sudo`, re-executes as `SUDO_USER` via `runuser`. On systems without `systemd --user` it installs the files and prints deferred activation instructions.
- `colors/GlassDarkFixed.colors` — The exact current `GlassDark (Fixed)` color scheme with correct opaque RGB `[Colors:Selection]` values. Wired into `colors/CMakeLists.txt` for system-wide install.

### Install/rebuild script behaviour
- `install.sh` runs `tools/glass-user-helper install` after a successful `cmake --install`. Build default is 2 jobs (`JOBS` env var overrides).
- `install.sh helper` (alias `user-helper`) installs only the per-user helper, for immutable hosts.
- `rebuild.sh` rebuilds the existing build directory and also calls `tools/glass-user-helper install`.
- `uninstall.sh` removes system files and delegates `tools/glass-user-helper remove` when `SUDO_USER` is set, with clear printed instructions when running as bare root.

### Rule: do not remove the user helper during pre-build cleanup
`remove_qt5_files` and `remove_qt6_files` in `install.sh` only remove system files. The per-user helper (files under `~/.local/bin` and `~/.config/systemd/user`) is only touched by explicit `helper install` / `helper remove` / full `remove` paths.
