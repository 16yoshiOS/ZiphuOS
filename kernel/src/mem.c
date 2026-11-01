/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/mem.c
 *  Tiny bump heap + memory accounting used by the MEM panel ("Used: X MB").
 *  Total RAM is taken from the multiboot info; the heap lives just above the
 *  kernel image and grows upward up to the reported upper-memory limit.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"

extern u32 end;                 /* linker symbol: end of kernel image */
static u32 heap_base;
static u32 heap_top;
static u32 heap_used;           /* bytes actually handed out */
static u32 total_ram_bytes;     /* physical RAM installed */

#define HEAP_ALIGN 16

void mem_init(u32 mem_lower_kib, u32 mem_upper_kib) {
    u32 img_end = (u32)&end;
    img_end = (img_end + 0xFFF) & ~0xFFF;        /* page align */
    heap_base = img_end + 0x100000;              /* leave 1 MiB slack */
    heap_top  = heap_base;
    heap_used = 0;
    total_ram_bytes = (mem_lower_kib + mem_upper_kib) * 1024;
    /* sanity floor so the panel always shows something sensible */
    if (total_ram_bytes == 0) total_ram_bytes = 32 * 1024 * 1024;
}

u32 mem_total_bytes(void)  { return total_ram_bytes; }
u32 mem_total_mb(void)     { return total_ram_bytes / (1024 * 1024); }

/* bump allocator: never frees. Good enough for a hobby OS desktop. */
void *kmalloc(size_t n) {
    if (n == 0) return NULL;
    n = (n + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
    u32 addr = heap_top;
    /* guard against growing past the bottom of the reported upper memory */
    u32 phys_limit = total_ram_bytes ? total_ram_bytes : (64 * 1024 * 1024);
    if (addr + n > phys_limit) {
        /* out of reported physical RAM: stop allocating */
        return NULL;
    }
    /* zero the block so callers get clean storage */
    zk_memset((void*)addr, 0, n);
    heap_top  += n;
    heap_used += n;
    return (void*)addr;
}
void *kzalloc(size_t n) { return kmalloc(n); }   /* kmalloc already zeroes */

u32 mem_heap_used(void) { return heap_used; }

/* Used bytes = heap allocations + kernel image footprint + framebuffer bytes.
 * The framebuffer term is added by the caller through mem_add_video(). */
static u32 video_bytes;
void mem_set_video_bytes(u32 b) { video_bytes = b; }

u32 mem_used_bytes(void) {
    u32 img = (u32)&end - 0x100000;       /* image footprint above 1 MiB */
    return heap_used + img + video_bytes;
}
u32 mem_used_mb(void) { return mem_used_bytes() / (1024 * 1024); }
