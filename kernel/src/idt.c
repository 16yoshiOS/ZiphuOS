/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/idt.c
 *  GDT, IDT, PIC remap, ISR/IRQ dispatch.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

static volatile u32 g_irq_count;

/* ---- GDT ----------------------------------------------------------------- */
typedef struct { u16 lo, mid; u32 hi4; } __attribute__((packed)) gdt_entry_t;
typedef struct { u16 limit; u32 base; } __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[6];
static gdt_ptr_t   gdt_ptr;

static void gdt_set(int i, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[i].lo  = (u16)(limit & 0xFFFF);
    gdt[i].mid = (u16)(base & 0xFFFF);
    gdt[i].hi4 = ((base >> 16) & 0xFF)            /* base 23:16  */
               | ((u32)access << 8)               /* access byte */
               | (((u32)(limit >> 16) & 0x0F) << 16)
               | (((u32)gran & 0xF0) << 16)        /* granularity hi nibble (G/D/L) */
               | (((u32)(base >> 24) & 0xFF) << 24); /* base 31:24 */
}

extern void load_gdt(u32);
extern void load_idt(u32);

static void gdt_install(void) {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (u32)&gdt;
    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  /* kernel code */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  /* kernel data */
    gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  /* user code */
    gdt_set(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  /* user data */
    gdt_set(5, 0, 0, 0, 0);                 /* TSS placeholder */
    load_gdt((u32)&gdt_ptr);
}

/* ---- IDT ----------------------------------------------------------------- */
typedef struct {
    u16 lo;
    u16 sel;
    u8  zero;
    u8  attr;
    u16 hi;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

#define IDT_GATE_INT 0x8E

static void idt_set(int i, u32 handler, u16 sel, u8 attr) {
    idt[i].lo   = (u16)(handler & 0xFFFF);
    idt[i].sel  = sel;
    idt[i].zero = 0;
    idt[i].attr = attr;
    idt[i].hi   = (u16)((handler >> 16) & 0xFFFF);
}

extern void isr0(void),  isr1(void),  isr2(void),  isr3(void),  isr4(void),
            isr5(void),  isr6(void),  isr7(void),  isr8(void),  isr9(void),
            isr10(void), isr11(void), isr12(void), isr13(void), isr14(void),
            isr15(void), isr16(void), isr17(void), isr18(void), isr19(void),
            isr20(void), isr21(void), isr22(void), isr23(void), isr24(void),
            isr25(void), isr26(void), isr27(void), isr28(void), isr29(void),
            isr30(void), isr31(void);
extern void irq0(void),  irq1(void),  irq2(void),  irq3(void),  irq4(void),
            irq5(void),  irq6(void),  irq7(void),  irq8(void),  irq9(void),
            irq10(void), irq11(void), irq12(void), irq13(void), irq14(void),
            irq15(void);

static void (*isr_routines[32])(regs_t *);
static void (*irq_routines[16])(regs_t *);

void isr_register(int v, void (*h)(regs_t *)) {
    if (v >= 0 && v < 32) isr_routines[v] = h;
}
void irq_register(int v, void (*h)(regs_t *)) {
    if (v >= 0 && v < 16) irq_routines[v] = h;
}

void isr_handler(regs_t *r) {
    void (*h)(regs_t *) = (r->int_no < 32) ? isr_routines[r->int_no] : NULL;
    if (h) h(r);
}
void irq_handler(regs_t *r) {
    int irq = r->int_no - 32;
    g_irq_count++;
    void (*h)(regs_t *) = (irq >= 0 && irq < 16) ? irq_routines[irq] : NULL;
    if (h) h(r);
    if (irq >= 8) outb(0xA0, 0x20);   /* slave EOI */
    outb(0x20, 0x20);                 /* master EOI */
}

u32 irq_total_count(void) { return g_irq_count; }

/* ---- PIC ----------------------------------------------------------------- */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define ICW1_ICW4 0x11
#define ICW4_8086 0x01

static void pic_remap(void) {
    outb(PIC1_CMD, ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_ICW4); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();   /* master offset 32 */
    outb(PIC2_DATA, 0x28); io_wait();   /* slave offset 40  */
    outb(PIC1_DATA, 0x04); io_wait();   /* cascade */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();
    /* mask everything except IRQ0 (timer) and IRQ1 (keyboard) for now */
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

static void idt_install(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (u32)&idt;
    for (int i = 0; i < 256; i++) { idt[i].lo = 0; idt[i].hi = 0; idt[i].sel = 0; idt[i].zero = 0; idt[i].attr = 0; }
    for (int i = 0; i < 32; i++) isr_routines[i] = NULL;
    for (int i = 0; i < 16; i++) irq_routines[i] = NULL;

    idt_set(0,  (u32)isr0,  0x08, IDT_GATE_INT);  idt_set(1,  (u32)isr1,  0x08, IDT_GATE_INT);
    idt_set(2,  (u32)isr2,  0x08, IDT_GATE_INT);  idt_set(3,  (u32)isr3,  0x08, IDT_GATE_INT);
    idt_set(4,  (u32)isr4,  0x08, IDT_GATE_INT);  idt_set(5,  (u32)isr5,  0x08, IDT_GATE_INT);
    idt_set(6,  (u32)isr6,  0x08, IDT_GATE_INT);  idt_set(7,  (u32)isr7,  0x08, IDT_GATE_INT);
    idt_set(8,  (u32)isr8,  0x08, IDT_GATE_INT);  idt_set(9,  (u32)isr9,  0x08, IDT_GATE_INT);
    idt_set(10, (u32)isr10, 0x08, IDT_GATE_INT);  idt_set(11, (u32)isr11, 0x08, IDT_GATE_INT);
    idt_set(12, (u32)isr12, 0x08, IDT_GATE_INT);  idt_set(13, (u32)isr13, 0x08, IDT_GATE_INT);
    idt_set(14, (u32)isr14, 0x08, IDT_GATE_INT);  idt_set(15, (u32)isr15, 0x08, IDT_GATE_INT);
    idt_set(16, (u32)isr16, 0x08, IDT_GATE_INT);  idt_set(17, (u32)isr17, 0x08, IDT_GATE_INT);
    idt_set(18, (u32)isr18, 0x08, IDT_GATE_INT);  idt_set(19, (u32)isr19, 0x08, IDT_GATE_INT);
    idt_set(20, (u32)isr20, 0x08, IDT_GATE_INT);  idt_set(21, (u32)isr21, 0x08, IDT_GATE_INT);
    idt_set(22, (u32)isr22, 0x08, IDT_GATE_INT);  idt_set(23, (u32)isr23, 0x08, IDT_GATE_INT);
    idt_set(24, (u32)isr24, 0x08, IDT_GATE_INT);  idt_set(25, (u32)isr25, 0x08, IDT_GATE_INT);
    idt_set(26, (u32)isr26, 0x08, IDT_GATE_INT);  idt_set(27, (u32)isr27, 0x08, IDT_GATE_INT);
    idt_set(28, (u32)isr28, 0x08, IDT_GATE_INT);  idt_set(29, (u32)isr29, 0x08, IDT_GATE_INT);
    idt_set(30, (u32)isr30, 0x08, IDT_GATE_INT);  idt_set(31, (u32)isr31, 0x08, IDT_GATE_INT);

    idt_set(32, (u32)irq0,  0x08, IDT_GATE_INT);  idt_set(33, (u32)irq1,  0x08, IDT_GATE_INT);
    idt_set(34, (u32)irq2,  0x08, IDT_GATE_INT);  idt_set(35, (u32)irq3,  0x08, IDT_GATE_INT);
    idt_set(36, (u32)irq4,  0x08, IDT_GATE_INT);  idt_set(37, (u32)irq5,  0x08, IDT_GATE_INT);
    idt_set(38, (u32)irq6,  0x08, IDT_GATE_INT);  idt_set(39, (u32)irq7,  0x08, IDT_GATE_INT);
    idt_set(40, (u32)irq8,  0x08, IDT_GATE_INT);  idt_set(41, (u32)irq9,  0x08, IDT_GATE_INT);
    idt_set(42, (u32)irq10, 0x08, IDT_GATE_INT);  idt_set(43, (u32)irq11, 0x08, IDT_GATE_INT);
    idt_set(44, (u32)irq12, 0x08, IDT_GATE_INT);  idt_set(45, (u32)irq13, 0x08, IDT_GATE_INT);
    idt_set(46, (u32)irq14, 0x08, IDT_GATE_INT);  idt_set(47, (u32)irq15, 0x08, IDT_GATE_INT);

    load_idt((u32)&idt_ptr);
}

void irq_mask(int irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  v    = inb(port) | (1 << (irq & 7));
    outb(port, v);
}
void irq_unmask(int irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  v    = inb(port) & ~(1 << (irq & 7));
    outb(port, v);
}

void arch_init(void) {
    gdt_install();
    pic_remap();
    idt_install();
}
