#!/bin/bash

# script to compile results

#sdx=/home/soeren/perforce/sbx-p4/REL/2018.2/prep/rdi/sdx
sdx=/proj/xbuilds/2018.2_daily_latest/installs/lin64/SDx/2018.2
ext=.o

export XILINX_SDX=${XILINX_SDX:=$sdx}
export XILINX_OPENCL=$XILINX_SDX
export LD_LIBRARY_PATH=$XILINX_SDX/lib/lnx64${ext}/Default:$XILINX_SDX/lib/lnx64${ext}:$XILINX_SDX/runtime/lib/x86_64:$XILINX_SDX/data/sdaccel/pcie/x86_64

echo "XILINX_OPENCL=$XILINX_OPENCL"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"

cmd="../build/opt/101_cdma/101_cdma.exe -k kernel.xclbin --s 10"

$cmd --j 1
$cmd --j 2
$cmd --j 4
$cmd --j 8
$cmd --j 16
$cmd --j 32
$cmd --j 64

$cmd --wl --j 1
$cmd --wl --j 2
$cmd --wl --j 4
$cmd --wl --j 8
$cmd --wl --j 16
$cmd --wl --j 32
$cmd --wl --j 64

$cmd --ert --j 1
$cmd --ert --j 2
$cmd --ert --j 4
$cmd --ert --j 8
$cmd --ert --j 16
$cmd --ert --j 32
$cmd --ert --j 64

$cmd --wl --ert --j 1
$cmd --wl --ert --j 2
$cmd --wl --ert --j 4
$cmd --wl --ert --j 8
$cmd --wl --ert --j 16
$cmd --wl --ert --j 32
$cmd --wl --ert --j 64




