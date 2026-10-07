OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)

SECTIONS
{
    . = 0x00100000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.entry))
        *(.text*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}
