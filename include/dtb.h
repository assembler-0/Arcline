#ifndef _DTB_H
#define _DTB_H

#include <kernel/types.h>

// DTB header structure
struct dtb_header {
    uint32_t magic;          // 0xd00dfeed
    uint32_t totalsize;      // Total size of DTB
    uint32_t off_dt_struct;  // Offset to structure block
    uint32_t off_dt_strings; // Offset to strings block
    uint32_t off_mem_rsvmap; // Offset to memory reservation map
    uint32_t version;        // DTB version
    uint32_t last_comp_version; // Last compatible version
    uint32_t boot_cpuid_phys;   // Physical CPU ID of boot CPU
    uint32_t size_dt_strings;   // Size of strings block
    uint32_t size_dt_struct;    // Size of structure block
};

// DTB tokens
#define DTB_BEGIN_NODE  0x00000001
#define DTB_END_NODE    0x00000002
#define DTB_PROP        0x00000003
#define DTB_NOP         0x00000004
#define DTB_END         0x00000009

extern uint64_t dtb_ptr;
extern uint64_t boot_x0;

void dtb_init(void);
void dtb_dump_info(void);

#endif // _DTB_H