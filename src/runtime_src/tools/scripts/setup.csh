#!/bin/csh -f

set called=($_)
set script_path=`readlink -f $called[2]`
set xrt_dir=`dirname $script_path`

if ( $xrt_dir !~ */opt/xilinx/xrt ) then
    echo "Invalid location: $xrt_dir"
    echo "This script must be sourced from XRT install directory"
    exit 1
endif

setenv XILINX_XRT $xrt_dir
if ( ! $?LD_LIBRARY_PATH ) then
   setenv LD_LIBRARY_PATH $XILINX_XRT/lib
else
   setenv LD_LIBRARY_PATH $XILINX_XRT/lib:$LD_LIBRARY_PATH
endif

if ( ! $?PATH ) then
   setenv PATH $XILINX_XRT/bin
else
   setenv PATH $XILINX_XRT/bin:$PATH
endif

unsetenv XILINX_SDACCEL
unsetenv XILINX_SDX
unsetenv XILINX_OPENCL
unsetenv XCL_EMULATION_MODE

echo "XILINX_XRT      : $XILINX_XRT"
echo "PATH            : $PATH"
echo "LD_LIBRARY_PATH : $LD_LIBRARY_PATH"
