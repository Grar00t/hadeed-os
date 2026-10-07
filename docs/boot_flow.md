# Boot flow and disk layout

## Control flow

```text
BIOS reset
  -> BIOS selects disk and loads LBA 0 to physical 0x7C00
  -> boot/bootloader.asm (16-bit real mode, DL boot drive)
      -> establish known DS/ES/SS/SP and clear direction flag
      -> M1: write banner into VGA text memory, halt
      -> M2+: A20 enable; INT 13h Extensions AH=42h loads stage1 sectors
      -> stage1 16-bit loader at physical 0x8000
          -> CPUID long-mode/NX capability checks
          -> build temporary GDT, IDT and 4-level paging structures
          -> set CR4.PAE
          -> set IA32_EFER.LME (and NXE when NX is used)
          -> load CR3 with PML4 physical address
          -> set CR0.PG then far jump to 64-bit code selector
      -> kernel/entry.asm (64-bit)
          -> install final GDT, TSS, IDT
          -> initialize physical frame allocator and heap
          -> remap 8259 PIC; program PIT; enable IRQ0/IRQ1
          -> initialize VGA console, keyboard line editor and shell
          -> sti; shell loop
```

Firmware disk reads use BIOS INT 13h Extensions `AH=42h` with a Disk Address Packet (DAP), not CHS. Enhanced Disk Drive Services provides LBA access through the INT 13h extensions. [web:20][web:22] The stage0 M1 sector intentionally has no disk read path; M2 introduces the DAP loader.

The IA-32e transition is: with paging disabled, enable CR4.PAE; set `IA32_EFER.LME`; load CR3 with a valid PML4; set CR0.PG; then perform a far branch to load a 64-bit code segment. Intel SDM Volume 3A, **9.8.5 Initializing IA-32e Mode** is the implementation authority. [web:16][web:21]

## Initial disk layout

| LBA | Count | Content | Load address | Notes |
|---:|---:|---|---:|---|
| 0 | 1 | MBR boot sector | `0x00007C00` | Ends with `0x55AA`; no partition table during development |
| 1 | 1 | Stage1 header | `0x00008000` | Magic, ABI version, sector count, kernel physical load address, entry offset, checksum |
| 2.. | `kernel_sectors` | Flat kernel payload | `0x00100000` | Produced from linked ELF `PT_LOAD` content, not loaded as ELF by the bootloader |
| next | optional | initrd / test data | profile-defined | Not part of M1–M5 |

The MBR loads a fixed number of stage1/header sectors only after the header convention exists. The stage1 header has fixed 32-byte layout:

```text
0x00  u32 magic = 0x48414445      // "HADE"
0x04  u16 version = 1
0x06  u16 header_bytes = 32
0x08  u32 kernel_lba_low          // initial format supports <2^32 LBA
0x0C  u32 kernel_sector_count
0x10  u32 kernel_phys = 0x00100000
0x14  u32 kernel_entry_phys
0x18  u32 payload_bytes
0x1C  u32 crc32
```

### Size discovery decision

| Option | Tradeoff |
|---|---|
| Fixed sector count compiled into sector 0 | Fits simple stage0, but every kernel size change requires changing/reassembling stage0 and risks stale count corruption |
| Fixed one-sector header at LBA 1 with count/CRC | Costs a header sector and stage0 validation code, but kernel image size is self-describing and validated |

**Pick: one-sector stage1 header.** Sector 0 always loads LBA 1, validates its magic/version/count bounds, then uses its DAP count to load the payload. The header is generated from the linked payload by the Makefile. Stage0 is never asked to parse ELF.

## Address plan before final VM

```text
0x00000500..0x00007BFF  BIOS conventional memory scratch; do not allocate blindly
0x00007C00..0x00007DFF  MBR
0x00008000..0x0000FFFF  stage1/header workspace
0x00010000..0x0009FFFF  temporary loader workspace / BIOS-safe low memory window
0x00100000..             kernel payload and early page tables
```

The loader gets E820 memory-map data before discarding BIOS services. It reserves all loaded image ranges, page-table pages, low loader memory and the VGA aperture before populating the frame allocator.
