/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK (Open-ZiphuKernel)
 *  include/zk_types.h  -  freestanding base types (no host libc)
 * ========================================================================== */
#ifndef ZK_TYPES_H
#define ZK_TYPES_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long long u64;
typedef signed   char  s8;
typedef signed   short s16;
typedef signed   int   s32;
typedef signed   long long s64;

typedef unsigned int   size_t;
typedef signed   int   ssize_t;
typedef int            bool;
#define true   1
#define false  0
#define NULL   ((void*)0)

#ifndef __cplusplus
#define inline __attribute__((always_inline)) inline
#endif

#define NULL_TERM 0

#endif /* ZK_TYPES_H */
