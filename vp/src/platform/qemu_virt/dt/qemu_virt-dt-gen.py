#!/usr/bin/env python
# coding: utf-8

# (C) 2025 Manfred Schlaegl <manfred.schlaegl@jku.at>, Institute for Complex Systems, JKU Linz
#
# SPDX-License-Identifier: BSD 3-clause "New" or "Revised" License

import sys
import argparse

# redirect all output (except dts output) to stderr
output_device = sys.stdout
sys.stdout = sys.stderr

#
# Configuration handling
#

class Config(object):
    def __init__(self):
        pass

    def dump(self):
        for key, val in self.__dict__.items():
            print(str(key).ljust(40) + ":     " + str(val))

    def apply(self, in_str, line_prefix=""):
        out_str = in_str
        for key, val in self.__dict__.items():
            out_str = out_str.replace("@" + key + "@", str(val))
        out_str = line_prefix + line_prefix.join(out_str.splitlines(True))
        return out_str

cfg = Config()


#
# Parameter parsing
#

print("""\
RISC-V VP++ Device Tree Generator for qemu-virt VPs (based on DT exported from QEMU)
(C) 2025 Manfred Schlaegl <manfred.schlaegl@jku.at>, Institute for Complex Systems, JKU Linz
""")

argp = argparse.ArgumentParser()
argp.add_argument("-t", "--target",
                  help = "vp variant",
                  choices = ["qemu_virt32-sc-vp", "qemu_virt32-mc-vp", "qemu_virt64-sc-vp", "qemu_virt64-mc-vp"],
                  required = True)
argp.add_argument("-m", "--memory-start",
                  help = "memory start address (default: 0x80000000)",
                  default = "0x80000000")
argp.add_argument("-s", "--memory-size",
                  help = "memory size in bytes (default: rv32: 1GiB; rv64: 2GiB)")
argp.add_argument("-b", "--bootargs",
                  help = "bootargs string in choosen node (default: not set)")
argp.add_argument("-o", "--output-file",
                  help = "file to write the device-tree source (dts) (default: stdout)")
argp.add_argument("-q", "--quiet",
                  help = "quiet mode (default: false)",
                  action = "store_true")
args = argp.parse_args()


#
# Configuration
#

cfg.BOOTARGS = ""
if args.bootargs is not None:
    cfg.BOOTARGS = "bootargs = \"" + str(args.bootargs) + "\";"

cfg.MEM_SIZE = None
if args.memory_size is not None:
    cfg.MEM_SIZE = int(args.memory_size, 0)

cfg.MEM_START = int(args.memory_start, 0)

# fixed configuration
cfg.RISCV_ISA_EXTENSIONS = ["i", "m", "a", "f", "d", "c", "v", "zicntr", "zicsr", "zifencei"]


if args.target == "qemu_virt32-sc-vp":
    cfg.RISCV_ISA_BASE = "rv32i"
    cfg.NUM_CORES = 1
elif args.target == "qemu_virt32-mc-vp":
    cfg.RISCV_ISA_BASE = "rv32i"
    cfg.NUM_CORES = 4
elif args.target == "qemu_virt64-sc-vp":
    cfg.RISCV_ISA_BASE = "rv64i"
    cfg.NUM_CORES = 1
elif args.target == "qemu_virt64-mc-vp":
    cfg.RISCV_ISA_BASE = "rv64i"
    cfg.NUM_CORES = 4
else:
    print("Internal error: Invalid target: \"" + str(args.target) + "\"!")
    sys.exit(1)

if cfg.MEM_SIZE is None:
    cfg.MEM_SIZE = 2*1024**3  # 2GiB

if cfg.RISCV_ISA_BASE == "rv32i":
    cfg.MMU_TYPE = "riscv,sv32"
    cfg.PCI_RANGES = "<0x1000000 0x00 0x00 0x00 0x3000000 0x00 0x10000 0x2000000 0x00 0x40000000 0x00 0x40000000 0x00 0x40000000 0x3000000 0x03 0x00 0x03 0x00 0x01 0x00>"
elif cfg.RISCV_ISA_BASE == "rv64i":
    cfg.MMU_TYPE = "riscv,sv57"
    cfg.PCI_RANGES = "<0x1000000 0x00 0x00 0x00 0x3000000 0x00 0x10000 0x2000000 0x00 0x40000000 0x00 0x40000000 0x00 0x40000000 0x3000000 0x04 0x00 0x04 0x00 0x04 0x00>"
else:
    print("Internal error: invalid ISA base \"" + str(cfg.RISCV_ISA_BASE) + "\"!")
    sys.exit(1)


# Function to generate RISC-V ISA string
# Args:
#  * isa_base: ISA base (e.g., "rv32i")
#  * isa_ext: ISA extensions (e.g., ["i", "m", "a", "c", "zicntr"])
# Returns:
#  * Resulting ISA string
def gen_riscv_isa_dt(isa_base, isa_ext):
    res = isa_base
    for e in isa_ext:
        if e == "i":
            continue
        if len(e) == 1:
            res += e
        else:
            res += "_" + e
    return res

# Function to generate RISC-V ISA extensions string
# Args:
#  * isa_base: ISA base (e.g., "rv32i")
#  * isa_ext: ISA extensions (e.g., ["i", "m", "a", "c", "zicntr"])
# Returns:
#  * Resulting ISA extensions string
def gen_riscv_isa_extensions_dt(isa_base, isa_ext):
    return "\"" + "\", \"".join(isa_ext) + "\""

# Function to generate memory size string
# Args:
#  * mem_size: Memory size as an integer
# Returns:
#  * Formatted memory size string
def gen_mem_size_dt(mem_size):
    high = mem_size >> 32  # Extract the upper 32 bits
    low = mem_size & ((1 << 32) - 1)  # Extract the lower 32 bits
    return "0x{high:X} 0x{low:08X}".format(high = high, low = low)


cfg.RISCV_ISA_DT = \
    gen_riscv_isa_dt(
            cfg.RISCV_ISA_BASE,
            cfg.RISCV_ISA_EXTENSIONS)

cfg.RISCV_ISA_EXTENSIONS_DT = \
    gen_riscv_isa_extensions_dt(
            cfg.RISCV_ISA_BASE,
            cfg.RISCV_ISA_EXTENSIONS)

cfg.MEM_START_HEX = "{mem_start:x}".format(mem_start = cfg.MEM_START)
cfg.MEM_START_DT = gen_mem_size_dt(cfg.MEM_START)
cfg.MEM_SIZE_DT = gen_mem_size_dt(cfg.MEM_SIZE)

if not args.quiet:
    print("Configuration: ")
    print("--------------")
    cfg.dump()
    print("--------------")


#
# CPU Entry generation
#

cpu_base_dt = """\
cpu@CPU_NR@: cpu@@CPU_NR@ {
	device_type = "cpu";
	reg = <@CPU_NR@>;
	status = "okay";
	compatible = "riscv";
	riscv,isa-extensions = @RISCV_ISA_EXTENSIONS_DT@;
	riscv,isa-base = "@RISCV_ISA_BASE@";
	riscv,isa = "@RISCV_ISA_DT@";
	clock-frequency = <100000000>;
	mmu-type = "@MMU_TYPE@";

	cpu@CPU_NR@_intc: interrupt-controller {
		#interrupt-cells = <0x01>;
		interrupt-controller;
		compatible = "riscv,cpu-intc";
	};
};"""

cpu_map_base_dt = """\
core@CPU_NR@ {
	cpu = <&cpu@CPU_NR@>;
};"""


cfg.PLIC_INT_EXT = ""
cfg.CLINT_INT_EXT = ""
cfg.CPUS = ""
cfg.CPU_MAP = ""
for cfg.CPU_NR in range(0, cfg.NUM_CORES):
    cfg.CPUS += cfg.apply(cpu_base_dt, line_prefix = "\t\t") + "\n"
    cfg.CPU_MAP += cfg.apply(cpu_map_base_dt, line_prefix = "\t\t\t\t") + "\n"
    cfg.CLINT_INT_EXT += "\n\t\t\t\t<&cpu{cpu_nr}_intc 0x03>, <&cpu{cpu_nr}_intc 0x07>,".format(
            cpu_nr = cfg.CPU_NR)
    cfg.PLIC_INT_EXT += "\n\t\t\t\t<&cpu{cpu_nr}_intc 0x0b>, <&cpu{cpu_nr}_intc 0x09>,".format(
            cpu_nr = cfg.CPU_NR)
cfg.CLINT_INT_EXT = cfg.CLINT_INT_EXT[:-1]
cfg.PLIC_INT_EXT = cfg.PLIC_INT_EXT[:-1]

#
# Full Device Tree generation
#

dt_base = """\
// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * Copyright (c) 2025 Manfred SCHLAEGL <manfred.schlaegl@gmx.at>
 *
 * based on QEMU DT export for rv32/rv64, and 1(sc) and 4(mc) cpus:
 * ```
 * qemu-system-riscv<32|64> -M virt -m 2048 -smp cpus=<1|4> -nographic -machine dumpdtb=qemu.dtb
 * ```
 * with adaptations for RISC-V VP++ qemu_virt:
 *  * commented out components not supported by RISC-V VP++
 *  * added bootargs to chosen node
 *  * configurable memory size and location
 *  * properties in "cpus" adapted to RISC-V VP++ ISS and CLINT
 */

/dts-v1/;

/ {
	#address-cells = <0x02>;
	#size-cells = <0x02>;
	compatible = "riscv-virtio";
	model = "riscv-virtio,qemu";

	poweroff {
		value = <0x5555>;
		offset = <0x00>;
		regmap = <&test>;
		compatible = "syscon-poweroff";
	};

	reboot {
		value = <0x7777>;
		offset = <0x00>;
		regmap = <&test>;
		compatible = "syscon-reboot";
	};

	platform-bus@4000000 {
		interrupt-parent = <&plic>;
		ranges = <0x00 0x00 0x4000000 0x2000000>;
		#address-cells = <0x01>;
		#size-cells = <0x01>;
		compatible = "qemu,platform", "simple-bus";
	};

	memory@@MEM_START_HEX@ {
		device_type = "memory";
		reg = <@MEM_START_DT@ @MEM_SIZE_DT@>;
	};

	cpus {
		#address-cells = <0x01>;
		#size-cells = <0x00>;

		timebase-frequency = <1000000>;

@CPUS@
		cpu-map {
			cluster0 {
@CPU_MAP@
			};
		};
	};

	pmu {
		riscv,event-to-mhpmcounters = <0x01 0x01 0x7fff9 0x02 0x02 0x7fffc 0x10019 0x10019 0x7fff8 0x1001b 0x1001b 0x7fff8 0x10021 0x10021 0x7fff8>;
		compatible = "riscv,pmu";
	};

/*
	// NOT SUPPORTED IN RISC-V VP++
	fw-cfg@10100000 {
		dma-coherent;
		reg = <0x00 0x10100000 0x00 0x18>;
		compatible = "qemu,fw-cfg-mmio";
	};
*/

/*
	// NOT SUPPORTED IN RISC-V VP++
	flash@20000000 {
		bank-width = <0x04>;
		reg = <0x00 0x20000000 0x00 0x2000000 0x00 0x22000000 0x00 0x2000000>;
		compatible = "cfi-flash";
	};
*/

	aliases {
		serial0 = "/soc/serial@10000000";
	};

	chosen {
		@BOOTARGS@
		stdout-path = "/soc/serial@10000000";
		rng-seed = <0xb6c7b060 0x8c471ce0 0xbcc9cdf8 0xe56bbd 0x8f629034 0xb9b4c9d2 0x545f8145 0xd80458e3>;
	};

	soc {
		#address-cells = <0x02>;
		#size-cells = <0x02>;
		compatible = "simple-bus";
		ranges;

/*
		// NOT SUPPORTED IN RISC-V VP++
		rtc@101000 {
			interrupts = <0x0b>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x101000 0x00 0x1000>;
			compatible = "google,goldfish-rtc";
		};
*/

		serial@10000000 {
			interrupts = <0x0a>;
			interrupt-parent = <&plic>;
			clock-frequency = "", "8@";
			reg = <0x00 0x10000000 0x00 0x100>;
			compatible = "ns16550a";
		};

		test: test@100000 {
			reg = <0x00 0x100000 0x00 0x1000>;
			compatible = "sifive,test1", "sifive,test0", "syscon";
		};

/*
		// NOT SUPPORTED IN RISC-V VP++
		virtio_mmio@10008000 {
			interrupts = <0x08>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10008000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10007000 {
			interrupts = <0x07>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10007000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10006000 {
			interrupts = <0x06>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10006000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10005000 {
			interrupts = <0x05>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10005000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10004000 {
			interrupts = <0x04>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10004000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10003000 {
			interrupts = <0x03>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10003000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10002000 {
			interrupts = <0x02>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10002000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};

		virtio_mmio@10001000 {
			interrupts = <0x01>;
			interrupt-parent = <&plic>;
			reg = <0x00 0x10001000 0x00 0x1000>;
			compatible = "virtio,mmio";
		};
*/

		plic: plic@c000000 {
			phandle = <0x03>;
			riscv,ndev = <0x5f>;
			reg = <0x00 0xc000000 0x00 0x600000>;
			interrupts-extended = @PLIC_INT_EXT@;
			interrupt-controller;
			compatible = "sifive,plic-1.0.0", "riscv,plic0";
			#address-cells = <0x00>;
			#interrupt-cells = <0x01>;
		};

		clint@2000000 {
			interrupts-extended = @CLINT_INT_EXT@;
			reg = <0x00 0x2000000 0x00 0x10000>;
			compatible = "sifive,clint0", "riscv,clint0";
		};

/*
		// NOT SUPPORTED IN RISC-V VP++
		pci@30000000 {
			interrupt-map-mask = <0x1800 0x00 0x00 0x07>;
			interrupt-map =
				<0x00 0x00 0x00 0x01 &plic 0x20>,
				<0x00 0x00 0x00 0x02 &plic 0x21>,
				<0x00 0x00 0x00 0x03 &plic 0x22>,
				<0x00 0x00 0x00 0x04 &plic 0x23>,
				<0x800 0x00 0x00 0x01 &plic 0x21>,
				<0x800 0x00 0x00 0x02 &plic 0x22>,
				<0x800 0x00 0x00 0x03 &plic 0x23>,
				<0x800 0x00 0x00 0x04 &plic 0x20>,
				<0x1000 0x00 0x00 0x01 &plic 0x22>,
				<0x1000 0x00 0x00 0x02 &plic 0x23>,
				<0x1000 0x00 0x00 0x03 &plic 0x20>,
				<0x1000 0x00 0x00 0x04 &plic 0x21>,
				<0x1800 0x00 0x00 0x01 &plic 0x23>,
				<0x1800 0x00 0x00 0x02 &plic 0x20>,
				<0x1800 0x00 0x00 0x03 &plic 0x21>,
				<0x1800 0x00 0x00 0x04 &plic 0x22>;
			ranges = @PCI_RANGES@;
			reg = <0x00 0x30000000 0x00 0x10000000>;
			dma-coherent;
			bus-range = <0x00 0xff>;
			linux,pci-domain = <0x00>;
			device_type = "pci";
			compatible = "pci-host-ecam-generic";
			#size-cells = <0x02>;
			#interrupt-cells = <0x01>;
			#address-cells = <0x03>;
		};
*/
	};
};
"""
dt = cfg.apply(dt_base)

#
# Output
#
if args.output_file is not None:
    output_device = open(args.output_file, "w")
output_device.write(dt)
