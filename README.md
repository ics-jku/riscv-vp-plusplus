# RISCV-VP++

*RISCV-VP++* is a extended and improved successor of the RISC-V based Virtual Prototype (VP) [RISC-V VP](https://github.com/agra-uni-bremen/riscv-vp).
It is maintained at the [Institute for Complex Systems](https://ics.jku.at/), Johannes Kepler University, Linz.

### Key features of *RISCV-VP++*
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

On Ubuntu 20, install these:
```bash
sudo apt-get install autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo libgoogle-perftools-dev libtool patchutils bc zlib1g-dev libexpat-dev libboost-iostreams-dev libboost-program-options-dev libboost-log-dev qt5-default libvncserver-dev
```

On Fedora, following actions are required:
```bash
sudo dnf install autoconf automake curl libmpc-devel mpfr-devel gmp-devel gawk bison flex texinfo gperf libtool patchutils bc zlib-devel expat-devel cmake boost-devel qt5-qtbase qt5-qtbase-devel libvncserver-devel
sudo dnf groupinstall "C Development Tools and Libraries"
#optional debuginfo
sudo dnf debuginfo-install boost-iostreams boost-program-options boost-regex bzip2-libs glibc libgcc libicu libstdc++ zlib
```

#### 2) Build this RISC-V Virtual Prototype:

**Note:** By default the VPs are build without optmization and debug symbols.
To enable the optimizations set the environment variable `RELEASE_BUILD=ON`.

To create a debug build without optimisations, type
```
make vps
```

To create an optimized release build, type
```
RELEASE_BUILD=ON make vps
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
make sim            # (requires *riscv-vp*, i.e. *vp/build/bin/riscv-vp*, executable in PATH)
```

Please note, if *make* is called without the *install* argument in step 2, then the *riscv-vp* executable is available in *vp/build/src/platform/basic/riscv-vp*.



This will also copy the VP binaries into the *vp/build/bin* folder.

#### Alternative Setup: Docker

Instead of compiling the riscv-vp manually, a `Dockerfile` is also
provided which eases this process. In order to build a Docker image from
this file run the following command:

	$ docker build -t riscv-vp .

Afterwards, start a new Docker container using:

	$ docker run --rm -it riscv-vp

Within this Docker container, the riscv-vp source tree is available in
`/home/riscv-vp/riscv-vp/`. A RISC-V cross compiler toolchain is also
part of the container. As such, it is possible to compile and run any of
the examples from the `./sw` subdirectory in this container. For
example:

	$ cd /home/riscv-vp/riscv-vp/sw/basic-c/
	$ make
	$ make sim

#### FAQ

**Q:** How do I exit the VP?

**A:** All VPs use the input TTY in raw mode and forward all control
characters to the guest. For this reason, one cannot use Ctrl-c to exit
the VP. Instead, press Ctrl-a to enter command mode and press Ctrl-x to
exit the VP.

**Q:** How do I emit a Ctrl-a control character?

**A:** Enter control mode using Ctrl-a and press Ctrl-a again to send a
literal Ctrl-a control character to the guest.

