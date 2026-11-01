/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/kmain.c
 *  Kernel C entry. Called from boot/entry.asm with (multiboot_magic, mb_info*).
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

extern u32 end;   /* linker symbol */

static void isr_default(regs_t *r) {
    (void)r;
    /* swallow stray exceptions silently; the desktop keeps running */
}

void kmain(u32 magic, mb_info_t *mb) {
    dbg_puts("[zk] kmain entry magic="); { char b[16]; dbg_puts(zk_utoa(magic, b, 16)); dbg_puts("\n"); }
    dbg_puts("[zk] mb flags="); { char b[16]; dbg_puts(mb?zk_utoa(mb->flags,b,16):"(null)"); dbg_puts(" fb_addr="); dbg_puts(mb?zk_utoa((u32)(u64)mb->framebuffer_addr,b,16):"-"); dbg_puts(" fb_type="); dbg_puts(mb?zk_utoa(mb->framebuffer_type,b,10):"-"); dbg_puts("\n"); }
    (void)magic;
    /* 1. arch: GDT, IDT, PIC remap */
    dbg_puts("[zk] arch_init\n"); arch_init();
    dbg_puts("[zk] arch_init done\n");
    for (int i = 0; i < 32; i++) isr_register(i, isr_default);

    /* 2. memory accounting from multiboot upper/lower mem */
    u32 lo = (mb && (mb->flags & MB_INFO_MEM)) ? mb->mem_lower : 640;
    u32 hi = (mb && (mb->flags & MB_INFO_MEM)) ? mb->mem_upper : (32 * 1024);
    dbg_puts("[zk] mem_init lo="); { char b[16]; dbg_puts(zk_utoa(lo, b, 10)); dbg_puts(" hi="); dbg_puts(zk_utoa(hi, b, 10)); dbg_puts("\n"); }
    mem_init(lo, hi);

    /* 3. video: VBE/BGA 640x480x32, fallback to VGA 0x13 */
    dbg_puts("[zk] video_init\n"); int vr = video_init(mb);
    dbg_puts("[zk] video kind="); { char b[16]; dbg_puts(zk_utoa((u32)vr, b, 10)); dbg_puts(" addr="); dbg_puts(zk_utoa((u32)g_fb.addr, b, 16)); dbg_puts(" pitch="); dbg_puts(zk_utoa(g_fb.pitch, b, 10)); dbg_puts("\n"); }
    extern u32 *video_backbuf_addr(void);
    dbg_puts("[zk] backbuf="); { char b[16]; dbg_puts(zk_utoa((u32)video_backbuf_addr(), b, 16)); dbg_puts("\n"); }
    mem_set_video_bytes(g_fb.size);

    /* 4. subsystems: timer + keyboard + cpu + vm + drives + vfs */
    dbg_puts("[zk] timer\n"); timer_init(100);
    dbg_puts("[zk] kbd\n"); keyboard_init();
    dbg_puts("[zk] mouse\n"); mouse_init();
    dbg_puts("[zk] cpuid\n"); cpuid_detect();
    dbg_puts("[zk] vm\n"); (void)vm_detect();
    dbg_puts("[zk] drives\n"); ide_init(); sata_init();
    dbg_puts("[zk] vfs\n"); vfs_init(); vfs_seed_examples(); vfs_mount_drives();

    /* 5. enable interrupts so the tick + keyboard fire */
    dbg_puts("[zk] sti\n"); sti();
    dbg_puts("[zk] eflags after sti="); { char b[16]; dbg_puts(zk_utoa(read_eflags(), b, 16)); dbg_puts("\n"); }

    /* 6. boot screen (Ziphu.OS wordmark + progress bar), then desktop */
    dbg_puts("[zk] gui_init\n"); gui_init();
    dbg_puts("[zk] boot_screen\n"); gui_boot_screen();
    dbg_puts("[zk] irq_count="); { char b[16]; dbg_puts(zk_utoa(irq_total_count(), b, 10)); dbg_puts("\n"); }
    dbg_puts("[zk] desktop\n"); gui_run_desktop();

    /* never reached */
    for (;;) hlt();
}
