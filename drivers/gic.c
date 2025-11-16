#include <drivers/gic.h>
#include <dtb.h>
#include <kernel/printk.h>

// GICv2 registers
#define GICD_CTLR 0x000
#define GICD_TYPER 0x004
#define GICD_ISENABLER 0x100
#define GICD_ICENABLER 0x180
#define GICD_IPRIORITYR 0x400
#define GICD_ITARGETSR 0x800
#define GICD_ICFGR 0xC00

#define GICC_CTLR 0x000
#define GICC_PMR 0x004
#define GICC_IAR 0x00C
#define GICC_EOIR 0x010

static volatile uint32_t *gicd_base = NULL;
static volatile uint32_t *gicc_base = NULL;
static int gic_version = 0;

// static inline uint32_t gicd_read(uint32_t offset) {
//     return gicd_base[offset / 4];
// }

static inline void gicd_write(uint32_t offset, uint32_t val) {
    gicd_base[offset / 4] = val;
}

static inline uint32_t gicc_read(uint32_t offset) {
    return gicc_base[offset / 4];
}

static inline void gicc_write(uint32_t offset, uint32_t val) {
    gicc_base[offset / 4] = val;
}

// GICv3 system registers
static inline uint32_t gicv3_read_icc_iar1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, icc_iar1_el1" : "=r"(val));
    return (uint32_t)val;
}

static inline void gicv3_write_icc_eoir1(uint32_t val) {
    __asm__ volatile("msr icc_eoir1_el1, %0" ::"r"((uint64_t)val));
}

static inline void gicv3_write_icc_pmr(uint32_t val) {
    __asm__ volatile("msr icc_pmr_el1, %0" ::"r"((uint64_t)val));
}

static inline void gicv3_write_icc_igrpen1(uint32_t val) {
    __asm__ volatile("msr icc_igrpen1_el1, %0" ::"r"((uint64_t)val));
}

static int gic_detect_version(void) {
    struct dtb_header *hdr = dtb_get();
    if (!hdr)
        return 2;

    const uint8_t *fdt = (const uint8_t *)hdr;
    uint32_t off_struct = __builtin_bswap32(hdr->off_dt_struct);
    uint32_t off_strings = __builtin_bswap32(hdr->off_dt_strings);
    const char *strings = (const char *)(fdt + off_strings);
    uint32_t p = off_struct;

    while (1) {
        uint32_t token = __builtin_bswap32(*(const uint32_t *)(fdt + p));
        p += 4;

        if (token == 1) { // BEGIN_NODE
            const char *name = (const char *)(fdt + p);
            while (*name)
                name++;
            p = (p + (name - (const char *)(fdt + p)) + 4) & ~3;
        } else if (token == 3) { // PROP
            uint32_t len = __builtin_bswap32(*(const uint32_t *)(fdt + p));
            p += 4;
            uint32_t nameoff = __builtin_bswap32(*(const uint32_t *)(fdt + p));
            p += 4;
            const char *pname = strings + nameoff;

            if (pname[0] == 'c' && pname[1] == 'o' && pname[2] == 'm' &&
                pname[3] == 'p' && pname[4] == 'a' && pname[5] == 't' &&
                pname[6] == 'i' && pname[7] == 'b' && pname[8] == 'l' &&
                pname[9] == 'e' && pname[10] == '\0') {
                const char *compat = (const char *)(fdt + p);
                if (compat[0] == 'a' && compat[1] == 'r' && compat[2] == 'm' &&
                    compat[3] == ',' && compat[4] == 'g' && compat[5] == 'i' &&
                    compat[6] == 'c' && compat[7] == '-' && compat[8] == 'v' &&
                    compat[9] == '3') {
                    return 3;
                }
            }
            p = (p + len + 3) & ~3;
        } else if (token == 2) { // END_NODE
        } else if (token == 9) { // END
            break;
        } else {
            break;
        }
    }
    return 2;
}

void gic_init(void) {
    gic_version = gic_detect_version();

    if (gic_version == 3) {
        printk("GIC: Detected GICv3\n");
        gicd_base = (volatile uint32_t *)0x08000000ULL;

        gicd_write(GICD_CTLR, 0);
        gicd_write(GICD_CTLR, 0x37);

        gicv3_write_icc_pmr(0xFF);
        gicv3_write_icc_igrpen1(1);

        __asm__ volatile("msr daifclr, #2" ::: "memory");
    } else {
        printk("GIC: Detected GICv2\n");
        gicd_base = (volatile uint32_t *)0x08000000ULL;
        gicc_base = (volatile uint32_t *)0x08010000ULL;

        gicd_write(GICD_CTLR, 0);
        for (int i = 0; i < 32; i++) {
            gicd_write(GICD_ICENABLER + i * 4, 0xFFFFFFFF);
        }
        gicd_write(GICD_CTLR, 1);

        gicc_write(GICC_PMR, 0xFF);
        gicc_write(GICC_CTLR, 1);

        __asm__ volatile("msr daifclr, #2" ::: "memory");
    }

    printk("GIC: initialized\n");
}

void gic_enable_irq(int irq) {
    if (irq < 0 || irq >= 1020)
        return;
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ISENABLER + reg * 4, 1U << bit);
}

void gic_disable_irq(int irq) {
    if (irq < 0 || irq >= 1020)
        return;
    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;
    gicd_write(GICD_ICENABLER + reg * 4, 1U << bit);
}

uint32_t gic_ack_irq(void) {
    if (gic_version == 3) {
        return gicv3_read_icc_iar1();
    } else {
        return gicc_read(GICC_IAR);
    }
}

void gic_eoi_irq(uint32_t irq) {
    if (gic_version == 3) {
        gicv3_write_icc_eoir1(irq);
    } else {
        gicc_write(GICC_EOIR, irq);
    }
}
