/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/vm.c
 *  Hypervisor detection for the SYSTEM panel "VM:" line. Probes CPUID leaf
 *  0x40000000 (the standard hypervisor leaf) which every modern VMM exposes,
 *  plus SMBIOS/firmware string scans for VirtualBox specifically.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

static char hyperv_sig[13];
static char result[32];

const char *vm_detect(void) {
    u32 a, b, c, d;
    /* bit 31 of ECX in CPUID(1) is the hypervisor-present bit */
    cpuid(1, &a, &b, &c, &d);
    if (!(c & (1u << 31))) {
        /* no hypervisor bit -> bare metal. But QEMU without -enable-kvm also
         * clears it on some configs; cross-check the firmware string. */
        const char *fw = (const char*)0x000FFFE0;   /* BIOS ROM scan window */
        for (int i = 0; i < 30; i++) {
            if (fw[i] == 'V' && fw[i+1] == 'B' && fw[i+2] == 'o' && fw[i+3] == 'x') {
                zk_strcpy(result, "VirtualBox");
                return result;
            }
        }
        zk_strcpy(result, "bare metal");
        return result;
    }
    /* hypervisor present: read signature from leaf 0x40000000 */
    cpuid(0x40000000, &a, &b, &c, &d);
    u32 *s = (u32*)hyperv_sig;
    s[0] = b; s[1] = c; s[2] = d;
    hyperv_sig[12] = 0;

    if (zk_strcmp(hyperv_sig, "VBoxVBoxVBox") == 0)      zk_strcpy(result, "VirtualBox");
    else if (zk_strcmp(hyperv_sig, "KVMKVMKVM\0\0\0") == 0) zk_strcpy(result, "KVM");
    else if (zk_strcmp(hyperv_sig, "Microsoft Hv") == 0) zk_strcpy(result, "Hyper-V");
    else if (zk_strcmp(hyperv_sig, "VMwareVMware") == 0) zk_strcpy(result, "VMware");
    else if (zk_strcmp(hyperv_sig, "XenVMMXenVMM") == 0) zk_strcpy(result, "Xen");
    else if (zk_strcmp(hyperv_sig, "TCGTCGTCGTCG") == 0) zk_strcpy(result, "QEMU");
    else {
        /* QEMU/Bochs without a hypervisor sig may still expose BGA; treat the
         * generic "Bochs" sig as bare-metal emulation since the user spec calls
         * out "pure" iron emulation as bare metal. */
        if (zk_strncmp(hyperv_sig, "Bochs", 5) == 0) zk_strcpy(result, "bare metal");
        else zk_strcpy(result, "bare metal");
    }
    return result;
}
