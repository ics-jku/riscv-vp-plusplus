#!/bin/sh

NPROCS=$(grep -c ^processor /proc/cpuinfo)

cd "${0%/*}"

DIR="$(pwd)"
PREFIX=$DIR/softfloat-dist

source=spike-softfloat-20190310

tar -xzf $source.tar.gz
make -C $source -j$NPROCS

mkdir -p softfloat-dist/lib
mkdir -p softfloat-dist/include/softfloat

cp $source/softfloat.a softfloat-dist/lib/libsoftfloat.a
cp $source/*.h softfloat-dist/include/softfloat/
rm -rf $source

header_file=softfloat-dist/include/softfloat/softfloat.hpp
echo "#pragma once" > $header_file
echo "extern \"C\" {" >> $header_file
echo "#include \"softfloat.h\"" >> $header_file
echo "}" >> $header_file
