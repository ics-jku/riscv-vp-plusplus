MAKEFLAGS += --no-print-directory

# Whether to use a system-wide SystemC library instead of the vendored one.
USE_SYSTEM_SYSTEMC ?= OFF

vps: vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c vp/build/Makefile
	$(MAKE) install -C vp/build

vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c:
	git submodule update --init vp/src/core/common/gdb-mc/libgdb/mpc

all: vps vp-display

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
	$(MAKE) -C env/basic/vp-display/build

vp-clean:
	rm -rf vp/build

qt-clean:
	rm -rf env/basic/vp-display/build

clean-all: vp-clean qt-clean

clean: vp-clean

codestyle:
	find . -type d \( -name .git -o -name dependencies \) -prune -o -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -print | xargs clang-format -i -style=file
