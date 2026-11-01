# Ziphu.OS 1.0 — OpenZK (Open-ZiphuKernel)

A 32-bit monolithic hobby operating system for the i386 architecture, with a
monochrome "Lightning" (Молния) graphical shell accented by neon-turquoise
window frames. Built bare-metal, no host libc.

## Build & run

Requires: gcc (multilib / `-m32`), nasm, ld, xorriso, grub-mkimage + grub i386-pc
modules, qemu-system-i386.

```
cd kernel
make                         # builds build/ziphuos.bin (multiboot ELF kernel)
../scripts/build/iso.sh      # builds build/ziphuos.iso (El Torito bootable)
qemu-system-i386 -cdrom build/ziphuos.iso -m 64M
```

## What is in here (step1)

| Component | File(s) | Notes |
|---|---|---|
| Multiboot header + entry | `boot/entry.asm`, `link.ld` | GRUB-loaded, 1 MiB |
| GDT/IDT/PIC + ISR stubs | `src/idt.c`, `boot/isr.asm` | PIC remapped to 32/40 |
| BGA/VBE video (640x480x32) | `src/video.c` | PCI BAR0 LFB discovery |
| VGA fallback (mode 13h) | `src/video.c` | 256-colour palette, 2x downscale |
| Boot screen + progress bar | `src/gui.c` `gui_boot_screen` | "Ziphu.OS" wordmark + bar |
| Lightning shell desktop | `src/gui.c` | apps list (left) + SYSTEM window (right) |
| SYSTEM: CPUID / VM / MEM | `src/cpuid.c`, `src/vm.c`, `src/mem.c` | live values |
| Settings: theme + language | `src/gui.c` `draw_settings` | Light/Dark, EN/ZH |
| Crash System (watchdog RSOD) | `src/crash.c`, `src/timer.c`, `src/rsod.c` | 5 s -> RSOD -> reboot |
| Crash VMS (kernel hang) | `src/crash.c` | cli+hlt forever |
| Keyboard IRQ1 + Shift inversion | `src/keyboard.c` | 1->!, ;->: |
| VFS (vfs_node_t) | `src/vfs.c` | open/close/read/write/readdir |
| /examples shared folder | `src/vfs.c` `vfs_seed_examples` | injected at boot |
| File Manager (dual pane) | `src/gui.c` `draw_file_manager` | drives | files, byte sizes |
| IDE + SATA drive probe | `src/drive.c` | PCI class 01/06 scan |
| FAT16/FAT32 structures | `src/drive.c` | BPB, dirent, `fat_read_cluster_chain` |

## Layout

```
kernel/
  boot/entry.asm        multiboot + _start
  boot/isr.asm          ISR/IRQ stubs, load_gdt/load_idt
  link.ld               linker script (1 MiB, .multiboot first)
  include/              zk_types.h, zk_port.h, zk_util.h, zk_video.h, zk_core.h
  src/                  all kernel C sources
scripts/
  build/iso.sh          El Torito ISO builder (grub-mkimage + xorriso)
  build/gen_font.py     8x16 font generator
  env.sh                toolchain PATH/LD_LIBRARY_PATH
  capture_qemu.py       headless QEMU screenshot helper
```

## Interactive keys (desktop)

- `j` / `k` — move selection in the apps list
- `Enter` — open selected app
- `Esc` — back to desktop
- Settings: `1`=Light, `2`=Dark, `e`=English, `z`=Chinese, `s`=Crash System, `v`=Crash VMS
- File Manager: `h`/`l` prev/next drive, `j`/`k` move file, `Enter` enter dir

## Theme

Default Dark. Toggle in Settings. Palette is strict monochrome greys with a
single neon-turquoise accent (`#00E5E5`) for window frames, the boot progress
bar, selected rows, and buttons.

## Localization

UI strings are English with a parallel Simplified-Chinese table behind a
`LANG_EN`/`LANG_ZH` switch in `src/gui.c`. The 8x16 bitmap font covers CP437;
a CJK font drop-in activates the translated labels.

## Screenshots

`screens/01_boot.png` — boot screen with wordmark + progress bar
`screens/02_desktop.png` — desktop: apps list + SYSTEM window (CPUID/VM/MEM)
`screens/03_settings.png` — settings: theme/language + crash-test buttons
`screens/04_fileman.png` — file manager: dual-pane drives | files
`screens/05_memlog.png` — MemLog: 256 rows of memory map
`screens/06_syslog.png` — SysLog: kernel event log
`screens/07_about.png` — about screen
