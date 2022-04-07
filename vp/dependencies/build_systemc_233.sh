#!/bin/sh

NPROCS=$(grep -c ^processor /proc/cpuinfo)

cd "${0%/*}"

DIR="$(pwd)"
PREFIX=$DIR/systemc-dist

version=2.3.3
source=systemc-$version.tar.gz

if [ ! -f "$source" ]; then
	wget http://www.accellera.org/images/downloads/standards/systemc/$source
fi

tar xzf $source
cd systemc-$version
mkdir -p build && cd build
../configure CXXFLAGS='-std=c++17' --prefix=$PREFIX --with-arch-suffix=
make -j$NPROCS
make install

cd $DIR
