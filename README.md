# Hadeed

x86-64 OS from sector 0. Custom bootloader. No GRUB. No libc.

## Current state

M1 is the committed target: a 512-byte NASM MBR boot sector clears VGA text mode, writes `HADEED`, and halts. It is an executable sector-0 artifact, not yet a kernel loader. The kernel entry/C/linker files are the M2 input skeleton; they are not included in the current image until the stage1 loader exists.

The requested Arabic word `حديد` cannot be rendered by a stock VGA ROM text font: VGA text mode displays one 8-bit glyph index per cell and does not provide Arabic glyphs or shaping. The current sector therefore displays ASCII `HADEED`. M2 installs a project-supplied 8×16 glyph table and renders pre-shaped glyph cells if Arabic text remains required.

## Toolchain

The host build uses these command-line tools:

- `nasm` to assemble the boot sector and kernel assembly.
- `make` to run the declared build graph.
- `qemu-system-x86_64` to run the raw disk image.
- `x86_64-elf-gcc` and `x86_64-elf-ld` for later kernel stages.

The current M1 target only needs `nasm`, `make`, `dd`, `od`, `tail`, `tr`, and QEMU. The image is a raw 1.44 MiB disk image with sector 0 occupied by the MBR. No libc or external bootloader is used by the image.

## Build and run

```sh
make
./scripts/run-qemu.sh build/hadeed.img
# equivalent:
make run
```

Exact QEMU invocation:

```sh
qemu-system-x86_64 \
  -machine pc,accel=tcg \
  -m 64M \
  -drive format=raw,file=build/hadeed.img,if=ide,index=0,media=disk \
  -boot c \
  -no-reboot -no-shutdown
```

Expected M1 VGA output:

```text
                                    HADEED
```

## Layout

```text
boot/bootloader.asm   512-byte MBR source
kernel/entry.asm      M2 64-bit entry skeleton
kernel/main.c         M2 freestanding VGA C skeleton
kernel/linker.ld      Kernel physical placement at 1 MiB
docs/boot_flow.md     BIOS-to-shell flow and disk/header convention
docs/decisions.md     IA-32e, paging, allocator, syscall choices
docs/milestones.md    M1–M5 acceptance tests and two-week plan
scripts/run-qemu.sh   Raw image QEMU command
```

## Decisions

- Real mode directly into IA-32e: PAE + LME + PML4 + CR0.PG, then far jump.
- 2 MiB identity bootstrap mapping plus 4 KiB mapped higher-half heap pages.
- Bitmap-backed physical buddy allocator before any slab layer.
- `syscall`/`sysret` for future ring-3 ABI; shell commands are direct kernel calls.

See the corresponding documents under `docs/`.

## License

MIT.
