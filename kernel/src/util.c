/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/util.c
 *  Freestanding string/memory/format helpers (no host libc).
 * ========================================================================== */
#include "zk_types.h"
#include "zk_util.h"

size_t zk_strlen(const char *s) {
    size_t n = 0; while (s && s[n]) n++; return n;
}
int zk_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (u8)*a - (u8)*b;
}
int zk_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (u8)a[i] - (u8)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}
char *zk_strcpy(char *d, const char *s) {
    char *r = d; while ((*d++ = *s++)) ; return r;
}
char *zk_strncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
char *zk_strcat(char *d, const char *s) {
    char *r = d; while (*d) d++; while ((*d++ = *s++)) ; return r;
}
void *zk_memset(void *d, int c, size_t n) {
    u8 *p = (u8*)d; while (n--) *p++ = (u8)c; return d;
}
void *zk_memcpy(void *d, const void *s, size_t n) {
    u8 *dp = d; const u8 *sp = s; while (n--) *dp++ = *sp++; return d;
}
int zk_memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}

/* unsigned int to ascii in given base (2..16) */
char *zk_utoa(u32 v, char *buf, int base) {
    char tmp[32]; int i = 0;
    if (v == 0) tmp[i++] = '0';
    while (v) { u32 d = v % base; tmp[i++] = (d < 10) ? ('0' + d) : ('A' + d - 10); v /= base; }
    int j = 0; while (i) buf[j++] = tmp[--i]; buf[j] = 0; return buf;
}
char *zk_itoa(s32 v, char *buf, int base) {
    if (v < 0 && base == 10) { buf[0] = '-'; zk_utoa((u32)(-v), buf + 1, base); return buf; }
    return zk_utoa((u32)v, buf, base);
}
char *zk_u64toa(u64 v, char *buf) {
    char tmp[32]; int i = 0;
    if (v == 0) tmp[i++] = '0';
    while (v) { u32 d = (u32)(v % 10); tmp[i++] = '0' + d; v /= 10; }
    int j = 0; while (i) buf[j++] = tmp[--i]; buf[j] = 0; return buf;
}

/* format "<n> MB" into buf from bytes (1 MB = 1048576) */
char *zk_bytes_to_mb(u32 bytes, char *buf) {
    u32 mb = bytes / (1024 * 1024);
    char nb[16]; zk_utoa(mb, nb, 10);
    zk_strcpy(buf, nb);
    zk_strcat(buf, " MB");
    return buf;
}

/* minimal printf into a fixed buffer (no floats) used for crash panels */
int zk_sprintf(char *out, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char *o = out;
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') { *o++ = *f; continue; }
        f++;
        switch (*f) {
        case 'd': { s32 v = va_arg(ap, s32); char b[16]; zk_itoa(v, b, 10); char *s = b; while (*s) *o++ = *s++; } break;
        case 'u': { u32 v = va_arg(ap, u32); char b[16]; zk_utoa(v, b, 10); char *s = b; while (*s) *o++ = *s++; } break;
        case 'x': { u32 v = va_arg(ap, u32); char b[16]; zk_utoa(v, b, 16); char *s = b; while (*s) *o++ = *s++; } break;
        case 's': { char *s = va_arg(ap, char*); while (*s) *o++ = *s++; } break;
        case 'c': { int c = va_arg(ap, int); *o++ = (char)c; } break;
        case '%': *o++ = '%'; break;
        case 0: f--; break;
        default: *o++ = '%'; *o++ = *f; break;
        }
    }
    *o = 0;
    va_end(ap);
    return (int)(o - out);
}

/* ---- debug console (QEMU -debugcon, port 0xE9) --------------------------- */
void dbg_putc(char c) {
    __asm__ volatile ("outb %0, $0xE9" : : "a"(c));
}
void dbg_puts(const char *s) {
    if (!s) return;
    while (*s) dbg_putc(*s++);
}
