#!/bin/bash

set -e


echo "PWD is $PWD"
export LD_LIBRARY_PATH=$PWD/../../build/Debug/opt/xilinx/xrt/lib:$LD_LIBRARY_PATH
echo "LD_LIBRARY_PATH is $LD_LIBRARY_PATH"
make


