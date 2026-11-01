/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/video.c
 *  Video backend: VBE/BGA linear framebuffer (640x480x32) primary,
 *  standard VGA 640x480x8 (mode 0x11 / 0x12-style planar-avoiding 256-colour
 *  banked) fallback so the GUI still paints on old S3 cards. Provides the
 *  unified fb_t interface consumed by the GUI.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_video.h"

fb_t g_fb;

/* Off-screen back buffer for the LFB32 backend (double buffering). All
 * drawing primitives write here; fb_present() blits it to the visible LFB in
 * one pass so the user never sees a half-drawn frame (no flicker). NULL in
 * VGA mode, where drawing still targets the card directly. */
static u32 *g_backbuf;
static u32  g_backbuf_size;
u32 *video_backbuf_addr(void) { return g_backbuf; }

/* ---- BGA (Bochs/QEMU) PCI IO ports ---------------------------------------- */
#define BGA_DISPI_INDEX  0x01CE
#define BGA_DISPI_DATA   0x01CF
#define BGA_INDEX_ID        0
#define BGA_INDEX_XRES      1
#define BGA_INDEX_YRES      2
#define BGA_INDEX_BPP       3
#define BGA_INDEX_ENABLE    4
#define BGA_DISPI_ENABLED   0x01
#define BGA_DISPI_DISABLE   0x00
#define BGA_DISPI_LFB       0x40   /* use linear framebuffer */
#define BGA_DISPI_NOCLEAR   0x80

/* PCI config space access (for reading the BGA BAR0 LFB aperture) */
#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC
static u32 pci_cfg_read(u8 bus, u8 dev, u8 fn, u8 off) {
    outl(PCI_ADDR, 0x80000000u | ((u32)bus << 16) | ((u32)dev << 11) | ((u32)fn << 8) | (off & 0xFC));
    return inl(PCI_DATA);
}

static u16 bga_read(u16 idx) {
    outw(BGA_DISPI_INDEX, idx);
    return inw(BGA_DISPI_DATA);
}
static void bga_write(u16 idx, u16 val) {
    outw(BGA_DISPI_INDEX, idx);
    outw(BGA_DISPI_DATA, val);
}
static int bga_present(void) {
    /* BGA id is always 0xB0C0 in Bochs/QEMU/VirtualBox */
    return (bga_read(BGA_INDEX_ID) & 0xFFF0) == 0xB0C0;
}
static void bga_set_mode(u16 w, u16 h, u16 bpp) {
    bga_write(BGA_INDEX_ENABLE, BGA_DISPI_DISABLE);
    bga_write(BGA_INDEX_XRES, w);
    bga_write(BGA_INDEX_YRES, h);
    bga_write(BGA_INDEX_BPP,  bpp);
    bga_write(BGA_INDEX_ENABLE, BGA_DISPI_ENABLED | BGA_DISPI_LFB | BGA_DISPI_NOCLEAR);
}

/* Find the BGA/Bochs VGA PCI device (vendor 1234 device 1111) and return its
 * BAR0 LFB physical address. Returns 0 if not found. */
static u32 bga_find_lfb(void) {
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            u32 id = pci_cfg_read((u8)bus, dev, 0, 0);
            if (id == 0xFFFFFFFF) continue;
            u16 vendor = id & 0xFFFF;
            u16 device = (id >> 16) & 0xFFFF;
            if (vendor == 0x1234 && (device == 0x1111 || device == 0x1100)) {
                u32 bar0 = pci_cfg_read((u8)bus, dev, 0, 0x10);
                return bar0 & 0xFFFFFFF0;   /* mask flags */
            }
        }
    }
    return 0;
}

/* ---- VGA: VGA attribute registers / DAC palette -------------------------- */
#define VGA_AC_INDEX   0x3C0
#define VGA_AC_WRITE   0x3C0
#define VGA_AC_READ    0x3C1
#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX  0x3C4
#define VGA_SEQ_DATA   0x3C5
#define VGA_DAC_READ   0x3C7
#define VGA_DAC_WRITE  0x3C8
#define VGA_DAC_DATA   0x3C9
#define VGA_GC_INDEX   0x3CE
#define VGA_GC_DATA    0x3CF
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA  0x3D5

/* 0x13 = 320x200x8 (256 colour, packed-pixel). The GUI scales its logical
 * 640x480 down to 320x200 so it still paints on a real VGA card. Mode 0x12
 * (640x480x16 planar) is harder to drive from C; 0x13 packed-pixel is the
 * pragmatic fallback that works on every VGA/S3 clone. */
static void vga_set_mode_13h(void) {
    /* set 320x200x256 by writing the standard register sequence via int 0x10
     * is not available in protected mode; we program the registers directly. */
    static const u16 seq[] = {0x0100,0x0101,0x0F02,0x0003,0x0604,0x0505,0x0F06,0x0007,0xFF08,0xFFFF};
    static const u16 crtc[] = {0x5F00,0x4F01,0x5002,0x8203,0x5404,0x8005,0xBF06,0x1F07,
                               0x0008,0x4109,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
                               0x9C10,0x8E11,0x8F12,0x2813,0x4014,0x9615,0xB916,0xA317,
                               0xFF18,0xFFFF};
    static const u16 gc[] = {0x0000,0x0001,0x0002,0x0003,0x0004,0x0005,0x0D06,0x0F07,0xFF08,0xFFFF};
    static const u8 ac[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                            0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
                            0x41,0x00,0x0F,0x00,0x00};

    /* misc: 0x63 -> 288-322 horizontal pixel clock select + color */
    outb(VGA_MISC_WRITE, 0x63);
    /* synchronous reset */
    outw(VGA_SEQ_INDEX, 0x0100); outw(VGA_SEQ_INDEX, 0x0101);
    for (int i = 0; seq[i] != 0xFFFF; i++) outw(VGA_SEQ_INDEX, seq[i]);
    outw(VGA_CRTC_INDEX, 0x0E11);   /* unprotect crtc */
    for (int i = 0; crtc[i] != 0xFFFF; i++) outw(VGA_CRTC_INDEX, crtc[i]);
    for (int i = 0; gc[i] != 0xFFFF; i++) outw(VGA_GC_INDEX, gc[i]);
    for (int i = 0; i < 20; i++) {
        inb(VGA_AC_READ);            /* reset flip-flop */
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, ac[i]);
    }
    inb(VGA_AC_READ);
    outb(VGA_AC_INDEX, 0x20);        /* enable video */
}

/* Build a 256-entry monochrome + neon palette: greys 0..15 plus a turquoise
 * accent slot. The GUI uses palette indices, not RGB, in VGA mode. */
#define PAL_BLACK   0
#define PAL_DGREY   1
#define PAL_MGREY   2
#define PAL_LGREY   3
#define PAL_WHITE   4
#define PAL_TURQ    5
#define PAL_RED     6
#define PAL_GREEN   7
#define PAL_BLUE    8
#define PAL_YELLOW  9

static void vga_palette_install(void) {
    /* fill 16 system colours and greys for indices 16..255 */
    outb(VGA_DAC_WRITE, 0);
    for (int i = 0; i < 256; i++) {
        u8 r, g, b;
        if (i == PAL_BLACK)  { r=0;   g=0;   b=0; }
        else if (i == PAL_DGREY) { r=30; g=30; b=30; }
        else if (i == PAL_MGREY) { r=64; g=64; b=64; }
        else if (i == PAL_LGREY) { r=128; g=128; b=128; }
        else if (i == PAL_WHITE) { r=235; g=235; b=235; }
        else if (i == PAL_TURQ)  { r=0;   g=229; b=229; }   /* neon turquoise */
        else if (i == PAL_RED)   { r=200; g=30;  b=30; }
        else if (i == PAL_GREEN) { r=40;  g=200; b=60; }
        else if (i == PAL_BLUE)  { r=40;  g=80;  b=200; }
        else if (i == PAL_YELLOW){ r=220; g=200; b=40; }
        else { /* grey ramp 16..255 */ u8 v = (u8)(16 + (i - 16) * 230 / 240); r=g=b=v; }
        /* VGA DAC is 6-bit per channel */
        outb(VGA_DAC_DATA, r >> 2);
        outb(VGA_DAC_DATA, g >> 2);
        outb(VGA_DAC_DATA, b >> 2);
        g_fb.pal_r[i] = r; g_fb.pal_g[i] = g; g_fb.pal_b[i] = b;
        g_fb.inv[i] = (u8)(255 - i);   /* crude inversion index for theme flip */
    }
}

/* map a logical 640x480 ARGB pixel onto the 320x200 VGA plane */
static u8 vga_argb_to_index(u32 argb) {
    u8 r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
    /* exact-match system colours first */
    if (r == 0   && g == 229 && b == 229) return PAL_TURQ;
    if (r >= 200 && g <= 50  && b <= 50)  return PAL_RED;
    if (r <= 50  && g >= 180 && b <= 80)  return PAL_GREEN;
    if (r <= 80  && g <= 120 && b >= 180) return PAL_BLUE;
    if (r >= 200 && g >= 180 && b <= 80)  return PAL_YELLOW;
    /* luminance -> grey index */
    u32 lum = (r * 299 + g * 587 + b * 114) / 1000;
    if (lum <  12) return PAL_BLACK;
    if (lum <  60) return PAL_DGREY;
    if (lum < 110) return PAL_MGREY;
    if (lum < 190) return PAL_LGREY;
    return PAL_WHITE;
}

/* ---- public init --------------------------------------------------------- */
int video_init(mb_info_t *mb) {
    zk_memset(&g_fb, 0, sizeof(g_fb));
    g_fb.kind = FB_NONE;

    /* Prefer the multiboot-provided linear framebuffer (GRUB set up VBE).
     * If absent, try the BGA PCI card, then fall back to standard VGA 0x13. */
    int use_lfb = 0;
    u32 lfb_addr = 0;
    u32 w = 640, h = 480, bpp = 32;

    if (mb && (mb->flags & MB_INFO_FRAMEBUFFER) && mb->framebuffer_type == 1) {
        lfb_addr = (u32)(u64)mb->framebuffer_addr;
        w  = mb->framebuffer_width  ? mb->framebuffer_width  : 640;
        h  = mb->framebuffer_height ? mb->framebuffer_height : 480;
        bpp = mb->framebuffer_bpp   ? mb->framebuffer_bpp    : 32;
        if (bpp == 32) use_lfb = 1;
    }
    if (!use_lfb && bga_present()) {
        u32 lfb = bga_find_lfb();
        if (!lfb) lfb = 0xE0000000;   /* legacy fallback aperture */
        bga_set_mode(640, 480, 32);
        lfb_addr = lfb;
        w = 640; h = 480; bpp = 32;
        use_lfb = 1;
    }

    if (use_lfb) {
        g_fb.kind  = FB_LFB32;
        g_fb.width = w; g_fb.height = h; g_fb.bpp = 32;
        g_fb.pitch = w * 4;
        g_fb.addr  = (u8*)lfb_addr;
        g_fb.size  = g_fb.pitch * h;
        /* Allocate the back buffer from the kernel heap so every frame is
         * composed off-screen and only revealed by fb_present(). This removes
         * the flicker that a direct-to-LFB clear+redraw cycle produces. */
        g_backbuf_size = (u32)w * h;
        extern void *kmalloc(size_t);
        g_backbuf = (u32*)kmalloc((size_t)g_backbuf_size * 4);
        return 1;
    }

    /* ---- VGA fallback ---- */
    vga_set_mode_13h();
    vga_palette_install();
    g_fb.kind  = FB_VGA8;
    /* The GUI thinks in logical 640x480; we downscale 2x in put/fill. */
    g_fb.width = 640; g_fb.height = 480; g_fb.bpp = 8;
    g_fb.pitch = 320;                 /* physical scanline width in mode 13h */
    g_fb.addr  = (u8*)0xA0000;
    g_fb.size  = 320 * 200;
    return 2;
}

/* ---- primitives ---------------------------------------------------------- */
/* In LFB32 mode every primitive targets the off-screen back buffer (no direct
 * LFB writes); fb_present() is the only path that touches the visible LFB, so
 * a frame is never seen half-drawn. VGA mode still writes through directly. */
void fb_putpixel(int x, int y, u32 argb) {
    if (x < 0 || y < 0 || x >= (int)g_fb.width || y >= (int)g_fb.height) return;
    if (g_fb.kind == FB_LFB32) {
        g_backbuf[y * g_fb.width + x] = argb;
    } else if (g_fb.kind == FB_VGA8) {
        int vx = x / 2, vy = y / 2;
        if (vx >= 320 || vy >= 200) return;
        u8 idx = vga_argb_to_index(argb);
        u8 *p = (u8*)(g_fb.addr + vy * g_fb.pitch);
        p[vx] = idx;
    }
}
void fb_fillrect(int x, int y, int w, int h, u32 argb) {
    if (w <= 0 || h <= 0) return;
    int x2 = x + w, y2 = y + h;
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x2 > (int)g_fb.width)  x2 = g_fb.width;
    if (y2 > (int)g_fb.height) y2 = g_fb.height;
    if (g_fb.kind == FB_LFB32) {
        int rw = x2 - x;
        u32 *row = g_backbuf + y * g_fb.width + x;
        for (int j = y; j < y2; j++) {
            for (int i = 0; i < rw; i++) row[i] = argb;
            row += g_fb.width;
        }
    } else if (g_fb.kind == FB_VGA8) {
        u8 idx = vga_argb_to_index(argb);
        for (int j = y; j < y2; j += 2) {
            int vy = j / 2; if (vy >= 200) break;
            u8 *p = (u8*)(g_fb.addr + vy * g_fb.pitch);
            for (int i = x; i < x2; i += 2) { int vx = i / 2; if (vx >= 320) break; p[vx] = idx; }
        }
    }
}
void fb_hline(int x, int y, int w, u32 argb) {
    if (w <= 0 || y < 0 || y >= (int)g_fb.height) return;
    int x2 = x + w; if (x < 0) x = 0; if (x2 > (int)g_fb.width) x2 = g_fb.width;
    for (int i = x; i < x2; i++) fb_putpixel(i, y, argb);
}
void fb_vline(int x, int y, int h, u32 argb) {
    if (h <= 0 || x < 0 || x >= (int)g_fb.width) return;
    int y2 = y + h; if (y < 0) y = 0; if (y2 > (int)g_fb.height) y2 = g_fb.height;
    for (int j = y; j < y2; j++) fb_putpixel(x, j, argb);
}
void fb_rect(int x, int y, int w, int h, u32 argb) {
    fb_hline(x, y, w, argb);
    fb_hline(x, y + h - 1, w, argb);
    fb_vline(x, y, h, argb);
    fb_vline(x + w - 1, y, h, argb);
}
void fb_copyrect(int dx, int dy, int w, int h, const u32 *src) {
    if (g_fb.kind != FB_LFB32) return;
    for (int j = 0; j < h; j++) {
        u32 *d = g_backbuf + (dy + j) * g_fb.width + dx;
        for (int i = 0; i < w; i++) d[i] = src[j * w + i];
    }
}
void fb_clear(u32 argb) { fb_fillrect(0, 0, g_fb.width, g_fb.height, argb); }
void fb_present(void) {
    /* Reveal the composed back buffer in one pass: this is the only routine
     * that writes the visible LFB, so the screen never flickers during a
     * redraw. VGA mode draws through directly and needs no flip. */
    if (g_fb.kind == FB_LFB32 && g_backbuf) {
        u32 *src = g_backbuf;
        u8  *dst = g_fb.addr;
        for (u32 y = 0; y < g_fb.height; y++) {
            u32 *drow = (u32*)(dst + y * g_fb.pitch);
            for (u32 x = 0; x < g_fb.width; x++) drow[x] = src[x];
            src += g_fb.width;
        }
    }
}
