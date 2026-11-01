/* =============================================================================
 *  Ziphu.OS 1.0  -  OpenZK  -  src/cpuid.c
 *  CPU vendor + brand string detection. Maps the 12-char vendor id to a short
 *  label for the SYSTEM panel and decodes the brand string for the model line.
 * ========================================================================== */
#include "zk_types.h"
#include "zk_port.h"
#include "zk_util.h"
#include "zk_core.h"

static char vendor_str[13];
static char brand_str[49];
static char short_vendor[16];

void cpuid_detect(void) {
    u32 a, b, c, d;
    /* leaf 0: max leaf + vendor id */
    cpuid(0, &a, &b, &c, &d);
    u32 *vs = (u32*)vendor_str;
    vs[0] = b; vs[1] = d; vs[2] = c;
    vendor_str[12] = 0;

    if (zk_strcmp(vendor_str, "GenuineIntel") == 0)      zk_strcpy(short_vendor, "Intel");
    else if (zk_strcmp(vendor_str, "AuthenticAMD") == 0) zk_strcpy(short_vendor, "AMD");
    else if (zk_strcmp(vendor_str, "CentaurHauls") == 0) zk_strcpy(short_vendor, "VIA");
    else if (zk_strcmp(vendor_str, "GenuineTMx86") == 0) zk_strcpy(short_vendor, "Transmeta");
    else if (zk_strcmp(vendor_str, "VIA VIA VIA ") == 0) zk_strcpy(short_vendor, "VIA");
    else zk_strcpy(short_vendor, "unknown");

    /* brand string leaves 0x80000002..0x80000004 (if supported) */
    u32 max_ext = 0;
    cpuid(0x80000000, &max_ext, &b, &c, &d);
    if (max_ext >= 0x80000004) {
        u32 *bs = (u32*)brand_str;
        u32 ea, eb, ec, ed;
        cpuid(0x80000002, &ea, &eb, &ec, &ed); bs[0]=ea; bs[1]=eb; bs[2]=ec; bs[3]=ed;
        cpuid(0x80000003, &ea, &eb, &ec, &ed); bs[4]=ea; bs[5]=eb; bs[6]=ec; bs[7]=ed;
        cpuid(0x80000004, &ea, &eb, &ec, &ed); bs[8]=ea; bs[9]=eb; bs[10]=ec; bs[11]=ed;
        brand_str[48] = 0;
        /* trim leading spaces */
        char *p = brand_str;
        while (*p == ' ') p++;
        if (p != brand_str) {
            char tmp[49]; zk_strcpy(tmp, p); zk_strcpy(brand_str, tmp);
        }
        if (brand_str[0] == 0) zk_strcpy(brand_str, "unknown model");
    } else {
        /* no brand string: synthesise from family/model/signature */
        cpuid(1, &a, &b, &c, &d);
        u32 family = (a >> 8) & 0xF;
        u32 model  = (a >> 4) & 0xF;
        char buf[48];
        zk_sprintf(buf, "%s family %d model %d", short_vendor, family, model);
        zk_strcpy(brand_str, buf);
    }
}

const char *cpuid_vendor_short(void) { return short_vendor; }
const char *cpuid_model_string(void) {
    if (brand_str[0] == 0) return "unknown model";
    return brand_str;
}
