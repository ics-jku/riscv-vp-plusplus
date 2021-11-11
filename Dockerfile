FROM alpine:edge

RUN apk update && apk add --no-cache \
	build-base cmake boost-dev git newlib-riscv-none-elf gdb-multiarch
RUN adduser -G users -g 'RISC-V VP User' -D riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp
RUN su - riscv-vp -c 'make -C /home/riscv-vp/riscv-vp'
CMD su - riscv-vp
