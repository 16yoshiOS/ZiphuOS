; =============================================================================
;  Ziphu.OS 1.0  -  OpenZK (Open-ZiphuKernel)  -  boot/entry.asm
;  Multiboot1 header + protected-mode entry stub. Loaded by GRUB at 1 MiB.
;  Hands the multiboot magic (eax) and info-struct pointer (ebx) to kmain().
; =============================================================================
MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x03                 ; ALIGN | MEMINFO  (no AOUT kludge)
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 65536                      ; 64 KiB kernel stack
stack_top:

section .text
global _start
extern kmain

; BSS bounds (defined by the linker script). Zeroed on entry: multiboot only
; reserves .bss, it does not guarantee its contents are clean, and uninitialised
; C globals (e.g. the watchdog flags) must read as zero.
extern __bss_start
extern __bss_end

_start:
    cld
    ; zero the BSS FIRST, before setting up the stack: the kernel stack lives
    ; inside .bss (stack_bottom..stack_top below), so any value pushed before
    ; the zeroing would be wiped. eax (multiboot magic) and ebx (mb_info*) are
    ; preserved in esi/edi, which the rep stosb loop does not touch.
    mov     esi, eax            ; save multiboot magic
    mov     edi, ebx            ; save mb_info pointer (temp; reused below)
    mov     eax, __bss_start
    mov     ecx, __bss_end
    sub     ecx, eax
    push    edi                 ; park mb_info on... no: stack not set yet.
    ; we cannot use the stack yet; hold mb_info in edi is clobbered by stosb.
    ; instead keep it in ebp (also untouched by rep stosb).
    mov     ebp, edi
    mov     edi, eax            ; destination for stosb
    xor     eax, eax
    rep     stosb
    ; now set up the stack and call kmain(magic, mb_info)
    mov     esp, stack_top
    mov     ebx, ebp            ; mb_info pointer
    mov     eax, esi            ; multiboot magic
    push    ebx
    push    eax
    call    kmain
.hang:
    cli
    hlt
    jmp     .hang
