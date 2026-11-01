/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/mouse.c
 *  PS/2 mouse driver. The 8042 keyboard controller also drives an auxiliary
 *  (mouse) port; we enable it, request 3-byte packet streaming, and parse the
 *  packets on IRQ12 (slave PIC, vector 44). Exposes absolute cursor position
 *  plus the three buttons to the GUI.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

#define KB_DATA  0x60
#define KB_STAT  0x64
#define KB_CMD   0x64

/* status register bits */
#define STAT_OBF   0x01   /* output buffer full */
#define STAT_IBF   0x02   /* input buffer full  */
#define STAT_AUX   0x20   /* data came from the aux (mouse) port */

static volatile int  m_x;        /* absolute cursor x (clamped to screen) */
static volatile int  m_y;        /* absolute cursor y */
static volatile u8   m_btns;     /* bit0 left, bit1 right, bit2 middle */
static volatile u8   m_click;    /* set on a left-button press edge */
static volatile u8   m_last_btn; /* previous buttons, for edge detection */

/* 3-byte packet reassembly state. We accept an optional 4th byte when the
 * Intellimouse extension is enabled (VirtualBox always reports a wheel, so it
 * streams 4-byte packets; the extra byte is the scroll delta and is ignored
 * for cursor movement). */
static volatile u8  pkt[4];
static volatile int pkt_i;
static volatile int pkt_len = 3;     /* set to 4 if the wheel extension took */

/* ---- 8042 controller helpers --------------------------------------------- */
static void wait_write(void) {
    /* wait until the input buffer is empty so we can write a command */
    for (int i = 0; i < 100000; i++) {
        if (!(inb(KB_STAT) & STAT_IBF)) return;
    }
}
static void wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(KB_STAT) & STAT_OBF) return;
    }
}
static u8 ctrl_read(void) { wait_read(); return inb(KB_DATA); }
static void ctrl_write_cmd(u8 cmd) { wait_write(); outb(KB_CMD, cmd); }
static void ctrl_write_data(u8 d)  { wait_write(); outb(KB_DATA, d); }

/* Send a byte to the mouse (not the keyboard) via the controller's aux path. */
static void mouse_write(u8 d) {
    ctrl_write_cmd(0xD4);   /* "next byte to 0x60 goes to the aux device" */
    wait_write();
    outb(KB_DATA, d);
}
static u8 mouse_read(void) { return ctrl_read(); }

void mouse_init(void) {
    m_x = g_fb.width / 2;
    m_y = g_fb.height / 2;
    m_btns = m_click = m_last_btn = 0;
    pkt_i = 0;

    /* 1. disable both ports while we reconfigure */
    ctrl_write_cmd(0xAD);   /* disable keyboard port */
    ctrl_write_cmd(0xA7);   /* disable aux (mouse) port */

    /* 2. read controller config, enable aux interrupts (IRQ12) + IRQ1 */
    ctrl_write_cmd(0x20);
    u8 cfg = ctrl_read();
    cfg |= 0x02;            /* bit1: sys flag (must be set for PS/2 mode) */
    cfg |= 0x10;            /* bit4: keyboard lock ignored; safe to leave */
    cfg |= 0x20;            /* bit5: aux interrupt enable -> IRQ12 */
    cfg &= ~0x40;           /* bit6: xlate off (we use raw scancodes) */
    ctrl_write_cmd(0x60);
    ctrl_write_data(cfg);

    /* 3. enable the aux device */
    ctrl_write_cmd(0xA8);   /* enable aux port */

    /* 4. reset, set defaults, enable streaming on the mouse */
    mouse_write(0xFF); mouse_read();          /* reset */
    mouse_write(0xF6); mouse_read();          /* set defaults */
    /* Negotiate the Intellimouse (3-button + wheel) extension so the device
     * streams 4-byte packets. VirtualBox's PS/2 mouse ALWAYS runs in this
     * mode once enabled; without it the extra byte desyncs our 3-byte parser
     * and the cursor "sticks" near the top edge (the classic ceiling bug).
     * The negotiation sequence (3x set-sample-rate with rates 200,200,100)
     * flips the device into wheel mode; a subsequent Get Device ID then
     * returns 0x03. Each command replies with an ACK (0xFA) before its data,
     * so we drain ACKs explicitly. */
    mouse_write(0xF3); (void)mouse_read();    /* set sample rate, ACK */
    mouse_write(0xC8); (void)mouse_read();    /* 200, ACK */
    mouse_write(0xF3); (void)mouse_read();
    mouse_write(0xC8); (void)mouse_read();    /* 200, ACK */
    mouse_write(0xF3); (void)mouse_read();
    mouse_write(0x64); (void)mouse_read();    /* 100, ACK -> device is now wheel */
    mouse_write(0xF2); (void)mouse_read();    /* Get Device ID, ACK */
    u8 id = mouse_read();                     /* the actual ID byte */
    if (id == 0x03) pkt_len = 4;              /* Intellimouse: 4-byte packets */
    mouse_write(0xF4); (void)mouse_read();    /* enable packet streaming, ACK */

    /* 5. re-enable the keyboard port */
    ctrl_write_cmd(0xAE);   /* enable keyboard port */

    /* 6. flush any stray bytes */
    while (inb(KB_STAT) & STAT_OBF) (void)inb(KB_DATA);

    /* 7. hook IRQ12 (mouse) on the slave PIC and unmask it */
    irq_register(12, mouse_handler);
    irq_unmask(12);
    /* IRQ2 is the cascade to the slave; irq_unmask handles slave bits, but the
     * master cascade bit must also be clear for ANY slave IRQ to arrive. */
    irq_unmask(2);
}

void mouse_handler(regs_t *r) {
    (void)r;
    u8 stat = inb(KB_STAT);
    if (!(stat & STAT_OBF)) return;
    if (!(stat & STAT_AUX)) {
        /* not mouse data (keyboard echoed here in rare configs); drain */
        (void)inb(KB_DATA);
        return;
    }
    u8 b = inb(KB_DATA);
    pkt[pkt_i++] = b;

    if (pkt_i == 1) {
        /* byte 0 must have bit3 set (always-1); resync if not */
        if (!(b & 0x08)) { pkt_i = 0; return; }
    }
    if (pkt_i < pkt_len) return;

    /* full packet */
    u8 flags = pkt[0];
    int dx = (int)(s8)pkt[1];
    int dy = (int)(s8)pkt[2];
    /* dy is inverted: mouse up = negative delta, screen up = smaller y */
    m_x += dx;
    m_y -= dy;
    if (m_x < 0) m_x = 0;
    if (m_y < 0) m_y = 0;
    if (m_x >= (int)g_fb.width)  m_x = g_fb.width - 1;
    if (m_y >= (int)g_fb.height) m_y = g_fb.height - 1;

    u8 btns = flags & 0x07;
    /* edge: left button (bit0) newly pressed */
    if ((btns & 1) && !(m_last_btn & 1)) m_click = 1;
    m_last_btn = btns;
    m_btns = btns;

    pkt_i = 0;
}

int  mouse_x(void)      { return m_x; }
int  mouse_y(void)      { return m_y; }
u8   mouse_buttons(void){ return m_btns; }
int  mouse_click(void)  {
    /* returns 1 once per left-button press edge, then clears */
    if (m_click) { m_click = 0; return 1; }
    return 0;
}
