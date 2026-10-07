/* Hadeed M2 64-bit kernel entry. GNU as syntax. */
.code64
.section .text.entry,"ax",@progbits
.global kernel_entry
.type kernel_entry, @function
.extern kmain

kernel_entry:
    cli
    movq $0x80000, %rsp
    xorq %rbp, %rbp
    call kmain
1:
    hlt
    jmp 1b
.size kernel_entry, . - kernel_entry
