virtual-bus-path=../../vp/src/platform/hwitl/virtual-bus
simple-sensor-path=../simple-sensor
tty=/dev/ttyUSB2

main : main.c $(simple-sensor-path)/irq.c $(simple-sensor-path)/irq.h $(simple-sensor-path)/bootstrap.S
	riscv32-unknown-elf-gcc main.c $(simple-sensor-path)/irq.c $(simple-sensor-path)/bootstrap.S -g -o main -march=rv32i -mabi=ilp32 -nostartfiles -Wl,--no-relax

$(virtual-bus-path)/responder-cli:
	make -C $(virtual-bus-path) responder-cli

sim: main $(virtual-bus-path)/responder-cli clearpipes
	make -C $(virtual-bus-path) pipe_left pipe_right
	$(virtual-bus-path)/responder-cli $(virtual-bus-path)/pipe_right &
	hwitl-vp main --error-on-zero-traphandler=true --virtual-bus-device $(virtual-bus-path)/pipe_left
	killall responder-cli
	killall socat

real: main
	hwitl-vp main --error-on-zero-traphandler=true --virtual-bus-device $(tty)

all: main

clearpipes:
	killall responder-cli || true
	killall socat || true

clean: clearpipes
	rm -f main

