#!/usr/bin/env python3
"""Capture all 7 Ziphu.OS screenshots via the demo ISO, but with a visible
mouse cursor moved to a sensible spot on each screen and Shift text typed
into the input field so the keyboard layer is demonstrated.

The demo ISO auto-cycles screens (2s each). We move the mouse + type while a
target screen is showing, then dump the screen at the right moment."""
import os, sys, subprocess, time

qemu = "qemu-system-i386"
bios = os.environ["QEMU_BIOS_DIR"]
iso  = sys.argv[1]
outdir = sys.argv[2]

def cap(name, delay_ms, mouse_xy=None, type_keys=None, type_at_ms=None):
    """Boot, wait delay_ms (plus optional type_at_ms offset), optionally move
    mouse to (x,y) and type shift keys, then screendump."""
    ppm = os.path.join(outdir, name + ".ppm")
    png = os.path.join(outdir, name + ".png")
    cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M",
           "-vga", "std", "-display", "none", "-monitor", "stdio"]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, bufsize=0)
    def send(s):
        p.stdin.write((s + "\n").encode()); p.stdin.flush()
    try:
        time.sleep(delay_ms / 1000.0)
        if type_at_ms:
            time.sleep(type_at_ms / 1000.0)
        if mouse_xy:
            mx, my = mouse_xy
            # move from (0,0) to (mx,my) in ~20 steps
            send("mouse_move 0 0")
            sx, sy = max(1, mx // 20), max(1, my // 20)
            for _ in range(20):
                send("mouse_move %d %d" % (sx, sy)); time.sleep(0.01)
        if type_keys:
            for k in type_keys:
                send("sendkey %s" % k); time.sleep(0.08)
        time.sleep(0.25)
        send("screendump %s" % ppm)
        time.sleep(0.6)
        send("quit")
    finally:
        try: p.wait(timeout=3)
        except: p.kill()
    from PIL import Image
    im = Image.open(ppm)
    im.save(png)
    print(">> %s: %s" % (name, png))

# demo cycle: boot~2s, desktop@3.5, settings@5.5, fileman@7.5, memlog@9.5,
# syslog@11.5, about@13.5
cap("01_boot",      2200, mouse_xy=(300, 240))
cap("02_desktop",   3500, mouse_xy=(100, 88),  type_keys=["shift-1","shift-semicolon","shift-a","shift-b"])
cap("03_settings",  5500, mouse_xy=(400, 120), type_keys=["shift-1","shift-2","shift-3"])
cap("04_fileman",   7500, mouse_xy=(300, 200), type_keys=["1","2","3","semicolon"])
cap("05_memlog",    9500, mouse_xy=(500, 300), type_keys=["a","b","c"])
cap("06_syslog",   11500, mouse_xy=(450, 150), type_keys=["shift-a","shift-b","shift-c"])
cap("07_about",    13500, mouse_xy=(400, 100), type_keys=["shift-1","shift-semicolon"])
