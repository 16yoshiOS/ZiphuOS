import os,subprocess,time
qemu="qemu-system-i386"; bios=os.environ["QEMU_BIOS_DIR"]
iso="/home/z/my-project/download/ZiphuOS-1.0.iso"
cmd=[qemu,"-L",bios,"-cdrom",iso,"-m","64M","-display","none","-monitor","stdio","-debugcon","file:/tmp/kb2.log"]
p=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(4)
# Type Shift+1 the documented QEMU way: "sendkey shift-1" sends a press+release
# of 1 while shift is held.
p.stdin.write(b"sendkey esc\n"); p.stdin.flush(); time.sleep(0.2)
p.stdin.write(b"sendkey shift-1\n"); p.stdin.flush(); time.sleep(0.3)
p.stdin.write(b"sendkey esc\n"); p.stdin.flush(); time.sleep(0.2)
p.stdin.write(b"sendkey shift-semicolon\n"); p.stdin.flush(); time.sleep(0.3)
p.stdin.write(b"quit\n"); p.stdin.flush()
try: p.wait(timeout=3)
except: p.kill()
