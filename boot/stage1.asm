/* Hadeed M2 stage1: load flat kernel, enable A20, build identity paging, enter IA-32e. */
.code16
.section .text
.global stage1

.set PML4_ADDR, 0x9000
.set PDPT_ADDR, 0xa000
.set PD_ADDR,   0xb000
.set KERNEL_BOUNCE, 0x10000
.set KERNEL_PHYS,   0x100000
.set KERNEL_LBA,    9

stage1:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    movb %dl, boot_drive

    movw $kernel_dap, %si
    movb $0x42, %ah
    int $0x13
    jc disk_error

    /* Fast A20 gate. QEMU pc chipset implements port 0x92. */
    inb $0x92, %al
    orb $0x02, %al
    andb $0xfe, %al
    outb %al, $0x92

    /* Clear PML4, PDPT, PD. */
    xorw %ax, %ax
    movw %ax, %es
    movw $PML4_ADDR, %di
    movw $6144, %cx
    rep stosw

    /* Identity-map 0..2 MiB using one 2 MiB page. */
    movl $(PDPT_ADDR | 0x003), PML4_ADDR
    movl $0, PML4_ADDR + 4
    movl $(PD_ADDR | 0x003), PDPT_ADDR
    movl $0, PDPT_ADDR + 4
    movl $0x00000083, PD_ADDR
    movl $0, PD_ADDR + 4

    lgdt gdt_descriptor

    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4

    movl $PML4_ADDR, %eax
    movl %eax, %cr3

    movl $0xc0000080, %ecx
    rdmsr
    orl $0x00000100, %eax
    wrmsr

    movl %cr0, %eax
    orl $0x80000001, %eax
    movl %eax, %cr0

    ljmpl $0x18, $long_mode

disk_error:
    movb $'D', %al
    outb %al, $0xe9
    movb $'\n', %al
    outb %al, $0xe9
1:
    hlt
    jmp 1b

.align 8
gdt:
    .quad 0x0000000000000000
    .quad 0x00cf9a000000ffff
    .quad 0x00cf92000000ffff
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt - 1
    .long gdt

.align 4
kernel_dap:
    .byte 0x10, 0x00
    .word KERNEL_SECTORS
    .word 0x0000
    .word 0x1000
    .quad KERNEL_LBA

boot_drive:
    .byte 0

.code64
long_mode:
    movw $0x20, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movq $0x70000, %rsp
    xorq %rbp, %rbp
    cld

    movq $KERNEL_BOUNCE, %rsi
    movq $KERNEL_PHYS, %rdi
    movl $(KERNEL_SECTORS * 512), %ecx
    rep movsb

    movq $KERNEL_PHYS, %rax
    jmp *%rax
