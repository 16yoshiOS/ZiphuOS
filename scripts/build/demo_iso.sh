#!/bin/bash
# Build the demo ISO (auto-cycling ZK_DEMO build) the same way iso.sh builds
# the production ISO: a self-contained GRUB core image with an embedded
# config + minimal modules, no on-disk module tree, no Joliet/Rock Ridge, no
# trailing pad. Lands at ~270 KB.
set -e
source /home/z/my-project/scripts/env.sh
K=/home/z/my-project/kernel
ROOT=/home/z/my-project/tools/iso/root
GRUB_LIB=$ROOT/usr/lib/grub/i386-pc
ISO=$K/build/ziphuos_demo.iso
WORK=$K/build/demo_work

rm -rf "$WORK"; mkdir -p "$WORK/isodir/boot/grub"
cp "$K/build/ziphuos_demo.bin" "$WORK/isodir/boot/ziphuos.bin"

cat > "$WORK/embed.cfg" <<'EOF'
set timeout=0
set default=0
menuentry "Ziphu.OS 1.0" {
    multiboot /boot/ziphuos.bin
    boot
}
EOF
cp "$WORK/embed.cfg" "$WORK/isodir/boot/grub/grub.cfg"

LD_LIBRARY_PATH="$LD_LIBRARY_PATH" "$ROOT/usr/bin/grub-mkimage" \
    -p /boot/grub -O i386-pc -d "$GRUB_LIB" \
    -o "$WORK/core.img" -c "$WORK/embed.cfg" \
    biosdisk iso9660 multiboot boot 2>&1 | head

cat "$GRUB_LIB/cdboot.img" "$WORK/core.img" > "$WORK/isodir/boot/eltorito.img"

xorriso -as mkisofs \
    -b boot/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table \
    --grub2-boot-info \
    -input-charset utf-8 -output "$ISO" "$WORK/isodir" 2>&1 | tail -3

echo "=== verify El Torito ==="
xorriso -indev "$ISO" -report_el_torito plain 2>&1 | grep -iE "torito|boot|catalog|platform" | head -8
ls -la "$ISO"
