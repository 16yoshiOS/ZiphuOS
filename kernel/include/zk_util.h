/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  include/zk_util.h
 * ========================================================================== */
#ifndef ZK_UTIL_H
#define ZK_UTIL_H
#include "zk_types.h"

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_arg(v, T)   __builtin_va_arg(v, T)
#define va_end(v)      __builtin_va_end(v)

size_t zk_strlen(const char *s);
int    zk_strcmp(const char *a, const char *b);
int    zk_strncmp(const char *a, const char *b, size_t n);
char  *zk_strcpy(char *d, const char *s);
char  *zk_strncpy(char *d, const char *s, size_t n);
char  *zk_strcat(char *d, const char *s);
void  *zk_memset(void *d, int c, size_t n);
void  *zk_memcpy(void *d, const void *s, size_t n);
int    zk_memcmp(const void *a, const void *b, size_t n);
char  *zk_utoa(u32 v, char *buf, int base);
char  *zk_itoa(s32 v, char *buf, int base);
char  *zk_u64toa(u64 v, char *buf);
char  *zk_bytes_to_mb(u32 bytes, char *buf);
int    zk_sprintf(char *out, const char *fmt, ...);

/* debug port 0xE9 (QEMU -debugcon) */
void dbg_putc(char c);
void dbg_puts(const char *s);

#endif
