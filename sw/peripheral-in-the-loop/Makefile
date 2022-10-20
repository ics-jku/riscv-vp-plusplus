virtual-bus-path=../../vp/src/platform/hwitl/virtual-bus

all : main.c
	riscv32-unknown-elf-gcc main.c bootstrap.S -o main -march=rv32i -mabi=ilp32 -nostartfiles -Wl,--no-relax

real-sim: all
	hwitl-vp main /dev/ttyUSB0

sim: all
	make -C $(virtual-bus-path) all
	make -C $(virtual-bus-path) pipe_left pipe_right
	$(virtual-bus-path)/responder-cli $(virtual-bus-path)/pipe_right &
	hwitl-vp main --virtual-bus-device $(virtual-bus-path)/pipe_left
	killall responder-cli
	killall socat
clean:
	rm -f main

