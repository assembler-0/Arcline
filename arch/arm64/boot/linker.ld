/*
 * Arcline Kernel Linker Script
 * AArch64 Version
 */

ENTRY(_start)

SECTIONS
{
    /* QEMU virt loads -kernel at 0x40080000 on AArch64 */
    . = 0x40080000;
    _kernel_start = .;

    .text : ALIGN(4K)
    {
        KEEP(*(.text.boot)) /* Keep the entry point first */
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
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    _kernel_end = .;

    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.exidx*)
    }
}
