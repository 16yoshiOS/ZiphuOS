/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/rsod.c + reboot.c
 *  Red Screen of Death (used by the Crash System watchdog) + reboot helpers.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

void reboot(void) {
    /* keyboard controller reset line */
    cli();
    while (inb(0x64) & 0x02) { }
    outb(0x64, 0xFE);
    /* if that didn't work, triple-fault */
    triple_fault_reboot();
}

void triple_fault_reboot(void) {
    /* load a broken IDT and trigger int3 -> guaranteed triple fault -> reset */
    static u8 bogus[6] = {0,0,0,0,0,0};
    __asm__ volatile ("lidt %0" : : "m"(bogus));
    __asm__ volatile ("int $0x03");
    for (;;) hlt();
}

/* ---- RSOD ---------------------------------------------------------------- */
/* Direct framebuffer painter; we can't assume the GUI's theme is intact. */
void rsod_show(const char *code, const char *message) {
    if (g_fb.kind == FB_LFB32) {
        u32 bg = 0xFFC83030;     /* opaque red */
        fb_clear(bg);
        /* heading bar */
        fb_fillrect(0, 0, g_fb.width, 48, 0xFF8B0000);
        const char *title = "Ziphu.OS - System Halted";
        int x = 24, y = 16;
        for (const char *p = title; *p; p++) {
            const u8 *g = zk_font8x16[(u8)*p];
            for (int r = 0; r < 16; r++) {
                u8 row = g[r];
                for (int c = 0; c < 8; c++) {
                    if (row & (0x80 >> c)) {
                        u32 *px = (u32*)(g_fb.addr + (y + r) * g_fb.pitch);
                        px[x + c] = 0xFFFFFFFF;
                    }
                }
            }
            x += 8;
        }
        /* code + message block */
        char line1[80]; zk_sprintf(line1, "Error code: %s", code);
        char line2[160]; zk_sprintf(line2, "%s", message);
        const char *line3 = "Rebooting in 5 seconds...";
        char *lines[] = { line1, line2, line3 };
        int yy = 80;
        for (int li = 0; li < 3; li++) {
            int xx = 24;
            for (const char *p = lines[li]; *p; p++) {
                const u8 *g = zk_font8x16[(u8)*p];
                for (int r = 0; r < 16; r++) {
                    u8 row = g[r];
                    for (int c = 0; c < 8; c++)
                        if (row & (0x80 >> c)) {
                            u32 *px = (u32*)(g_fb.addr + (yy + r) * g_fb.pitch);
                            px[xx + c] = 0xFFFFFFFF;
                        }
                }
                xx += 8;
            }
            yy += 24;
        }
    } else {
        /* VGA fallback: paint red background via fillrect */
        fb_clear(0xFFC83030);
    }
    fb_present();
}

void rsod_panic(const char *code, const char *message) {
    cli();
    rsod_show(code, message);
    for (;;) hlt();
}
