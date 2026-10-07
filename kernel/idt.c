typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef unsigned long usize;

#define IDT_ENTRIES 256UL
#define KERNEL_CS 0x18U
#define GATE_INTERRUPT 0x8eU
#define GATE_TRAP 0x8fU

struct __attribute__((packed)) idt_gate {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
};

struct __attribute__((packed)) idtr {
    u16 limit;
    u64 base;
};

struct trap_frame {
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;
    u64 vector;
    u64 error;
    u64 rip;
    u64 cs;
    u64 rflags;
};

extern void (*isr_stub_table[48])(void);
extern void isr_default(void);
extern volatile u64 g_ticks;
extern void pic_send_eoi(u8 irq);

__attribute__((used, section(".idt"), aligned(16)))
static struct idt_gate idt[IDT_ENTRIES];

static void debug_putc(char c)
{
    __asm__ volatile ("outb %0, $0xe9" : : "a" ((u8)c));
}

static void idt_set_gate(usize vector, void (*handler)(void), u8 type_attr)
{
    const u64 address = (u64)(usize)handler;
    struct idt_gate *const gate = &idt[vector];

    gate->offset_low = (u16)(address & 0xffffUL);
    gate->selector = KERNEL_CS;
    gate->ist = 0U;
    gate->type_attr = type_attr;
    gate->offset_mid = (u16)((address >> 16) & 0xffffUL);
    gate->offset_high = (u32)(address >> 32);
    gate->zero = 0U;
}

void idt_install(void)
{
    struct idtr descriptor;
    usize i;

    for (i = 0UL; i < IDT_ENTRIES; ++i) {
        idt_set_gate(i, isr_default, GATE_TRAP);
    }
    for (i = 0UL; i < 32UL; ++i) {
        idt_set_gate(i, isr_stub_table[i], GATE_TRAP);
    }
    for (i = 32UL; i < 48UL; ++i) {
        idt_set_gate(i, isr_stub_table[i], GATE_INTERRUPT);
    }

    descriptor.limit = (u16)(sizeof idt - 1UL);
    descriptor.base = (u64)(usize)&idt[0];
    __asm__ volatile ("lidt %0" : : "m" (descriptor) : "memory");
}

__attribute__((noreturn)) static void exception_halt(void)
{
    debug_putc('E');
    debug_putc('X');
    debug_putc('C');
    debug_putc('\n');
    __asm__ volatile ("cli" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void isr_dispatch(u64 vector, struct trap_frame *frame)
{
    (void)frame;
    if (vector == 32UL) {
        ++g_ticks;
        pic_send_eoi(0U);
        return;
    }
    if (vector >= 33UL && vector <= 47UL) {
        pic_send_eoi((u8)(vector - 32UL));
        return;
    }
    if (vector < 32UL) {
        exception_halt();
    }
}
