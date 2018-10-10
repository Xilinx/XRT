#!/usr/bin/env bash

set -e


echo "PWD is $PWD"
export LD_LIBRARY_PATH=/opt/xilinx/xrt/lib:$LD_LIBRARY_PATH
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"

echo "XILINX_SDX is: $XILINX_SDX"

#set xma include path
BUILD=2018.3
export XMA_INCLUDE=${XMA_INCLUDE:=/proj/xbuilds/${BUILD}_daily_latest/xbb/xrt/packages/xrt-2.1.0-centos/opt/xilinx/xrt/include}

echo "XMA_INCLUDE is $XMA_INCLUDE"


make


