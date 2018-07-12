# Script to setup environment for XRT
# This script is installed in /opt/xilinx/xrt and must
# be sourced from that location

XILINX_XRT=$(readlink -f $(dirname ${BASH_SOURCE[0]}))

if [[ $XILINX_XRT != *"/opt/xilinx/xrt" ]]; then
    echo "Invalid location: $XILINX_XRT"
    echo "This script must be sourced from XRT install directory"
    exit 1
fi

export XILINX_XRT
export LD_LIBRARY_PATH=$XILINX_XRT/lib:$LD_LIBRARY_PATH
export PATH=$XILINX_XRT/bin:$PATH
unset XILINX_SDACCEL
unset XILINX_SDX
unset XILINX_OPENCL
unset XCL_EMULATION_MODE

echo "XILINX_XRT      : $XILINX_XRT"
echo "PATH            : $PATH"
echo "LD_LIBRARY_PATH : $LD_LIBRARY_PATH"
