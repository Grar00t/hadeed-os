void pit_init_100hz(void)
{
    /* Channel 0, lobyte/hibyte, mode 3, binary; 1193182 / 11932 ~= 99.9985 Hz. */
    __asm__ volatile (
        "movb $0x36, %%al\n\t"
        "outb %%al, $0x43\n\t"
        "movb $0x9c, %%al\n\t"
        "outb %%al, $0x40\n\t"
        "movb $0x2e, %%al\n\t"
        "outb %%al, $0x40\n\t"
        : : : "rax", "memory");
}
