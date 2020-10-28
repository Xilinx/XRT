#!/usr/bin/env bash

echo "INFO: Compiling soft kernel"
cd soft_kernel
echo "DIR: $(pwd)"
source ./compileit.sh

echo "INFO: Generating xclbin"
cd ../xclbin
echo "DIR: $(pwd)"
source ./build_xclbin.sh

echo "INFO: Compiled Host Executable: host.exe"
cd ..
echo "DIR: $(pwd)"

export XILINX_XRT=/opt/xilinx/xrt
source $XILINX_XRT/setup.sh

rm -rf ./libdummy_plugin_enc.so ./dummy_plugin_enc.o
gcc -I$XILINX_XRT/include/xma2 -I$XILINX_XRT/include -fPIC -Wall -Wextra -O1 -g -c -o dummy_plugin_enc.o dummy_plugin_enc.c
gcc -shared -o libdummy_plugin_enc.so dummy_plugin_enc.o

rm -rf hello_world.exe
g++ -std=c++11 hello_world.cpp -lxma2api -lxma2plugin -ldl -Wl,--unresolved-symbols=ignore-in-shared-libs -g -Wall -fmessage-length=0 -lpthread -lrt -I$XILINX_XRT/include/xma2 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o hello_world.exe
echo "INFO: Compiled Host Executable: host.exe"

echo "Run Board Test case - START"

cat xrt.ini

xbutil query

rm -rf xma_log.txt
./hello_world.exe ./xclbin/soft_kernel_hello_world.xclbin 0 ./file1.txt ./file2.txt 4 128

xbutil query
echo "Run Board Test case - DONE"
