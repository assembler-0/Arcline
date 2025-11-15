/*
 * Arcline Kernel Linker Script
 * AArch64 Version
 */

ENTRY(_start)

/*
 * Make the kernel load address configurable.
 * Default for QEMU aarch64 -kernel on virt is 0x40080000.
 * Can be overridden by the linker with: -Wl,-defsym=KERNEL_LOAD_ADDR=0xXXXXXXXX
 */
PROVIDE(KERNEL_LOAD_ADDR = 0x40080000);

SECTIONS
{
    /* Use configurable load address (defaults to QEMU virt) */
    . = KERNEL_LOAD_ADDR;
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
