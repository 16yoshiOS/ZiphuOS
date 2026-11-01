/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/keyboard.c
 *  IRQ1 PS/2 keyboard driver with the "Shift inversion" rule:
 *  when Shift is held/active, digits become their symbol pair (1->! ...) and
 *  ';' becomes ':'.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

#define KB_DATA  0x60
#define KB_STAT  0x64
#define KB_CMD   0x64

#define BUF_SZ 256
static volatile u8  ring[BUF_SZ];
static volatile u32 ring_head;
static volatile u32 ring_tail;

static volatile int shift_state;     /* 1 if either shift is down */

/* wait until the 8042 input buffer is empty so we can write a command/data
 * byte to the controller without dropping it. */
static void kb_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(KB_STAT) & 0x02)) return;
    }
}

/* set 1 scan code -> ASCII for the unshifted base layer */
static const u8 set1_base[128] = {
    0, 0x1B, '1','2','3','4','5','6','7','8','9','0','-','=', 0x08,
    0x09, 'q','w','e','r','t','y','u','i','o','p','[',']', 0x0D,
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*',
    0, ' ', 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,';',0,'/',0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

/* shifted layer: digits -> symbols, punctuation flipped, letters -> upper */
static const u8 set1_shift[128] = {
    0, 0x1B, '!','@','#','$','%','^','&','*','(',')','_','+', 0x08,
    0x09, 'Q','W','E','R','T','Y','U','I','O','P','{','}', 0x0D,
    0, 'A','S','D','F','G','H','J','K','L',':','"','~', 0,
    '|','Z','X','C','V','B','N','M','<','>','?', 0, '*',
    0, ' ', 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,':',0,'?',0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void push(u8 ch) {
    u32 next = (ring_head + 1) % BUF_SZ;
    if (next == ring_tail) return;   /* drop on overflow */
    ring[ring_head] = ch;
    ring_head = next;
}

void keyboard_init(void) {
    ring_head = ring_tail = 0;
    shift_state = 0;
    /* flush any pending bytes from the controller output buffer */
    while (inb(KB_STAT) & 0x01) (void)inb(KB_DATA);

    /* Force scan-code set 1 (the XT set our tables expect). QEMU and most
     * PS/2 controllers default to set 2 (AT set, 0xF0-prefixed breaks); our
     * set1_base/set1_shift tables are indexed by set-1 codes, so without this
     * every key maps to the wrong glyph. Command 0xF0 = "select set", data
     * 0x01 = set 1. */
    kb_wait_write(); outb(KB_DATA, 0xF0);
    kb_wait_write(); outb(KB_DATA, 0x01);
    /* drain the ACK (0xFA) the keyboard replies with */
    for (int i = 0; i < 1000; i++) {
        if (inb(KB_STAT) & 0x01) { (void)inb(KB_DATA); break; }
    }

    irq_register(1, keyboard_handler);
    irq_unmask(1);
}

void keyboard_handler(regs_t *r) {
    (void)r;
    u8 sc = inb(KB_DATA);
    if (sc & 0x80) {
        /* key release */
        u8 brk = sc & 0x7F;
        if (brk == 0x2A || brk == 0x36) shift_state = 0;
        return;
    }
    /* key press */
    if (sc == 0x2A || sc == 0x36) { shift_state = 1; return; }

    if (sc == 0x3A) return;   /* caps lock: ignored, shift rules absolute */
    if (sc == 0x1D) return;   /* ctrl */
    if (sc == 0x38) return;   /* alt */

    u8 ascii;
    if (shift_state) ascii = set1_shift[sc];
    else             ascii = set1_base[sc];

    /* Hard Shift-inversion rule (the user's spec): even if our table already
     * covers it, enforce the exact pairings explicitly so the behaviour is
     * unambiguous and survives table edits. */
    if (shift_state) {
        if (ascii == '1') ascii = '!';
        else if (ascii == ';') ascii = ':';
    }

    if (ascii) push(ascii);
}

int kb_shift_active(void) { return shift_state; }

int kb_getchar(void) {
    if (ring_head == ring_tail) return -1;
    u8 ch = ring[ring_tail];
    ring_tail = (ring_tail + 1) % BUF_SZ;
    return ch;
}
