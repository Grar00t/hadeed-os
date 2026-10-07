OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS
{
    . = 0x00100000;
    .text : ALIGN(16) { KEEP(*(.text.entry)) *(.text*) } :text
    .rodata : ALIGN(16) { *(.rodata*) } :text
    .data : ALIGN(16) { *(.data*) } :data
    .bss : ALIGN(16) { *(COMMON) *(.bss*) } :data
    ASSERT(. <= 0x00102000, "kernel loadable sections overlap fixed IDT address")
    . = 0x00102000;
    .idt (NOLOAD) : ALIGN(16) { KEEP(*(.idt)) } :data
    . = ALIGN(4096);
    .m4meta (NOLOAD) : { *(.m4meta*) } :data
    . = ALIGN(4096);
    .m4pt (NOLOAD) : { *(.m4pt*) } :data
    __kernel_reserved_end = .;
    ASSERT(__kernel_reserved_end <= 0x00200000, "M4 metadata exceeds identity-mapped low 2 MiB")
    /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) }
}
