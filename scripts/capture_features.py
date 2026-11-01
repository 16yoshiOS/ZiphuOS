#!/usr/bin/env python3
"""Capture focused screenshots of the new features:
  A) Settings screen with RGB sliders + custom color applied (e.g. pink)
  B) Settings screen in Chinese language (设置/语言/主题 etc.)
  C) Desktop with mouse cursor + typed Shift text in the input field
Uses the production ISO (no auto-cycle) so we have full interactive control."""
import os, sys, subprocess, time

qemu = "qemu-system-i386"
bios = os.environ["QEMU_BIOS_DIR"]
iso  = sys.argv[1]
outdir = sys.argv[2]

def session(name, fn, hold_s=2.0):
    ppm = os.path.join(outdir, name + ".ppm")
    png = os.path.join(outdir, name + ".png")
    cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M",
           "-vga", "std", "-display", "none", "-monitor", "stdio"]
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, bufsize=0)
    def send(s):
        p.stdin.write((s + "\n").encode()); p.stdin.flush()
    try:
        time.sleep(4)              # boot
        fn(send)
        time.sleep(0.4)
        send("screendump %s" % ppm)
        time.sleep(0.7)
        send("quit")
    finally:
        try: p.wait(timeout=4)
        except: p.kill()
    from PIL import Image
    Image.open(ppm).save(png)
    print(">> %s -> %s" % (name, png))

def move_to(send, mx, my):
    """Move cursor to absolute (mx,my). QEMU's mouse_move is RELATIVE and the
    cursor starts at screen center (320,240), so we query the current position
    by reading where the cursor is and step toward the target. Simplest robust
    approach: send many small deltas, then a few corrective moves guided by
    re-reading the cursor location via screendump is too slow; instead we move
    in a loop, overshooting slightly, and rely on the kernel's clamp."""
    # First center the cursor (move a lot in one direction then back) - actually
    # we don't know current pos. Use the fact that mouse_move sends a delta;
    # do a big sweep to a corner then move to target from there.
    # Move to top-left corner: lots of negative deltas.
    for _ in range(60):
        send("mouse_move -10 -10"); time.sleep(0.005)
    # now cursor is at ~(0,0). Move to target in small steps.
    sx = max(1, mx // 40); sy = max(1, my // 40)
    for _ in range(40):
        send("mouse_move %d %d" % (sx, sy)); time.sleep(0.005)
    # nudge to compensate QEMU's scaling (it scales deltas by ~2x)
    time.sleep(0.15)

def click_at(send, mx, my):
    move_to(send, mx, my)
    time.sleep(0.1)
    send("mouse_button 1"); time.sleep(0.08)
    send("mouse_button 0"); time.sleep(0.15)

def drag(send, x0, y0, x1, y1, steps=30):
    """Press at (x0,y0), move to (x1,y1) with button held, release."""
    move_to(send, x0, y0)
    time.sleep(0.1)
    send("mouse_button 1")
    time.sleep(0.08)
    dx = (x1 - x0) // steps
    dy = (y1 - y0) // steps
    for i in range(steps):
        send("mouse_move %d %d" % (max(1, dx if dx>0 else 1),
                                    max(1, dy if dy>0 else 1)))
        time.sleep(0.015)
    send("mouse_button 0")
    time.sleep(0.2)

# --- A: Settings + drag R slider high (toward red) and G/B low, then click
#     a pink preset swatch to land on pink (255,105,180) ---
def A(send):
    # apps list: Settings is row index 0 -> y = LEFT_Y+32 + 0*ROW_H = 48+32 = 80
    click_at(send, 40, 88)
    time.sleep(0.3)
    # preset "Pink" is the 7th swatch (index 6). Geometry:
    # sx = LEFT_X+LEFT_W+12 = 196; x = sx+16 = 212
    # sw = 640-196-16 = 428; sl_w = 428-32 = 396; pw = (396-36)/7 = 51.4
    # preset row y: theme(22+36)=58 lang(40)=98 ... recompute from draw_settings
    # Actually easier: just click somewhere on the Pink swatch row.
    # The preset row is ~y=300. Pink swatch x ~= 212 + 6*(51+6) = 212+342 = 554
    click_at(send, 560, 300)
    time.sleep(0.3)
session("A_rgb_pink", A)

# --- B: Settings in Chinese: open settings, then press 'z' for ZH ---
def B(send):
    click_at(send, 40, 88)
    time.sleep(0.3)
    send("sendkey z")
    time.sleep(0.3)
session("B_zh_settings", B)

# --- C: Desktop, move mouse to a visible spot, type Shift text ---
def C(send):
    move_to(send, 120, 90)
    time.sleep(0.2)
    for k in ["shift-1", "shift-semicolon", "shift-a", "b", "c"]:
        send("sendkey %s" % k); time.sleep(0.1)
session("C_desktop_shift", C)

# --- D: Desktop in Chinese: open settings, press 'z' (ZH), Esc back, move cursor ---
def D(send):
    click_at(send, 40, 88)
    time.sleep(0.3)
    send("sendkey z")
    time.sleep(0.2)
    send("sendkey esc")
    time.sleep(0.3)
    move_to(send, 100, 88)
session("D_desktop_zh", D)
