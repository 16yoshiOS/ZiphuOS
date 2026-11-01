#!/usr/bin/env python3
"""Boot Ziphu.OS, wait for desktop, send key sequence, capture screenshot.
Usage: sendkey_capture.py <iso> <out> <wait_for_desktop_ms> <keys_csv> <capture_after_ms>
keys_csv is a comma list of QEMU key names, e.g. "j,j,ret" or "j,j,j,ret"
"""
import os, sys, subprocess, time, select

iso = sys.argv[1]
out = sys.argv[2]
wait_ms = int(sys.argv[3])
keys = sys.argv[4].split(",")
cap_after = int(sys.argv[5])

qemu = "qemu-system-i386"
bios = os.environ["QEMU_BIOS_DIR"]
cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M", "-vga", "std",
       "-display", "none", "-monitor", "stdio", "-no-reboot",
       "-debugcon", "file:/tmp/dbg.log"]
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)

def drain(t=0.2):
    r,_,_ = select.select([p.stdout],[],[],t)
    if r:
        try: os.read(p.stdout.fileno(), 8192)
        except: pass

time.sleep(wait_ms / 1000.0)
drain(0.3)

# send each key with a small delay so the kernel processes it
for k in keys:
    k = k.strip()
    if not k: continue
    p.stdin.write(("sendkey " + k + "\n").encode())
    p.stdin.flush()
    time.sleep(0.25)
    drain(0.15)

time.sleep(cap_after / 1000.0)
ppm = out + ".ppm"
p.stdin.write(("screendump " + ppm + "\n").encode())
p.stdin.flush()
time.sleep(0.6)
p.stdin.write(b"quit\n"); p.stdin.flush()
try: p.wait(timeout=3)
except: p.kill()

if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
    from PIL import Image
    im = Image.open(ppm); png = out + ".png"; im.save(png)
    print("PNG:", png, im.size)
else:
    print("NO PPM")
