# Research Synthesis — 2026-10-07

## Papers read for this repository

- **Morello-Cerise: A Proof of Strong Encapsulation for the Arm Morello Capability Hardware Architecture** — PLDI 2025, DOI 10.1145/3729329. Mechanism: CHERI/Morello architectural capabilities plus mechanised proofs over the production-scale ISA model.
- **Enhancing IPC Performance in Microkernels Through Watchpoints** — ICCWAMTIP 2025, DOI 10.1109/ICCWAMTIP68645.2025.11352706. The supplied 2026 year is incorrect. Mechanism: ARMv8 hardware watchpoints, kernel-address-space service placement, gate-code syscall redirection, RPC, and preinitialised resource pools.
- **HSEB: optimizing microkernel performance for network I/O-intensive applications via High-Speed Event Bus mechanism** — Cluster Computing 2025, DOI 10.1007/s10586-025-05758-3. Mechanism: asynchronous IPC using coroutines and publish/subscribe event dispatch.
- **A Compact SHA256 Accelerator in 22nm for Energy Bounded Use-Cases with 8.2GHash/J** — ISCAS 2025, DOI 10.1109/ISCAS56072.2025.11043597. Mechanism is a 22nm CMOS SHA256 datapath with pipelining and a hybrid shift-FIFO.

## Changes committed

- None. The repository is at boot/kernel-entry skeleton stage and has no user processes, scheduler, IPC path, capability machine, or cryptographic subsystem on which these mechanisms can be implemented faithfully.

## Papers read but rejected

- **Morello-Cerise** — rejected: requires CHERI/Arm Morello capability hardware and proof infrastructure; x86-64 C/assembly cannot reproduce the hardware capability semantics.
- **Watchpoint IPC** — rejected: requires ARMv8 watchpoint behavior and an existing microkernel IPC path; neither exists in Hadeed.
- **HSEB** — rejected: requires a functioning process/scheduler/IPC substrate and coroutine/event runtime; adding it now would build unrelated prerequisites rather than implement the paper.
- **SHA256 accelerator** — rejected: the frequency/energy gains come from CMOS datapath, latch, pipeline, and FIFO design, not a C/x86-64 software transformation.

## Papers requiring operator decision

- None.
