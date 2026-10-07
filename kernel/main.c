#include "heap.h"

typedef unsigned short vga_u16;

volatile u64 g_ticks;

__attribute__((used, section(".rodata.m2"))) static const char m2_banner[] = "HADEED64";
__attribute__((used, section(".rodata.m2"))) static const char m2_debug_marker[] = "LM64\n";
__attribute__((used, section(".rodata.m4"))) static const char m4_pages_prefix[] = "USABLE_PAGES ";
__attribute__((used, section(".rodata.m4"))) static const char m4_ok_marker[] = "M4 OK\n";
__attribute__((used, section(".rodata.m4"))) static const char m4_fail_marker[] = "M4 FAIL\n";
__attribute__((used, section(".rodata.m4"))) static const u16 m4_page_counts[] = {1U,2U,3U,4U,5U,8U,16U,17U,257U};

static void debug_putc(char c){__asm__ volatile("outb %0, $0xe9"::"a"((u8)c));}
static void debug_write(const char *s){while(*s!='\0'){debug_putc(*s);++s;}}
static void debug_write_u64(u64 value){char digits[20];usize n=0UL;if(value==0UL){debug_putc('0');return;}while(value!=0UL){digits[n++]=(char)('0'+(value%10UL));value/=10UL;}while(n!=0UL){--n;debug_putc(digits[n]);}}
__attribute__((noreturn)) static void finish(const char *marker){debug_write(marker);__asm__ volatile("cli":::"memory");for(;;)__asm__ volatile("hlt");}

__attribute__((noreturn)) void kmain(void)
{
    volatile vga_u16 *const vga=(volatile vga_u16 *)0xb8000;
    const usize base=(12UL*80UL)+36UL;
    u64 blocks[sizeof m4_page_counts / sizeof m4_page_counts[0]];
    u32 orders[sizeof m4_page_counts / sizeof m4_page_counts[0]];
    u64 usable_pages;
    u64 probe;
    void *a;
    void *b;
    void *c;
    usize i;

    for(i=0UL;i<(sizeof m2_banner-1UL);++i)vga[base+i]=(vga_u16)(0x0f00U|(u8)m2_banner[i]);
    debug_write(m2_debug_marker);

    usable_pages=memory_init();
    heap_init();
    if(usable_pages==0UL)finish(m4_fail_marker);
    debug_write(m4_pages_prefix);debug_write_u64(usable_pages);debug_putc('\n');

    for(i=0UL;i<sizeof m4_page_counts/sizeof m4_page_counts[0];++i){
        orders[i]=pages_to_order((u64)m4_page_counts[i]);
        if(orders[i]>HADEED_MAX_ORDER)finish(m4_fail_marker);
        blocks[i]=page_alloc(orders[i]);
        if(blocks[i]==0UL)finish(m4_fail_marker);
    }
    for(i=sizeof m4_page_counts/sizeof m4_page_counts[0];i!=0UL;){--i;page_free(blocks[i],orders[i]);}

    probe=page_alloc(0U);
    if(probe==0UL)finish(m4_fail_marker);
    page_free(probe,0U);

    a=kmalloc(24UL);
    b=kmalloc(4096UL);
    c=kmalloc(33UL);
    if(a==(void *)0||b==(void *)0||c==(void *)0)finish(m4_fail_marker);
    ((volatile u8 *)a)[0]=0x5aU;((volatile u8 *)b)[4095]=0xa5U;((volatile u8 *)c)[32]=0x3cU;
    if(((volatile u8 *)a)[0]!=0x5aU||((volatile u8 *)b)[4095]!=0xa5U||((volatile u8 *)c)[32]!=0x3cU)finish(m4_fail_marker);
    kfree(c);kfree(b);kfree(a);

    finish(m4_ok_marker);
}
