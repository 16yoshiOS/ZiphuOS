#!/usr/bin/env python3
"""Boot ISO in QEMU, send monitor commands at intervals, print output.
Usage: probe_qemu.py <iso> <total_seconds> [cmd@delay ...]
"""
import os, sys, subprocess, time, select, threading

iso = sys.argv[1]
total = int(sys.argv[2])
cmds = sys.argv[3:]   # "info registers@3" => send at t=3

qemu = "qemu-system-i386"
bios = os.environ["QEMU_BIOS_DIR"]

cmd = [qemu, "-L", bios, "-cdrom", iso, "-m", "64M", "-vga", "std",
       "-display", "none", "-monitor", "stdio", "-no-reboot",
       "-debugcon", "file:/tmp/dbg.log"]
print(">> ", " ".join(cmd))
p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, bufsize=0)

scheduled = []
for c in cmds:
    if "@" in c:
        s, t = c.rsplit("@", 1)
        scheduled.append((float(t), s))
    else:
        scheduled.append((1.0, c))

start = time.time()
out = b""
done = False
while time.time() - start < total and p.poll() is None:
    # send any due commands
    for (t, s) in list(scheduled):
        if time.time() - start >= t:
            try:
                p.stdin.write((s + "\n").encode())
                p.stdin.flush()
            except Exception:
                pass
            scheduled.remove((t, s))
    r, _, _ = select.select([p.stdout], [], [], 0.2)
    if r:
        chunk = os.read(p.stdout.fileno(), 4096)
        if chunk:
            out += chunk
    time.sleep(0.05)

try:
    p.stdin.write(b"quit\n"); p.stdin.flush()
except Exception:
    pass
try:
    p.wait(timeout=2)
except subprocess.TimeoutExpired:
    p.kill()

time.sleep(0.3)
r, _, _ = select.select([p.stdout], [], [], 0.5)
if r:
    out += os.read(p.stdout.fileno(), 4096)

print("==== MONITOR ====")
print(out.decode(errors="replace"))
print("==== DEBUGCON ====")
try:
    print(open("/tmp/dbg.log").read())
except Exception as e:
    print("(no debugcon log:", e, ")")
