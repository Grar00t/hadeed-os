typedef unsigned char u8;

void pic_remap_irq0(void)
{
    __asm__ volatile (
        "movb $0x11, %%al\n\t"
        "outb %%al, $0x20\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x11, %%al\n\t"
        "outb %%al, $0xa0\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x20, %%al\n\t"
        "outb %%al, $0x21\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x28, %%al\n\t"
        "outb %%al, $0xa1\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x04, %%al\n\t"
        "outb %%al, $0x21\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x02, %%al\n\t"
        "outb %%al, $0xa1\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x01, %%al\n\t"
        "outb %%al, $0x21\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0x01, %%al\n\t"
        "outb %%al, $0xa1\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0xfe, %%al\n\t"
        "outb %%al, $0x21\n\t"
        "outb %%al, $0x80\n\t"
        "movb $0xff, %%al\n\t"
        "outb %%al, $0xa1\n\t"
        "outb %%al, $0x80\n\t"
        : : : "rax", "memory");
}

void pic_send_eoi(u8 irq)
{
    if (irq >= 8U) {
        __asm__ volatile (
            "movb $0x20, %%al\n\t"
            "outb %%al, $0xa0\n\t"
            : : : "rax", "memory");
    }
    __asm__ volatile (
        "movb $0x20, %%al\n\t"
        "outb %%al, $0x20\n\t"
        : : : "rax", "memory");
}
