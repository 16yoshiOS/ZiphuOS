#!/usr/bin/env python3
"""Test keyboard + Shift: send key events, capture, OCR the input field to
confirm 1->! and ;->: (and general shift pairs)."""
import os, sys, subprocess, time

qemu = os.environ.get("QEMU_BIN", "qemu-system-i386")
bios = os.environ["QEMU_BIOS_DIR"]
iso = sys.argv[1]
out = sys.argv[2]

cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M",
       "-vga", "std", "-display", "none", "-monitor", "stdio"]
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)

def send(s):
    p.stdin.write((s + "\n").encode()); p.stdin.flush()
    time.sleep(0.12)

time.sleep(4)  # boot

# Type without shift: "1;abc"
for k in ["1", "semicolon", "a", "b", "c"]:
    send("sendkey %s" % k)
time.sleep(0.3)
send("screendump %s_noshift.ppm" % out)
time.sleep(0.5)

# clear (esc) then type WITH shift: Shift+1=! Shift+;=:
send("sendkey esc")
time.sleep(0.2)
# QEMU sendkey shift is hold-then-key-then release via "shift" toggling
send("sendkey shift-aux")  # not always supported; use shift-down pattern
# Use the standard approach: sendkey shift (down) then the key
send("sendkey shift")
send("sendkey 1")
send("sendkey noshift")
send("sendkey shift")
send("sendkey semicolon")
send("sendkey noshift")
time.sleep(0.3)
send("screendump %s_shift.ppm" % out)
time.sleep(0.5)

send("quit")
try: p.wait(timeout=3)
except: p.kill()

from PIL import Image
for tag in ["noshift", "shift"]:
    im = Image.open("%s_%s.ppm" % (out, tag)).convert("L")
    px = im.load()
    # input field is y=452..476, text starts at x=20
    print("=== %s input field (y456-472, x16-180) ===" % tag)
    for y in range(456,472,2):
        row=''
        for x in range(16,200):
            row += '#' if px[x,y]<100 else ' '
        if '#' in row: print(y,row)
