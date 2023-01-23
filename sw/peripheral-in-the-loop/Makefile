virtual-bus-path=../../vp/src/platform/hwitl/virtual-bus
simple-sensor-path=../simple-sensor

all : main.c $(simple-sensor-path)/irq.c $(simple-sensor-path)/irq.h $(simple-sensor-path)/bootstrap.S
	riscv32-unknown-elf-gcc main.c $(simple-sensor-path)/irq.c $(simple-sensor-path)/bootstrap.S -g -o main -march=rv32i -mabi=ilp32 -nostartfiles -Wl,--no-relax

real-sim: all
	hwitl-vp main /dev/ttyUSB0

sim: all
	make -C $(virtual-bus-path) all
	make -C $(virtual-bus-path) pipe_left pipe_right
	$(virtual-bus-path)/responder-cli $(virtual-bus-path)/pipe_right &
	hwitl-vp main --error-on-zero-traphandler=true --virtual-bus-device $(virtual-bus-path)/pipe_left
	killall responder-cli
	killall socat

real: all
	hwitl-vp main --error-on-zero-traphandler=true --virtual-bus-device /dev/ttyUSB2

clean:
	rm -f main

