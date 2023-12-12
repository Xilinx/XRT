#!/usr/bin/env bash

set -e


usage()
{
    echo "Usage: xma_test.sh [options]"
    echo
    echo "[-xrt <path>]    xrt install location, e.g. /opt/xilinx/xrt"
    exit 1
}

xrt_install_path="/opt/xilinx/xrt"

while [ $# -gt 0 ]; do
    case "$1" in
        -xrt)
            shift
            xrt_install_path=$1
            shift
            ;;
        *)
            echo "unknown option"
            usage
            ;;
    esac
done

echo "PWD is $PWD"
echo "XILINX_SDX is: $XILINX_SDX"

export XRT_INSTALL_PATH=${xrt_install_path}
echo "XRT_INSTALL_PATH is: $XRT_INSTALL_PATH"

#set xma include path
BUILD=2018.3
#export XMA_INCLUDE=${XMA_INCLUDE:=/proj/xbuilds/${BUILD}_daily_latest/xbb/xrt/packages/xrt-2.1.0-centos/opt/xilinx/xrt/include}
#export XMA_LIBS=${XMA_LIBS:=/proj/xbuilds/${BUILD}_daily_latest/xbb/xrt/packages/xrt-2.1.0-centos/opt/xilinx/xrt/lib}
export XMA_INCLUDE=${XMA_INCLUDE:=${xrt_install_path}/include}
export XMA_LIBS=${XMA_LIBS:=${xrt_install_path}/lib}

echo "XMA_INCLUDE is $XMA_INCLUDE"
echo "XMA_LIBS is $XMA_LIBS"

#export LD_LIBRARY_PATH=/opt/xilinx/xrt/lib:$XMA_LIBS:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${xrt_install_path}/lib
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"

source ${xrt_install_path}/setup.sh
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"

make


