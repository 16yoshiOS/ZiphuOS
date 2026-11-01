#!/usr/bin/env python3
"""Precise Shift test: type '1' then clear, type Shift+'1' (expect '!'),
type ';' then clear, type Shift+';' (expect ':'). Compare glyph shapes."""
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

time.sleep(4)
send("sendkey 1")          # expect '1'
time.sleep(0.3); send("screendump %s_one.ppm" % out); time.sleep(0.5)
send("sendkey esc")        # clear
send("sendkey shift"); send("sendkey 1"); send("sendkey noshift")  # expect '!'
time.sleep(0.3); send("screendump %s_bang.ppm" % out); time.sleep(0.5)
send("sendkey esc")
send("sendkey semicolon")  # expect ';'
time.sleep(0.3); send("screendump %s_semi.ppm" % out); time.sleep(0.5)
send("sendkey esc")
send("sendkey shift"); send("sendkey semicolon"); send("sendkey noshift")  # expect ':'
time.sleep(0.3); send("screendump %s_colon.ppm" % out); time.sleep(0.5)
send("quit")
try: p.wait(timeout=3)
except: p.kill()

# IBM VGA glyph fingerprints (8x16, the char after the '>' prompt at x20)
# Compare the typed char (at x20..28) against known IBM VGA shapes.
from PIL import Image
def char_at(tag):
    im = Image.open("%s_%s.ppm" % (out, tag)).convert("L")
    px = im.load()
    # input field text starts at x=20; glyph is 8 wide, 16 tall, baseline y~458
    rows = []
    for y in range(456, 472):
        r = 0
        for c in range(8):
            if px[20+c, y] < 100: r |= (0x80 >> c)
        rows.append(r)
    return rows

for tag, expect in [("one","1"),("bang","!"),("semi",";"),("colon",":")]:
    g = char_at(tag)
    # render for visual check
    art = "\n".join("".join("#" if (g[y]>>(7-c))&1 else "." for c in range(8)) for y in range(16))
    print("=== %s (expected '%s') ===" % (tag, expect))
    print(art)
    print()
