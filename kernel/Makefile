# =============================================================================
#  Ziphu.OS 1.0  -  OpenZK  -  Makefile
#  Builds a freestanding i386 kernel and a bootable ISO via GRUB.
# =============================================================================
SRC := $(wildcard src/*.c)
ASM := boot/entry.asm boot/isr.asm
OBJ := $(SRC:src/%.c=build/%.o) $(ASM:boot/%.asm=build/%.o)
KERNEL := build/ziphuos.bin
ISO   := build/ziphuos.iso

CC      := gcc
CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables \
           -Wall -Wextra -O2 -std=gnu11 -Iinclude -fno-builtin -mno-red-zone \
           -mno-mmx -mno-sse -mno-sse2 -fno-omit-frame-pointer
LD      := ld
LDFLAGS := -m elf_i386 -T link.ld -nostdlib --oformat elf32-i386

NASM    := nasm
NASMFLAGS := -f elf32

GRUB    := grub-mkrescue
XORRISO := xorriso

# Font is generated from a python script; ensure it exists before compile.
FONT_GEN := ../scripts/build/gen_font.py

.PHONY: all clean iso run font

all: $(ISO)

font:
	@python3 $(FONT_GEN) >/dev/null 2>&1 || true

build:
	@mkdir -p build build/isodir/boot/grub

build/%.o: src/%.c | build font
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: boot/%.asm | build
	$(NASM) $(NASMFLAGS) $< -o $@

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

$(ISO): $(KERNEL) | build
	@mkdir -p build/isodir/boot/grub
	@cp $(KERNEL) build/isodir/boot/ziphuos.bin
	@printf "timeout=0\nset default=0\nmenuentry \"Ziphu.OS 1.0\" {\n\tmultiboot /boot/ziphuos.bin\n\tboot\n}\n" > build/isodir/boot/grub/grub.cfg
	$(GRUB) -o $@ build/isodir 2>/dev/null

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 64M -display none -serial stdio

clean:
	@rm -rf build
