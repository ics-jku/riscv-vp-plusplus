#/bin/bash
set -x  # print commands
trap 'sleep 1' DEBUG  # Wait for 1 second after each command

cd ..
PATH=`pwd`/vp/build/bin:$PATH  # append to PATH
make  # build the project
cd sw/basic-c-zfinx  # go to the zfinx test directory
make clean sim clean  # clean build and run simulation
cd ../..
# set ZFINX to 0
find vp -type f -name 'iss.cpp' -exec sed -i 's/#define ZFINX 1/#define ZFINX 0/' {} +
make  # rebuild the project with ZFINX=0
cd sw/basic-c-float  # go to the float test directory
make clean sim clean  # clean build and run simulation
find ../../vp -type f -name 'iss.cpp' -exec sed -i 's/#define ZFINX 0/#define ZFINX 1/' {} +  # set ZFINX back to 1