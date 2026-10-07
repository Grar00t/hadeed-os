#include "memory.h"

#define E820_INFO_ADDR 0x5000UL
#define E820_ENTRIES_ADDR 0x5010UL
#define E820_MAX_ENTRIES 128U
#define E820_USABLE 1U
#define LOW_RESERVED_END 0x00200000UL
#define PML4_ADDR 0x00009000UL
#define PTE_PRESENT 0x001UL
#define PTE_WRITE 0x002UL
#define PTE_PS 0x080UL
#define PTE_ADDR_MASK 0x000ffffffffff000UL
#define PT_POOL_TABLES 24U
#define BUDDY_TOTAL_BITS 131071UL

struct __attribute__((packed)) e820_entry {
    u64 base;
    u64 length;
    u32 type;
    u32 attributes;
};

__attribute__((used, section(".rodata.m4")))
const u64 m4_e820_info_addr = E820_INFO_ADDR;
__attribute__((used, section(".rodata.m4")))
const u64 m4_e820_entries_addr = E820_ENTRIES_ADDR;

__attribute__((used, section(".rodata.m4")))
const u32 buddy_order_offsets[HADEED_MAX_ORDER + 2U] = {
    0U, 65536U, 98304U, 114688U, 122880U, 126976U,
    129024U, 130048U, 130560U, 130816U, 130944U, 131008U,
    131040U, 131056U, 131064U, 131068U, 131070U, 131071U
};

__attribute__((used, section(".m4meta"), aligned(64)))
u8 buddy_free_bitmap[16384];

__attribute__((used, section(".m4meta"), aligned(64)))
u8 page_usable_bitmap[8192];

__attribute__((used, section(".m4pt"), aligned(4096)))
static u64 page_table_pool[PT_POOL_TABLES][512];

__attribute__((section(".m4meta")))
static u32 page_table_pool_used;

static void bytes_zero(u8 *p, usize n)
{
    usize i;
    for (i = 0UL; i < n; ++i) p[i] = 0U;
}

static int bit_get(const u8 *map, u64 bit)
{
    return (map[bit >> 3] >> (bit & 7UL)) & 1U;
}

static void bit_set(u8 *map, u64 bit)
{
    map[bit >> 3] = (u8)(map[bit >> 3] | (u8)(1U << (bit & 7UL)));
}

static void bit_clear(u8 *map, u64 bit)
{
    map[bit >> 3] = (u8)(map[bit >> 3] & (u8)~(u8)(1U << (bit & 7UL)));
}

static u64 buddy_bit(u32 order, u64 page)
{
    return (u64)buddy_order_offsets[order] + (page >> order);
}

static int buddy_is_free(u64 page, u32 order)
{
    return bit_get(buddy_free_bitmap, buddy_bit(order, page));
}

static void buddy_mark_free(u64 page, u32 order)
{
    bit_set(buddy_free_bitmap, buddy_bit(order, page));
}

static void buddy_mark_used(u64 page, u32 order)
{
    bit_clear(buddy_free_bitmap, buddy_bit(order, page));
}

static void add_usable_run(u64 start_page, u64 end_page)
{
    u64 page = start_page;
    while (page < end_page) {
        u32 order = 0U;
        while (order < HADEED_MAX_ORDER) {
            const u64 next_pages = 1UL << (order + 1U);
            if ((page & (next_pages - 1UL)) != 0UL || page + next_pages > end_page) break;
            ++order;
        }
        buddy_mark_free(page, order);
        page += 1UL << order;
    }
}

u64 memory_init(void)
{
    const volatile u32 *const info = (const volatile u32 *)E820_INFO_ADDR;
    const struct e820_entry *const entries = (const struct e820_entry *)E820_ENTRIES_ADDR;
    const u64 limit = HADEED_MAX_PHYS_PAGES * HADEED_PAGE_SIZE;
    u32 count = info[0];
    const u32 entry_size = info[1];
    u64 usable_pages = 0UL;
    u64 page;
    u32 i;

    bytes_zero(buddy_free_bitmap, sizeof buddy_free_bitmap);
    bytes_zero(page_usable_bitmap, sizeof page_usable_bitmap);
    page_table_pool_used = 0U;

    if (entry_size != 24U) return 0UL;
    if (count > E820_MAX_ENTRIES) count = E820_MAX_ENTRIES;

    for (i = 0U; i < count; ++i) {
        const struct e820_entry *const e = &entries[i];
        u64 raw_end;
        u64 start;
        u64 end;
        if (e->type != E820_USABLE || e->length == 0UL || (e->attributes & 1U) == 0U) continue;
        if (e->base >= limit || e->base > (~0UL - e->length)) continue;
        raw_end = e->base + e->length;
        if (raw_end > limit) raw_end = limit;
        start = (e->base + (HADEED_PAGE_SIZE - 1UL)) & ~(HADEED_PAGE_SIZE - 1UL);
        end = raw_end & ~(HADEED_PAGE_SIZE - 1UL);
        if (end <= start) continue;
        for (page = start / HADEED_PAGE_SIZE; page < end / HADEED_PAGE_SIZE; ++page) {
            bit_set(page_usable_bitmap, page);
        }
    }

    /* Reserved/non-type-1 entries override overlapping usable ranges. */
    for (i = 0U; i < count; ++i) {
        const struct e820_entry *const e = &entries[i];
        u64 raw_end;
        u64 start;
        u64 end;
        if (e->type == E820_USABLE || e->length == 0UL) continue;
        if (e->base >= limit || e->base > (~0UL - e->length)) continue;
        raw_end = e->base + e->length;
        if (raw_end > limit) raw_end = limit;
        start = e->base & ~(HADEED_PAGE_SIZE - 1UL);
        end = (raw_end + (HADEED_PAGE_SIZE - 1UL)) & ~(HADEED_PAGE_SIZE - 1UL);
        if (end > limit) end = limit;
        if (end <= start) continue;
        for (page = start / HADEED_PAGE_SIZE; page < end / HADEED_PAGE_SIZE; ++page) {
            bit_clear(page_usable_bitmap, page);
        }
    }

    for (page = 0UL; page < LOW_RESERVED_END / HADEED_PAGE_SIZE; ++page) {
        bit_clear(page_usable_bitmap, page);
    }

    page = LOW_RESERVED_END / HADEED_PAGE_SIZE;
    while (page < HADEED_MAX_PHYS_PAGES) {
        u64 run_start;
        if (!bit_get(page_usable_bitmap, page)) {
            ++page;
            continue;
        }
        run_start = page;
        while (page < HADEED_MAX_PHYS_PAGES && bit_get(page_usable_bitmap, page)) {
            ++usable_pages;
            ++page;
        }
        add_usable_run(run_start, page);
    }

    return usable_pages;
}

u32 pages_to_order(u64 pages)
{
    u32 order = 0U;
    u64 block = 1UL;
    if (pages == 0UL) return HADEED_MAX_ORDER + 1U;
    while (block < pages && order < HADEED_MAX_ORDER) {
        block <<= 1;
        ++order;
    }
    if (block < pages) return HADEED_MAX_ORDER + 1U;
    return order;
}

__attribute__((noinline)) u64 page_alloc(u32 order)
{
    u32 current;
    if (order > HADEED_MAX_ORDER) return 0UL;
    for (current = order; current <= HADEED_MAX_ORDER; ++current) {
        const u64 blocks = HADEED_MAX_PHYS_PAGES >> current;
        u64 block;
        for (block = 0UL; block < blocks; ++block) {
            const u64 page = block << current;
            if (!buddy_is_free(page, current)) continue;
            buddy_mark_used(page, current);
            while (current > order) {
                const u64 split_page = page + (1UL << (current - 1U));
                --current;
                buddy_mark_free(split_page, current);
            }
            return page * HADEED_PAGE_SIZE;
        }
    }
    return 0UL;
}

__attribute__((noinline)) void page_free(u64 physical_page, u32 order)
{
    u64 page;
    if (order > HADEED_MAX_ORDER || (physical_page & (HADEED_PAGE_SIZE - 1UL)) != 0UL) return;
    page = physical_page / HADEED_PAGE_SIZE;
    if (page >= HADEED_MAX_PHYS_PAGES || (page & ((1UL << order) - 1UL)) != 0UL) return;

    while (order < HADEED_MAX_ORDER) {
        const u64 buddy = page ^ (1UL << order);
        if (buddy >= HADEED_MAX_PHYS_PAGES || !buddy_is_free(buddy, order)) break;
        buddy_mark_used(buddy, order);
        if (buddy < page) page = buddy;
        ++order;
    }
    buddy_mark_free(page, order);
}

static u64 *pt_alloc_table(void)
{
    u64 *table;
    usize i;
    if (page_table_pool_used >= PT_POOL_TABLES) return (u64 *)0;
    table = &page_table_pool[page_table_pool_used][0];
    ++page_table_pool_used;
    for (i = 0UL; i < 512UL; ++i) table[i] = 0UL;
    return table;
}

static u64 *next_table(u64 *table, u64 index)
{
    u64 entry = table[index];
    u64 *next;
    if ((entry & PTE_PRESENT) != 0UL) {
        if ((entry & PTE_PS) != 0UL) return (u64 *)0;
        return (u64 *)(usize)(entry & PTE_ADDR_MASK);
    }
    next = pt_alloc_table();
    if (next == (u64 *)0) return (u64 *)0;
    table[index] = ((u64)(usize)next & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITE;
    return next;
}

__attribute__((noinline)) int map_page_4k(u64 physical_page, u64 virtual_address)
{
    u64 *pml4 = (u64 *)PML4_ADDR;
    u64 *pdpt;
    u64 *pd;
    u64 *pt;
    const u64 top = virtual_address >> 48;
    const u64 sign = (virtual_address >> 47) & 1UL;
    const u64 pml4_i = (virtual_address >> 39) & 0x1ffUL;
    const u64 pdpt_i = (virtual_address >> 30) & 0x1ffUL;
    const u64 pd_i = (virtual_address >> 21) & 0x1ffUL;
    const u64 pt_i = (virtual_address >> 12) & 0x1ffUL;

    if ((physical_page & (HADEED_PAGE_SIZE - 1UL)) != 0UL ||
        (virtual_address & (HADEED_PAGE_SIZE - 1UL)) != 0UL) return -1;
    if ((!sign && top != 0UL) || (sign && top != 0xffffUL)) return -1;

    pdpt = next_table(pml4, pml4_i);
    if (pdpt == (u64 *)0) return -1;
    pd = next_table(pdpt, pdpt_i);
    if (pd == (u64 *)0) return -1;
    pt = next_table(pd, pd_i);
    if (pt == (u64 *)0) return -1;
    pt[pt_i] = (physical_page & PTE_ADDR_MASK) | PTE_PRESENT | PTE_WRITE;
    __asm__ volatile ("invlpg (%0)" : : "r" ((void *)(usize)virtual_address) : "memory");
    return 0;
}
