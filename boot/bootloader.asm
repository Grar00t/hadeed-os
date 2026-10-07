/* Hadeed M2 sector-0 loader. GNU as syntax, BIOS loads at 0000:7c00. */
.code16
.section .text
.global start

.set STAGE1_SECTORS, 8
.set STAGE1_LOAD, 0x8000

start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x7c00, %sp
    cld
    movb %dl, boot_drive

    movw $stage1_dap, %si
    movb $0x42, %ah
    int $0x13
    jc disk_error

    movb boot_drive, %dl
    ljmp $0x0000, $STAGE1_LOAD

disk_error:
    movb $'B', %al
    outb %al, $0xe9
    movb $'\n', %al
    outb %al, $0xe9
1:
    hlt
    jmp 1b

.align 4
stage1_dap:
    .byte 0x10, 0x00
    .word STAGE1_SECTORS
    .word STAGE1_LOAD
    .word 0x0000
    .quad 1

boot_drive:
    .byte 0

.fill 510 - (. - start), 1, 0
.word 0xaa55
