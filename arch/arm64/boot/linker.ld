/*
 * Arcline Kernel Linker Script
 * AArch64 Version
 */

ENTRY(_start)

SECTIONS
{
    /* Start at 1MB, a common physical address for kernels. */
    . = 0x100000;
    _kernel_start = .;

    .text : ALIGN(4K)
    {
        KEEP(*(.text.boot)) /* Keep the entry point */
        *(.text .text.*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(4K)
    {
        __bss_start = .; /* Symbol for clearing BSS */
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .; /* Symbol for clearing BSS */
    }

    _kernel_end = .;

    /* Discard unnecessary sections to reduce binary size. */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
