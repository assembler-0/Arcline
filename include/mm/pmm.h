#ifndef ARCLINE_MM_PMM_H
#define ARCLINE_MM_PMM_H

#include <kernel/types.h>
#include <stddef.h>
#include <stdint.h>

#define PMM_PAGE_SIZE 4096ULL

void pmm_init_from_dtb(void);

void* pmm_alloc_pages(size_t count);
void* pmm_alloc_page(void);
void  pmm_free_pages(void* addr, size_t count);
void  pmm_free_page(void* addr);

size_t pmm_total_pages(void);
size_t pmm_free_pages_count(void);

#endif // ARCLINE_MM_PMM_H
