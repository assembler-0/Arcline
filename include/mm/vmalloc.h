#ifndef ARCLINE_MM_VMALLOC_H
#define ARCLINE_MM_VMALLOC_H

#include <stdint.h>

void* vmalloc(uint64_t size);
void vfree(void *ptr, uint64_t size);
void vmalloc_stats(void);

#endif // ARCLINE_MM_VMALLOC_H
