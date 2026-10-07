OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x00100000;

    .text : ALIGN(16)
    {
        KEEP(*(.text.entry))
        *(.text*)
    } :text

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    } :text

    .data : ALIGN(16)
    {
        *(.data*)
    } :data

    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss*)
    } :data

    ASSERT(. <= 0x00102000, "kernel loadable sections overlap fixed IDT address")
    . = 0x00102000;
    .idt (NOLOAD) : ALIGN(16)
    {
        KEEP(*(.idt))
    } :data

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
        *(.eh_frame*)
    }
}
