FROM ubuntu:focal

ARG IP

ENV TZ=Europe/Vienna
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y build-essential zip wget python make cmake git \
	gdb-multiarch libboost-all-dev libsystemc libsystemc-dev gcc-riscv64-unknown-elf qt5-default

# Examples expect riscv32-unknown-* compiler triplet.
# TODO: Find a better way to deal with this problem.
RUN sh -c 'ln -s $(command -v riscv-none-elf-gcc) /usr/local/bin/riscv32-unknown-elf-gcc && \
           ln -s $(command -v riscv-none-elf-g++) /usr/local/bin/riscv32-unknown-elf-g++ && \
           ln -s $(command -v riscv-none-elf-as) /usr/local/bin/riscv32-unknown-elf-as && \
           ln -s $(command -v riscv-none-elf-ld) /usr/local/bin/riscv32-unknown-elf-ld'

RUN useradd -s /bin/bash -d /home/riscv-vp -m -G sudo riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp
USER riscv-vp
RUN USE_SYSTEM_SYSTEMC=OFF make -C /home/riscv-vp/riscv-vp
RUN echo export PATH=\"$PATH:/home/riscv-vp/riscv-vp/vp/build/bin\" >> ~/.bashrc
ENV LIBGL_ALWAYS_INDIRECT 1
ENV DISPLAY $IP:0
