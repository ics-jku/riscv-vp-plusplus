# GDB target descriptions

The GDB server sends these files to GDB via `qXfer:features:read`, so GDB knows which registers the target has. They are
embedded into the binary at build time (`../embed_xml.cmake` generates `gdb_xml_data.cpp`).

## Files

| File | Origin |
|---|---|
| `target-rv32.xml`, `target-rv32e.xml`, `target-rv64.xml`, `target-rv64e.xml`, `target-rv64y.xml` | Written for the VP. Top level description per target. Selected in `gdb_xml::target_file()` |
| `riscv-32bit-cpu.xml`, `riscv-64bit-cpu.xml` | Copy of GDB `gdb/features/riscv/{32,64}bit-cpu.xml`, explicit `regnum` for `pc` added |
| `riscv-32bit-cpu-e.xml`, `riscv-64bit-cpu-e.xml` | E base ISA: cpu file without x16-x31 (same as GDB's `rv32e-xregs.xml`, plus explicit `regnum` for `pc`).|
| `riscv-64bit-fpu.xml` | Copy of QEMU `gdb-xml/riscv-64bit-fpu.xml` (originally from GDB), explicit `regnum` for `ft0` added. Used for all targets, since all cores have FLEN=64 |
| `riscv-64bit-cheri.xml` | Copy from CHERI GDB / CHERI-QEMU, explicit `regnum` for `cnull` added |
| `riscv-32bit-virtual.xml`, `riscv-64bit-virtual.xml` | Written for the VP, follows QEMU. Virtual `priv` register |
| `riscv-32bit-csr.xml`, `riscv-64bit-csr.xml`, `riscv-64bit-csr-rvy.xml` | Written for the VP. CSR list as implemented by the cores.|
| `riscv-64bit-ycsr.xml` | Written for the VP, follows CHERI-QEMU (`riscv_gen_dynamic_ycsr_xml()`), including its pre CHERI 0.9.8.2 names (e.g. `mtvecc`) |

The files copied from GDB/QEMU keep their FSF copyright notice, as their license requires.

## Register numbers

Numbers are the same for all targets and match CHERI-QEMU (RV64 with D extension). They must match `GdbRegNum` in
`../gdb_regs.h`:

| Registers | Number |
|---|---|
| x0-x31, pc | 0-31, 32 |
| f0-f31 | 33-64 |
| priv | 65 |
| cnull-ct6, pcc, ddc (RVY) | 66-97, 98, 99 |
| CSRs | 100 + CSR address |
| YCSRs (RVY) | 4196 + CSR address |

## Changing files

- New files in this directory are picked up automatically (CMake glob). The build fails if a file includes
  (`xi:include`) a file that does not exist here.
- A CSR listed in a CSR file must be readable by `read_csr_gdb()` of the respective core.
