#!/usr/bin/env bash

#XILINX_VITIS = /proj/xbuilds/2020.2_daily_latest/installs
#XPFM_FILE_PATH = /proj/xbuilds/2020.2_daily_latest/internal_platforms/xilinx_u30_gen3x4_1_202020_1/xilinx_u30_gen3x4_1_202020_1.xpfm
#XILINX_VITIS_VPP = /proj/xbuilds/SWIP/2020.2_1026_2201/installs/lin64/Vitis/2020.2/bin/v++

#source /proj/xbuilds/2020.2_daily_latest/installs/lin64/DocNav/.settings64-DocNav.sh 
#source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Vivado/2020.2/.settings64-Vivado.sh 
#source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Vitis/2020.2/.settings64-Vitis.sh 
#source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Model_Composer/2020.2/.settings64-Model_Composer.sh 

export XILINX_XRT=/opt/xilinx/xrt
source $XILINX_XRT/setup.sh

rm -f *.xo
rm -f *.json
#which vivado
#which vpl
#which v++
#v++ --help

$XILINX_VITIS_VPP -s  --jobs 4 --xp vivado_param:general.maxThreads=1 --xp param:compiler.enablePerformanceTrace=true --log_dir vpp_log --report_dir reports  -t hw --platform $XPFM_FILE_PATH -I. -c -o hello.xo hello.cl -k hello 

rm -f *.xclbin
$XILINX_VITIS_VPP -s --jobs 4 --xp vivado_param:general.maxThreads=1 --xp param:compiler.enablePerformanceTrace=true --log_dir vpp_log --report_dir reports -t hw --platform $XPFM_FILE_PATH -I. -l -o hello.xclbin ./hello.xo --config connectivity.cfg

xclbinutil --input ./hello.xclbin --add-section SOFT_KERNEL[hello_world]-OBJ:RAW:../soft_kernel/hello_world.so --add-section SOFT_KERNEL[hello_world]-METADATA:JSON:hello_world.rtd -o soft_kernel_hello_world.xclbin

