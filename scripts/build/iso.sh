#!/bin/bash
# Build a minimal El Torito bootable ISO for Ziphu.OS. We build a GRUB core
# image with an embedded config (self-contained: no on-disk modules needed),
# prepend the El Torito cdboot stub, and register it as the boot catalog.
#
# Size budget: the only files in the ISO are the boot image (~145 KB, mostly
# the LZMA-compressed module payload) and the kernel (~52 KB). We omit Joliet
# and Rock Ridge (GRUB reads raw ISO9660) and skip trailing pad, landing the
# whole image at ~270 KB instead of the previous 4 MB.
set -e
source /home/z/my-project/scripts/env.sh
K=/home/z/my-project/kernel
ROOT=/home/z/my-project/tools/iso/root
GRUB_LIB=$ROOT/usr/lib/grub/i386-pc
ISO=$K/build/ziphuos.iso
WORK=$K/build/iso_work
rm -rf "$WORK"; mkdir -p "$WORK/isodir/boot/grub"

# 1. copy the kernel only (GRUB modules are NOT needed on disk: core.img
#    below already embeds everything it runs)
cp "$K/build/ziphuos.bin" "$WORK/isodir/boot/ziphuos.bin"

# 2. grub.cfg embedded into core.img (and a copy on disk for inspection)
cat > "$WORK/embed.cfg" <<'EOF'
set timeout=0
set default=0
menuentry "Ziphu.OS 1.0" {
    multiboot /boot/ziphuos.bin
    boot
}
EOF
cp "$WORK/embed.cfg" "$WORK/isodir/boot/grub/grub.cfg"

# 3. build core image with the minimal module set: disk access (biosdisk),
#    ISO9660 filesystem, the multiboot loader, and the boot command. The
#    embedded config runs in GRUB's rescue context, so normal/ls/cat/echo/
#    configfile are not required.
LD_LIBRARY_PATH="$LD_LIBRARY_PATH" "$ROOT/usr/bin/grub-mkimage" \
    -p /boot/grub -O i386-pc -d "$GRUB_LIB" \
    -o "$WORK/core.img" -c "$WORK/embed.cfg" \
    biosdisk iso9660 multiboot boot 2>&1 | head

# 4. prepend the El Torito boot image (cdboot.img + core.img)
cat "$GRUB_LIB/cdboot.img" "$WORK/core.img" > "$WORK/isodir/boot/eltorito.img"

# 5. build the ISO. No -R (Rock Ridge) / -J (Joliet): GRUB's iso9660 module
#    reads plain ISO9660 and the extra trees only bloat the image. We keep
#    xorriso's default trailing pad: SeaBIOS needs the El Torito boot image
#    to sit on a sufficiently large image or it mis-loads the kernel payload
#    (the kernel boots but its data segments come back wrong and the screen
#    stays blank); the ~300 KB of pad is cheaper than chasing the size floor.
xorriso -as mkisofs \
    -b boot/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table \
    --grub2-boot-info \
    -input-charset utf-8 -output "$ISO" "$WORK/isodir" 2>&1 | tail -4

echo "=== verify El Torito ==="
xorriso -indev "$ISO" -report_el_torito plain 2>&1 | grep -iE "torito|boot|catalog|platform" | head -8
ls -la "$ISO"
