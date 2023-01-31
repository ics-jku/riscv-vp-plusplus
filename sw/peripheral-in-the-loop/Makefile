OBJECTS = main.o irq.o bootstrap.o
CFLAGS  = -march=rv32i -mabi=ilp32
LDFLAGS = -nostartfiles -Wl,--no-relax

virtual-bus-path=../../vp/src/platform/hwitl/virtual-bus
simple-sensor-path=../simple-sensor
tty=/dev/ttyUSB2

$(virtual-bus-path)/responder-cli:
	make -C $(virtual-bus-path) responder-cli

sim-bus: $(EXECUTABLE) $(virtual-bus-path)/responder-cli
	make -C $(virtual-bus-path) pipe_left
	sleep 1 #needed sometimes as pipe is not immediately there
	$(virtual-bus-path)/responder-cli $(virtual-bus-path)/pipe_right &
	hwitl-vp $(EXECUTABLE) --error-on-zero-traphandler=true --virtual-bus-device $(virtual-bus-path)/pipe_left
	killall responder-cli
	killall socat

real: $(EXECUTABLE)
	hwitl-vp $(EXECUTABLE) --error-on-zero-traphandler=true --virtual-bus-device $(tty)

clearpipes:
	killall responder-cli || true
	killall socat || true

SIM_TARGET = sim-bus
include ../Makefile.common
