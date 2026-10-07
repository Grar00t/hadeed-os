typedef unsigned char u8;
typedef unsigned long usize;
typedef long isize;

#define SYS_READ 0L
#define SYS_WRITE 1L
#define SYS_OPEN 2L
#define SYS_CLOSE 3L
#define SYS_EXIT 60L

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

static usize cstrlen(const char *s)
{
    usize n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static void write_text(long fd, const char *s)
{
    usize left = cstrlen(s);
    while (left != 0UL) {
        const long n = syscall3(SYS_WRITE, fd, (long)s, (long)left);
        if (n <= 0L) {
            syscall1(SYS_EXIT, 111L);
            for (;;) { }
        }
        s += (usize)n;
        left -= (usize)n;
    }
}

__attribute__((noreturn)) static void fail(const char *s)
{
    write_text(2L, s);
    syscall1(SYS_EXIT, 1L);
    for (;;) { }
}

static long open_ro(const char *path)
{
    return syscall3(SYS_OPEN, (long)path, 0L, 0L);
}

static isize read_all(long fd, u8 *buf, usize cap)
{
    usize total = 0;
    while (total < cap) {
        const long n = syscall3(SYS_READ, fd, (long)(buf + total), (long)(cap - total));
        if (n < 0L) {
            return -1;
        }
        if (n == 0L) {
            break;
        }
        total += (usize)n;
    }
    return (isize)total;
}

static int contains(const u8 *buf, usize len, const char *needle)
{
    const usize nlen = cstrlen(needle);
    usize i;
    usize j;

    if (nlen == 0UL || nlen > len) {
        return 0;
    }
    for (i = 0; i + nlen <= len; ++i) {
        for (j = 0; j < nlen; ++j) {
            if (buf[i + j] != (u8)needle[j]) {
                break;
            }
        }
        if (j == nlen) {
            return 1;
        }
    }
    return 0;
}

static u8 boot[513];
static u8 stage1[4097];
static u8 kernel[65536];

void _start(void)
{
    long fd;
    isize n;

    fd = open_ro("build/boot.bin");
    if (fd < 0L) {
        fail("M2_STATIC_FAIL boot open\n");
    }
    n = read_all(fd, boot, sizeof boot);
    (void)syscall1(SYS_CLOSE, fd);
    if (n != 512 || boot[510] != 0x55U || boot[511] != 0xaaU) {
        fail("M2_STATIC_FAIL boot signature/size\n");
    }

    fd = open_ro("build/stage1.bin");
    if (fd < 0L) {
        fail("M2_STATIC_FAIL stage1 open\n");
    }
    n = read_all(fd, stage1, sizeof stage1);
    (void)syscall1(SYS_CLOSE, fd);
    if (n <= 0 || n > 4096) {
        fail("M2_STATIC_FAIL stage1 size\n");
    }

    fd = open_ro("build/kernel.bin");
    if (fd < 0L) {
        fail("M2_STATIC_FAIL kernel open\n");
    }
    n = read_all(fd, kernel, sizeof kernel);
    (void)syscall1(SYS_CLOSE, fd);
    if (n <= 0 || n >= (isize)sizeof kernel) {
        fail("M2_STATIC_FAIL kernel size\n");
    }
    if (!contains(kernel, (usize)n, "HADEED64") || !contains(kernel, (usize)n, "LM64\n")) {
        fail("M2_STATIC_FAIL kernel markers\n");
    }

    write_text(1L, "M2_STATIC_PASS\n");
    syscall1(SYS_EXIT, 0L);
    for (;;) { }
}
