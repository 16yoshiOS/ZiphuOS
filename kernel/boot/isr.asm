; =============================================================================
;  Ziphu.OS 1.0  -  OpenZK  -  boot/isr.asm
;  IDT entry stubs + common IRQ/exception dispatch.
; =============================================================================
[bits 32]

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0          ; fake error code for uniform frame
    push dword %1
    jmp   isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp   isr_common
%endmacro

%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp   irq_common
%endmacro

; exceptions 0..31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

; IRQ0..IRQ15  ->  vector 32..47
IRQ_STUB 0,  32
IRQ_STUB 1,  33
IRQ_STUB 2,  34
IRQ_STUB 3,  35
IRQ_STUB 4,  36
IRQ_STUB 5,  37
IRQ_STUB 6,  38
IRQ_STUB 7,  39
IRQ_STUB 8,  40
IRQ_STUB 9,  41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

extern isr_handler
extern irq_handler

[EXTERN kpanic_dump]
isr_common:
    pusha
    mov   ax, ds
    push  eax
    mov   ax, 0x10
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    lea   eax, [esp+4]            ; skip the ds we just pushed -> points at regs
    push  eax
    call  isr_handler
    add   esp, 4
    pop   eax
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    popa
    add   esp, 8               ; drop vector + errcode
    iret

irq_common:
    pusha
    mov   ax, ds
    push  eax
    mov   ax, 0x10
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    lea   eax, [esp+4]            ; skip the ds we just pushed -> points at regs
    push  eax
    call  irq_handler
    add   esp, 4
    pop   eax
    mov   ds, ax
    mov   es, ax
    mov   fs, ax
    mov   gs, ax
    popa
    add   esp, 8
    iret

global load_idt
global load_gdt

load_idt:
    mov  eax, [esp + 4]
    lidt [eax]
    ret

load_gdt:
    mov  eax, [esp + 4]
    lgdt [eax]
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    jmp  0x08:.flush
.flush:
    ret

global irq_enable
irq_enable:
    mov  al, [esp + 4]
    test al, al
    jz   .off
    sti
    ret
.off:
    cli
    ret

; out port, value  (C-callable: void out_port(u16 port, u8 val)) -- not used, kept tiny
