#!/usr/bin/env python3
"""Boot Ziphu.OS in QEMU and capture PPM screenshots via the HMP monitor.

Usage: capture_qemu.py <iso> <out_prefix> <delay_ms> [extra qemu args...]
Writes <out_prefix>.ppm and converts to PNG via PIL.
"""
import os, sys, subprocess, time, signal

iso = sys.argv[1]
out = sys.argv[2]
delay_ms = int(sys.argv[3])
extra = sys.argv[4:]

qemu = os.environ.get("QEMU_BIN", "qemu-system-i386")
bios = os.environ["QEMU_BIOS_DIR"]

cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M",
       "-vga", "std", "-display", "none",
       "-monitor", "stdio"]
cmd += extra

print(">> launching:", " ".join(cmd))
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)

ppm = out + ".ppm"
# give the kernel time to boot, then dump the screen
time.sleep(delay_ms / 1000.0)
try:
    p.stdin.write(b"screendump %s\n" % ppm.encode())
    p.stdin.flush()
except Exception as e:
    print("write err:", e)
time.sleep(0.6)   # let the dump land
# also dump the VBE memory state for diagnostics
try:
    p.stdin.write(b"info registers\n")
    p.stdin.flush()
except Exception:
    pass
# stop qemu
try:
    p.stdin.write(b"quit\n")
    p.stdin.flush()
except Exception:
    pass
try:
    p.wait(timeout=3)
except subprocess.TimeoutExpired:
    p.kill()

# read any monitor output
import select
out_data = b""
while True:
    r, _, _ = select.select([p.stdout], [], [], 0.3)
    if not r:
        break
    chunk = os.read(p.stdout.fileno(), 4096)
    if not chunk:
        break
    out_data += chunk
print(">> monitor output:")
print(out_data.decode(errors="replace")[:2000])

if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
    print(">> PPM ok:", ppm, os.path.getsize(ppm), "bytes")
    # convert to PNG
    try:
        from PIL import Image
        im = Image.open(ppm)
        png = out + ".png"
        im.save(png)
        print(">> PNG:", png, im.size)
    except Exception as e:
        print(">> PIL convert failed:", e)
else:
    print(">> NO PPM produced")
