OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)
SECTIONS {
  . = 0x00100000;
  .text : ALIGN(4096) { *(.text*) }
  .rodata : ALIGN(4096) { *(.rodata*) }
  .data : ALIGN(4096) { *(.data*) }
  .bss : ALIGN(4096) { *(COMMON) *(.bss*) }
}
