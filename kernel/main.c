/* Hadeed kernel stage skeleton. Freestanding: VGA memory only, no libc. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u64;

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static u16 row, col;

static void putc(char c) {
    if (c == '\n') { col = 0; if (++row == 25) row = 0; return; }
    vga[row * 80 + col] = (u16)0x0F00 | (u8)c;
    if (++col == 80) { col = 0; if (++row == 25) row = 0; }
}
static void puts(const char *s) { while (*s) putc(*s++); }
void kmain(void) {
    puts("Hadeed kernel: long mode entry reached\n");
    for (;;) __asm__ volatile ("hlt");
}
