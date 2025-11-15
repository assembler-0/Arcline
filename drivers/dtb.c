#include <dtb.h>
#include <kernel/printk.h>
#include <kernel/types.h>
#include <string.h>

// Minimal map for alias -> path
struct alias_entry { const char *name; const char *path; };

static inline uint32_t be32_to_cpu(uint32_t val);
static inline uint32_t align4(uint32_t x) { return (x + 3) & ~3u; }

static int path_equals(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == '\0' && *b == '\0';
}

// Extract path or alias from stdout-path value (strip options like ":115200n8" or ",keep")
static void extract_path_token(const char *val, uint32_t len, const char **out_start, uint32_t *out_len) {
    const char *start = val;
    uint32_t l = 0;
    while (l < len && start[l] != '\0' && start[l] != ':' && start[l] != ',') {
        l++;
    }
    *out_start = start;
    *out_len = l;
}

// Convert big-endian to little-endian
static inline uint32_t be32_to_cpu(uint32_t val) {
    return ((val & 0xFF) << 24) | 
           ((val & 0xFF00) << 8) | 
           ((val & 0xFF0000) >> 8) | 
           ((val & 0xFF000000) >> 24);
}

// Search for DTB in memory
static uint64_t dtb_search(void) {
    // Common QEMU virt DTB locations to try
    static const uint64_t locations[5] = {
        0x48000000,  // 128MB from RAM start
        0x7ff00000,  // End of 1GB - 1MB
        0x44000000,  // 64MB from RAM start
        0x50000000,  // 256MB from RAM start
        0
    };
    
    // Try fixed locations first
    for (int i = 0; locations[i] != 0; i++) {
        uint32_t *ptr = (uint32_t *)locations[i];
        printk("DTB: Checking %p = %x\n", (void*)locations[i], *ptr);
        
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return locations[i];
        }
    }
    
    // Scan memory from kernel end
    extern char _kernel_end[];
    uint64_t start = ((uint64_t)_kernel_end + 0xfffff) & ~0xfffff; // Align to 1MB
    uint64_t end = 0x50000000; // Search up to 256MB from RAM start
    
    printk("DTB: Scanning from %p to %p\n", (void*)start, (void*)end);
    
    for (uint64_t addr = start; addr < end; addr += 0x1000) { // 4KB steps
        uint32_t *ptr = (uint32_t *)addr;
        if (be32_to_cpu(*ptr) == 0xd00dfeed) {
            return addr;
        }
    }
    
    return 0;
}

void dtb_init(void) {
    printk("DTB: boot_x0 = %p, dtb_ptr = %p\n", (void*)boot_x0, (void*)dtb_ptr);
    
    // Try using boot_x0 if dtb_ptr is 0
    uint64_t dtb_addr = dtb_ptr ? dtb_ptr : boot_x0;
    
    // If still no DTB, search for it
    if (!dtb_addr) {
        printk("DTB: Searching for DTB in memory...\n");
        dtb_addr = dtb_search();
    }
    
    if (!dtb_addr) {
        printk("DTB: No DTB found\n");
        return;
    }
    
    // Update dtb_ptr for other functions
    dtb_ptr = dtb_addr;
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_addr;
    
    // Check magic number
    if (be32_to_cpu(hdr->magic) != 0xd00dfeed) {
        printk("DTB: Invalid magic number: %x\n", be32_to_cpu(hdr->magic));
        return;
    }
    
    printk("DTB: Found valid device tree at %p\n", (void*)dtb_addr);
}

void dtb_dump_info(void) {
    if (!dtb_ptr) {
        printk("DTB: No DTB available\n");
        return;
    }
    
    struct dtb_header *hdr = (struct dtb_header *)dtb_ptr;
    
    printk("DTB Info:\n");
    printk("  Magic: %x\n", be32_to_cpu(hdr->magic));
    printk("  Total size: %x\n", be32_to_cpu(hdr->totalsize));
    printk("  Version: %x\n", be32_to_cpu(hdr->version));
}

struct dtb_header* dtb_get() {
    return (struct dtb_header *)dtb_ptr;
}

int dtb_get_stdout_uart_base(uint64_t *out_base) {
    if (!dtb_ptr || !out_base) return -1;

    struct dtb_header *hdr = (struct dtb_header *)dtb_ptr;
    if (be32_to_cpu(hdr->magic) != 0xd00dfeed) {
        return -1;
    }

    uint8_t *fdt = (uint8_t *)hdr;
    uint32_t off_struct = be32_to_cpu(hdr->off_dt_struct);
    uint32_t off_strings = be32_to_cpu(hdr->off_dt_strings);

    const char *strings = (const char *)(fdt + off_strings);
    const char *stdout_token = NULL; uint32_t stdout_token_len = 0;

    // simple alias table (we'll collect up to a few entries)
    struct alias_entry aliases[16];
    int alias_count = 0;

    // First pass: collect stdout-path and aliases
    int depth = 0;
    const char *chosen_path = "/chosen";
    const char *aliases_path = "/aliases";

    uint32_t p = off_struct;
    while (1) {
        uint32_t token = be32_to_cpu(*(uint32_t *)(fdt + p));
        p += 4;
        if (token == DTB_BEGIN_NODE) {
            const char *current_path_stack[16];
            const char *name = (const char *)(fdt + p);
            size_t name_len = 0; while (name[name_len] != '\0') name_len++;
            p += align4((uint32_t)(name_len + 1));

            // build a simple absolute path by stacking names
            if (depth < 16) current_path_stack[depth++] = name;

            // Determine if we are inside /chosen or /aliases
            int in_chosen = 0, in_aliases = 0;
            if (depth >= 1) {
                // Reconstruct minimal check: at depth 1, name should be "" (root)
                // At depth 2, name[1] is first-level node, so simple compare
                if (depth == 2 && name) {
                    if (strncmp(name, "chosen", 6) == 0) in_chosen = 1;
                    if (strncmp(name, "aliases", 7) == 0) in_aliases = 1;
                }
            }

            // Read properties until END_NODE
            while (1) {
                uint32_t ptok = be32_to_cpu(*(uint32_t *)(fdt + p));
                p += 4;
                if (ptok == DTB_PROP) {
                    const int MAX_ALIASES = 16;
                    uint32_t len = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
                    uint32_t nameoff = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
                    const char *pname = strings + nameoff;
                    const char *pdata = (const char *)(fdt + p);

                    if (in_chosen && (strncmp(pname, "stdout-path", 11) == 0 || strncmp(pname, "stdout", 6) == 0)) {
                        extract_path_token(pdata, len, &stdout_token, &stdout_token_len);
                    } else if (in_aliases && alias_count < MAX_ALIASES) {
                        // pdata is a NUL-terminated path string
                        aliases[alias_count].name = pname; // e.g., "serial0"
                        aliases[alias_count].path = pdata; // e.g., "/soc/serial@..."
                        alias_count++;
                    }

                    p += align4(len);
                } else if (ptok == DTB_END_NODE) {
                    depth--;
                    break;
                } else if (ptok == DTB_NOP) {
                    // nothing
                } else if (ptok == DTB_END) {
                    // reached end unexpectedly
                    goto pass1_done;
                } else if (ptok == DTB_BEGIN_NODE) {
                    // nested node under /chosen or /aliases; handle in outer loop
                    // back up 4 to let outer loop process this token as a new node
                    p -= 4;
                    break;
                } else {
                    // Unknown token; bail out
                    goto pass1_done;
                }
            }
        } else if (token == DTB_END_NODE) {
            if (depth > 0) depth--;
        } else if (token == DTB_PROP) {
            uint32_t len = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
            p += 4; // nameoff
            p += align4(len);
        } else if (token == DTB_NOP) {
            // skip
        } else if (token == DTB_END) {
            break;
        } else {
            break;
        }
    }
pass1_done:

    if (!stdout_token || stdout_token_len == 0) {
        return -1;
    }

    // Resolve alias if needed
    char resolved_path_buf[256];
    const char *target_path = NULL;
    if (stdout_token[0] == '/') {
        // copy path
        uint32_t n = (stdout_token_len < sizeof(resolved_path_buf)-1) ? stdout_token_len : (uint32_t)sizeof(resolved_path_buf)-1;
        for (uint32_t i = 0; i < n; ++i) resolved_path_buf[i] = stdout_token[i];
        resolved_path_buf[n] = '\0';
        target_path = resolved_path_buf;
    } else {
        // alias like "serial0"
        for (int i = 0; i < alias_count; ++i) {
            const char *an = aliases[i].name;
            // alias name is the property name; ensure full match
            size_t j = 0; while (an[j] && an[j] != '\0' && stdout_token[j] && j < stdout_token_len && an[j] == stdout_token[j]) j++;
            if (j == stdout_token_len && an[j] == '\0') {
                // copy alias path
                const char *ap = aliases[i].path;
                size_t k = 0; while (ap[k] && k < sizeof(resolved_path_buf)-1) { resolved_path_buf[k] = ap[k]; k++; }
                resolved_path_buf[k] = '\0';
                target_path = resolved_path_buf;
                break;
            }
        }
        if (!target_path) return -1;
    }

    // Second pass: find node by absolute path and read its "reg"
    p = off_struct; depth = 0;
    // We'll reconstruct a path progressively (limited implementation).
    char current_path[256]; current_path[0] = '/'; current_path[1] = '\0';

    while (1) {
        uint32_t token = be32_to_cpu(*(uint32_t *)(fdt + p));
        p += 4;
        if (token == DTB_BEGIN_NODE) {
            const char *name = (const char *)(fdt + p);
            size_t name_len = 0; while (name[name_len] != '\0') name_len++;
            p += align4((uint32_t)(name_len + 1));

            // Update current_path (append '/' + name)
            if (!(depth == 0 && name_len == 0)) { // root has empty name
                size_t cur_len = 0; while (current_path[cur_len] != '\0') cur_len++;
                if (cur_len > 1) current_path[cur_len++] = '/';
                for (size_t i = 0; i < name_len && cur_len < sizeof(current_path)-1; ++i) current_path[cur_len++] = name[i];
                current_path[cur_len] = '\0';
            }
            depth++;

            int at_target = target_path && path_equals(current_path, target_path);
            while (1) {
                uint32_t ptok = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
                if (ptok == DTB_PROP) {
                    uint32_t len = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
                    uint32_t nameoff = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
                    const char *pname = strings + nameoff;
                    const uint8_t *pdata = (const uint8_t *)(fdt + p);

                    if (at_target && strncmp(pname, "reg", 3) == 0) {
                        uint64_t base = 0;
                        if (len >= 8) {
                            uint32_t hi = be32_to_cpu(*(const uint32_t *)(pdata + 0));
                            uint32_t lo = be32_to_cpu(*(const uint32_t *)(pdata + 4));
                            base = (((uint64_t)hi) << 32) | lo;
                        } else if (len >= 4) {
                            base = be32_to_cpu(*(const uint32_t *)(pdata + 0));
                        } else {
                            return -1;
                        }
                        *out_base = base;
                        return 0;
                    }
                    p += align4(len);
                } else if (ptok == DTB_END_NODE) {
                    // Pop path component
                    // Reset to parent path by truncating to last '/'
                    size_t cur_len = 0; while (current_path[cur_len] != '\0') cur_len++;
                    while (cur_len > 1 && current_path[cur_len-1] != '/') cur_len--;
                    if (cur_len > 1) current_path[cur_len-1] = '\0'; else current_path[1] = '\0';
                    depth--;
                    break;
                } else if (ptok == DTB_NOP) {
                    // skip
                } else if (ptok == DTB_BEGIN_NODE) {
                    p -= 4; // let outer handle
                    break;
                } else if (ptok == DTB_END) {
                    return -1;
                } else {
                    return -1;
                }
            }
        } else if (token == DTB_PROP) {
            uint32_t len = be32_to_cpu(*(uint32_t *)(fdt + p)); p += 4;
            p += 4; // nameoff
            p += align4(len);
        } else if (token == DTB_END_NODE) {
            // Pop at root? ignore
        } else if (token == DTB_NOP) {
        } else if (token == DTB_END) {
            break;
        } else {
            break;
        }
    }

    return -1;
}