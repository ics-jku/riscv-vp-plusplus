# QEMU-Virt style platform VPs (RV32/RV64; single/multi(4) core, and RV64Y single core)

Implements components necessary to boot for example a FreeBSD or CheriBSD system:
 * Core(s), MMU, Bus, CLINT, PLIC, Memory, Bus, ns16550a UART (console), Sifive_test (system halt)

All other qemu-virt peripherals (fw-cfg, flash, rtc, pci and virtio_mmio) are integrated simply as DUMMY_TLM_TARGET devices
 * see vp/src/platform/common/dummy_tlm_target.h
 * all writes are ignored, all reads return 0
 * debug messages are enabled by default. (see --dummy-tlm-target-debug above) -> Disabling the messages is save and has no negative effect

NOTE: Peripherals known from other RISC-V VP++ platforms, such as syscall emulation or networking via UART SLIP are currently NOT supported.

For `qemu_virt64y-sc-vp`, stop gracefully with Ctrl-A followed by `q` to print
the number of valid capabilities remaining in RAM, and instruction statistics if
the ISS is built with `ISS_CT_STATS_ENABLED` (see vp/src/core/rv64y/iss_ctemplate.h).
Ctrl-A followed by `f` toggles the enforcement of CHERI exceptions at runtime.
Instruction totals count dispatched instructions, including instructions that trap,
independently of the guest's `instret` CSR. CHERI totals and per-opcode counts cover
explicit capability instructions, including capability atomics; ordinary opcodes
with capability-mode behavior are excluded. The RAM total counts set tags (one
per 16-byte slot), including copies and excluding registers, once after simulation
stops. Ctrl-A followed by `x` or Ctrl-X exits immediately without the final report.
