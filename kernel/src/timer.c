/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/timer.c
 *  PIT channel-0 system tick (default 100 Hz) + watchdog countdown used by the
 *  "Crash System" crash test (arms a 5 s timer; if the system is wedged it
 *  fires an RSOD then soft-reboots).
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

#define PIT_DATA0 0x40
#define PIT_CMD   0x43

static volatile u32 ticks;
static u32 hz;
static u32 ms_per_tick;

/* watchdog state */
static volatile int  wd_armed;
static volatile u32  wd_remaining_ticks;
static u32           wd_total_ticks;

void timer_init(u32 frequency) {
    hz = frequency ? frequency : 100;
    ms_per_tick = 1000 / hz;
    u16 div = (u16)(1193182 / hz);
    if (div == 0) div = 1;
    /* channel 0, lo+hi bytes, mode 2 (rate generator) -> reliable periodic IRQ */
    outb(PIT_CMD, 0x34);
    outb(PIT_DATA0, (u8)(div & 0xFF));
    outb(PIT_DATA0, (u8)((div >> 8) & 0xFF));
    irq_register(0, timer_handler);
    irq_unmask(0);
}

void timer_handler(regs_t *r) {
    (void)r;
    ticks++;
    if (wd_armed) {
        if (wd_remaining_ticks > 0) wd_remaining_ticks--;
        /* when the watchdog expires and the system is NOT already wedged, the
         * kernel is still ticking so this branch runs: it means the test was
         * cancelled or the system stayed alive. But the Crash System button
         * is supposed to WEDGE the kernel so the tick stops firing. In QEMU we
         * also wedge via cli+hlt, so the real reboot path here is a backup. */
        if (wd_remaining_ticks == 0 && wd_armed) {
            wd_armed = 0;
            rsod_show("WD_TIMEOUT",
                      "Watchdog expired: kernel thread did not return within 5s");
            /* wait 5 more seconds of ticks then reboot */
            u32 wait = hz * 5;
            while (wait--) {
                u32 t = ticks;
                while (ticks == t) { /* spin on the still-running tick */ }
            }
            reboot();
        }
    }
}

u32 timer_ticks(void)   { return ticks; }
u32 timer_uptime_ms(void) { return ticks * ms_per_tick; }

void watchdog_arm(u32 seconds) {
    wd_total_ticks    = seconds * hz;
    wd_remaining_ticks = wd_total_ticks;
    wd_armed = 1;
}
void watchdog_disarm(void) { wd_armed = 0; }
int  watchdog_armed(void)  { return wd_armed; }
void watchdog_tick(void)   { /* placeholder kept for API symmetry */ }
