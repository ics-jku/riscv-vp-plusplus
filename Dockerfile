FROM alpine:3.15

RUN apk update && apk add --no-cache  build-base cmake boost-dev \
	systemc-dev systemc-static git newlib-riscv-none-elf gdb-multiarch

# Examples expect riscv32-unknown-* compiler triplet.
# TODO: Find a better way to deal with this problem.
RUN sh -c 'ln -s $(command -v riscv-none-elf-gcc) /usr/local/bin/riscv32-unknown-elf-gcc && \
           ln -s $(command -v riscv-none-elf-g++) /usr/local/bin/riscv32-unknown-elf-g++ && \
           ln -s $(command -v riscv-none-elf-as) /usr/local/bin/riscv32-unknown-elf-as && \
           ln -s $(command -v riscv-none-elf-ld) /usr/local/bin/riscv32-unknown-elf-ld'

RUN adduser -G users -g 'RISC-V VP User' -D riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp
RUN su - riscv-vp -c 'USE_SYSTEM_SYSTEMC=ON make -C /home/riscv-vp/riscv-vp'
RUN su - riscv-vp -c 'echo export PATH=\"$PATH:/home/riscv-vp/riscv-vp/vp/build/bin\" >> /home/riscv-vp/.profile'
CMD su - riscv-vp
