# Integration Tests

Tests for peripherals and other utilities of the [riscv-vp][riscv-vp gitlab].

## Dependencies

* riscv-vp binaries must be in `$PATH`
* GCC cross toolchain (see below)
* GDB with RISC-V support
* CMake

### Notes on toolchain

The tests are supposed to work with both RV32 and RV64. As such, testing
requires a toolchain for both targets for the `riscv-unknown-elf` triple
with newlib support. The easiest way to build these two toolchains is
using the RISC-V [riscv-gnu-toolchain repository][riscv-gnu-toolchain
github]. Currently, it seems to be necessary to compile desperate
compilers for RV32 and RV64. This can be achieved by building a cross
toolchain twice.

For RV32:

	./configure --prefix=/opt/rv32 --disable-gdb \
		--disable-linux --with-arch=rv32imafdc --with-abi=ilp32

For RV64:

	./configure --prefix=/opt/rv64 --disable-gdb \
		--disable-linux --with-arch=rv64imafdc --with-abi=lp64

Afterwards both toolchains have to be added to `$PATH`, e.g. using:

	export PATH="$PATH:/opt/rv32/bin:/opt/rv64/bin"

[riscv-vp gitlab]: https://gitlab.informatik.uni-bremen.de/riscv/riscv-vp
[riscv-gnu-toolchain github]: https://github.com/riscv/riscv-gnu-toolchain
