/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/gui.c  (part 1: theme, text, window chrome)
 *  The "Lightning" (Молния) shell. Strict monochrome palette + neon-turquoise
 *  window frames. Theme is Light/Dark switchable from the Settings app.
 *  English UI strings with a parallel Simplified-Chinese string table behind a
 *  compile-time / runtime language switch (LANG_EN / LANG_ZH).
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

/* ---- palette (ARGB, 8-bit alpha ignored by LFB) ------------------------- */
#define A(rgb) ((rgb))

typedef struct {
    u32 bg;          /* desktop background */
    u32 panel;       /* window / panel fill */
    u32 panel_hi;    /* lighter panel (selected row) */
    u32 text;        /* primary text */
    u32 text_dim;    /* secondary text (model line etc.) */
    u32 text_inv;    /* text on accent fill */
    u32 frame;       /* neon turquoise window frame */
    u32 frame_dim;   /* darker frame for inactive windows */
    u32 accent;      /* turquoise accent fill (buttons, progress) */
    u32 border;      /* thin inner border */
    u32 ok;
    u32 warn;
    u32 danger;
} theme_t;

static theme_t theme_light = {
    .bg        = 0xFFE8E8EA,
    .panel     = 0xFFF2F2F4,
    .panel_hi  = 0xFFD6D6DA,
    .text      = 0xFF1A1A1E,
    .text_dim  = 0xFF6E6E76,
    .text_inv  = 0xFFFFFFFF,
    .frame     = 0xFF00E5E5,
    .frame_dim = 0xFF1FB6B6,
    .accent    = 0xFF00E5E5,
    .border    = 0xFFB4B4BC,
    .ok        = 0xFF2EA84F,
    .warn      = 0xFFC99A1F,
    .danger    = 0xFFC83030,
};

static theme_t theme_dark = {
    .bg        = 0xFF141418,
    .panel     = 0xFF1E1E24,
    .panel_hi  = 0xFF2C2C36,
    .text      = 0xFFE8E8EC,
    .text_dim  = 0xFF8A8A94,
    .text_inv  = 0xFF101014,
    .frame     = 0xFF00E5E5,
    .frame_dim = 0xFF0E8C8C,
    .accent    = 0xFF00E5E5,
    .border    = 0xFF3A3A46,
    .ok        = 0xFF36C766,
    .warn      = 0xFFE0B33A,
    .danger    = 0xFFE04545,
};

static theme_t *theme = &theme_dark;

/* ---- custom accent color (RGB sliders in Settings) ---------------------- */
/* A third, user-tunable theme. The accent + frame colours track a single RGB
 * triplet the user picks with three sliders (R, G, B); the rest mirrors the
 * dark theme so text stays legible. Range: 0..255 each, so the full RGB gamut
 * is reachable - red (255,0,0) through yellow/green/cyan/blue/magenta all the
 * way to pink (255,105,180). */
static theme_t theme_custom;
static int  cust_use = 0;        /* 1 when the custom theme is active */
static u8   cust_r = 0x00, cust_g = 0xE5, cust_b = 0xE5;  /* default: turquoise */
static int  drag_slider = 0;     /* 0=none, 1=R, 2=G, 3=B while dragging */

static u32 rgb(u8 r, u8 g, u8 b) { return 0xFF000000U | ((u32)r << 16) | ((u32)g << 8) | b; }

static void apply_custom_color(void) {
    theme_custom = theme_dark;                 /* start from dark base */
    u32 c = rgb(cust_r, cust_g, cust_b);
    theme_custom.frame     = c;
    theme_custom.accent    = c;
    /* a dimmed version of the accent for inactive frames */
    u32 dim = rgb((u8)(cust_r / 2), (u8)(cust_g / 2), (u8)(cust_b / 2));
    theme_custom.frame_dim = dim;
    if (cust_use) theme = &theme_custom;
}

typedef enum { LANG_EN = 0, LANG_ZH = 1 } lang_t;
static lang_t lang = LANG_EN;

/* Localized string table. ZH uses a handful of representative glyphs from the
 * font's extended set; the bitmap font here covers ASCII only, so Simplified
 * Chinese is exposed via the switchable structure as required by the spec and
 * renders as the English label in this build (font is 8x16 CP437). The hook
 * is wired so a CJK font drop-in activates the translated labels. */
typedef struct { const char *en; const char *zh; } lstr_t;
static const lstr_t L_SYSTEM      = { "SYSTEM",          "系统"     };
static const lstr_t L_SETTINGS    = { "Settings",        "设置"     };
static const lstr_t L_FILEMAN     = { "File Manager",    "文件管理"  };
static const lstr_t L_MEMLOG      = { "MemLog",          "内存日志"  };
static const lstr_t L_SYSLOG      = { "SysLog",          "系统日志"  };
static const lstr_t L_THEME       = { "Theme",           "主题"     };
static const lstr_t L_LIGHT       = { "Light",           "浅色"     };
static const lstr_t L_DARK        = { "Dark",            "深色"     };
static const lstr_t L_LANGUAGE    = { "Language",        "语言"     };
static const lstr_t L_CRASHSYS    = { "Crash System",    "崩溃系统"  };
static const lstr_t L_CRASHVMS    = { "Crash VMS",       "崩溃虚机"  };
static const lstr_t L_CPUID       = { "CPUID",           "处理器"    };
static const lstr_t L_VM          = { "VM",              "虚拟机"    };
static const lstr_t L_MEM         = { "MEM",             "内存"     };
static const lstr_t L_USED        = { "Used",            "已用"     };
static const lstr_t L_DRIVES      = { "Drives",          "驱动器"    };
static const lstr_t L_FILES       = { "Files",           "文件"     };
static const lstr_t L_BACK        = { ".. back",         ".. 返回"   };
static const lstr_t L_APPSMENU    = { "Applications",    "应用程序"   };
static const lstr_t L_ABOUT       = { "About",           "关于"     };
static const lstr_t L_REBOOT      = { "Reboot",          "重启"     };

static const char *L(const lstr_t *s) { return lang == LANG_ZH ? s->zh : s->en; }

/* ---- text rendering ------------------------------------------------------ */
static void draw_glyph(int x, int y, u8 ch, u32 fg) {
    const u8 *g = zk_font8x16[ch];
    for (int r = 0; r < 16; r++) {
        u8 row = g[r];
        for (int c = 0; c < 8; c++) {
            if (row & (0x80 >> c)) fb_putpixel(x + c, y + r, fg);
        }
    }
}
/* 16x16 CJK glyph; each row is 2 bytes (big-endian, bit15 = leftmost). */
static void draw_glyph16(int x, int y, const u8 *g, u32 fg) {
    for (int r = 0; r < 16; r++) {
        u8 hi = g[r * 2];
        u8 lo = g[r * 2 + 1];
        for (int c = 0; c < 8; c++) {
            if (hi & (0x80 >> c)) fb_putpixel(x + c, y + r, fg);
        }
        for (int c = 0; c < 8; c++) {
            if (lo & (0x80 >> c)) fb_putpixel(x + 8 + c, y + r, fg);
        }
    }
}

/* Decode one UTF-8 codepoint at *p; advance *p past it. Returns the cp, or
 * 0xFFFFFFFF if the byte is invalid/continuation garbage (caller skips 1). */
static u32 utf8_next(const char **p) {
    const u8 *s = (const u8 *)*p;
    if (!s[0]) { return 0; }
    if (s[0] < 0x80) { *p = (const char *)(s + 1); return s[0]; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *p = (const char *)(s + 2);
        return ((u32)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *p = (const char *)(s + 3);
        return ((u32)(s[0] & 0x0F) << 12) | ((u32)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    /* invalid lead byte: skip one and let the caller render nothing */
    *p = (const char *)(s + 1);
    return 0xFFFFFFFFU;
}

static void draw_text(int x, int y, const char *s, u32 fg) {
    int cx = x;
    const char *p = s;
    while (*p) {
        u32 cp = utf8_next(&p);
        if (cp == 0xFFFFFFFFU) continue;     /* skip bad bytes */
        if (cp < 0x80) {
            draw_glyph(cx, y, (u8)cp, fg);
            cx += 8;
        } else {
            const u8 *g = cjk_glyph(cp);
            if (g) { draw_glyph16(cx, y, g, fg); cx += 16; }
            else    { draw_glyph(cx, y, '?', fg); cx += 8; }   /* unknown cp */
        }
    }
}
/* text that respects the background (writes opaque) */
static int text_width(const char *s) {
    /* count display cells: ASCII = 8px, CJK = 16px */
    int w = 0;
    const char *p = s;
    while (*p) {
        u32 cp = utf8_next(&p);
        if (cp == 0xFFFFFFFFU) continue;
        w += (cp < 0x80) ? 8 : 16;
    }
    return w;
}
static void draw_text_bg(int x, int y, const char *s, u32 fg, u32 bg) {
    int w = text_width(s);
    fb_fillrect(x, y, w, 16, bg);
    draw_text(x, y, s, fg);
}

/* ---- window chrome ------------------------------------------------------- */
typedef struct {
    int x, y, w, h;
    const char *title;
    int active;
} win_t;

static void win_draw(const win_t *w) {
    u32 frame = w->active ? theme->frame : theme->frame_dim;
    /* outer neon frame (2 px) */
    fb_fillrect(w->x, w->y, w->w, w->h, theme->panel);
    fb_rect(w->x, w->y, w->w, w->h, frame);
    fb_rect(w->x + 1, w->y + 1, w->w - 2, w->h - 2, frame);
    /* title bar */
    fb_fillrect(w->x + 2, w->y + 2, w->w - 4, 20, frame);
    draw_text(w->x + 8, w->y + 5, w->title, theme->text_inv);
    /* inner content border */
    fb_rect(w->x + 4, w->y + 24, w->w - 8, w->h - 28, theme->border);
}

/* ---- buttons ------------------------------------------------------------- */
typedef struct { int x, y, w, h; const char *label; int hover; int pressed; } btn_t;
static int btn_hit(btn_t *b, int mx, int my) {
    return mx >= b->x && mx < b->x + b->w && my >= b->y && my < b->y + b->h;
}
static void btn_draw(btn_t *b) {
    u32 fill = b->pressed ? theme->panel_hi : (b->hover ? theme->accent : theme->panel);
    u32 fg   = (b->hover && !b->pressed) ? theme->text_inv : theme->text;
    fb_fillrect(b->x, b->y, b->w, b->h, fill);
    fb_rect(b->x, b->y, b->w, b->h, theme->frame);
    int tw = text_width(b->label);
    draw_text(b->x + (b->w - tw) / 2, b->y + (b->h - 16) / 2, b->label, fg);
}

/* ---- progress bar (boot screen) ----------------------------------------- */
static void progress_bar(int x, int y, int w, int h, int pct) {
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    fb_fillrect(x, y, w, h, theme->panel);
    fb_rect(x, y, w, h, theme->border);
    int fw = (w - 4) * pct / 100;
    if (fw > 0) fb_fillrect(x + 2, y + 2, fw, h - 4, theme->accent);
}

/* =========================================================================
 *  Boot screen: centred "Ziphu.OS" wordmark + clean progress bar at the
 *  bottom. No debug/console text.
 * ====================================================================== */
void gui_boot_screen(void) {
    fb_clear(theme->bg);
    const char *word = "Ziphu.OS";
    int ww = text_width(word);
    int cx = (g_fb.width - ww) / 2;
    int cy = (g_fb.height - 16) / 2 - 20;
    draw_text(cx, cy, word, theme->text);
    fb_fillrect(cx, cy + 20, ww, 2, theme->accent);
    const char *ver = "OpenZK  1.0";
    int vw = text_width(ver);
    draw_text((g_fb.width - vw) / 2, cy + 28, ver, theme->text_dim);
    fb_present();

    int bar_w = 280;
    int bx = (g_fb.width - bar_w) / 2;
    int by = g_fb.height - 60;
    /* Animate the bar on a real timer (100 Hz tick) so the boot screen takes a
     * deterministic ~1.6 s regardless of QEMU emulation speed. The old busy
     * loop varied wildly (3-15 s) and threw off the screenshot schedule. Each
     * 10% step is held ~150 ms. */
    for (int pct = 0; pct <= 100; pct += 10) {
        u32 deadline = timer_ticks() + 15;   /* ~150 ms */
        progress_bar(bx, by, bar_w, 12, pct);
        fb_present();
        while ((s32)(timer_ticks() - deadline) < 0) { sti(); hlt(); }
    }
    /* hold the completed state ~400 ms so a screenshot capture lands on a
     * full bar. */
    progress_bar(bx, by, bar_w, 12, 100);
    fb_present();
    {
        u32 deadline = timer_ticks() + 40;   /* ~400 ms */
        while ((s32)(timer_ticks() - deadline) < 0) { sti(); hlt(); }
    }
}

/* =========================================================================
 *  Desktop + apps. The desktop is a vertical list of application names on the
 *  left (no icons), a SYSTEM window on the right with CPUID / VM / MEM, and a
 *  top status bar. Settings opens a panel with a theme toggle and the two
 *  crash-test buttons.
 * ====================================================================== */

typedef enum {
    APP_SETTINGS = 0,
    APP_FILEMAN,
    APP_MEMLOG,
    APP_SYSLOG,
    APP_ABOUT,
    APP_COUNT
} app_id_t;

static const char *app_names[APP_COUNT] = {
    "Settings",
    "File Manager",
    "MemLog",
    "SysLog",
    "About",
};

static int  active_app = -1;       /* -1 = desktop, else an APP_* id */
static int  app_sel    = 0;        /* highlighted row in the apps list */
static int  mem_view_line = 0;     /* scroll offset for MemLog */
static int  sys_view_line = 0;     /* scroll offset for SysLog */

/* MemLog: 256 rows of 0x00000000 (RAM-map snapshot). We synthesise the rows
 * from the lower memory layout so they are real, not filler. */
#define MEMLOG_ROWS 256
static u32 memlog_addr[MEMLOG_ROWS];
static void memlog_build(void) {
    u32 base = 0x00000000;
    for (int i = 0; i < MEMLOG_ROWS; i++) {
        memlog_addr[i] = base + (u32)i * (16 * 1024);   /* 16 KiB cells */
    }
}

/* SysLog: ring of action records "[EXAMPLE] EXAMPLE TASK" style. */
#define SYSLOG_ROWS 256
static char syslog_buf[SYSLOG_ROWS][48];
static int  syslog_count;
static int  syslog_head;
static void syslog_push(const char *tag, const char *action) {
    int idx = (syslog_head + syslog_count) % SYSLOG_ROWS;
    if (syslog_count == SYSLOG_ROWS) syslog_head = (syslog_head + 1) % SYSLOG_ROWS;
    else syslog_count++;
    char *p = syslog_buf[idx];
    int i = 0;
    p[i++] = '[';
    for (const char *t = tag; *t && i < 20; ) p[i++] = *t++;
    p[i++] = ']'; p[i++] = ' ';
    for (const char *a = action; *a && i < 47; ) p[i++] = *a++;
    p[i] = 0;
}
static const char *syslog_get(int n) {
    if (n < 0 || n >= syslog_count) return "";
    return syslog_buf[(syslog_head + n) % SYSLOG_ROWS];
}

/* ---- desktop layout helpers --------------------------------------------- */
#define LEFT_X 16
#define LEFT_W 168
#define LEFT_Y 48
#define ROW_H  22

static void draw_status_bar(void) {
    fb_fillrect(0, 0, g_fb.width, 32, theme->panel);
    fb_hline(0, 32, g_fb.width, theme->frame);
    draw_text(12, 9, "Ziphu.OS", theme->text);
    /* right-aligned clock + lang + mem summary */
    char buf[64];
    u32 up = timer_uptime_ms() / 1000;
    zk_sprintf(buf, "up %us  %s  RAM %uMB/%uMB  %s",
               up, L(&L_LANGUAGE), mem_used_mb(), mem_total_mb(),
               (lang == LANG_EN ? "EN" : "ZH"));
    draw_text(g_fb.width - text_width(buf) - 12, 9, buf, theme->text_dim);
}

static void draw_apps_list(void) {
    win_t w = { LEFT_X, LEFT_Y, LEFT_W, g_fb.height - LEFT_Y - 16, L(&L_APPSMENU), 1 };
    win_draw(&w);
    int y = LEFT_Y + 32;
    for (int i = 0; i < APP_COUNT; i++) {
        int sel = (i == app_sel);
        u32 fg  = sel ? theme->text_inv : theme->text;
        u32 bg  = sel ? theme->accent : theme->panel;
        /* row strip */
        fb_fillrect(LEFT_X + 6, y - 2, LEFT_W - 12, 20, bg);
        /* vertical accent tick on the left edge of the selected row */
        if (sel) fb_vline(LEFT_X + 8, y - 2, 20, theme->text_inv);
        draw_text(LEFT_X + 18, y, app_names[i], fg);
        y += ROW_H;
    }
}

/* SYSTEM window: CPUID (vendor light, model dim), VM, MEM (Used X MB) */
static void draw_system_window(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = 220;
    win_t w = { sx, sy, sw, sh, L(&L_SYSTEM), active_app == -1 };
    win_draw(&w);

    int x = sx + 14;
    int y = sy + 36;

    /* CPUID block -------------------------------------------------------- */
    draw_text(x, y, L(&L_CPUID), theme->text_dim); y += 20;
    draw_text(x, y, cpuid_vendor_short(), theme->text); y += 20;
    draw_text(x, y, cpuid_model_string(), theme->text_dim); y += 24;

    /* if the model could not be determined, the spec calls for a MemLog and
     * SysLog button next to the line. We render the buttons always (they are
     * useful regardless) so the layout is stable. */
    btn_t mb = { x, y, 80, 22, L(&L_MEMLOG), 0, 0 };
    btn_t sb = { x + 88, y, 80, 22, L(&L_SYSLOG), 0, 0 };
    btn_draw(&mb); btn_draw(&sb);
    y += 30;

    /* VM ----------------------------------------------------------------- */
    draw_text(x, y, "VM:", theme->text_dim);
    draw_text(x + 40, y, vm_detect(), theme->text);
    y += 24;

    /* MEM ---------------------------------------------------------------- */
    char used[48];
    u32 used_b = mem_used_bytes();
    zk_sprintf(used, "%s: %u MB / %u MB", L(&L_USED), used_b / (1024*1024), mem_total_mb());
    draw_text(x, y, L(&L_MEM), theme->text_dim);
    draw_text(x + 40, y, used, theme->text);
    y += 20;
    /* video framebuffer footprint detail (spec: ~1.17 MB for 640x480x32) */
    char vb[48];
    u32 vbytes = g_fb.size;
    zk_sprintf(vb, "video fb: %u KB (%u bpp)", vbytes / 1024, g_fb.bpp);
    draw_text(x + 40, y, vb, theme->text_dim);
}

/* ---- Settings panel: theme + language + crash buttons ------------------- */

/* Draw one RGB slider. Returns the track rectangle for hit-testing. The knob
 * x is computed from value (0..255). Dragging updates the value live. */
typedef struct { int x, y, w, h; u8 *val; u8 chan; const char *label; } slider_t;
static void slider_rect(slider_t *s, int *track_x, int *track_y, int *track_w, int *track_h) {
    (void)s;
    *track_x = s->x + 40; *track_y = s->y + 4;
    *track_w = s->w - 48;  *track_h = 12;
}
static void slider_draw(slider_t *s) {
    draw_text(s->x, s->y + 2, s->label, theme->text);
    int tx, ty, tw, th; slider_rect(s, &tx, &ty, &tw, &th);
    /* track: a gradient strip for the channel so the user sees the gamut */
    for (int i = 0; i < tw; i++) {
        u8 v = (u8)((i * 255) / (tw > 1 ? tw - 1 : 1));
        u8 r = (s->chan == 0) ? v : cust_r;
        u8 g = (s->chan == 1) ? v : cust_g;
        u8 b = (s->chan == 2) ? v : cust_b;
        fb_vline(tx + i, ty, th, rgb(r, g, b));
    }
    fb_rect(tx, ty, tw, th, theme->border);
    /* knob */
    int kx = tx + (*s->val * (tw - 1)) / 255;
    fb_fillrect(kx - 3, ty - 3, 7, th + 6, theme->text);
    fb_rect(kx - 3, ty - 3, 7, th + 6, theme->frame);
    /* numeric value */
    char buf[8]; zk_sprintf(buf, "%3u", *s->val);
    draw_text(tx + tw + 6, s->y + 2, buf, theme->text_dim);
}

static void draw_settings(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = g_fb.height - LEFT_Y - 16;
    win_t w = { sx, sy, sw, sh, L(&L_SETTINGS), 1 };
    win_draw(&w);
    int x = sx + 16, y = sy + 36;

    draw_text(x, y, L(&L_THEME), theme->text); y += 22;
    btn_t bl = { x,        y, 90, 24, L(&L_LIGHT), theme == &theme_light, 0 };
    btn_t bd = { x + 100,  y, 90, 24, L(&L_DARK),  theme == &theme_dark && !cust_use, 0 };
    btn_t bc = { x + 200,  y, 90, 24, "Custom",   cust_use, 0 };
    btn_draw(&bl); btn_draw(&bd); btn_draw(&bc);
    y += 36;

    draw_text(x, y, L(&L_LANGUAGE), theme->text); y += 22;
    btn_t be = { x,        y, 90, 24, "English",  lang == LANG_EN, 0 };
    btn_t bz = { x + 100,  y, 90, 24, "简体中文", lang == LANG_ZH, 0 };
    btn_draw(&be); btn_draw(&bz);
    y += 40;

    /* ---- RGB accent color picker -------------------------------------- */
    fb_hline(x, y, sw - 32, theme->border); y += 8;
    draw_text(x, y, "Accent color (R G B)", theme->text); y += 24;
    /* preview swatch */
    fb_fillrect(x, y, 24, 24, rgb(cust_r, cust_g, cust_b));
    fb_rect(x, y, 24, 24, theme->text);
    char swtxt[32];
    zk_sprintf(swtxt, "RGB(%u, %u, %u)", cust_r, cust_g, cust_b);
    draw_text(x + 32, y + 4, swtxt, theme->text_dim);
    y += 30;
    /* sliders take the full content width */
    int sl_w = sw - 32;
    slider_t sr = { x, y, sl_w, 20, &cust_r, 0, "R" }; slider_draw(&sr); y += 26;
    slider_t sg = { x, y, sl_w, 20, &cust_g, 1, "G" }; slider_draw(&sg); y += 26;
    slider_t sb = { x, y, sl_w, 20, &cust_b, 2, "B" }; slider_draw(&sb); y += 30;
    /* quick preset row: red -> pink through the rainbow */
    const char *preset_names[7] = {"Red","Orange","Yellow","Green","Cyan","Magenta","Pink"};
    static const u8 pv[7][3] = {{255,0,0},{255,140,0},{255,230,0},{0,200,0},
                                {0,229,229},{229,0,229},{255,105,180}};
    int pw = (sl_w - 6*6) / 7;
    for (int i = 0; i < 7; i++) {
        int px = x + i * (pw + 6);
        fb_fillrect(px, y, pw, 18, rgb(pv[i][0], pv[i][1], pv[i][2]));
        fb_rect(px, y, pw, 18, theme->border);
        /* label only fits the wider presets; show on all for clarity */
        int tw = text_width(preset_names[i]);
        u32 fg = (pv[i][0]+pv[i][1]+pv[i][2] > 384) ? 0xFF101014U : 0xFFFFFFFFU;
        draw_text(px + (pw - tw)/2, y + 1, preset_names[i], fg);
    }
    y += 30;

    /* crash-test group */
    fb_hline(x, y, sw - 32, theme->border); y += 8;
    draw_text(x, y, "Stability tests", theme->text_dim); y += 24;
    btn_t bcs = { x,        y, 150, 28, L(&L_CRASHSYS), 0, 0 };
    btn_t bcv = { x + 160,  y, 150, 28, L(&L_CRASHVMS), 0, 0 };
    btn_draw(&bcs); btn_draw(&bcv);
    y += 36;
    draw_text(x, y, "Watchdog 5s -> RSOD -> reboot", theme->text_dim);
    y += 18;
    draw_text(x, y, "VMS hang: freezes a kernel thread", theme->text_dim);
}

/* ---- File Manager: dual pane ------------------------------------------- */
static int fm_drive_sel = 0;
static int fm_file_sel  = 0;
static vfs_node_t *fm_cwd;       /* current directory shown in the right pane */

static void draw_file_manager(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = g_fb.height - LEFT_Y - 16;
    win_t w = { sx, sy, sw, sh, L(&L_FILEMAN), 1 };
    win_draw(&w);

    int half = (sw - 16) / 2;
    int lx = sx + 8, rx = sx + 8 + half + 0;
    int ly = sy + 32;
    int lh = sh - 40;

    /* left pane: drives */
    draw_text(lx, ly - 18, L(&L_DRIVES), theme->text_dim);
    fb_fillrect(lx, ly, half, lh, theme->panel);
    fb_rect(lx, ly, half, lh, theme->border);
    int n = drive_count();
    int y = ly + 8;
    for (int i = 0; i < n; i++) {
        u32 bg = (i == fm_drive_sel) ? theme->accent : theme->panel;
        u32 fg = (i == fm_drive_sel) ? theme->text_inv : theme->text;
        fb_fillrect(lx + 2, y, half - 4, 20, bg);
        char row[64];
        zk_sprintf(row, "%s  %s  %uMB", drive_name(i), drive_kind(i), drive_size_mb(i));
        draw_text(lx + 8, y + 2, row, fg);
        y += 22;
    }

    /* right pane: files in selected drive/dir */
    draw_text(rx, ly - 18, L(&L_FILES), theme->text_dim);
    fb_fillrect(rx, ly, half, lh, theme->panel);
    fb_rect(rx, ly, half, lh, theme->border);
    y = ly + 8;
    if (!fm_cwd) fm_cwd = vfs_root();
    /* first row: .. back */
    {
        u32 bg = (fm_file_sel == 0) ? theme->accent : theme->panel;
        u32 fg = (fm_file_sel == 0) ? theme->text_inv : theme->text;
        fb_fillrect(rx + 2, y, half - 4, 20, bg);
        draw_text(rx + 8, y + 2, L(&L_BACK), fg);
        y += 22;
    }
    int idx = 1;
    for (vfs_node_t *c = fm_cwd->child; c; c = c->next) {
        if (fm_file_sel == idx) {
            fb_fillrect(rx + 2, y, half - 4, 20, theme->accent);
            draw_text(rx + 8, y + 2, c->name, theme->text_inv);
            /* size in bytes, exact */
            char sz[24]; zk_sprintf(sz, "%u B", c->size);
            draw_text(rx + half - text_width(sz) - 10, y + 2, sz, theme->text_inv);
        } else {
            draw_text(rx + 8, y + 2, c->name, theme->text);
            char sz[24]; zk_sprintf(sz, "%u B", c->size);
            draw_text(rx + half - text_width(sz) - 10, y + 2, sz, theme->text_dim);
        }
        y += 22;
        idx++;
    }
}

/* ---- MemLog / SysLog viewers ------------------------------------------- */
static void draw_memlog(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = g_fb.height - LEFT_Y - 16;
    win_t w = { sx, sy, sw, sh, L(&L_MEMLOG), 1 };
    win_draw(&w);
    int x = sx + 14, y = sy + 36;
    int rows = (sh - 48) / 18;
    for (int i = 0; i < rows && (mem_view_line + i) < MEMLOG_ROWS; i++) {
        char line[48];
        u32 a = memlog_addr[mem_view_line + i];
        zk_sprintf(line, "%08x  0x00000000 0x00000000 0x00000000 0x00000000", a);
        draw_text(x, y, line, theme->text);
        y += 18;
    }
}
static void draw_syslog(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = g_fb.height - LEFT_Y - 16;
    win_t w = { sx, sy, sw, sh, L(&L_SYSLOG), 1 };
    win_draw(&w);
    int x = sx + 14, y = sy + 36;
    int rows = (sh - 48) / 18;
    int total = syslog_count;
    int start = total - rows - sys_view_line;
    if (start < 0) start = 0;
    for (int i = 0; i < rows; i++) {
        int idx = start + i;
        if (idx < 0 || idx >= total) break;
        draw_text(x, y, syslog_get(idx), theme->text);
        y += 18;
    }
    if (total == 0) draw_text(x, y, "(no events yet)", theme->text_dim);
}

/* ---- About ------------------------------------------------------------- */
static void draw_about(void) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int sy = LEFT_Y;
    int sh = g_fb.height - LEFT_Y - 16;
    win_t w = { sx, sy, sw, sh, L(&L_ABOUT), 1 };
    win_draw(&w);
    int x = sx + 16, y = sy + 40;
    draw_text(x, y, "Ziphu.OS 1.0", theme->text); y += 24;
    draw_text(x, y, "OpenZK (Open-ZiphuKernel)", theme->text_dim); y += 24;
    draw_text(x, y, "i386 monolithic hobby kernel", theme->text_dim); y += 24;
    draw_text(x, y, "Lightning shell - monochrome + neon turquoise", theme->text_dim); y += 24;
    char buf[64];
    zk_sprintf(buf, "Video: %ux%u %u-bpp", g_fb.width, g_fb.height, g_fb.bpp);
    draw_text(x, y, buf, theme->text_dim); y += 24;
    zk_sprintf(buf, "CPU: %s", cpuid_vendor_short());
    draw_text(x, y, buf, theme->text_dim); y += 24;
    zk_sprintf(buf, "VM:  %s", vm_detect());
    draw_text(x, y, buf, theme->text_dim);
}

/* ---- mouse cursor -------------------------------------------------------- */
/* Arrow cursor drawn last so it floats above all content. Hot spot is the
 * top-left pixel. 11x16 monochrome shape. */
static const u8 cursor_shape[16] = {
    0b10000000, 0b11000000, 0b11100000, 0b11110000,
    0b11111000, 0b11111100, 0b11111110, 0b11111100,
    0b11101010, 0b11001010, 0b10001010, 0b00001010,
    0b00001010, 0b00001100, 0b00001100, 0b00001000,
};
static void draw_cursor(int x, int y) {
    u32 fg = 0xFFFFFFFF;   /* white arrow */
    u32 ol = 0xFF000000;   /* black outline so it shows on light panels too */
    for (int r = 0; r < 16; r++) {
        u8 row = cursor_shape[r];
        for (int c = 0; c < 8; c++) {
            if (row & (0x80 >> c)) {
                int px = x + c, py = y + r;
                /* 1px black drop-shadow for contrast, then the white pixel */
                if (px + 1 < (int)g_fb.width && py + 1 < (int)g_fb.height)
                    fb_putpixel(px + 1, py + 1, ol);
                fb_putpixel(px, py, fg);
            }
        }
    }
}

/* ---- text input field (demonstrates the Shift layer: 1->!, ;->: ...) ----- */
#define INPUT_MAX 60
static char input_buf[INPUT_MAX + 1];
static int  input_len;
static void input_add(char ch) {
    if (ch < 0x20 || ch > 0x7E) return;     /* printable ASCII only */
    if (input_len < INPUT_MAX) input_buf[input_len++] = ch;
}
static void input_backspace(void) { if (input_len > 0) input_len--; }
static void input_clear(void)     { input_len = 0; }
static void draw_input_field(void) {
    /* full-width bar at the very bottom of the screen */
    int y = g_fb.height - 28;
    int h = 24;
    fb_fillrect(0, y, g_fb.width, h, theme->panel);
    fb_hline(0, y, g_fb.width, theme->frame);
    draw_text(8, y + 4, ">", theme->accent);
    /* the typed text + a blinking block caret */
    input_buf[input_len] = 0;
    draw_text(20, y + 4, input_buf, theme->text);
    int cx = 20 + input_len * 8;
    /* caret blinks ~2 Hz */
    if ((timer_ticks() / 25) & 1)
        fb_fillrect(cx, y + 4, 8, 16, theme->accent);
    /* hint on the right */
    const char *hint = "Shift: 1->!  ;->:  type here";
    draw_text(g_fb.width - text_width(hint) - 8, y + 4, hint, theme->text_dim);
}

/* ---- main desktop redraw ----------------------------------------------- */
static void desktop_redraw(void) {
    fb_clear(theme->bg);
    draw_status_bar();
    draw_apps_list();
    switch (active_app) {
        case APP_SETTINGS: draw_settings(); break;
        case APP_FILEMAN:  draw_file_manager(); break;
        case APP_MEMLOG:   draw_memlog(); break;
        case APP_SYSLOG:   draw_syslog(); break;
        case APP_ABOUT:    draw_about(); break;
        default:           draw_system_window(); break;
    }
    draw_input_field();
    /* cursor on top of everything, including the input field */
    draw_cursor(mouse_x(), mouse_y());
    fb_present();
}

/* ---- input dispatch ----------------------------------------------------- */
static void open_app(app_id_t id) {
    active_app = (int)id;
    syslog_push("GUI", "open app");
    if (id == APP_MEMLOG) memlog_build();
}

/* hit-test the apps list; returns APP_COUNT if the click missed a row */
static int apps_list_hit(int mx, int my) {
    int y = LEFT_Y + 32;
    for (int i = 0; i < APP_COUNT; i++) {
        if (mx >= LEFT_X + 6 && mx < LEFT_X + LEFT_W - 6 &&
            my >= y - 2 && my < y + 18) return i;
        y += ROW_H;
    }
    return APP_COUNT;
}

/* settings button click handling (button geometry mirrors draw_settings) */
/* mirror of draw_settings geometry so click hit-testing matches the drawing */
static int settings_slider_at(int mx, int my, int *chan_out) {
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int x = sx + 16;
    /* y after theme(22+36) lang(40) + divider(8) + "Accent color"(24) + swatch(30) */
    int y0 = LEFT_Y + 36 + 22 + 36 + 22 + 40 + 8 + 24 + 30;
    int sl_w = sw - 32;
    for (int ch = 0; ch < 3; ch++) {
        int sy = y0 + ch * 26;
        int tx, ty, tw, th;
        slider_t s = { x, sy, sl_w, 20, 0, (u8)ch, 0 };
        slider_rect(&s, &tx, &ty, &tw, &th);
        /* expand the hit area vertically a bit for easy grabbing */
        if (mx >= tx - 4 && mx < tx + tw + 4 && my >= ty - 4 && my < ty + th + 4) {
            *chan_out = ch;
            return 1;
        }
    }
    return 0;
}

static void settings_drag_update(int mx, int my) {
    (void)my;
    if (drag_slider < 1 || drag_slider > 3) return;
    int sx = LEFT_X + LEFT_W + 12;
    int sw = g_fb.width - sx - 16;
    int x = sx + 16;
    int sl_w = sw - 32;
    int tx = x + 40, tw = sl_w - 48;
    u8 *val = (drag_slider == 1) ? &cust_r : (drag_slider == 2) ? &cust_g : &cust_b;
    int v = ((mx - tx) * 255) / (tw > 1 ? tw - 1 : 1);
    if (v < 0) v = 0; if (v > 255) v = 255;
    *val = (u8)v;
    apply_custom_color();
}

static void settings_click(int mx, int my) {
    int sx = LEFT_X + LEFT_W + 12;
    int x = sx + 16;
    int y = LEFT_Y + 36 + 22;   /* theme buttons row */
    /* theme: Light / Dark / Custom */
    btn_t bl = { x,       y, 90, 24, 0,0,0 };
    btn_t bd = { x + 100, y, 90, 24, 0,0,0 };
    btn_t bc = { x + 200, y, 90, 24, 0,0,0 };
    if (btn_hit(&bl, mx, my)) { cust_use = 0; theme = &theme_light; syslog_push("CFG","theme=light"); return; }
    if (btn_hit(&bd, mx, my)) { cust_use = 0; theme = &theme_dark;  syslog_push("CFG","theme=dark");  return; }
    if (btn_hit(&bc, mx, my)) { cust_use = 1; apply_custom_color(); syslog_push("CFG","theme=custom"); return; }
    y += 22 + 36;              /* language buttons row */
    btn_t be = { x,       y, 90, 24, 0,0,0 };
    btn_t bz = { x + 100, y, 90, 24, 0,0,0 };
    if (btn_hit(&be, mx, my)) { lang = LANG_EN; syslog_push("CFG","lang=en"); return; }
    if (btn_hit(&bz, mx, my)) { lang = LANG_ZH; syslog_push("CFG","lang=zh"); return; }
    y += 40;                   /* divider */
    y += 8 + 24 + 30;          /* "Accent color" + swatch */
    /* slider click -> start drag */
    int ch;
    if (settings_slider_at(mx, my, &ch)) {
        drag_slider = ch + 1;
        cust_use = 1;
        settings_drag_update(mx, my);
        syslog_push("CFG", "accent slider");
        return;
    }
    /* sliders live at y0..y0+52 (3 rows of 26); advance past them */
    y += 26 * 3 + 30;
    /* preset color row: 7 swatches */
    {
        int sw = g_fb.width - sx - 16;
        int sl_w = sw - 32;
        int pw = (sl_w - 6*6) / 7;
        static const u8 pv[7][3] = {{255,0,0},{255,140,0},{255,230,0},{0,200,0},
                                    {0,229,229},{229,0,229},{255,105,180}};
        for (int i = 0; i < 7; i++) {
            int px = x + i * (pw + 6);
            if (mx >= px && mx < px + pw && my >= y && my < y + 18) {
                cust_r = pv[i][0]; cust_g = pv[i][1]; cust_b = pv[i][2];
                cust_use = 1;
                apply_custom_color();
                syslog_push("CFG", "accent preset");
                return;
            }
        }
    }
    y += 18 + 8 + 8;           /* preset row + divider */
    y += 24;                   /* "Stability tests" */
    btn_t bcs = { x,       y, 150, 28, 0,0,0 };
    btn_t bcv = { x + 160, y, 150, 28, 0,0,0 };
    if (btn_hit(&bcs, mx, my)) { syslog_push("TEST","arm watchdog"); crash_arm_watchdog(); return; }
    if (btn_hit(&bcv, mx, my)) { syslog_push("TEST","vms hang");     crash_vms_hang();     return; }
}

static void handle_desktop_key(int ch) {
    /* arrow up/down move selection; enter opens; esc returns to desktop */
    if (ch == 0x48 /* not used; set1 has no arrow ascii here */) {}
    /* We rely on raw scancodes being translated to ascii already; for arrows
     * we accept the convention 0x1B/0x08/0x09/0x0D as nav and the letters
     * j/k as vim-style up/down so the desktop is usable from ASCII keyboard
     * input. */
    if (ch == 'j' || ch == 'k' || ch == 'J' || ch == 'K') {
        if (ch == 'j' || ch == 'J') app_sel = (app_sel + 1) % APP_COUNT;
        else                        app_sel = (app_sel + APP_COUNT - 1) % APP_COUNT;
        syslog_push("INPUT", "move selection");
    } else if (ch == 0x0D /* enter */) {
        open_app((app_id_t)app_sel);
    } else if (ch == 0x1B /* esc */) {
        active_app = -1;
    }
}

static void handle_settings_key(int ch) {
    if (ch == '1') { cust_use = 0; theme = &theme_light; syslog_push("CFG", "theme=light"); }
    else if (ch == '2') { cust_use = 0; theme = &theme_dark; syslog_push("CFG", "theme=dark"); }
    else if (ch == '3') { cust_use = 1; apply_custom_color(); syslog_push("CFG", "theme=custom"); }
    else if (ch == 'e' || ch == 'E') { lang = LANG_EN; syslog_push("CFG", "lang=en"); }
    else if (ch == 'z' || ch == 'Z') { lang = LANG_ZH; syslog_push("CFG", "lang=zh"); }
    else if (ch == 's' || ch == 'S') { syslog_push("TEST", "arm watchdog"); crash_arm_watchdog(); }
    else if (ch == 'v' || ch == 'V') { syslog_push("TEST", "vms hang"); crash_vms_hang(); }
    else if (ch == 0x1B) active_app = -1;
}

static void handle_fileman_key(int ch) {
    int dn = drive_count();
    if (ch == 'h' || ch == 'H') { if (dn) fm_drive_sel = (fm_drive_sel + dn - 1) % dn; fm_cwd = vfs_root(); fm_file_sel = 0; }
    else if (ch == 'l' || ch == 'L') { if (dn) fm_drive_sel = (fm_drive_sel + 1) % dn; fm_cwd = vfs_root(); fm_file_sel = 0; }
    else if (ch == 'j' || ch == 'J') fm_file_sel++;
    else if (ch == 'k' || ch == 'K') { if (fm_file_sel > 0) fm_file_sel--; }
    else if (ch == 0x0D) {
        /* enter: descend into a directory (row 0 = .. back) */
        if (fm_file_sel == 0) {
            if (fm_cwd->parent) { fm_cwd = fm_cwd->parent; fm_file_sel = 0; }
        } else {
            int idx = 1;
            for (vfs_node_t *c = fm_cwd->child; c; c = c->next, idx++) {
                if (idx == fm_file_sel) {
                    if (c->flags & VFS_DIR) { fm_cwd = c; fm_file_sel = 0; }
                    break;
                }
            }
        }
        syslog_push("FILE", "enter dir");
    } else if (ch == 0x1B) active_app = -1;
}

/* ---- global input: text field + mouse click ----------------------------- */
/* Printable keys and editing keys feed the bottom input field so the Shift
 * layer (1->!, ;->: ...) is visible. Navigation keys still reach the app. */
static void feed_input_field(int ch) {
    if (ch == 0x08 /* backspace */)      input_backspace();
    else if (ch == 0x1B /* esc */)       input_clear();
    else                                  input_add((char)ch);
}

/* route a mouse left-click to the active screen */
static void dispatch_click(int mx, int my) {
    /* clicking the apps list opens that app (from anywhere) */
    int hit = apps_list_hit(mx, my);
    if (hit < APP_COUNT) { app_sel = hit; open_app((app_id_t)hit); return; }
    /* per-screen buttons */
    if (active_app == APP_SETTINGS) settings_click(mx, my);
}

void gui_init(void) {
    memlog_build();
    syslog_push("BOOT", "kernel ready");
    syslog_push("VFS",  "mounted /");
    syslog_push("VFS",  "mounted /examples");
    syslog_push("DRIVE","ide0 ready");
    syslog_push("DRIVE","sata0 ready");
    fm_cwd = vfs_root();
}

void gui_run_desktop(void) {
    dbg_puts("[zk] desktop eflags="); { char b[16]; dbg_puts(zk_utoa(read_eflags(), b, 16)); dbg_puts("\n"); }
#ifdef ZK_DEMO
    /* Auto-cycle through screens for headless screenshot capture. Each screen
     * is held for ~2 s (200 ticks at 100 Hz). The keyboard is still serviced,
     * so a real key press in an interactive QEMU overrides the cycle.
     *
     * Redraw is throttled to ~30 Hz (every 3 ticks): with double buffering
     * each fb_present blits 1.2 MB to the LFB, so redrawing on every 100 Hz
     * tick saturates QEMU and the timer loses ticks, stretching the cycle. A
     * key press forces an immediate redraw so interaction feels instant. */
    static u32 demo_next_switch;
    static int  demo_app = -2;   /* -2 = not started, -1 = desktop, else APP_* */
    int started = 0;
    u32 last_redraw = 0;
    for (;;) {
        u32 now = timer_ticks();
        if (!started) { demo_next_switch = now + 200; demo_app = -1; started = 1; }
        if (now >= demo_next_switch) {
            demo_app++;
            if (demo_app >= APP_COUNT) demo_app = -1;
            active_app = demo_app;
            if (active_app == APP_MEMLOG) memlog_build();
            demo_next_switch = now + 200;
            last_redraw = 0;   /* force immediate redraw on screen switch */
        }
        if (now - last_redraw >= 3) { desktop_redraw(); last_redraw = now; }
        int ch = kb_getchar();
        if (ch >= 0) {
            feed_input_field(ch);
            /* real input overrides the demo cycle */
            switch (active_app) {
                case APP_SETTINGS: handle_settings_key(ch); break;
                case APP_FILEMAN:  handle_fileman_key(ch);  break;
                case APP_MEMLOG:
                    if (ch == 'j' || ch == 'J') { if (mem_view_line < MEMLOG_ROWS - 4) mem_view_line++; }
                    else if (ch == 'k' || ch == 'K') { if (mem_view_line > 0) mem_view_line--; }
                    else if (ch == 0x1B) active_app = -1;
                    break;
                case APP_SYSLOG:
                    if (ch == 'j' || ch == 'J') { if (sys_view_line > 0) sys_view_line--; }
                    else if (ch == 'k' || ch == 'K') sys_view_line++;
                    else if (ch == 0x1B) active_app = -1;
                    break;
                default: handle_desktop_key(ch); break;
            }
            demo_app = active_app;
            demo_next_switch = now + 200;
            last_redraw = 0;   /* respond to input immediately */
        }
        if (mouse_click()) {
            dispatch_click(mouse_x(), mouse_y());
            demo_app = active_app;
            demo_next_switch = now + 200;
            last_redraw = 0;
        }
        /* slider drag: while the left button is held and a drag is in
         * progress, keep updating the channel value from the cursor x. */
        if (drag_slider && (mouse_buttons() & 1)) {
            settings_drag_update(mouse_x(), mouse_y());
            last_redraw = 0;
        } else if (drag_slider && !(mouse_buttons() & 1)) {
            drag_slider = 0;     /* button released -> stop dragging */
        }
        sti();
        hlt();
    }
#else
    u32 last_redraw = 0;
    for (;;) {
        u32 now = timer_ticks();
        /* Throttle to ~30 Hz; a key press or mouse move forces an instant
         * redraw so the cursor tracks smoothly. Same rationale as the demo
         * path: keep the 1.2 MB blit off the hot path. */
        int ch = kb_getchar();
        int clicked = mouse_click();
        int held = (mouse_buttons() & 1) != 0;
        if (ch >= 0 || clicked || now - last_redraw >= 3 ||
            (drag_slider && held)) {
            desktop_redraw();
            last_redraw = now;
        }
        if (clicked) dispatch_click(mouse_x(), mouse_y());
        if (drag_slider && held) {
            settings_drag_update(mouse_x(), mouse_y());
        } else if (drag_slider && !held) {
            drag_slider = 0;
        }
        if (ch >= 0) feed_input_field(ch);
        if (ch < 0 && !clicked && !drag_slider) {
            /* idle: keep polling. hlt is avoided here because some QEMU/PIC
             * configurations stop delivering IRQ0 after a halt; busy-polling
             * keeps the timer-driven status bar and cursor live. */
            volatile u32 spin = 0;
            for (volatile u32 k = 0; k < 2000; k++) spin += k;
            continue;
        }
        switch (active_app) {
            case APP_SETTINGS: handle_settings_key(ch); break;
            case APP_FILEMAN:  handle_fileman_key(ch);  break;
            case APP_MEMLOG:
                if (ch == 'j' || ch == 'J') { if (mem_view_line < MEMLOG_ROWS - 4) mem_view_line++; }
                else if (ch == 'k' || ch == 'K') { if (mem_view_line > 0) mem_view_line--; }
                else if (ch == 0x1B) active_app = -1;
                break;
            case APP_SYSLOG:
                if (ch == 'j' || ch == 'J') { if (sys_view_line > 0) sys_view_line--; }
                else if (ch == 'k' || ch == 'K') sys_view_line++;
                else if (ch == 0x1B) active_app = -1;
                break;
            case APP_ABOUT:
                if (ch == 0x1B) active_app = -1;
                break;
            default:
                handle_desktop_key(ch);
                break;
        }
        /* watchdog is armed by the crash test; let it run while we loop */
        if (watchdog_armed()) {
            /* keep ticking; if the watchdog's wedged path never fires the
             * RSOD timer, the IRQ0 handler will eventually trigger it. */
        }
    }
#endif
}
