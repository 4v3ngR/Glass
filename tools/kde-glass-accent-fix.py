#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import ctypes
import struct

CONFIG_DIR = os.path.expanduser("~/.config")
KDEGLOBALS_PATH = os.path.join(CONFIG_DIR, "kdeglobals")

def fix_kdeglobals():
    if not os.path.exists(KDEGLOBALS_PATH):
        return False

    try:
        with open(KDEGLOBALS_PATH, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Error reading kdeglobals: {e}", file=sys.stderr)
        return False

    in_selection_section = False
    modified = False
    new_lines = []

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            in_selection_section = (stripped == "[Colors:Selection]")
            new_lines.append(line)
            continue

        if in_selection_section:
            if "=" in line:
                key, val = line.split("=", 1)
                val_clean = val.rstrip("\r\n")
                parts = val_clean.split(",")
                if len(parts) == 4:
                    # Strip 4th alpha component (0, 1, 255) to restore standard opaque RGB
                    new_val = ",".join(parts[:3])
                    new_line = f"{key}={new_val}\n"
                    if new_line != line:
                        modified = True
                        line = new_line
        new_lines.append(line)

    if modified:
        try:
            with open(KDEGLOBALS_PATH, "w", encoding="utf-8") as f:
                f.writelines(new_lines)
            print("Fixed transparent/invisible selection colors in kdeglobals")
            subprocess.run(
                ["dbus-send", "--session", "--type=signal", "/KGlobalSettings", "org.kde.KGlobalSettings.notifyChange", "int32:0", "int32:0"],
                capture_output=True,
                check=False
            )
            return True
        except Exception as e:
            print(f"Error writing kdeglobals: {e}", file=sys.stderr)
            return False
    return False

def main():
    # Run once on startup
    fix_kdeglobals()

    # Inotify syscall setup
    libc = ctypes.CDLL(None)
    IN_CLOSE_WRITE = 0x00000008
    IN_MOVED_TO = 0x00000080

    fd = libc.inotify_init()
    if fd < 0:
        print("Failed to initialize inotify, falling back to polling", file=sys.stderr)
        while True:
            time.sleep(3)
            fix_kdeglobals()
        return

    # Watch ~/.config directory
    watch_mask = IN_CLOSE_WRITE | IN_MOVED_TO
    wd = libc.inotify_add_watch(fd, CONFIG_DIR.encode("utf-8"), watch_mask)
    if wd < 0:
        print(f"Failed to add watch on {CONFIG_DIR}", file=sys.stderr)
        os.close(fd)
        return

    EVENT_FMT = "iIII"
    EVENT_SIZE = struct.calcsize(EVENT_FMT)

    try:
        while True:
            data = os.read(fd, 4096)
            if not data:
                break
            offset = 0
            while offset + EVENT_SIZE <= len(data):
                wd, mask, cookie, name_len = struct.unpack_from(EVENT_FMT, data, offset)
                offset += EVENT_SIZE
                name = ""
                if name_len > 0:
                    name_bytes = data[offset : offset + name_len]
                    name = name_bytes.split(b"\0", 1)[0].decode("utf-8", errors="ignore")
                    offset += name_len
                if name == "kdeglobals":
                    time.sleep(0.1)
                    fix_kdeglobals()
    except KeyboardInterrupt:
        pass
    finally:
        os.close(fd)

if __name__ == "__main__":
    main()
