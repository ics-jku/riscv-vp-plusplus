# QEMU-Virt style platform VPs (RV32/RV64; single/multi(4) core)

Implements components necessary to boot for example a FreeBSD system:
 * Core(s), MMU, Bus, CLINT, PLIC, Memory, Bus, ns16550a UART (console), Sifive_test (system halt)

All other qemu-virt peripherals (fw-cfg, flash, rtc, pci and virtio_mmio) are integrated simply as DUMMY_TLM_TARGET devices
 * see vp/src/platform/common/dummy_tlm_target.h
 * all writes are ignored, all reads return 0
 * debug messages are enabled by default. (see --dummy-tlm-target-debug above) -> Disabling the messages is save and has no negative effect

NOTE: Peripherals known from other RISC-V VP++ platforms, such as syscall emulation or networking via UART SLIP are currently NOT supported.
