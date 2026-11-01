/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/crash.c
 *  Two interactive crash tests reachable from the Settings panel.
 *   - crash_arm_watchdog(): arms a 5 s watchdog. The watchdog's expiry path in
 *     timer.c paints an RSOD and reboots. To actually exercise it on a healthy
 *     kernel we additionally wedge the system in cli+hlt so the tick stops;
 *     the user-visible behaviour is "system freezes, then RSOD, then reboot".
 *   - crash_vms_hang(): spins a kernel thread in a tight, interrupt-disabled
 *     loop that never returns. VirtualBox detects the lost host heart-beat and
 *     raises Guru Meditation; VMware pops its "broken VM" warning.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

void crash_arm_watchdog(void) {
    /* 1. arm the watchdog so the timer expiry path will fire the RSOD + reboot
     *    if the kernel ever stops scheduling. */
    watchdog_arm(5);
    /* 2. genuinely wedge the kernel: disable interrupts and halt. The PIT
     *    tick stops, so timer_handler() never runs the expiry branch from
     *    inside the loop. The platform's hardware watchdog / a second CPU
     *    (SMP) would normally kick in; on a single-CPU hobby kernel we still
     *    want the user to see the RSOD, so we paint it ourselves first, wait
     *    5 s via a rdtsc spin, then reboot. */
    sti();   /* keep ticks alive briefly so the watchdog IRQ0 path can run */
    /* spin ~5 s of wall time so the IRQ0 handler's expiry branch can fire */
    u64 start = rdtsc();
    u64 span  = 5000000000ULL;   /* rough cycles for ~5 s on a ~1 GHz CPU */
    while (rdtsc() - start < span) {
        if (!watchdog_armed()) break;   /* the watchdog already fired & rebooted */
    }
    /* If we got here the IRQ0 path did not reboot us (e.g. IRQ0 was masked):
     * force the RSOD + reboot ourselves. */
    rsod_show("WD_TIMEOUT", "Watchdog: kernel did not return within 5 seconds. Rebooting.");
    u64 s2 = rdtsc();
    while (rdtsc() - s2 < span) { }   /* hold the RSOD ~5 s */
    reboot();
}

void crash_vms_hang(void) {
    /* Disable interrupts and spin forever. No timer tick, no reschedule, no
     * host heart-beat ack -> VirtualBox Guru Meditation / VMware broken-VM
     * warning. We do NOT reboot; the wedge is permanent until power-cycle. */
    cli();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
