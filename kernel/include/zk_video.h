/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  include/zk_video.h
 *  Unified framebuffer interface: 32-bpp linear (BGA/VBE) primary, 8-bpp VGA
 *  palette-mode fallback for legacy cards (S3 etc.). The GUI talks only to
 *  the abstraction here; it never touches hardware registers directly.
 * ========================================================================== */
#ifndef ZK_VIDEO_H
#define ZK_VIDEO_H
#include "zk_types.h"

typedef enum { FB_NONE = 0, FB_LFB32, FB_VGA8 } fb_kind_t;

typedef struct {
    fb_kind_t kind;
    u32  width;
    u32  height;
    u32  bpp;                 /* bits per pixel as reported to the GUI */
    u32  pitch;               /* bytes per scanline */
    u8  *addr;                /* framebuffer base */
    u32  size;                /* framebuffer size in bytes */
    /* VGA-only palette tables (256 entries), unused in LFB32 mode */
    u8   pal_r[256];
    u8   pal_g[256];
    u8   pal_b[256];
    u8   inv[256];            /* vga palette index -> index of inverted color */
} fb_t;

extern fb_t g_fb;

int  video_init(mb_info_t *mb);   /* returns 1 on LFB32, 2 on VGA8, 0 fail */

/* pixel primitives (auto-route to the active backend) */
void fb_putpixel(int x, int y, u32 argb);
void fb_fillrect(int x, int y, int w, int h, u32 argb);
void fb_hline(int x, int y, int w, u32 argb);
void fb_vline(int x, int y, int h, u32 argb);
void fb_rect(int x, int y, int w, int h, u32 argb);
void fb_copyrect(int dx, int dy, int w, int h, const u32 *src);
void fb_clear(u32 argb);
void fb_present(void);            /* no-op for LFB; vsync/palette sync for VGA */

/* 8x16 bitmap font (code page 437 subset, monochrome 1-bpp rows) */
extern const u8 zk_font8x16[256][16];

/* 16x16 CJK bitmap font (for Simplified-Chinese UI strings). Each entry is
 * {unicode codepoint, 32-byte glyph}. Lookup is linear (small table). */
typedef struct { u32 cp; u8 g[32]; } cjk_entry_t;
extern const cjk_entry_t zk_font_cjk[];
extern const u32 zk_font_cjk_count;
const u8 *cjk_glyph(u32 cp);   /* returns 32-byte glyph or NULL if not found */

#endif
