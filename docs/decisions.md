# Core decisions

## Long-mode entry

| Option | Tradeoff |
|---|---|
| Real mode -> protected mode -> long mode | Gives a 32-bit protected-mode staging point, but adds a second GDT transition and a mode-specific loader path without eliminating the required PAE/page-table setup |
| Real mode -> enable PAE/LME/paging -> far jump directly to 64-bit code | Fewer state transitions, one early paging/GDT path, but the 16-bit loader must construct 64-bit page-table entries carefully |

**Pick: direct real-mode to IA-32e transition.** The loader remains 16-bit until it builds a PML4/PDPT/PD hierarchy, executes `CR4.PAE=1`, `EFER.LME=1`, loads CR3, sets `CR0.PG=1`, and far-jumps to a 64-bit code descriptor. A transient 32-bit protected-mode body adds code and validation burden without changing mandatory long-mode prerequisites. Intel SDM Volume 3A, 9.8.5 governs the order. [web:16][web:21]

## Paging

| Option | Tradeoff |
|---|---|
| 2 MiB identity huge page only | One PDE establishes the initial mapping, but cannot apply separate guard/permission policy within the first 2 MiB and cannot support a fine-grained heap map |
| 4 KiB pages everywhere | Exact permissions and guard pages, but 512 PTEs just to cover the initial 2 MiB and greater bootstrap complexity |
| Hybrid: identity-map first 2 MiB with one 2 MiB PDE; map heap with 4 KiB PTEs | Fast early path, then fine-grained heap policy; requires both PDE large-page and PTE walkers |

**Pick: hybrid.** Build one PML4, one PDPT and one PD. PDE[0] maps physical/virtual `0x00000000..0x001FFFFF` with `P|RW|PS`; this covers loader, VGA aperture, kernel at 1 MiB and early tables. Reserve virtual `0xFFFF800000200000..0xFFFF8000005FFFFF` as the first higher-half kernel heap window and map it through 4 KiB PTEs on demand. Before enabling the higher-half alias, code runs identity-mapped. After relocation/transition, kernel text becomes read/execute and data/heap read/write; `CR0.WP=1` is set after page-table validation. NX requires `EFER.NXE`; Intel SDM Vol. 3A paging chapters are the authority. [web:16][web:19]

## Physical allocator and heap

| Option | Tradeoff |
|---|---|
| Buddy allocator | Represents contiguous 2^n page runs, coalesces free blocks, supports page tables/DMA and exposes fragmentation state; has split/merge metadata and order selection |
| Slab allocator only | Fast fixed-object allocation and low per-object overhead, but cannot directly source contiguous physical page runs and requires a page allocator underneath |

**Pick: buddy allocator first.** The kernel needs physical pages before it has a heap: page tables, stacks, DMA-capable buffers and heap backing all require contiguous page allocation. Initialize a bitmap-backed buddy over E820 usable ranges after reservations; orders 0..N use 4 KiB base pages. The initial heap is a boundary-tag allocator backed by mapped order-0/compound buddy pages. A slab layer may be added later for high-churn kernel object classes; it is not the initial allocator.

## System calls

| Option | Tradeoff |
|---|---|
| `int 0x80` / interrupt gate | Simple privilege transition through IDT; saves broad interrupt frame and is useful for early bring-up, but has higher overhead and ties ABI to an interrupt path |
| `syscall` / `sysret` | Architectural fast path; saves return RIP in RCX and RFLAGS in R11, enters at `IA32_LSTAR`, but requires explicit MSR/GDT/flag-mask setup and strict canonical-address validation before `sysret` |

**Pick: `syscall` / `sysret` for the user-mode ABI; no user mode is required before M5.** Kernel-internal shell commands call direct C functions. Once ring 3 exists, configure `IA32_EFER.SCE`, `IA32_STAR`, `IA32_LSTAR`, and `IA32_FMASK`; entry saves RCX/R11 plus scratch/callee state onto a kernel stack, validates the syscall number and user pointers, then returns through `sysretq` only to validated canonical user RIP/RSP. `write`, `read`, `exit` use `RAX=number`, `RDI`, `RSI`, `RDX` arguments and negative errno-style returns. Intel SDM Volume 2, **SYSCALL** and **SYSRET**, and Volume 4 MSR definitions are authority. [web:17][web:23][web:30]

## Interrupt substrate

Final GDT includes null, kernel code/data, user code/data and a 16-byte 64-bit TSS descriptor. The TSS supplies `RSP0` and an IST stack for double fault. The final IDT has 256 entries with assembly stubs that normalize error-code/no-error-code frames before calling C. Remap PIC master/slave vectors to `0x20`/`0x28`, mask all except IRQ0/IRQ1, send EOI after servicing, and program PIT channel 0 mode 2 or 3 at the selected integer divisor. The keyboard driver consumes set-1 scan codes from port `0x60`; it does not issue BIOS keyboard calls once long mode is active.
