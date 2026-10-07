# Milestones

Every milestone runs headless under `qemu-system-x86_64` with `-no-reboot -no-shutdown`. Tests capture debug output through QEMU's `isa-debugcon` (`0xE9`) in addition to the VGA-visible result. The debug port is a test observability channel, not a kernel dependency.

## M1 — Sector 0 executes

| Field | Definition |
|---|---|
| Input | 1.44 MiB raw image with `boot/bootloader.asm` in LBA 0, signature `0x55AA` at offsets 510–511. |
| Observable output | Text-mode screen displays `HADEED` at row 12, columns 36–41; CPU reaches the halt loop. |
| Acceptance test | `make m1` checks image byte count, boot signature, runs QEMU for a bounded wall-time, captures a deterministic debug marker emitted by the M1 test build, and asserts QEMU does not reset/triple-fault. Screenshot/hash test checks the six VGA cells. |
| Done | Cold boot succeeds with `-boot c`; no BIOS call occurs after the initial setup except the documented current M1 video-mode call. |
| Failure modes | Missing `0xAA55`; wrong `org 0x7C00`; DS not initialized before `lodsb`; direction flag set; writing segment instead of offset to VGA; code exceeds 510 bytes. |

## M2 — Kernel reaches IA-32e mode

| Field | Definition |
|---|---|
| Input | Image containing MBR, stage1 header and a kernel payload linked at physical `0x00100000`. |
| Observable output | VGA line: `Hadeed kernel: long mode entry reached`; debug trace emits `LM64\n`. |
| Acceptance test | QEMU with `-d int,cpu_reset -D build/qemu.log`; test requires exactly one reset at boot, no #GP/#PF/#DF/triple-fault after paging enable, and marker `LM64`. `readelf -h build/kernel.elf` must report ELF64/x86-64; `objdump -d` must show 64-bit entry code. |
| Done | Stage1 enables PAE/LME/paging in the documented order, far-jumps to 64-bit code, loads final stack, and invokes C. |
| Failure modes | CR3 not 4 KiB aligned; PML4/PDPT/PDE not present; `CR4.PAE` omitted; `EFER.LME` written after `CR0.PG`; far jump uses non-64-bit code descriptor; kernel payload overwritten by loader workspace. |

## M3 — PIT timer IRQ fires

| Field | Definition |
|---|---|
| Input | M2 image plus final GDT/IDT/TSS, remapped PIC, PIT channel 0 at selected divisor, IRQ0 unmasked and `sti`. |
| Observable output | VGA timer counter increments at 100 Hz; debug output contains `TICK 100` after one second of emulated time. |
| Acceptance test | QEMU `-icount shift=0,align=off,sleep=off`; run until counter 100; assert monotonic count, PIC EOI count equals IRQ0 count, and no unexpected vector is entered. Run twice and compare debug trace byte-for-byte. |
| Done | IRQ0 vector, stack frame normalization, EOI ordering, PIT divisor and IDT gate attributes are exercised. |
| Failure modes | PIC not remapped/masked; IRQ hits old exception vector; missing EOI; incorrect IDT selector/type; compiler-generated red-zone use; `sti` before IDT/TSS are valid. |

## M4 — Paging and heap live

| Field | Definition |
|---|---|
| Input | E820 map, physical buddy allocator, 2 MiB identity map, higher-half 4 KiB heap mappings. |
| Observable output | `meminfo` reports E820 usable/reserved pages, buddy free blocks per order, mapped heap range and allocation counters. A test allocates/free patterns and prints stable checksums. |
| Acceptance test | Allocate 1, 2, 3, 17 and 257 pages; verify alignment, no overlap, fill/readback, free in shuffled order, then require the buddy free-list histogram to return to baseline. Map heap pages, verify PTE flags through a software walk; optional deliberate guard-page access must produce #PF with expected error code and CR2 under a test build. |
| Done | All E820 reservations are excluded; page tables, stacks, VGA range and loaded kernel are never handed out. Heap extensions source pages solely through the buddy allocator. |
| Failure modes | E820 length overflow; reserved range coalesced into usable; order rounding error; double free; PTE stale after map/unmap; TLB invalidation omitted; kernel assumes physical == higher-half virtual after transition. |

## M5 — Keyboard and interactive shell

| Field | Definition |
|---|---|
| Input | M4 image plus IRQ1 handler, set-1 decoder, line editor, VGA console, command dispatch. |
| Observable output | Prompt `hadeed> `; commands `help`, `echo`, `clear`, `meminfo`, `uptime`; typed characters, backspace and Enter are visible and functional. |
| Acceptance test | QEMU monitor sends deterministic scan-code sequence for `echo abc`, Enter; VGA/debug capture must contain `abc`. Repeat for `help`, `meminfo`, `uptime`, `clear`, an unknown command, backspace correction and a 255-byte line boundary. No keyboard byte is lost under a concurrent 100 Hz PIT load. |
| Done | IRQ1 drains port `0x60`, EOI is issued, decoder handles make/break and modifiers required by the selected layout, line buffer is bounded, and commands run without BIOS services. |
| Failure modes | Controller output buffer not drained; EOI omitted; make/break confused; command buffer overwrite; console mutation from IRQ context races with shell; shell sleeps with interrupts disabled. |

# Two-week plan

| Day | Concrete artifacts |
|---:|---|
| 1 | `boot/bootloader.asm`, `Makefile`, raw image check, M1 VGA/banner test |
| 2 | `boot/stage1.asm`, DAP `AH=42h` reader, generated LBA1 header, flat-payload image builder |
| 3 | A20 routine, CPUID gates, temporary GDT, PML4/PDPT/PD, M2 long-mode entry |
| 4 | `kernel/gdt.c`, `kernel/idt.c`, `kernel/tss.c`, assembly ISR stubs, exception VGA dump |
| 5 | `kernel/pic.c`, `kernel/pit.c`, IRQ0 handler, deterministic M3 timer trace |
| 6 | `kernel/e820.c`, range reservation, bitmap-backed buddy allocator, `meminfo` backing data |
| 7 | `kernel/vm.c`, 4 KiB heap mapper, boundary-tag `kmalloc/kfree`, M4 allocation/guard tests |
| 8 | `kernel/vga.c`, scrolling console, formatting subset (`%s`, `%x`, `%u`), debugcon test path |
| 9 | `kernel/keyboard.c`, port `0x60` ISR, set-1 decoder and bounded input queue |
| 10 | `kernel/shell.c`, line editor, `help`, `echo`, `clear`; M5 scripted input harness |
| 11 | `meminfo`, `uptime`, unknown-command/error path; IRQ/console concurrency audit |
| 12 | `kernel/syscall.asm`, MSR setup, syscall dispatch skeleton for write/read/exit; ring-3 test task scaffold |
| 13 | Deterministic trace runner, repeated QEMU test target, panic/exception diagnostics |
| 14 | Image-layout audit, boot-size audit, clean rebuild test, M1–M5 regression log and fixed-hash release candidate |

M5 completes the requested interactive kernel shell. The user-mode syscall ABI is scaffolded on day 12; full user process loading is outside this two-week boundary.
