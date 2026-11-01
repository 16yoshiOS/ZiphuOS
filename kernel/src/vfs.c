/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/vfs.c
 *  In-RAM virtual file system. vfs_node_t (declared in zk_core.h) carries
 *  open/close/read/write/readdir function pointers; directories form a tree via
 *  parent/child/next links. On boot we mount a root, a shared /examples folder
 *  injected with example files, and stub IDE/SATA drive nodes produced by the
 *  drive layer.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

static vfs_node_t root_node;

/* ---- default callbacks (in-RAM file backed by an in-memory buffer) ------- */
typedef struct { u8 *data; u32 len; u32 cap; } ramfile_t;

static vfs_node_t *ram_open(vfs_node_t *parent, const char *name, int flags) {
    (void)parent; (void)name; (void)flags;
    return NULL;   /* lookups traverse the tree directly; no dynamic open */
}
static void ram_close(vfs_node_t *node) { (void)node; }
static int ram_read(vfs_node_t *node, u64 off, u32 count, u8 *buf) {
    ramfile_t *rf = node ? (ramfile_t*)node->fs_data : NULL;
    if (!rf) return 0;
    if (off >= rf->len) return 0;
    u32 n = (u32)((off + count > rf->len) ? (rf->len - off) : count);
    zk_memcpy(buf, rf->data + off, n);
    return (int)n;
}
static int ram_write(vfs_node_t *node, u64 off, u32 count, const u8 *buf) {
    ramfile_t *rf = node ? (ramfile_t*)node->fs_data : NULL;
    if (!rf) return 0;
    if (off + count > rf->cap) {
        u32 newcap = rf->cap ? rf->cap * 2 : 4096;
        while (newcap < off + count) newcap *= 2;
        u8 *nd = (u8*)kmalloc(newcap);
        if (!nd) return 0;
        if (rf->data) zk_memcpy(nd, rf->data, rf->len);
        rf->data = nd; rf->cap = newcap;
    }
    zk_memcpy(rf->data + off, buf, count);
    if (off + count > rf->len) rf->len = (u32)(off + count);
    node->size = rf->len;
    return (int)count;
}
static int ram_readdir(vfs_node_t *dir, u32 index, char *name_out, u32 *size_out) {
    if (!dir) return 0;
    vfs_node_t *c = dir->child;
    for (u32 i = 0; c && i < index; i++) c = c->next;
    if (!c) return 0;
    zk_strncpy(name_out, c->name, 63); name_out[63] = 0;
    if (size_out) *size_out = c->size;
    return 1;
}

vfs_node_t *vfs_root(void) { return &root_node; }

vfs_node_t *vfs_add_child(vfs_node_t *parent, const char *name, u32 flags) {
    vfs_node_t *n = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!n) return NULL;
    zk_memset(n, 0, sizeof(*n));
    zk_strncpy(n->name, name, 63); n->name[63] = 0;
    n->flags = flags;
    n->open = ram_open; n->close = ram_close;
    n->read = ram_read; n->write = ram_write; n->readdir = ram_readdir;
    n->parent = parent;
    n->next = parent->child;
    parent->child = n;
    return n;
}

static vfs_node_t *vfs_make_file(vfs_node_t *parent, const char *name, const char *content) {
    vfs_node_t *n = vfs_add_child(parent, name, VFS_FILE);
    if (!n) return NULL;
    ramfile_t *rf = (ramfile_t*)kmalloc(sizeof(ramfile_t));
    if (!rf) return NULL;
    u32 len = zk_strlen(content);
    rf->cap = (len + 4095) & ~4095;
    rf->data = (u8*)kmalloc(rf->cap);
    rf->len = len;
    zk_memcpy(rf->data, content, len);
    n->fs_data = rf;
    n->size = len;
    return n;
}

vfs_node_t *vfs_lookup(const char *path) {
    if (!path || path[0] != '/') return NULL;
    vfs_node_t *cur = &root_node;
    const char *p = path + 1;
    while (*p) {
        char seg[64]; int i = 0;
        while (*p && *p != '/' && i < 63) seg[i++] = *p++;
        seg[i] = 0;
        if (*p == '/') p++;
        if (!seg[0]) continue;
        vfs_node_t *c = cur->child;
        vfs_node_t *found = NULL;
        for (; c; c = c->next) if (zk_strcmp(c->name, seg) == 0) { found = c; break; }
        if (!found) return NULL;
        cur = found;
    }
    return cur;
}

void vfs_init(void) {
    zk_memset(&root_node, 0, sizeof(root_node));
    zk_strcpy(root_node.name, "/");
    root_node.flags = VFS_DIR;
    root_node.open = ram_open; root_node.close = ram_close;
    root_node.read = ram_read; root_node.write = ram_write; root_node.readdir = ram_readdir;
}

/* /examples (vfs/shared): injected with example files at boot */
void vfs_seed_examples(void) {
    vfs_node_t *ex = vfs_add_child(&root_node, "examples", VFS_DIR);
    vfs_make_file(ex, "readme.txt",
        "Ziphu.OS 1.0 - examples folder\r\n"
        "===========================\r\n"
        "This shared folder is injected by the kernel at boot.\r\n"
        "Open the File Manager to browse drives and files.\r\n");
    vfs_make_file(ex, "hello.c",
        "/* Ziphu.OS example */\r\n"
        "void main(void) {\r\n"
        "    /* bare-metal: no libc */\r\n"
        "    for (;;) { }\r\n"
        "}\r\n");
    vfs_make_file(ex, "shift.map",
        "Shift inversion map (keyboard driver):\r\n"
        "  1 -> !\r\n  2 -> @\r\n  3 -> #\r\n  ; -> :\r\n");
    vfs_node_t *docs = vfs_add_child(ex, "docs", VFS_DIR);
    vfs_make_file(docs, "boot.txt",
        "Boot sequence:\r\n"
        "1. GRUB loads OpenZK at 1 MiB\r\n"
        "2. arch_init: GDT/IDT/PIC\r\n"
        "3. video_init: VBE/BGA 640x480x32 (VGA fallback)\r\n"
        "4. gui_boot_screen: Ziphu.OS wordmark + progress bar\r\n"
        "5. gui_run_desktop: Lightning shell\r\n");
}

/* Drive mountpoints produced by the IDE/SATA layer (drive.c). */
void vfs_mount_drives(void) {
    vfs_node_t *d = vfs_add_child(&root_node, "drives", VFS_DIR);
    for (int i = 0; i < drive_count(); i++) {
        char nm[16]; zk_sprintf(nm, "%s", drive_name(i));
        vfs_node_t *dn = vfs_add_child(d, nm, VFS_DIR);
        dn->size = drive_size_mb(i) * 1024 * 1024;
        /* a representative file inside each drive so the right pane is non-empty */
        char fn[32]; zk_sprintf(fn, "%s.info", drive_kind(i));
        char body[80]; zk_sprintf(body, "%s %s drive, %u MB\r\n", drive_name(i), drive_kind(i), drive_size_mb(i));
        vfs_make_file(dn, fn, body);
    }
}
