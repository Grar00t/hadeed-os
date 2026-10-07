# Milestones

## M1 — sector 0 executes

Completed before M2. The BIOS boot sector established the initial executable image and `55 aa` signature.

## M2 — kernel reaches IA-32e mode

Input: `build/hadeed.img` containing sector0, eight padded stage1 sectors, and a flat kernel payload.

Required target behavior:

- sector0 loads stage1 through BIOS INT 13h extensions;
- stage1 loads the kernel into a low-memory bounce buffer;
- A20 is enabled;
- PML4/PDPT/PD create an identity-mapped 2 MiB bootstrap page;
- `CR4.PAE`, `CR3`, `IA32_EFER.LME`, and `CR0.PE|PG` are established before the far jump;
- selector `0x18` enters 64-bit code;
- the kernel payload is copied to `0x00100000`;
- `kernel_entry` invokes freestanding C;
- VGA receives `HADEED64`;
- debugcon receives the exact bytes `LM64\n`;
- the CPU enters a halt loop.

Acceptance commands:

```sh
make clean
make
make static-test
make runtime-test
```

Exact success lines:

```text
M2_STATIC_PASS
M2_RUNTIME_PASS
```

`runtime-test` compares the debugcon output byte-for-byte and fails if the QEMU log contains `triple fault`.

## M3 — PIT timer IRQ fires

Next milestone: install an IDT, remap the PIC, program PIT channel 0, enable IRQ0, and prove a deterministic timer interrupt trace from 64-bit kernel mode.
