#!/usr/bin/env python3
"""Interactive QEMU test: send mouse move + click events via the HMP monitor,
then capture a screenshot. Confirms the cursor tracks and clicks open apps."""
import os, sys, subprocess, time, select

qemu = os.environ.get("QEMU_BIN", "qemu-system-i386")
bios = os.environ["QEMU_BIOS_DIR"]
iso = sys.argv[1] if len(sys.argv) > 1 else "/home/z/my-project/kernel/build/ziphuos_demo.iso"
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/mousetest"

cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M",
       "-vga", "std", "-display", "none", "-monitor", "stdio"]
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)

def send(s):
    p.stdin.write((s + "\n").encode()); p.stdin.flush()
    time.sleep(0.15)

# let it boot + show desktop
time.sleep(4)

# capture before mouse move (cursor at center)
send("mouse_move 0 0")
time.sleep(0.3)
send("screendump %s_before.ppm" % out)
time.sleep(0.5)

# move mouse right + down by sending relative deltas
for _ in range(20):
    send("mouse_move 8 6")
time.sleep(0.3)
send("screendump %s_moved.ppm" % out)
time.sleep(0.5)

# convert both to png and compare cursor position
from PIL import Image
for tag in ["before", "moved"]:
    try:
        im = Image.open("%s_%s.ppm" % (out, tag)).convert("RGB")
        im.save("%s_%s.png" % (out, tag))
        px = im.load()
        # find the leftmost white pixel that's part of an arrow (topmost white)
        for y in range(im.size[1]):
            row_w = [x for x in range(0, im.size[0], 1) if px[x,y]==(255,255,255)]
            if row_w:
                print("%s: first white px at x=%d y=%d" % (tag, row_w[0], y))
                break
    except Exception as e:
        print(tag, "err:", e)

# click to test dispatch
send("mouse_button 1")
time.sleep(0.1)
send("mouse_button 0")
time.sleep(0.5)
send("screendump %s_clicked.ppm" % out)
time.sleep(0.5)
try:
    im = Image.open("%s_clicked.ppm" % out).convert("RGB")
    im.save("%s_clicked.png" % out)
    print("click screenshot saved")
except Exception as e:
    print("click err:", e)

send("quit")
try: p.wait(timeout=3)
except: p.kill()
