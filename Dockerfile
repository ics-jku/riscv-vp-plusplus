FROM debian:13 AS debian
RUN apt update && apt -y install git cmake autoconf automake autotools-dev clang-format-19 curl libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo libgoogle-perftools-dev libtool patchutils bc zlib1g-dev libexpat-dev libboost-iostreams-dev libboost-program-options-dev libboost-log-dev qtbase5-dev qt5-qmake libvncserver-dev nlohmann-json3-dev wget tar gdb-multiarch socat
RUN wget -O /opt/riscv.tar.xz https://cloud.ics.jku.at/public.php/dav/files/niy65itSmKZCii6/?accept=zip
RUN tar -xf /opt/riscv.tar.xz -C /opt/
RUN adduser --disabled-password --gecos "RISC-V VP User" riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp
RUN su - riscv-vp -c 'export PATH=$PATH:/opt/riscv-gnu-toolchain-multi-2025.09.28/bin && env MAKEFLAGS="-j$(nproc)" make -C /home/riscv-vp/riscv-vp'
WORKDIR /home/riscv-vp/riscv-vp
ENV PATH="$PATH:/opt/riscv-gnu-toolchain-multi-2025.09.28/bin:/home/riscv-vp/riscv-vp/vp/build/bin"
USER riscv-vp

FROM alpine:edge AS alpine
RUN apk update && apk add build-base cmake boost-dev \
	systemc-dev systemc-static git gcc-riscv-none-elf \
	g++-riscv-none-elf newlib-riscv-none-elf gdb-multiarch libvncserver-dev nlohmann-json socat valgrind

RUN adduser -G users -g 'RISC-V VP User' -D riscv-vp
ADD --chown=riscv-vp:users . /home/riscv-vp/riscv-vp

RUN su - riscv-vp -c 'env MAKEFLAGS="-j$(nproc)" make -C /home/riscv-vp/riscv-vp'
WORKDIR /home/riscv-vp/riscv-vp
ENV PATH="$PATH:/home/riscv-vp/riscv-vp/vp/build/bin" RISCV_PREFIX="riscv-none-elf-"
USER riscv-vp


