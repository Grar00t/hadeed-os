# Hadeed

x86-64 OS boot path from sector 0. No GRUB. No libc. No hosted runtime in the target image.

## Current milestone: M2

The raw image now contains three executable layers:

1. LBA 0: a 512-byte BIOS boot sector at `0x7c00`.
2. LBA 1-8: stage1 loaded at `0x8000`.
3. LBA 9+: a flat kernel payload linked for physical `0x00100000`.

Stage1 loads the kernel into a low-memory bounce buffer, enables A20, creates a 0..2 MiB identity map with one 2 MiB page, enables `CR4.PAE`, loads `CR3`, sets `IA32_EFER.LME`, sets `CR0.PE|PG`, then far-jumps through a 64-bit code descriptor. In 64-bit mode it copies the flat kernel to `0x00100000` and jumps to `kernel_entry`.

The kernel writes `HADEED64` to VGA text memory and writes the exact debug-port bytes `LM64\n` to I/O port `0xe9`, then halts.

## Build

Required host commands: `make`, `gcc`, GNU `as`, GNU `ld`, `objcopy`, `dd`, `od`, `tail`, `tr`.

```sh
make clean
make
make static-test
```

Expected static acceptance output:

```text
M2_STATIC_PASS
```

The C translation units are built with:

```text
-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding -nostdlib -fno-stack-protector -fno-pic -mno-red-zone -m64
```

No target binary links libc.

## Runtime acceptance

With `qemu-system-x86_64` and `timeout` available:

```sh
make runtime-test
```

The target emits exactly `LM64\n` on QEMU debugcon. The Makefile compares that file byte-for-byte and rejects a QEMU log containing `triple fault`.

Expected final line:

```text
M2_RUNTIME_PASS
```

For the visible VGA boot:

```sh
make run
```

The screen contains:

```text
HADEED64
```

## Layout

```text
boot/bootloader.asm    BIOS sector-0 loader
boot/stage1.asm        A20 + paging + IA-32e transition + kernel copy
kernel/entry.asm       64-bit kernel entry
kernel/main.c          VGA/debugcon M2 payload
kernel/linker.ld       physical link address 0x00100000
tests/m2_image_test.c  freestanding static acceptance binary
tests/host_linker.ld   no-libc host-test link layout
scripts/run-qemu.sh    visible QEMU launch
```

## License

MIT.
