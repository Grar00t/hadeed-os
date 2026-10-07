typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef unsigned long usize;
typedef long isize;
typedef int i32;

#define SYS_READ 0L
#define SYS_WRITE 1L
#define SYS_OPEN 2L
#define SYS_CLOSE 3L
#define SYS_PREAD64 17L
#define SYS_EXIT 60L

#define IMAGE_PATH "build/hadeed.img"
#define ELF_PATH "build/kernel.elf"
#define SECTOR 512UL
#define STAGE1_LBA 1UL
#define KERNEL_LBA 9UL
#define KERNEL_PHYS 0x00100000UL
#define IDT_PHYS 0x00102000UL
#define IDT_BYTES 4096UL

struct symbol {
    u64 value;
    u64 size;
};

static u8 stage1[4096];
static u8 kernel[65536];
static u8 elf[131072];
static usize kernel_len;
static usize elf_len;
static const u8 *symtab;
static usize sym_count;
static usize sym_entsize;
static const u8 *strtab;
static usize strtab_size;

static long syscall1(long n, long a1)
{
    long r;
    __asm__ volatile ("syscall" : "=a" (r) : "a" (n), "D" (a1) : "rcx", "r11", "memory");
    return r;
}

static long syscall3(long n, long a1, long a2, long a3)
{
    long r;
    __asm__ volatile ("syscall" : "=a" (r) : "a" (n), "D" (a1), "S" (a2), "d" (a3) : "rcx", "r11", "memory");
    return r;
}

static long syscall4(long n, long a1, long a2, long a3, long a4)
{
    register long r10 __asm__("r10") = a4;
    long r;
    __asm__ volatile ("syscall" : "=a" (r) : "a" (n), "D" (a1), "S" (a2), "d" (a3), "r" (r10) : "rcx", "r11", "memory");
    return r;
}

static usize cstrlen(const char *s)
{
    usize n = 0UL;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static int streq(const char *a, const char *b)
{
    usize i = 0UL;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == b[i];
}

static void write_text(long fd, const char *s)
{
    usize left = cstrlen(s);
    while (left != 0UL) {
        const long n = syscall3(SYS_WRITE, fd, (long)s, (long)left);
        if (n <= 0L) {
            syscall1(SYS_EXIT, 119L);
            for (;;) { }
        }
        s += (usize)n;
        left -= (usize)n;
    }
}

static void write_hex(u64 value)
{
    char out[19];
    static const char digits[] = "0123456789abcdef";
    unsigned int i;
    out[0] = '0';
    out[1] = 'x';
    for (i = 0U; i < 16U; ++i) {
        const unsigned int shift = (15U - i) * 4U;
        out[2U + i] = digits[(value >> shift) & 0xfUL];
    }
    out[18] = '\0';
    write_text(1L, out);
}

static void trace(const char *event, u64 addr, u64 value, const char *src)
{
    write_text(1L, "{\"event\":\"");
    write_text(1L, event);
    write_text(1L, "\",\"addr\":\"");
    write_hex(addr);
    write_text(1L, "\",\"value\":\"");
    write_hex(value);
    write_text(1L, "\",\"src\":\"");
    write_text(1L, src);
    write_text(1L, "\"}\n");
}

__attribute__((noreturn)) static void fail(long code, const char *message)
{
    char d[4];
    unsigned long x = (unsigned long)code;
    unsigned int n = 0U;

    write_text(2L, "M3_TRACE_FAIL code=");
    if (x >= 100UL) d[n++] = (char)('0' + ((x / 100UL) % 10UL));
    if (x >= 10UL) d[n++] = (char)('0' + ((x / 10UL) % 10UL));
    d[n++] = (char)('0' + (x % 10UL));
    d[n] = '\0';
    write_text(2L, d);
    write_text(2L, " message=");
    write_text(2L, message);
    write_text(2L, "\n");
    syscall1(SYS_EXIT, code);
    for (;;) { }
}

static u16 rd16(const u8 *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static u64 rd64(const u8 *p)
{
    return (u64)rd32(p) | ((u64)rd32(p + 4) << 32);
}

static int bytes_equal(const u8 *a, const u8 *b, usize n)
{
    usize i;
    for (i = 0UL; i < n; ++i) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static isize find_bytes(const u8 *buf, usize len, const u8 *needle, usize nlen)
{
    usize i;
    if (nlen == 0UL || nlen > len) return -1;
    for (i = 0UL; i + nlen <= len; ++i) {
        if (bytes_equal(buf + i, needle, nlen)) return (isize)i;
    }
    return -1;
}

static long open_ro(const char *path)
{
    return syscall3(SYS_OPEN, (long)path, 0L, 0L);
}

static void pread_exact(long fd, u8 *buf, usize len, u64 off, long code, const char *msg)
{
    usize done = 0UL;
    while (done < len) {
        const long n = syscall4(SYS_PREAD64, fd, (long)(buf + done), (long)(len - done), (long)(off + done));
        if (n <= 0L) fail(code, msg);
        done += (usize)n;
    }
}

static usize read_file(long fd, u8 *buf, usize cap, long code, const char *msg)
{
    usize done = 0UL;
    while (done < cap) {
        const long n = syscall3(SYS_READ, fd, (long)(buf + done), (long)(cap - done));
        if (n < 0L) fail(code, msg);
        if (n == 0L) break;
        done += (usize)n;
    }
    if (done == cap) fail(code, "file too large");
    return done;
}

static void load_kernel_from_image(void)
{
    long fd;
    usize i;
    u16 sectors = 0U;

    fd = open_ro(IMAGE_PATH);
    if (fd < 0L) fail(10L, "cannot open build/hadeed.img");
    pread_exact(fd, stage1, sizeof stage1, STAGE1_LBA * SECTOR, 11L, "cannot read stage1");

    for (i = 0UL; i + 16UL <= sizeof stage1; ++i) {
        if (stage1[i] == 0x10U && stage1[i + 1UL] == 0U &&
            rd16(stage1 + i + 4UL) == 0U && rd16(stage1 + i + 6UL) == 0x1000U &&
            rd64(stage1 + i + 8UL) == KERNEL_LBA) {
            sectors = rd16(stage1 + i + 2UL);
            break;
        }
    }
    if (sectors == 0U || (usize)sectors * SECTOR > sizeof kernel) fail(12L, "kernel DAP invalid");
    kernel_len = (usize)sectors * SECTOR;
    pread_exact(fd, kernel, kernel_len, KERNEL_LBA * SECTOR, 13L, "cannot read kernel payload");
    (void)syscall1(SYS_CLOSE, fd);
    trace("m3.kernel_payload", KERNEL_LBA * SECTOR, kernel_len, "boot/stage1.asm:92-97");
}

static void load_elf_symbols(void)
{
    long fd;
    u64 shoff;
    u16 shentsize;
    u16 shnum;
    u16 i;

    fd = open_ro(ELF_PATH);
    if (fd < 0L) fail(20L, "cannot open build/kernel.elf");
    elf_len = read_file(fd, elf, sizeof elf, 21L, "cannot read kernel ELF");
    (void)syscall1(SYS_CLOSE, fd);

    if (elf_len < 64UL || elf[0] != 0x7fU || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F' ||
        elf[4] != 2U || elf[5] != 1U) fail(22L, "ELF64 header invalid");
    shoff = rd64(elf + 0x28);
    shentsize = rd16(elf + 0x3a);
    shnum = rd16(elf + 0x3c);
    if (shentsize < 64U || shoff + (u64)shentsize * shnum > elf_len) fail(23L, "ELF section table invalid");

    for (i = 0U; i < shnum; ++i) {
        const u8 *const sh = elf + (usize)shoff + (usize)i * shentsize;
        if (rd32(sh + 4UL) == 2U) {
            const u64 off = rd64(sh + 24UL);
            const u64 size = rd64(sh + 32UL);
            const u32 link = rd32(sh + 40UL);
            const u64 entsize = rd64(sh + 56UL);
            const u8 *linked;
            u64 str_off;
            u64 str_size;
            if (link >= shnum || entsize < 24UL || off + size > elf_len) fail(24L, "ELF symtab invalid");
            linked = elf + (usize)shoff + (usize)link * shentsize;
            str_off = rd64(linked + 24UL);
            str_size = rd64(linked + 32UL);
            if (str_off + str_size > elf_len) fail(25L, "ELF strtab invalid");
            symtab = elf + (usize)off;
            sym_count = (usize)(size / entsize);
            sym_entsize = (usize)entsize;
            strtab = elf + (usize)str_off;
            strtab_size = (usize)str_size;
            return;
        }
    }
    fail(26L, "ELF symtab missing");
}

static struct symbol find_symbol(const char *name, long code)
{
    usize i;
    for (i = 0UL; i < sym_count; ++i) {
        const u8 *const s = symtab + i * sym_entsize;
        const u32 name_off = rd32(s);
        if (name_off < strtab_size && streq((const char *)(strtab + name_off), name)) {
            struct symbol out;
            out.value = rd64(s + 8UL);
            out.size = rd64(s + 16UL);
            return out;
        }
    }
    fail(code, name);
}

static const u8 *kernel_ptr(u64 address, usize need, long code, const char *msg)
{
    const u64 end = address + need;
    if (address < KERNEL_PHYS || end < address || end > KERNEL_PHYS + kernel_len) fail(code, msg);
    return kernel + (usize)(address - KERNEL_PHYS);
}

static int resolve_rip_target(u64 insn, usize insn_len, usize disp_off, u64 expected)
{
    const u8 *p = kernel_ptr(insn, insn_len, 32L, "RIP-relative instruction outside kernel");
    const long disp = (long)(i32)rd32(p + disp_off);
    const u64 target = insn + insn_len + (u64)disp;
    return target == expected;
}

static isize find_direct_call(struct symbol from, u64 target, usize start)
{
    const u8 *p = kernel_ptr(from.value, (usize)from.size, 33L, "caller outside kernel");
    usize i;
    for (i = start; i + 5UL <= from.size; ++i) {
        if (p[i] == 0xe8U) {
            const long rel = (long)(i32)rd32(p + i + 1UL);
            const u64 dst = from.value + i + 5UL + (u64)rel;
            if (dst == target) return (isize)i;
        }
    }
    return -1;
}

static void verify_idt_and_stubs(void)
{
    const struct symbol idt = find_symbol("idt", 40L);
    const struct symbol install = find_symbol("idt_install", 41L);
    const struct symbol table = find_symbol("isr_stub_table", 42L);
    const u8 *install_code = kernel_ptr(install.value, (usize)install.size, 43L, "idt_install outside kernel");
    const u8 *table_bytes = kernel_ptr(table.value, (usize)table.size, 44L, "ISR table outside kernel");
    static const u8 idt_base_load[] = {0xba,0x00,0x20,0x10,0x00};
    static const u8 idt_end_cmp[] = {0x48,0x3d,0x00,0x30,0x10,0x00};
    static const u8 irq_loop_end[] = {0x48,0x83,0xf8,0x30};
    static const u8 lidt_seq[] = {0x0f,0x01,0x5c,0x24,0x06};
    unsigned int v;
    int trap_attr_seen = 0;
    int irq_attr_seen = 0;
    usize i;

    if (idt.value != IDT_PHYS || idt.size != IDT_BYTES) fail(45L, "IDT physical address/size mismatch");
    if (table.size != 48UL * 8UL) fail(46L, "ISR table size mismatch");
    if (find_bytes(install_code, (usize)install.size, idt_base_load, sizeof idt_base_load) < 0 ||
        find_bytes(install_code, (usize)install.size, idt_end_cmp, sizeof idt_end_cmp) < 0 ||
        find_bytes(install_code, (usize)install.size, irq_loop_end, sizeof irq_loop_end) < 0 ||
        find_bytes(install_code, (usize)install.size, lidt_seq, sizeof lidt_seq) < 0) {
        fail(47L, "IDT install/lidt sequence missing");
    }

    for (i = 0UL; i + 7UL <= install.size; ++i) {
        if (install_code[i] == 0x0fU && install_code[i + 1UL] == 0xb7U &&
            (install_code[i + 2UL] == 0x35U || install_code[i + 2UL] == 0x3dU)) {
            const long disp = (long)(i32)rd32(install_code + i + 3UL);
            const u64 target = install.value + i + 7UL + (u64)disp;
            const u8 *word = kernel_ptr(target, 2UL, 48L, "IDT gate attribute outside kernel");
            if (rd16(word) == 0x8f00U) trap_attr_seen = 1;
            if (rd16(word) == 0x8e00U) irq_attr_seen = 1;
        }
    }
    if (!trap_attr_seen || !irq_attr_seen) fail(49L, "IDT trap/interrupt gate attributes not referenced");
    trace("idt.physical", idt.value, idt.size, "kernel/linker.ld:36-41");
    trace("idt.gate_types", install.value, 0x0000000000008e8fUL, "kernel/idt.c:77-95");

    for (v = 32U; v <= 47U; ++v) {
        const u64 target = rd64(table_bytes + (usize)v * 8UL);
        const u8 *stub = kernel_ptr(target, 6UL, 50L, "IRQ stub target outside kernel payload");
        const u64 gate_addr = idt.value + (u64)v * 16UL;
        if (stub[0] != 0x6aU || stub[1] != 0U || stub[2] != 0x6aU || stub[3] != (u8)v ||
            (stub[4] != 0xebU && stub[4] != 0xe9U)) {
            fail(51L, "IRQ stub encoding invalid");
        }
        trace("idt.irq_gate_target", gate_addr, target, "kernel/idt.c:88-90;kernel/isr.asm:57-72,128-136");
    }
}

static void verify_pic(void)
{
    const struct symbol pic = find_symbol("pic_remap_irq0", 60L);
    static const u8 expected[] = {
        0xb0,0x11,0xe6,0x20,0xe6,0x80,
        0xb0,0x11,0xe6,0xa0,0xe6,0x80,
        0xb0,0x20,0xe6,0x21,0xe6,0x80,
        0xb0,0x28,0xe6,0xa1,0xe6,0x80,
        0xb0,0x04,0xe6,0x21,0xe6,0x80,
        0xb0,0x02,0xe6,0xa1,0xe6,0x80,
        0xb0,0x01,0xe6,0x21,0xe6,0x80,
        0xb0,0x01,0xe6,0xa1,0xe6,0x80,
        0xb0,0xfe,0xe6,0x21,0xe6,0x80,
        0xb0,0xff,0xe6,0xa1,0xe6,0x80
    };
    const u8 *code = kernel_ptr(pic.value, (usize)pic.size, 61L, "PIC function outside kernel");
    if (pic.size < sizeof expected || !bytes_equal(code, expected, sizeof expected)) fail(62L, "8259 ICW/mask sequence mismatch");
    trace("pic.icw1_icw4_masks", pic.value, 0x11202804020101feUL, "kernel/pic.c:3-37");
}

static void verify_pit(void)
{
    const struct symbol pit = find_symbol("pit_init_100hz", 70L);
    static const u8 expected[] = {0xb0,0x36,0xe6,0x43,0xb0,0x9c,0xe6,0x40,0xb0,0x2e,0xe6,0x40};
    const u8 *code = kernel_ptr(pit.value, (usize)pit.size, 71L, "PIT function outside kernel");
    if (pit.size < sizeof expected || !bytes_equal(code, expected, sizeof expected)) fail(72L, "PIT mode/divisor sequence mismatch");
    trace("pit.channel0_mode3", pit.value, 0x36UL, "kernel/pit.c:1-12");
    trace("pit.divisor_11932", pit.value + 4UL, 0x2e9cUL, "kernel/pit.c:7-10");
}

static void verify_irq0_counter(void)
{
    const struct symbol dispatch = find_symbol("isr_dispatch", 80L);
    const struct symbol ticks = find_symbol("g_ticks", 81L);
    const struct symbol eoi = find_symbol("pic_send_eoi", 82L);
    const u8 *code = kernel_ptr(dispatch.value, (usize)dispatch.size, 83L, "dispatcher outside kernel");
    static const u8 irq0_cmp[] = {0x48,0x83,0xff,0x20};
    static const u8 inc[] = {0x48,0x83,0xc0,0x01};
    usize i;
    int read_seen = 0;
    int write_seen = 0;
    int eoi_seen = 0;

    if (find_bytes(code, (usize)dispatch.size, irq0_cmp, sizeof irq0_cmp) < 0 ||
        find_bytes(code, (usize)dispatch.size, inc, sizeof inc) < 0) {
        fail(84L, "IRQ0 vector/increment path missing");
    }
    for (i = 0UL; i + 7UL <= dispatch.size; ++i) {
        if (code[i] == 0x48U && code[i + 1UL] == 0x8bU && code[i + 2UL] == 0x05U &&
            resolve_rip_target(dispatch.value + i, 7UL, 3UL, ticks.value)) read_seen = 1;
        if (code[i] == 0x48U && code[i + 1UL] == 0x89U && code[i + 2UL] == 0x05U &&
            resolve_rip_target(dispatch.value + i, 7UL, 3UL, ticks.value)) write_seen = 1;
        if ((code[i] == 0xe8U || code[i] == 0xe9U) && i + 5UL <= dispatch.size) {
            const long rel = (long)(i32)rd32(code + i + 1UL);
            const u64 target = dispatch.value + i + 5UL + (u64)rel;
            if (target == eoi.value) eoi_seen = 1;
        }
    }
    if (!read_seen || !write_seen || !eoi_seen) fail(85L, "IRQ0 counter/EOI path incomplete");
    trace("irq0.counter_increment", dispatch.value, ticks.value, "kernel/idt.c:109-115");
    trace("irq0.master_eoi", dispatch.value, eoi.value, "kernel/idt.c:112-115;kernel/pic.c:39-51");
}

static void verify_main_path(void)
{
    const struct symbol main_sym = find_symbol("kmain", 90L);
    const struct symbol ticks = find_symbol("g_ticks", 91L);
    const struct symbol install = find_symbol("idt_install", 92L);
    const struct symbol pic = find_symbol("pic_remap_irq0", 93L);
    const struct symbol pit = find_symbol("pit_init_100hz", 94L);
    const struct symbol ok = find_symbol("m3_ok_marker", 95L);
    const struct symbol tick_prefix = find_symbol("m3_tick_prefix", 96L);
    const u8 *code = kernel_ptr(main_sym.value, (usize)main_sym.size, 97L, "kmain outside kernel");
    const u8 *ok_bytes = kernel_ptr(ok.value, 7UL, 98L, "M3 OK marker outside payload");
    const u8 *prefix_bytes = kernel_ptr(tick_prefix.value, 6UL, 99L, "TICK prefix outside payload");
    static const u8 ok_expected[] = {'M','3',' ','O','K','\n','\0'};
    static const u8 prefix_expected[] = {'T','I','C','K',' ','\0'};
    static const u8 next100[] = {0xbf,0x64,0x00,0x00,0x00};
    static const u8 cmp300[] = {0x48,0x81,0xff,0x2c,0x01,0x00,0x00};
    static const u8 add100[] = {0x48,0x83,0xc7,0x64};
    isize call_idt;
    isize call_pic;
    isize call_pit;
    isize init100;
    isize cmp300_off;
    isize add100_off;
    usize i;
    usize sti_off = (usize)-1;
    int zero_seen = 0;
    int tick_read_seen = 0;
    u64 branch_target;

    if (!bytes_equal(ok_bytes, ok_expected, sizeof ok_expected) ||
        !bytes_equal(prefix_bytes, prefix_expected, sizeof prefix_expected)) fail(100L, "M3 output strings mismatch");

    for (i = 0UL; i + 11UL <= main_sym.size; ++i) {
        if (code[i] == 0x48U && code[i + 1UL] == 0xc7U && code[i + 2UL] == 0x05U &&
            rd32(code + i + 7UL) == 0U && resolve_rip_target(main_sym.value + i, 11UL, 3UL, ticks.value)) {
            zero_seen = 1;
            break;
        }
    }
    if (!zero_seen) fail(101L, "g_ticks is not initialized to zero");

    call_idt = find_direct_call(main_sym, install.value, 0UL);
    call_pic = call_idt < 0 ? -1 : find_direct_call(main_sym, pic.value, (usize)call_idt + 5UL);
    call_pit = call_pic < 0 ? -1 : find_direct_call(main_sym, pit.value, (usize)call_pic + 5UL);
    if (call_idt < 0 || call_pic < 0 || call_pit < 0) fail(102L, "IDT/PIC/PIT call sequence missing");
    for (i = (usize)call_pit + 5UL; i < main_sym.size; ++i) {
        if (code[i] == 0xfbU) {
            sti_off = i;
            break;
        }
    }
    if (sti_off == (usize)-1) fail(103L, "sti missing after PIT initialization");
    if ((usize)call_idt >= (usize)call_pic || (usize)call_pic >= (usize)call_pit || (usize)call_pit >= sti_off) {
        fail(104L, "IDT/PIC/PIT/sti order invalid");
    }
    trace("main.idt_pic_pit_sti", main_sym.value + (u64)call_idt, main_sym.value + sti_off, "kernel/main.c:77-81");

    init100 = find_bytes(code + sti_off + 1UL, (usize)main_sym.size - sti_off - 1UL, next100, sizeof next100);
    if (init100 < 0) fail(105L, "next_report 100 initialization missing");
    init100 += (isize)sti_off + 1;

    for (i = (usize)init100; i + 7UL <= main_sym.size; ++i) {
        if (code[i] == 0x48U && code[i + 1UL] == 0x8bU && code[i + 2UL] == 0x05U &&
            resolve_rip_target(main_sym.value + i, 7UL, 3UL, ticks.value)) {
            tick_read_seen = 1;
            break;
        }
    }
    if (!tick_read_seen) fail(106L, "main loop does not read initialized g_ticks");

    cmp300_off = find_bytes(code + (usize)init100, (usize)main_sym.size - (usize)init100, cmp300, sizeof cmp300);
    add100_off = find_bytes(code + (usize)init100, (usize)main_sym.size - (usize)init100, add100, sizeof add100);
    if (cmp300_off < 0 || add100_off < 0) fail(107L, "100/200/300 deterministic threshold path missing");
    cmp300_off += init100;
    add100_off += init100;
    if (add100_off <= cmp300_off) fail(108L, "100 tick increment is not after 300 comparison");

    if ((usize)cmp300_off + 9UL > main_sym.size || code[(usize)cmp300_off + 7UL] != 0x74U) {
        fail(109L, "M3 OK branch is not direct JE from 300 comparison");
    }
    branch_target = main_sym.value + (u64)cmp300_off + 9UL +
                    (u64)(long)(signed char)code[(usize)cmp300_off + 8UL];
    if (branch_target < main_sym.value || branch_target + 5UL > main_sym.value + main_sym.size) {
        fail(110L, "M3 OK branch target outside kmain");
    }
    {
        const u8 *target = kernel_ptr(branch_target, 5UL, 111L, "M3 OK target outside payload");
        if (target[0] != 0xbaU || (u64)rd32(target + 1UL) != ok.value) fail(112L, "M3 OK branch does not reference marker");
    }
    {
        const usize target_off = (usize)(branch_target - main_sym.value);
        int cli_seen = 0;
        int hlt_seen = 0;
        for (i = target_off; i < main_sym.size && i < target_off + 64UL; ++i) {
            if (code[i] == 0xfaU) cli_seen = 1;
            if (cli_seen && code[i] == 0xf4U) {
                hlt_seen = 1;
                break;
            }
        }
        if (!cli_seen || !hlt_seen) fail(113L, "M3 OK path does not cli/hlt");
    }
    trace("main.tick_threshold_start", main_sym.value + (u64)init100, 100UL, "kernel/main.c:69,83-95");
    trace("main.tick_threshold_300", main_sym.value + (u64)cmp300_off, 300UL, "kernel/main.c:85-92");
    trace("main.m3_ok_reachable", branch_target, ok.value, "kernel/main.c:89-90;kernel/main.c:56-63");
}

void _start(void)
{
    load_kernel_from_image();
    load_elf_symbols();
    verify_idt_and_stubs();
    verify_pic();
    verify_pit();
    verify_irq0_counter();
    verify_main_path();
    write_text(1L, "M3_TRACE_PASS\n");
    syscall1(SYS_EXIT, 0L);
    for (;;) { }
}
