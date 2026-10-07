typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long usize;

__attribute__((used, section(".rodata.m2")))
static const char m2_banner[] = "HADEED64";

__attribute__((used, section(".rodata.m2")))
static const char m2_debug_marker[] = "LM64\n";

static void debug_putc(char c)
{
    __asm__ volatile ("outb %0, $0xe9" : : "a" ((u8)c));
}

static void debug_write(const char *s)
{
    while (*s != '\0') {
        debug_putc(*s);
        ++s;
    }
}

__attribute__((noreturn)) void kmain(void)
{
    volatile u16 *const vga = (volatile u16 *)0xb8000;
    const usize base = (12UL * 80UL) + 36UL;
    usize i;

    for (i = 0; i < (sizeof m2_banner - 1UL); ++i) {
        vga[base + i] = (u16)(0x0f00U | (u8)m2_banner[i]);
    }

    debug_write(m2_debug_marker);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
