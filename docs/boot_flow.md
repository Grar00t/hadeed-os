# M2 boot flow

## Disk image

| LBA | Count | Content | Runtime address |
|---:|---:|---|---:|
| 0 | 1 | BIOS boot sector | `0x00007c00` |
| 1 | 8 | fixed-size padded stage1 | `0x00008000` |
| 9 | generated | flat kernel payload | bounce `0x00010000`, final `0x00100000` |

The build rejects a boot sector that is not exactly 512 bytes or does not end in `55 aa`. It rejects a stage1 larger than 4096 bytes and a kernel larger than 64 sectors for the current BIOS read path.

## Control flow

```text
BIOS
  -> load LBA0 at 0000:7c00
  -> sector0 initializes DS/ES/SS/SP
  -> INT 13h AH=42 loads 8 stage1 sectors from LBA1 to 0000:8000
  -> far jump 0000:8000
  -> stage1 INT 13h AH=42 loads kernel sectors from LBA9 to 1000:0000
  -> enable A20 through port 0x92
  -> clear page-table pages at 0x9000, 0xa000, 0xb000
  -> PML4[0] -> PDPT, PDPT[0] -> PD, PD[0] = 2 MiB present/rw/large
  -> lgdt with 32-bit data and 64-bit code descriptors
  -> CR4.PAE = 1
  -> CR3 = 0x9000
  -> IA32_EFER.LME = 1
  -> CR0.PE|PG = 1
  -> far jump to selector 0x18
  -> 64-bit stage1 copies rounded kernel sectors from 0x10000 to 0x100000
  -> jump 0x100000
  -> kernel_entry sets stack at 0x80000 and calls kmain
  -> kmain writes HADEED64 to VGA and LM64\n to debugcon
  -> hlt loop
```

## Bootstrap paging invariant

One 2 MiB page identity-maps physical/virtual `0x00000000..0x001fffff`. That range contains the boot code, stage1, page tables, bounce buffer, kernel destination at 1 MiB, VGA aperture at `0xb8000`, and the bootstrap stack. No address used before `kmain` lies outside that mapping.

## M2 observability

`make static-test` checks the sector size/signature, stage1 size bound, kernel size bound, and that the emitted kernel binary contains the committed `HADEED64` and `LM64\n` markers.

`make runtime-test` boots the image headless in QEMU, captures I/O port `0xe9`, compares the capture byte-for-byte with `LM64\n`, and rejects `triple fault` in the QEMU log.
