typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u64;
typedef unsigned long usize;

extern void idt_install(void);
extern void pic_remap_irq0(void);
extern void pit_init_100hz(void);

volatile u64 g_ticks;

__attribute__((used, section(".rodata.m2")))
static const char m2_banner[] = "HADEED64";

__attribute__((used, section(".rodata.m2")))
static const char m2_debug_marker[] = "LM64\n";

__attribute__((used, section(".rodata.m3")))
static const char m3_tick_prefix[] = "TICK ";

__attribute__((used, section(".rodata.m3")))
static const char m3_ok_marker[] = "M3 OK\n";

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

static void debug_write_u64(u64 value)
{
    char digits[20];
    usize n = 0UL;

    if (value == 0UL) {
        debug_putc('0');
        return;
    }
    while (value != 0UL) {
        digits[n++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }
    while (n != 0UL) {
        --n;
        debug_putc(digits[n]);
    }
}

__attribute__((noreturn)) static void finish_m3(void)
{
    debug_write(m3_ok_marker);
    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

__attribute__((noreturn)) void kmain(void)
{
    volatile u16 *const vga = (volatile u16 *)0xb8000;
    const usize base = (12UL * 80UL) + 36UL;
    u64 next_report = 100UL;
    usize i;

    for (i = 0UL; i < (sizeof m2_banner - 1UL); ++i) {
        vga[base + i] = (u16)(0x0f00U | (u8)m2_banner[i]);
    }
    debug_write(m2_debug_marker);

    g_ticks = 0UL;
    idt_install();
    pic_remap_irq0();
    pit_init_100hz();
    __asm__ volatile ("sti" : : : "memory");

    for (;;) {
        const u64 ticks = g_ticks;
        if (ticks >= next_report) {
            debug_write(m3_tick_prefix);
            debug_write_u64(next_report);
            debug_putc('\n');
            if (next_report == 300UL) {
                finish_m3();
            }
            next_report += 100UL;
        }
        __asm__ volatile ("hlt");
    }
}
