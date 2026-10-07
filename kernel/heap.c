#include "heap.h"

#define HEAP_BASE 0xffff800040000000UL
#define HEAP_ARENA_ORDER 4U
#define HEAP_ARENA_PAGES (1UL << HEAP_ARENA_ORDER)
#define HEAP_ARENA_BYTES (HEAP_ARENA_PAGES * HADEED_PAGE_SIZE)
#define HEAP_MAX_ARENAS 4U
#define HEAP_TAG_GUARD 0x4841444545444d34UL
#define HEAP_USED 1UL
#define HEAP_TAG_BYTES 16UL
#define HEAP_OVERHEAD (HEAP_TAG_BYTES * 2UL)
#define HEAP_MIN_BLOCK 48UL

struct heap_tag { u64 size_flags; u64 guard; };
struct heap_arena { u64 physical; u64 virtual_base; };

__attribute__((used, section(".m4meta")))
static struct heap_arena heap_arenas[HEAP_MAX_ARENAS];
__attribute__((section(".m4meta")))
static u32 heap_arena_count;

void heap_init(void)
{
    u32 i;
    heap_arena_count = 0U;
    for (i = 0U; i < HEAP_MAX_ARENAS; ++i) {
        heap_arenas[i].physical = 0UL;
        heap_arenas[i].virtual_base = 0UL;
    }
}

static u64 align16(u64 value) { return (value + 15UL) & ~15UL; }
static u64 tag_size(const struct heap_tag *tag) { return tag->size_flags & ~15UL; }
static int tag_used(const struct heap_tag *tag) { return (tag->size_flags & HEAP_USED) != 0UL; }
static int tag_valid(const struct heap_tag *tag) { return tag->guard == HEAP_TAG_GUARD && tag_size(tag) >= HEAP_OVERHEAD; }

static void write_tags(u64 address, u64 size, int used)
{
    struct heap_tag *const head = (struct heap_tag *)(usize)address;
    struct heap_tag *const foot = (struct heap_tag *)(usize)(address + size - HEAP_TAG_BYTES);
    const u64 flags = size | (used ? HEAP_USED : 0UL);
    head->size_flags = flags; head->guard = HEAP_TAG_GUARD;
    foot->size_flags = flags; foot->guard = HEAP_TAG_GUARD;
}

__attribute__((noinline)) static int new_arena(void)
{
    u64 physical;
    u64 virtual_base;
    u64 i;
    if (heap_arena_count >= HEAP_MAX_ARENAS) return -1;
    physical = page_alloc(HEAP_ARENA_ORDER);
    if (physical == 0UL) return -1;
    virtual_base = HEAP_BASE + (u64)heap_arena_count * HEAP_ARENA_BYTES;
    for (i = 0UL; i < HEAP_ARENA_PAGES; ++i) {
        if (map_page_4k(physical + i * HADEED_PAGE_SIZE, virtual_base + i * HADEED_PAGE_SIZE) != 0) {
            page_free(physical, HEAP_ARENA_ORDER);
            return -1;
        }
    }
    heap_arenas[heap_arena_count].physical = physical;
    heap_arenas[heap_arena_count].virtual_base = virtual_base;
    ++heap_arena_count;
    write_tags(virtual_base, HEAP_ARENA_BYTES, 0);
    return 0;
}

static void *alloc_from_arena(u64 base, u64 bytes)
{
    u64 offset = 0UL;
    const u64 need = align16(bytes) + HEAP_OVERHEAD;
    while (offset < HEAP_ARENA_BYTES) {
        struct heap_tag *const head = (struct heap_tag *)(usize)(base + offset);
        const u64 size = tag_size(head);
        if (!tag_valid(head) || size > HEAP_ARENA_BYTES - offset) return (void *)0;
        if (!tag_used(head) && size >= need) {
            const u64 remain = size - need;
            if (remain >= HEAP_MIN_BLOCK) {
                write_tags(base + offset, need, 1);
                write_tags(base + offset + need, remain, 0);
            } else {
                write_tags(base + offset, size, 1);
            }
            return (void *)(usize)(base + offset + HEAP_TAG_BYTES);
        }
        offset += size;
    }
    return (void *)0;
}

__attribute__((noinline)) void *kmalloc(usize bytes)
{
    u32 i;
    void *p;
    if (bytes == 0UL || bytes > HEAP_ARENA_BYTES - HEAP_OVERHEAD) return (void *)0;
    for (i = 0U; i < heap_arena_count; ++i) {
        p = alloc_from_arena(heap_arenas[i].virtual_base, (u64)bytes);
        if (p != (void *)0) return p;
    }
    if (new_arena() != 0) return (void *)0;
    return alloc_from_arena(heap_arenas[heap_arena_count - 1U].virtual_base, (u64)bytes);
}

static int arena_for_pointer(u64 p, u32 *index)
{
    u32 i;
    for (i = 0U; i < heap_arena_count; ++i) {
        const u64 base = heap_arenas[i].virtual_base;
        if (p >= base + HEAP_TAG_BYTES && p < base + HEAP_ARENA_BYTES) { *index = i; return 1; }
    }
    return 0;
}

__attribute__((noinline)) void kfree(void *ptr)
{
    u64 p = (u64)(usize)ptr;
    u32 arena_index;
    u64 base;
    u64 head_addr;
    struct heap_tag *head;
    u64 size;
    if (ptr == (void *)0 || !arena_for_pointer(p, &arena_index)) return;
    base = heap_arenas[arena_index].virtual_base;
    head_addr = p - HEAP_TAG_BYTES;
    head = (struct heap_tag *)(usize)head_addr;
    if (!tag_valid(head) || !tag_used(head)) return;
    size = tag_size(head);
    write_tags(head_addr, size, 0);

    if (head_addr + size < base + HEAP_ARENA_BYTES) {
        struct heap_tag *const next = (struct heap_tag *)(usize)(head_addr + size);
        if (tag_valid(next) && !tag_used(next)) {
            size += tag_size(next);
            write_tags(head_addr, size, 0);
        }
    }

    if (head_addr > base) {
        struct heap_tag *const prev_foot = (struct heap_tag *)(usize)(head_addr - HEAP_TAG_BYTES);
        if (tag_valid(prev_foot) && !tag_used(prev_foot)) {
            const u64 prev_size = tag_size(prev_foot);
            if (prev_size <= head_addr - base) {
                const u64 prev_head_addr = head_addr - prev_size;
                struct heap_tag *const prev_head = (struct heap_tag *)(usize)prev_head_addr;
                if (tag_valid(prev_head) && !tag_used(prev_head) && tag_size(prev_head) == prev_size) {
                    size += prev_size;
                    write_tags(prev_head_addr, size, 0);
                }
            }
        }
    }
}
