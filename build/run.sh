#!/bin/bash

# Loader for XRT
# Usage:
#  % run.sh ./host.exe kernel.xclin
#  % run.sh -dbg ./host.exe kernel.xclin
#  % run.sh -dbg emacs
XRTBUILD=$(readlink -f $(dirname ${BASH_SOURCE[0]}))

# Define SDX and VIVADO to allow this loader script to be used
# through other scripts (e.g. sprite scripts) that invoke SDx
# and Vivado tools in addition to using XRT.

# Set to location of your preferred SDx install
vitis=/proj/xbuilds/2019.2_released/installs/lin64/Vitis/2019.2

# Set to location of your preferred Vivado install
vivado=/proj/xbuilds/2019.2_released/installs/lin64/Vivado/2019.2

ext=.o
rel="Release"
cmd=""
em=""
conf=""
xrt=""

usage()
{
    echo "Usage:"
    echo
    echo "[-help]                    List this help"
    echo "[-dbg]                     Set env for debug"
    echo "[-em <sw_emu | hw_emu>]    Run emulation"
    echo "[-conf]                    Run conformance mode testing"
    echo "[-ini <path>]              Set SDACCEL_INI_PATH"
    echo "[-vitis <path>]            Specify Vitis install (default: $vitis)"
    echo "[-xrt <path>]              Path to XRT install (default: $XRTBUILD/opt/xilinx/xrt)"
    echo "[-ldp <path>]              Prepend path to LD_LIBRARY_PATH"

    exit 1
}

while [ $# -gt 0 ]; do
    case "$1" in
        -help)
            usage
            ;;
        -dbg)
            ext=.g
            rel="Debug"
            shift
            ;;
        -em)
            shift
            em=$1
            shift
            ;;
        -conf)
            shift
            conf=1
            ;;
        -xrt)
            shift
            xrt=$1
            shift
            ;;
        -vitis)
            shift
            vitis=$1
            shift
            ;;
        -ini)
            shift
            ini=$1
            shift
            ;;
        -ldp)
            shift
            ldp=$1
            shift
            ;;
        *)
            cmd="$cmd $1"
            shift
            ;;
    esac
done

if [ "X$ini" != "X" ] ; then
 echo "SDACCEL_INI_PATH=$ini"
 export SDACCEL_INI_PATH=$ini
fi

if [ "X$em" != "X" ] ; then
 echo "XCL_EMULATION_MODE=$em"
 export XCL_EMULATION_MODE=$em
fi

if [ "X$conf" != "X" ] ; then
 echo "XCL_CONFORMANCE=1"
 export XCL_CONFORMANCE=1
fi

if [ "X$xrt" == "X" ] ; then
 xrt=$XRTBUILD/$rel/opt/xilinx/xrt
fi

if [[ "X$xrt" != "X" && -d "$xrt" ]] ; then
 export XILINX_XRT=${XILINX_XRT:=$xrt}
 export LD_LIBRARY_PATH=$XILINX_XRT/lib
 export PATH=$XILINX_XRT/bin:${PATH}
fi

if [[ "X$vitis" != "X" && -d "$vitis" ]] ; then
 export XILINX_VITIS=${XILINX_VITIS:=$vitis}
 export XILINX_OPENCL=$XILINX_VITIS
 export VITIS_CXX_PATH=${VITIS_CXX_PATH:=$XILINX_VITIS/bin/xcpp}
 export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$XILINX_VITIS/lib/lnx64${ext}/Default:$XILINX_VITIS/lib/lnx64${ext}:$XILINX_VITIS/runtime/lib/x86_64
fi

if [[ "X$vivado" != "X" && -d "$vivado" ]] ; then
 export XILINX_VIVADO=${XILINX_VIVADO:=$vivado}
 export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:$XILINX_VIVADO/lib/lnx64${ext}
fi

if [ "X$ldp" != "X" ] ; then
 export LD_LIBRARY_PATH=$ldp:${LD_LIBRARY_PATH}
fi

echo "XILINX_OPENCL=$XILINX_OPENCL"
echo "XILINX_XRT=$XILINX_XRT"
echo "XILINX_VITIS=$XILINX_VITIS"
echo "XILINX_VIVADO=$XILINX_VIVADO"
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "PATH=$PATH"

$cmd
