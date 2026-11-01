/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK (Open-ZiphuKernel)
 *  include/zk_port.h  -  port I/O + cpu helpers (inline asm)
 * ========================================================================== */
#ifndef ZK_PORT_H
#define ZK_PORT_H

#include "zk_types.h"

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline u8 inb(u16 port) {
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(u16 port, u16 val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline u16 inw(u16 port) {
    u16 v;
    __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline u32 inl(u16 port) {
    u32 v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void io_wait(void) { outb(0x80, 0); }

static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void sti(void) { __asm__ volatile ("sti"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }
static inline void cpuid(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d) {
    __asm__ volatile ("cpuid"
                      : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                      : "a"(leaf));
}
static inline u64 rdtsc(void) {
    u32 hi, lo;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}
static inline void invlpg(void *addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}
static inline u32 read_eflags(void) {
    u32 f; __asm__ volatile ("pushfl; pop %0" : "=r"(f)); return f;
}
static inline void wrmsr(u32 msr, u64 val) {
    __asm__ volatile ("wrmsr" : : "a"((u32)val), "d"((u32)(val >> 32)), "c"(msr));
}

/* multiboot1 layout (the part of the info struct we consume) */
#define MB_INFO_MEM        0x00000001
#define MB_INFO_BOOT_DEV   0x00000002
#define MB_INFO_CMDLINE    0x00000004
#define MB_INFO_MODS       0x00000008
#define MB_INFO_VBE        0x00000020
#define MB_INFO_FRAMEBUFFER 0x00001000

typedef struct {
    u32 flags;
    u32 mem_lower;          /* KiB */
    u32 mem_upper;          /* KiB */
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8  framebuffer_bpp;
    u8  framebuffer_type;
    u8  color_info[6];
} mb_info_t;

#endif /* ZK_PORT_H */
