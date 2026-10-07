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
#define STAGE1_PATH "build/stage1.bin"
#define KERNEL_PATH "build/kernel.bin"
#define ELF_PATH "build/kernel.elf"
#define KERNEL_PHYS 0x00100000UL

static u8 stage1[4096];
static u8 kernel[65536];
static u8 elf[262144];
static usize stage1_len;
static usize kernel_len;
static usize elf_len;

struct symbol { u64 value; u64 size; };

static long syscall1(long n,long a1){long r;__asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a1):"rcx","r11","memory");return r;}
static long syscall3(long n,long a1,long a2,long a3){long r;__asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a1),"S"(a2),"d"(a3):"rcx","r11","memory");return r;}
static usize cstrlen(const char*s){usize n=0;while(s[n]!='\0')++n;return n;}
static void write_text(long fd,const char*s){usize left=cstrlen(s);while(left){long n=syscall3(SYS_WRITE,fd,(long)s,(long)left);if(n<=0)syscall1(SYS_EXIT,120);s+=(usize)n;left-=(usize)n;}}
__attribute__((noreturn)) static void fail(long code,const char*msg){write_text(2,"M4_TRACE_FAIL ");write_text(2,msg);write_text(2,"\n");syscall1(SYS_EXIT,code);for(;;){}}
static u16 rd16(const u8*p){return (u16)((u16)p[0]|((u16)p[1]<<8));}
static u32 rd32(const u8*p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static u64 rd64(const u8*p){return (u64)rd32(p)|((u64)rd32(p+4)<<32);}
static int bytes_equal(const u8*a,const u8*b,usize n){usize i;for(i=0;i<n;++i)if(a[i]!=b[i])return 0;return 1;}
static isize find_bytes(const u8*b,usize len,const u8*n,usize nl){usize i;if(nl==0||nl>len)return -1;for(i=0;i+nl<=len;++i)if(bytes_equal(b+i,n,nl))return (isize)i;return -1;}
static long open_ro(const char*path){return syscall3(SYS_OPEN,(long)path,0L,0L);}
static usize read_file(const char*path,u8*buf,usize cap,long code){long fd=open_ro(path);usize done=0;if(fd<0)fail(code,"open failed");while(done<cap){long n=syscall3(SYS_READ,fd,(long)(buf+done),(long)(cap-done));if(n<0)fail(code,"read failed");if(n==0)break;done+=(usize)n;}syscall1(SYS_CLOSE,fd);if(done==cap)fail(code,"file too large");return done;}

static int streq(const char*a,const char*b){usize i=0;for(;;){if(a[i]!=b[i])return 0;if(a[i]=='\0')return 1;++i;}}

static struct symbol find_symbol(const char*name,long code)
{
    u64 shoff;u16 shentsize,shnum;u16 i;
    if(elf_len<64||elf[0]!=0x7fU||elf[1]!='E'||elf[2]!='L'||elf[3]!='F'||elf[4]!=2U||elf[5]!=1U)fail(code,"bad ELF64");
    shoff=rd64(elf+0x28);shentsize=rd16(elf+0x3a);shnum=rd16(elf+0x3c);
    if(shentsize<64U||shoff+(u64)shentsize*shnum>elf_len)fail(code,"bad ELF section table");
    for(i=0U;i<shnum;++i){
        const u8*sh=elf+(usize)shoff+(usize)i*shentsize;
        const u32 type=rd32(sh+4);
        if(type==2U){
            const u64 off=rd64(sh+24),size=rd64(sh+32),entsize=rd64(sh+56);const u32 link=rd32(sh+40);u64 j;
            const u8*strsh;u64 stroff,strsize;
            if(entsize<24UL||off+size>elf_len||link>=shnum)fail(code,"bad symtab");
            strsh=elf+(usize)shoff+(usize)link*shentsize;stroff=rd64(strsh+24);strsize=rd64(strsh+32);
            if(stroff+strsize>elf_len)fail(code,"bad strtab");
            for(j=0;j+entsize<=size;j+=entsize){
                const u8*sym=elf+(usize)(off+j);const u32 noff=rd32(sym);const char*s;
                if(noff>=strsize) continue;
                s=(const char*)(elf+(usize)stroff+noff);
                if(streq(s,name)){struct symbol out;out.value=rd64(sym+8);out.size=rd64(sym+16);return out;}
            }
        }
    }
    fail(code,"symbol missing");
}

static const u8 *flat_ptr(u64 address,usize n,long code)
{
    u64 off;if(address<KERNEL_PHYS)fail(code,"symbol below kernel");off=address-KERNEL_PHYS;if(off+n>kernel_len)fail(code,"symbol outside flat kernel");return kernel+(usize)off;
}

static isize find_direct_call(struct symbol from,u64 target,usize start)
{
    const u8*code=flat_ptr(from.value,(usize)from.size,70L);usize i;
    for(i=start;i+5UL<=from.size;++i){if(code[i]==0xe8U){long rel=(long)(i32)rd32(code+i+1);u64 dst=from.value+i+5UL+(u64)rel;if(dst==target)return (isize)i;}}
    return -1;
}

static usize count_direct_calls(struct symbol from,u64 target)
{
    const u8*code=flat_ptr(from.value,(usize)from.size,71L);usize i,count=0;
    for(i=0;i+5UL<=from.size;++i){if(code[i]==0xe8U){long rel=(long)(i32)rd32(code+i+1);u64 dst=from.value+i+5UL+(u64)rel;if(dst==target)++count;}}
    return count;
}

static void verify_e820_stage1(void)
{
    static const u8 mov_e820[]={0x66,0xb8,0x20,0xe8,0x00,0x00};
    static const u8 mov_smap[]={0x66,0xba,0x50,0x41,0x4d,0x53};
    static const u8 mov_24[]={0x66,0xb9,0x18,0x00,0x00,0x00};
    static const u8 int15[]={0xcd,0x15};
    isize a=find_bytes(stage1,stage1_len,mov_e820,sizeof mov_e820);
    isize b=find_bytes(stage1,stage1_len,mov_smap,sizeof mov_smap);
    isize c=find_bytes(stage1,stage1_len,mov_24,sizeof mov_24);
    isize d=find_bytes(stage1,stage1_len,int15,sizeof int15);
    if(a<0||b<0||c<0||d<0||!(a<b&&b<c&&c<d))fail(20,"E820 SMAP call sequence missing");
}

static void verify_handoff_constants(void)
{
    const struct symbol info=find_symbol("m4_e820_info_addr",30L);
    const struct symbol entries=find_symbol("m4_e820_entries_addr",31L);
    if(info.size!=8UL||entries.size!=8UL)fail(32,"E820 handoff symbols wrong size");
    if(rd64(flat_ptr(info.value,8UL,33L))!=0x5000UL||rd64(flat_ptr(entries.value,8UL,34L))!=0x5010UL)fail(35,"E820 handoff addresses mismatch");
}

static void verify_buddy_layout(void)
{
    static const u32 expected[]={0U,65536U,98304U,114688U,122880U,126976U,129024U,130048U,130560U,130816U,130944U,131008U,131040U,131056U,131064U,131068U,131070U,131071U};
    const struct symbol offs=find_symbol("buddy_order_offsets",40L);
    const struct symbol freebm=find_symbol("buddy_free_bitmap",41L);
    const struct symbol usable=find_symbol("page_usable_bitmap",42L);
    const struct symbol ptpool=find_symbol("page_table_pool",43L);
    const u8*p;usize i;
    if(offs.size!=sizeof expected||freebm.size!=16384UL||usable.size!=8192UL||ptpool.size!=24UL*512UL*8UL)fail(44,"buddy metadata sizes mismatch");
    if((ptpool.value&0xfffUL)!=0UL)fail(45,"page-table pool not 4KiB aligned");
    p=flat_ptr(offs.value,sizeof expected,46L);for(i=0;i<sizeof expected/sizeof expected[0];++i)if(rd32(p+i*4UL)!=expected[i])fail(47,"buddy order offsets mismatch");
}

static void verify_allocator_symbols(void)
{
    const struct symbol alloc=find_symbol("page_alloc",50L);const struct symbol fre=find_symbol("page_free",51L);const struct symbol map=find_symbol("map_page_4k",52L);const struct symbol km=find_symbol("kmalloc",53L);const struct symbol kf=find_symbol("kfree",54L);const struct symbol arena=find_symbol("new_arena",55L);
    if(alloc.size<16UL||fre.size<16UL||map.size<16UL||km.size<16UL||kf.size<16UL||arena.size<16UL)fail(56,"allocator function too small");
    if(find_direct_call(arena,alloc.value,0UL)<0||find_direct_call(arena,map.value,0UL)<0)fail(57,"heap arena is not backed by buddy plus map_page_4k");
}

static void verify_kmain_sequence(void)
{
    static const u8 counts[]={1,0,2,0,3,0,4,0,5,0,8,0,16,0,17,0,1,1};
    static const u8 ok[]={ 'M','4',' ','O','K','\n','\0' };
    const struct symbol main_sym=find_symbol("kmain",60L);const struct symbol km=find_symbol("kmalloc",61L);const struct symbol kf=find_symbol("kfree",62L);const struct symbol page_counts=find_symbol("m4_page_counts",63L);const struct symbol ok_sym=find_symbol("m4_ok_marker",64L);
    isize first_km,first_kf;
    if(page_counts.size!=sizeof counts||!bytes_equal(flat_ptr(page_counts.value,sizeof counts,65L),counts,sizeof counts))fail(66,"M4 page-count sequence mismatch");
    if(ok_sym.size!=sizeof ok||!bytes_equal(flat_ptr(ok_sym.value,sizeof ok,67L),ok,sizeof ok))fail(68,"M4 OK marker mismatch");
    if(count_direct_calls(main_sym,km.value)!=3UL||count_direct_calls(main_sym,kf.value)!=3UL)fail(69,"kmain kmalloc/kfree call counts mismatch");
    first_km=find_direct_call(main_sym,km.value,0UL);first_kf=find_direct_call(main_sym,kf.value,0UL);if(first_km<0||first_kf<0||first_kf<=first_km)fail(70,"kmain kmalloc/kfree order mismatch");
}

void _start(void)
{
    stage1_len=read_file(STAGE1_PATH,stage1,sizeof stage1,10L);
    kernel_len=read_file(KERNEL_PATH,kernel,sizeof kernel,11L);
    elf_len=read_file(ELF_PATH,elf,sizeof elf,12L);
    verify_e820_stage1();
    verify_handoff_constants();
    verify_buddy_layout();
    verify_allocator_symbols();
    verify_kmain_sequence();
    write_text(1,"M4_TRACE_PASS\n");
    syscall1(SYS_EXIT,0L);for(;;){}
}
