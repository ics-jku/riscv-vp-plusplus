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
RISC-V VP++ Device Tree Generator for qemu-virt VPs
(C) 2025 Manfred Schlaegl <manfred.schlaegl@jku.at>, Institute for Complex Systems, JKU Linz
""")

argp = argparse.ArgumentParser()
argp.add_argument("-t", "--target",
                  help = "vp variant",
                  choices = ["qemu_virt32-sc-vp", "qemu_virt32-vp", "qemu_virt64-sc-vp", "qemu_virt64-vp"],
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
cfg.RISCV_ISA_EXTENSIONS_CPU0 = ["i", "m", "a", "c", "zicntr", "zicsr", "zifencei"]
cfg.RISCV_ISA_EXTENSIONS = ["i", "m", "a", "f", "d", "c", "v", "zicntr", "zicsr", "zifencei"]


if args.target == "qemu_virt32-sc-vp":
    cfg.RISCV_ISA_BASE = "rv32i"
    cfg.NUM_CORES = 1
elif args.target == "qemu_virt32-vp":
    cfg.RISCV_ISA_BASE = "rv32i"
    cfg.NUM_CORES = 4
elif args.target == "qemu_virt64-sc-vp":
    cfg.RISCV_ISA_BASE = "rv64i"
    cfg.NUM_CORES = 1
elif args.target == "qemu_virt64-vp":
    cfg.RISCV_ISA_BASE = "rv64i"
    cfg.NUM_CORES = 4
else:
    print("Internal error: Invalid target: \"" + str(args.target) + "\"!")
    sys.exit(1)


if cfg.RISCV_ISA_BASE == "rv32i":
    cfg.MMU_TYPE = "riscv,sv32"
    cfg.MRAM_SIZE = "0x4000000"   # 64MiB
    if cfg.MEM_SIZE is None:
        cfg.MEM_SIZE = 1*1024**3  # 1GiB
elif cfg.RISCV_ISA_BASE == "rv64i":
    cfg.MMU_TYPE="riscv,sv39"
    cfg.MRAM_SIZE="0x20000000"    # 512MiB
    if cfg.MEM_SIZE is None:
        cfg.MEM_SIZE = 2*1024**3  # 2GiB
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


cfg.RISCV_ISA_CPU0_DT = \
    gen_riscv_isa_dt(
            cfg.RISCV_ISA_BASE,
            cfg.RISCV_ISA_EXTENSIONS_CPU0)
cfg.RISCV_ISA_DT = \
    gen_riscv_isa_dt(
            cfg.RISCV_ISA_BASE,
            cfg.RISCV_ISA_EXTENSIONS)

cfg.RISCV_ISA_EXTENSIONS_CPU0_DT = \
    gen_riscv_isa_extensions_dt(
            cfg.RISCV_ISA_BASE,
            cfg.RISCV_ISA_EXTENSIONS_CPU0)
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
	compatible = "sifive,u54-mc", "sifive,rocket0", "riscv";
	device_type = "cpu";
	mmu-type = "@MMU_TYPE@";
	reg = <@CPU_NR@>;
	riscv,isa = "@RISCV_ISA_DT@";
	riscv,isa-base = "@RISCV_ISA_BASE@";
	riscv,isa-extensions = @RISCV_ISA_EXTENSIONS_DT@;
	clock-frequency = <100000000>;
	cpu@CPU_NR@_intc: interrupt-controller {
		#interrupt-cells = <1>;
		compatible = "riscv,cpu-intc";
		interrupt-controller;
	};
};"""

cpu_map_base_dt = """\
core@CPU_NR@ {
	cpu = <&cpu@CPU_NR@>;
};"""


cfg.PLIC_INT_EXT = "\n\t\t\t\t<&cpu0_intc 0xffffffff>"
cfg.CLINT_INT_EXT = "\n\t\t\t\t<&cpu0_intc 3>, <&cpu0_intc 7>"
cfg.CPUS = ""
cfg.CPU_MAP = ""
for cfg.CPU_NR in range(1, cfg.NUM_CORES + 1):
    cfg.CPUS += cfg.apply(cpu_base_dt, line_prefix = "\t\t") + "\n"
    cfg.CPU_MAP += cfg.apply(cpu_map_base_dt, line_prefix = "\t\t\t\t") + "\n"
    cfg.CLINT_INT_EXT += ",\n\t\t\t\t<&cpu{cpu_nr}_intc 3>, <&cpu{cpu_nr}_intc 7>".format(
            cpu_nr = cfg.CPU_NR)
    cfg.PLIC_INT_EXT += ",\n\t\t\t\t<&cpu{cpu_nr}_intc 0xffffffff>, <&cpu{cpu_nr}_intc 9>".format(
            cpu_nr = cfg.CPU_NR)


#
# Full Device Tree generation
#

dt_base = """
// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * Copyright (c) 2022-2025 Manfred SCHLAEGL <manfred.schlaegl@gmx.at>
 * Copyright (c) 2018-2019 SiFive, Inc
 */

/dts-v1/;

/ {
	#address-cells = <2>;
	#size-cells = <2>;
	compatible = "sifive,fu540-c000", "sifive,fu540";

	aliases {
		serial0 = &uart0;
		serial1 = &uart1;
	};

	chosen {
		@BOOTARGS@
		stdout-path = &uart0;
	};

	cpus {
		#address-cells = <1>;
		#size-cells = <0>;

		timebase-frequency = <1000000>;

		cpu0: cpu@0 {
			compatible = "sifive,e51", "sifive,rocket0", "riscv";
			device_type = "cpu";
			reg = <0>;
			riscv,isa = "@RISCV_ISA_CPU0_DT@";
			riscv,isa-base = "@RISCV_ISA_BASE@";
			riscv,isa-extensions = @RISCV_ISA_EXTENSIONS_CPU0_DT@;
			clock-frequency = <100000000>;
			status = "disabled";
			cpu0_intc: interrupt-controller {
				#interrupt-cells = <1>;
				compatible = "riscv,cpu-intc";
				interrupt-controller;
			};
		};
@CPUS@
		cpu-map {
			cluster0 {
				core0 {
					cpu = <&cpu0>;
				};
@CPU_MAP@
			};
		};
	};

	memory@@MEM_START_HEX@ {
		device_type = "memory";
		reg = <@MEM_START_DT@ @MEM_SIZE_DT@>;
	};

	refclk: refclk {
		#clock-cells = <0>;
		compatible = "fixed-clock";
		clock-frequency = <33333333>;
		clock-output-names = "refclk";
	};

	soc {
		#address-cells = <2>;
		#size-cells = <2>;
		compatible = "simple-bus";
		ranges;

		test: test@100000 {
			reg = <0x0 0x100000 0x0 0x1000>;
			compatible = "sifive,test1", "sifive,test0", "syscon";
		};

		clint0: clint@2000000 {
			compatible = "sifive,fu540-c000-clint", "sifive,clint0";
			reg = <0x0 0x2000000 0x0 0xC000>;
			interrupts-extended = @CLINT_INT_EXT@;
		};

		plic0: interrupt-controller@c000000 {
			compatible = "sifive,fu540-c000-plic", "sifive,plic-1.0.0";
			reg = <0x0 0xc000000 0x0 0x4000000>;
			#address-cells = <0>;
			#interrupt-cells = <1>;
			interrupt-controller;
			interrupts-extended = @PLIC_INT_EXT@;
			riscv,ndev = <53>;
		};

		rng: hwrng@10001000 {
			compatible = "timeriomem_rng";
			reg = <0x0 0x10001000 0x0 0x4>;
			period = <1>;
			quality = <100>;
		};

		uart0: serial@10010000 {
			compatible = "sifive,fu540-c000-uart", "sifive,uart0";
			reg = <0x0 0x10010000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <4>;
			clocks = <&refclk>;
			current-speed = <115200>;
			status = "okay";
		};

		uart1: serial@10011000 {
			compatible = "sifive,fu540-c000-uart", "sifive,uart0";
			reg = <0x0 0x10011000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <5>;
			clocks = <&refclk>;
			current-speed = <115200>;
			status = "okay";
		};

		gpio: gpio@10060000 {
			compatible = "sifive,fu540-c000-gpio", "sifive,gpio0";
			interrupt-parent = <&plic0>;
			interrupts = <7>, <8>, <9>, <10>, <11>, <12>, <13>,
					<14>, <15>, <16>, <17>, <18>, <19>, <20>,
					<21>, <22>;
			reg = <0x0 0x10060000 0x0 0x1000>;
			gpio-controller;
			#gpio-cells = <2>;
			interrupt-controller;
			#interrupt-cells = <2>;
			clocks = <&refclk>;
			status = "okay";
		};

		i2c0: i2c@10030000 {
			compatible = "sifive,fu540-c000-i2c", "sifive,i2c0";
			reg = <0x0 0x10030000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <50>;
			//clocks = <&prci FU540_PRCI_CLK_TLCLK>;
			clocks = <&refclk>;
			reg-shift = <2>;
			reg-io-width = <1>;
			#address-cells = <1>;
			#size-cells = <0>;
			status = "okay";

			rtc@68 {
				compatible = "dallas,ds1307";
				reg = <0x68>;
				//interrupt-parent = <&gpio4>;
				//interrupts = <20 0>;
				//trickle-resistor-ohms = <250>;
			};
		};

		qspi0: spi@10040000 {
			/* spi flash interface not supported yet */
			compatible = "sifive,fu540-c000-spi", "sifive,spi0";
			reg = <0x0 0x10040000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <51>;
			clocks = <&refclk>;
			#address-cells = <1>;
			#size-cells = <0>;
			status = "okay";
		};

		qspi1: spi@10041000 {
			/* spi flash interface not supported yet */
			compatible = "sifive,fu540-c000-spi", "sifive,spi0";
			reg = <0x0 0x10041000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <52>;
			clocks = <&refclk>;
			#address-cells = <1>;
			#size-cells = <0>;
			status = "okay";
		};

		qspi2: spi@10050000 {
			compatible = "sifive,fu540-c000-spi", "sifive,spi0";
			reg = <0x0 0x10050000 0x0 0x1000>;
			interrupt-parent = <&plic0>;
			interrupts = <6>;
			clocks = <&refclk>;
			#address-cells = <1>;
			#size-cells = <0>;
			status = "okay";

			mmc@0 {
				compatible = "mmc-spi-slot";
				reg = <0>;
				spi-max-frequency = <20000000>;
				voltage-ranges = <3300 3300>;
				disable-wp;
				/* dt includes not handled yet */
				//gpios = <&gpio 11 GPIO_ACTIVE_LOW>;
				gpios = <&gpio 11 1>;
			};
		};

		framebuffer0: framebuffer@11000000 {
			compatible = "allwinner,simple-framebuffer", "simple-framebuffer";
			reg = <0x0 0x11000000 0x0 0x1000000>;
			width = <800>;
			height = <480>;
			stride = <1600>; /* width * byte/pixel */
			format = "r5g6b5";
		};

		simpleinputptr0: simpleinputptr@12000000 {
			reg = <0x0 0x12000000 0x0 0x0000fff>;
			interrupt-parent = <&plic0>;
			interrupts = <10>;
			compatible = "ics,simpleinputptr";
		};

		simpleinputkbd0: simpleinputkbd@12001000 {
			reg = <0x0 0x12001000 0x0 0x0000fff>;
			interrupt-parent = <&plic0>;
			interrupts = <11>;
			compatible = "ics,simpleinputkbd";
		};

		mram_rootfs: mram@40000000 {
			reg = <0x0 0x40000000 0x0 @MRAM_SIZE@>;
			bank-width = <4>;
			compatible = "mtd-ram";

			#address-cells = <2>;
			#size-cells = <2>;
			rootfs@0 {
				label = "rootfs";
				reg = <0x0 0x00000000 0x0 @MRAM_SIZE@>;
			};
		};

		mram_data: mram@60000000 {
			reg = <0x0 0x60000000 0x0 @MRAM_SIZE@>;
			bank-width = <4>;
			compatible = "mtd-ram";

			#address-cells = <2>;
			#size-cells = <2>;
			data@0 {
				label = "data";
				reg = <0x0 0x00000000 0x0 @MRAM_SIZE@>;
			};
		};
	};

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
};
"""
dt = cfg.apply(dt_base)

#
# Output
#
if args.output_file is not None:
    output_device = open(args.output_file, "w")
output_device.write(dt)
