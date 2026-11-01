#!/bin/bash
# Source this to get the bare-metal toolchain on PATH.
#   source /home/z/my-project/scripts/env.sh
MYP=/home/z/my-project/tools
export PATH="$MYP/nasm/root/usr/bin:$MYP/iso/root/usr/bin:$MYP/qemu/root/usr/bin:$PATH"
LIBS="$MYP/iso/root/usr/lib/x86_64-linux-gnu:$MYP/qemu/root/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LIBS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# QEMU needs its BIOS/VGABIOS data dir (we extracted debs, no system install)
export QEMU_BIOS_DIR="$MYP/qemu/root/usr/share/qemu-bios"
# Sanity
true
