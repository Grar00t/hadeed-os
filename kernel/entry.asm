; Hadeed stage-1 kernel entry. NASM syntax; entered in 64-bit mode by boot/loader.
bits 64
default rel
section .text
extern kmain
global kernel_entry
kernel_entry:
    cli
    lea rsp, [stack_top]
    xor rbp, rbp
    call kmain
.hang:
    hlt
    jmp .hang
section .bss
align 16
stack_bottom: resb 16384
stack_top:
