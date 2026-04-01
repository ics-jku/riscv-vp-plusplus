MAKEFLAGS += --no-print-directory

# Whether to use a system-wide SystemC library instead of the vendored one.
USE_SYSTEM_SYSTEMC ?= OFF
# Release build on by default
RELEASE_BUILD ?= ON
ifeq ($(RELEASE_BUILD),ON)
	CMAKE_BUILD_TYPE = Release
else
	CMAKE_BUILD_TYPE = Debug
endif

vps: vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c vp/build/Makefile
	$(MAKE) install -C vp/build

vp/src/core/common/gdb-mc/libgdb/mpc/mpc.c:
	git submodule update --init vp/src/core/common/gdb-mc/libgdb/mpc

all: vps vp-display gd32-breadboard

vp/build/Makefile:
	mkdir -p vp/build
	cd vp/build && cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DUSE_SYSTEM_SYSTEMC=$(USE_SYSTEM_SYSTEMC) ..

vp-eclipse:
	mkdir -p vp-eclipse
	cd vp-eclipse && cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) ../vp/ -G "Eclipse CDT4 - Unix Makefiles"

env/basic/vp-display/build/Makefile:
	mkdir -p env/basic/vp-display/build
	cd env/basic/vp-display/build && cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) ..

vp-display: env/basic/vp-display/build/Makefile
	$(MAKE) -C env/basic/vp-display/build

env/gd32/vp-breadboard/build/Makefile:
	mkdir -p env/gd32/vp-breadboard/build
	cd env/gd32/vp-breadboard/build && cmake ..

gd32-breadboard: env/gd32/vp-breadboard/build/Makefile
	$(MAKE) -C env/gd32/vp-breadboard/build

vp-clean:
	rm -rf vp/build

qt-clean:
	rm -rf env/basic/vp-display/build
	rm -rf env/gd32/vp-breadboard/build

tests-clean:
	rm -rf vp/tests/*/CMakeFiles
	rm -rf vp/tests/*/CMakeCache.txt
	rm -rf vp/tests/*/Makefile
	rm -rf vp/tests/*/*/*.cmake
	rm -rf vp/tests/*/*/CMakeFiles
	rm -rf vp/tests/gdb/*.cmake
	rm -rf vp/tests/gdb/*/Makefile

	rm -rf vp/tests/gdb/connect-and-quit/connect-and-quit
	rm -rf vp/tests/gdb/hit-breakpoint/hit-breakpoint
	rm -rf vp/tests/gdb/mc-info-threads/mc-info-threads
	rm -rf vp/tests/gdb/mc-wfi-thread/mc-wfi-thread
	rm -rf vp/tests/gdb/read-memory/read-memory
	rm -rf vp/tests/gdb/read-registers/read-registers
	rm -rf vp/tests/gdb/remove-breakpoint/remove-breakpoint
	rm -rf vp/tests/gdb/write-memory/write-memory
	rm -rf vp/tests/gdb/single-step/single-step
	rm -rf vp/tests/gdb/mc-dead-thread/mc-dead-thread
	rm -rf vp/tests/libgdb/build/
	rm -rf vp/tests/libgdb/libptest.a
	rm -rf vp/tests/libgdb/libgdb-tests
	rm -rf vp/tests/libgdb/*.cmake
	rm -rf vp/tests/integration/*/Makefile
	rm -rf vp/tests/integration/basic-c/basic-c
	rm -rf vp/tests/integration/basic-asm/basic-asm
	rm -rf vp/tests/integration/printf/printf
	rm -rf vp/tests/integration/basic-c++/basic-c++
	rm -rf vp/tests/integration/sys-read/sys-read
	rm -rf vp/tests/integration/mc-basic/mc-basic
	rm -rf sw/*/*.o
	rm -rf sw/*/main
	rm -rf sw/basic-gcov/main.gcda
	rm -rf sw/basic-gcov/main.gcno


clean-all: vp-clean qt-clean tests-clean

clean: vp-clean

codestyle:
	find 									\
		\(								\
			-name "*.h" -or						\
			-name "*.hpp" -or					\
			-name "*.c" -or						\
			-name "*.cpp"						\
		\) -and								\
		-not \(								\
			-path "./.git/*" -or					\
			-path "./env/gd32/vp-breadboard/LuaBridge3/*" -or	\
			-path "./vp/src/core/common/gdb-mc/libgdb/mpc/*" -or	\
			-path "./vp/src/platform/hifive/vbb-protocol/*" -or	\
			-path "./vp/src/platform/hwitl/virtual-bus/*" -or	\
			-path "./vp/src/vendor/*" -or				\
			-path "./vp/tests/libgdb/ptest/*" -or			\
			-path "./*/build/*"					\
		\)								\
		-print								\
		| xargs clang-format-19 -i -style=file

docker-debian:
	docker buildx build --progress=plain --target debian -t riscv-vp-plusplus:debian .

run-docker-debian:
	docker run -it riscv-vp-plusplus:debian /bin/bash