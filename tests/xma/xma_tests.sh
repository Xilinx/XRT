#!/usr/bin/env bash

set -e


echo "PWD is $PWD"
echo "XILINX_SDX is: $XILINX_SDX"

#set xma include path
BUILD=2018.3
#export XMA_INCLUDE=${XMA_INCLUDE:=/proj/xbuilds/${BUILD}_daily_latest/xbb/xrt/packages/xrt-2.1.0-centos/opt/xilinx/xrt/include}
#export XMA_LIBS=${XMA_LIBS:=/proj/xbuilds/${BUILD}_daily_latest/xbb/xrt/packages/xrt-2.1.0-centos/opt/xilinx/xrt/lib}
export XMA_INCLUDE=${XMA_INCLUDE:=/opt/xilinx/xrt/include}
export XMA_LIBS=${XMA_LIBS:=/opt/xilinx/xrt/lib}

echo "XMA_INCLUDE is $XMA_INCLUDE"
echo "XMA_LIBS is $XMA_LIBS"

#export LD_LIBRARY_PATH=/opt/xilinx/xrt/lib:$XMA_LIBS:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/xilinx/xrt/lib
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"

source /opt/xilinx/xrt/setup.sh
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"

make


