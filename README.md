# RISC-V VP++

*RISC-V VP++* is a extended and improved successor of the RISC-V based Virtual Prototype (VP) [RISC-V VP](https://github.com/agra-uni-bremen/riscv-vp).
It is maintained at the [Institute for Complex Systems](https://ics.jku.at/), Johannes Kepler University, Linz.

A BibTex entry to cite the paper presenting *RISC-V VP++*, [Manfred Schlägl, Christoph Hazott, and Daniel Große. RISC-V VP++: Next generation open-source virtual prototype. In Workshop on Open-Source Design Automation, 2024.](https://ics.jku.at/files/2024OSDA_RISCV-VP-plusplus.pdf), can be found in the last section.

### Key features of *RISC-V VP++*
 * Support for *Capability Hardware Enhanced RISC Instructions* (CHERI) RISC-V ISA, Version 9
   * *Manfred Schlägl, Andreas Hinterdorfer, and Daniel Große. A RISC-V CHERI VP: Enabling system-level evaluation of the capability-based CHERI architecture. In ASP-DAC, 2026.*
   * RV64 CHERIv9 ISS (+MMU, Tagged Memory Interface, ...) based on [Capability Hardware Enhanced RISC Instructions: CHERI Instruction-Set Architecture (Version 9)](https://www.cl.cam.ac.uk/techreports/UCAM-CL-TR-987.pdf)
     * includes interfaces for testing with [TestRIG](https://github.com/CTSRD-CHERI/TestRIG) (RVFI and DII)
   * RV64 CHERIv9 Platforms:
     * *qemu_virt64-cheriv9-sc-vp* and *linux64-cheriv9-sc-vp*
       * Similar to other (non-cheri) qemu_virt-style and linux(fu540-style) VP platforms, but with support for CHERIv9 RV64
       * Capable of running CheriBSD (purecap) from [cheribuild](https://github.com/CTSRD-CHERI/cheribuild) (tested with git-hash dbafb3f67e)
     * *tiny64-cheriv9-vp* and *riscv64-cheriv9-vp*
       * Similar to other (non-cheri) tiny/basic platforms, but with support for CHERIv9 RV64
       * Capable of running bare-metal Cheri (purecap) software
 * Support for RISC-V Half-Precision Floating-Point Extension (Zfh), Version 1.0
   (Can be enabled with the "--en-ext-Zfh" command line option on all platforms)
 * Fast Interpreter-Based ISS
   * [Manfred Schlägl and Daniel Große. Fast interpreter-based instruction set simulation for virtual prototypes, In DATE, 2025.](https://ics.jku.at/files/2025DATE_Fast_Interpreter-based_ISS.pdf)
   * DBBCache: Dynamic Basic Block Cache to speed up ISS instruction processing
   * LSCache: Direct translation of in-simulation virtual addresses to (dmi-capable) host system memory addresses to speed up ISS memory accesses
   * ISS optimizations based on DBBCache (and DBBCacheDummy): Computed goto, Threaded code, Fast/Medium/Slow-Path, Lazy/Approximate tlm quantum checks, Executed cycles and PC calculated on demand, On demand performance counters, ...
   * PLEASE NOTE: LSCache and DBBCache are included at compile time, but disabled at runtime by default!
     * Use the new command line switches "--use-dbbcache" and "--use-lscache" to enable the caches at runtime ("--use-lscache" will automatically enable DMI)
     * To completely disable the caches (replacing them with dummy implementations) at compile time un-define ```DBBCACHE_ENABLED``` and ```LSCACHE_ENABLED``` in ```vp/src/core/common/dbbcache.h``` and ```vp/src/core/common/lscache.h```, respectively
     * To enable the caches independent of command line switches define ```DBBCACHE_ENABLED``` + ```DBBCACHE_FORCED_ENABLED``` and ```LSCACHE_ENABLED``` + ```LSCACHE_FORCED_ENABLED``` in see ```vp/src/core/common/dbbcache.h``` and ```vp/src/core/common/lscache.h```, respectively
     * Some ISS optimisations based on DBBCache are active when the cache is disabled. However, significant performance improvements are only achieved when both caches are enabled!
 * Support for RV32E and RV64E
   (Can be enabled with the "--use-E-base-isa" command line option on riscv32-vp, riscv64-vp, microrv32-vp and all tiny* vp platforms)
 * Support for the *GD32VF103VBT6* microcontroller (*Nuclei N205*) including UI
   * More detailed information can be found [here](doc/GD32/README.md)
 * Support for *RISC-V "V" Vector Extension* (RVV) version 1.0
   * [Manfred Schlägl, Moritz Stockinger, and Daniel Große. A RISC-V "V" VP: Unlocking vector processing for evaluation at the system level. In DATE, 2024.](https://ics.jku.at/files/2024DATE_RISCV-VP-plusplus_RVV.pdf)
 * Full integration of [GUI-VP](https://github.com/ics-jku/GUI-VP), which enables the simulation of interactive graphical Linux applications
   * All further work on *GUI-VP* will take place here in *RISC-V VP++*
   * More detailed information can be found [here](doc/GUI-VP/README.md)
   * **Note: [GUI-VP Kit](https://github.com/ics-jku/GUI-VP_Kit) provides an easy-to-use build system and experimentation platform for RISC-V VP++, Linux and RVV**
 * Linux shutdown (stop simulation from within)
 * Linux RV32 and RV64, single and quad-core VPs
 * Harmonized coding style (make codestyle)
 * Based on [RISC-V VP](https://github.com/agra-uni-bremen/riscv-vp) (commit 9418b8abb5)

More related information can be found at http://www.systemc-verification.org/riscv-vp.


### Key features of the original *RISC-V VP*
(commit 9418b8abb5)

 - RV32GC and RV64GC core support (i.e. RV32IMAFDC and RV64IMAFDC)
 - Implemented in SystemC TLM-2.0
 - SW debug capabilities (GDB RSP interface) with Eclipse
 - Virtual Breadboard GUI (interactive IO) featuring C++ and Lua modeled digital devices (separate repository)
 - FreeRTOS, RIOT, Zephyr, Linux support
 - Generic and configurable bus
 - CLINT and PLIC-based interrupt controller + additional peripherals
 - Instruction-based timing model + annotated TLM 2.0 transaction delays
 - Peripherals, e.g. display, flash controller, preliminary ethernet
 - Example configuration for the SiFive HiFive1 (currently only Rev. A) board available
 - Support for simulation of multi-core platforms
 - Machine-, Supervisor- and User-mode (including user traps) privilege levels and CSRs
 - Virtual memory support (Sv32, Sv39, Sv48)

The original documentation of *RISC-V VP* can be found [here](doc/RISCV-VP/README.md)


#### 1) Build requirements

Mainly the usual build tools and boost is required:

On Debian 13, install these:
```bash
sudo apt install cmake autoconf automake autotools-dev clang-format-19 curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo libgoogle-perftools-dev libtool patchutils bc zlib1g-dev libexpat-dev libboost-iostreams-dev libboost-program-options-dev libboost-log-dev qtbase5-dev qt5-qmake libvncserver-dev nlohmann-json3-dev
```

On Fedora, following actions are required:
```bash
sudo dnf install autoconf automake curl libmpc-devel mpfr-devel gmp-devel gawk bison flex texinfo gperf libtool patchutils bc zlib-devel expat-devel cmake boost-devel qt5-qtbase qt5-qtbase-devel libvncserver-devel json-devel
sudo dnf groupinstall "C Development Tools and Libraries"
#optional debuginfo
sudo dnf debuginfo-install boost-iostreams boost-program-options boost-regex bzip2-libs glibc libgcc libicu libstdc++ zlib
```

#### 2) Build this RISC-V Virtual Prototype:

**Note:** By default the VPs are build without optmization and debug symbols.
To enable the optimizations set the environment variable `RELEASE_BUILD=ON`.

To create an optimized release build, type
```
RELEASE_BUILD=ON make vps
```

To create a debug build without optimisations, type
```
RELEASE_BUILD=OFF make vps
```

#### 3) Building SW examples using the GNU toolchain

##### Requirements

In order to test the software examples, a configured RISC-V GNU toolchain is required in your `$PATH`.
Several standard packages are required to build the toolchain.
For more information on prerequisites for the RISC-V GNU toolchain visit https://github.com/riscv/riscv-gnu-toolchain.
With the packages installed, the toolchain can be build as follows:

```bash
# in some source folder
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
git submodule update --init --recursive # this may take a while
./configure --prefix=$(pwd)/../riscv-gnu-toolchain-dist-rv32imac-ilp32 --with-arch=rv32imac --with-abi=ilp32
make -j$(nproc)
```

If wanted, move the `riscv-gnu-toolchain-dist-rv32imac-ilp32` folder to your `/opt/` folder and add it to your path in your `~/.bashrc`
(e.g. `PATH=$PATH:/opt/riscv-gnu-toolchain-dist-rv32imac-ilp32/bin`)

##### Running the examples

In *sw*:

```bash
cd simple-sensor    # can be replaced with different example
make                # (requires RISC-V GNU toolchain in PATH)
make sim            # (requires *riscv32-vp*, i.e. *vp/build/bin/riscv32-vp*, executable in PATH)
```

Please note, if *make* is called without the *install* argument in step 2, then the *riscv32-vp* executable is available in *vp/build/src/platform/basic/riscv32-vp*.



This will also copy the VP binaries into the *vp/build/bin* folder.

#### FAQ

**Q:** How do I exit the VP?

**A:** All VPs use the input TTY in raw mode and forward all control
characters to the guest. For this reason, one cannot use Ctrl-c to exit
the VP. Instead, press Ctrl-a to enter command mode and press Ctrl-x to
exit the VP.

**Q:** How do I emit a Ctrl-a control character?

**A:** Enter control mode using Ctrl-a and press Ctrl-a again to send a
literal Ctrl-a control character to the guest.

### *RISC-V VP++: Next generation open-source virtual prototype*

[Manfred Schlägl, Christoph Hazott, and Daniel Große. RISC-V VP++: Next generation open-source virtual prototype. In Workshop on Open-Source Design Automation, 2024.](https://ics.jku.at/files/2024OSDA_RISCV-VP-plusplus.pdf)

```
@inproceedings{SHG:2024,
  author =        {Manfred Schl{\"{a}}gl and Christoph Hazott and
                   Daniel Gro{\ss}e},
  booktitle =     {Workshop on Open-Source Design Automation},
  title =         {{RISC-V VP++}: Next Generation Open-Source Virtual
                   Prototype},
  year =          {2024},
}
```
