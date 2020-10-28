#!/usr/bin/env bash

rm -rf ./hello_world.so ./hello_world.o
#GCC="/proj/xbuilds/2019.2_released/installs/lin64/Vitis/2019.2/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-gcc"

eval "$GCC -I. -fPIC -fvisibility=hidden -Wall -O2 -g -c -o hello_world.o hello_world.c"
eval "$GCC -shared -o hello_world.so hello_world.o"

