NPROCS:=$(shell grep -c ^processor /proc/cpuinfo)

# We are duplicating the CMake logic here, we should get rid of the
# Makefile alltogether and simply build the entire thing inlcuding
# vp/dependencies from CMake.
USE_SYSTEM_SYSTEMC ?= OFF
ifeq ($(USE_SYSTEM_SYSTEMC),ON)
	SYSTEMC_DEPENDENCY =
else
	SYSTEMC_DEPENDENCY = vp/dependencies/systemc-dist
endif

vps: vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c $(SYSTEMC_DEPENDENCY) vp/dependencies/softfloat-dist vp/build/Makefile
	make install -C vp/build -j$(NPROCS)

vp/dependencies/systemc-dist:
	cd vp/dependencies/ && ./build_systemc_233.sh

vp/dependencies/softfloat-dist:
	cd vp/dependencies/ && ./build_softfloat.sh
	
vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c:
	git submodule update --init vp/src/core/common/gdb-mc/libgdb/mpc

all: vps vp-display vp-breadboard

vp/build/Makefile:
	mkdir -p vp/build
	cd vp/build && cmake -DUSE_SYSTEM_SYSTEMC=$(USE_SYSTEM_SYSTEMC) ..

vp-eclipse:
	mkdir -p vp-eclipse
	cd vp-eclipse && cmake ../vp/ -G "Eclipse CDT4 - Unix Makefiles"

env/basic/vp-display/build/Makefile:
	mkdir -p env/basic/vp-display/build
	cd env/basic/vp-display/build && cmake ..

vp-display: env/basic/vp-display/build/Makefile
	make -C  env/basic/vp-display/build -j$(NPROCS)

env/hifive/vp-breadboard/build/Makefile:
	mkdir -p env/hifive/vp-breadboard/build
	cd env/hifive/vp-breadboard/build && cmake ..

vp-breadboard: env/hifive/vp-breadboard/build/Makefile
	make -C  env/hifive/vp-breadboard/build -j$(NPROCS)

vp-clean:
	rm -rf vp/build

qt-clean:
	rm -rf env/basic/vp-display/build
	rm -rf env/hifive/vp-breadboard/build

sysc-clean:
	rm -rf vp/dependencies/systemc*

softfloat-clean:
	rm -rf vp/dependencies/softfloat-dist

clean-all: vp-clean qt-clean sysc-clean softfloat-clean

clean: vp-clean

codestyle:
	find . -type d \( -name .git -o -name dependencies \) -prune -o -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -print | xargs clang-format -i -style=file
