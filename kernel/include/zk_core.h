/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  include/zk_core.h
 *  Cross-subsystem declarations shared by kmain and the GUI.
 * ========================================================================== */
#ifndef ZK_CORE_H
#define ZK_CORE_H
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_video.h"

/* idt.c */
void arch_init(void);
typedef struct {
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, esp2, ss;
} regs_t;
void isr_register(int v, void (*h)(regs_t *));
void irq_register(int v, void (*h)(regs_t *));
void irq_mask(int irq);
void irq_unmask(int irq);
u32  irq_total_count(void);

/* mem.c */
void  mem_init(u32 mem_lower_kib, u32 mem_upper_kib);
void *kmalloc(size_t n);
void *kzalloc(size_t n);
u32   mem_total_bytes(void);
u32   mem_total_mb(void);
u32   mem_used_bytes(void);
u32   mem_used_mb(void);
u32   mem_heap_used(void);
void  mem_set_video_bytes(u32 b);

/* timer.c */
void timer_init(u32 hz);
void timer_handler(regs_t *r);
u32  timer_ticks(void);
u32  timer_uptime_ms(void);
void watchdog_arm(u32 seconds);
void watchdog_disarm(void);
int  watchdog_armed(void);
void watchdog_tick(void);

/* keyboard.c */
void keyboard_init(void);
void keyboard_handler(regs_t *r);
int  kb_getchar(void);          /* -1 if none */
int  kb_shift_active(void);

/* mouse.c */
void mouse_init(void);
void mouse_handler(regs_t *r);
int  mouse_x(void);             /* absolute cursor x, clamped to screen */
int  mouse_y(void);             /* absolute cursor y, clamped to screen */
u8   mouse_buttons(void);       /* bit0 left, bit1 right, bit2 middle */
int  mouse_click(void);         /* 1 once per left-press edge, then clears */

/* cpuid.c */
void cpuid_detect(void);
const char *cpuid_vendor_short(void);   /* "Intel" / "AMD" / "VIA" / "unknown" */
const char *cpuid_model_string(void);   /* long brand or "unknown model" */

/* vm.c */
const char *vm_detect(void);            /* "VirtualBox" / "QEMU" / "VMware" / "bare metal" */

/* rsod.c */
void rsod_show(const char *code, const char *message);
void rsod_panic(const char *code, const char *message) __attribute__((noreturn));

/* reboot */
void reboot(void);
void triple_fault_reboot(void);

/* vfs */
struct vfs_node;
typedef struct vfs_node vfs_node_t;
typedef vfs_node_t *(*vfs_open_fn)(vfs_node_t *parent, const char *name, int flags);
typedef void        (*vfs_close_fn)(vfs_node_t *node);
typedef int         (*vfs_read_fn)(vfs_node_t *node, u64 off, u32 count, u8 *buf);
typedef int         (*vfs_write_fn)(vfs_node_t *node, u64 off, u32 count, const u8 *buf);
typedef int         (*vfs_readdir_fn)(vfs_node_t *dir, u32 index, char *name_out, u32 *size_out);

struct vfs_node {
    char name[64];
    u32  flags;                 /* VFS_DIR / VFS_FILE */
    u32  size;                  /* bytes (for files) */
    u32  inode;
    vfs_open_fn    open;
    vfs_close_fn   close;
    vfs_read_fn    read;
    vfs_write_fn   write;
    vfs_readdir_fn readdir;
    vfs_node_t    *parent;
    vfs_node_t    *child;       /* first child for directories */
    vfs_node_t    *next;        /* sibling link */
    void          *fs_data;     /* backend private data */
};
#define VFS_DIR  1
#define VFS_FILE 2

void    vfs_init(void);
vfs_node_t *vfs_root(void);
vfs_node_t *vfs_lookup(const char *path);
vfs_node_t *vfs_add_child(vfs_node_t *parent, const char *name, u32 flags);
void     vfs_seed_examples(void);
void     vfs_mount_drives(void);

/* ide.c / sata.c */
void ide_init(void);
void sata_init(void);
int  drive_count(void);
const char *drive_name(int i);
const char *drive_kind(int i);
u32  drive_size_mb(int i);

/* gui.c */
void gui_init(void);
void gui_boot_screen(void);
void gui_run_desktop(void);

/* crash.c */
void crash_arm_watchdog(void);   /* Crash System: 5s RSOD + reboot */
void crash_vms_hang(void);       /* Crash VMS: hang a kernel thread forever */

#endif
