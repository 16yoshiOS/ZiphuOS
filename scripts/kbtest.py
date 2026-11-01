import os,subprocess,time
qemu="qemu-system-i386"; bios=os.environ["QEMU_BIOS_DIR"]
iso="/home/z/my-project/download/ZiphuOS-1.0.iso"
cmd=[qemu,"-L",bios,"-cdrom",iso,"-m","64M","-display","none","-monitor","stdio","-debugcon","file:/tmp/kb.log"]
p=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,bufsize=0)
time.sleep(4)
p.stdin.write(b"sendkey a\n"); p.stdin.flush(); time.sleep(0.3)
p.stdin.write(b"sendkey shift\n"); p.stdin.flush(); time.sleep(0.1)
p.stdin.write(b"sendkey 1\n"); p.stdin.flush(); time.sleep(0.1)
p.stdin.write(b"sendkey noshift\n"); p.stdin.flush(); time.sleep(0.3)
p.stdin.write(b"quit\n"); p.stdin.flush()
try: p.wait(timeout=3)
except: p.kill()
