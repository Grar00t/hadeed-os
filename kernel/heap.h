#ifndef HADEED_HEAP_H
#define HADEED_HEAP_H
#include "memory.h"
void heap_init(void);
__attribute__((noinline)) void *kmalloc(usize bytes);
__attribute__((noinline)) void kfree(void *ptr);
#endif
