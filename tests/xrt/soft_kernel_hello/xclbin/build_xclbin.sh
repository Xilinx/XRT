#!/usr/bin/env bash

source /proj/xbuilds/2020.2_daily_latest/installs/lin64/DocNav/.settings64-DocNav.sh 
source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Vivado/2020.2/.settings64-Vivado.sh 
source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Vitis/2020.2/.settings64-Vitis.sh 
source /proj/xbuilds/2020.2_daily_latest/installs/lin64/Model_Composer/2020.2/.settings64-Model_Composer.sh 

export XILINX_XRT=/opt/xilinx/xrt
source $XILINX_XRT/setup.sh

rm -f *.xo
rm -f *.json
which vivado
which vpl
which v++
#v++ --help

v++ -s  --jobs 4 --xp vivado_param:general.maxThreads=1 --remote_ip_cache /proj/fis/results/2020.2_SAM/SDX_GLOBAL_CACHE_FOR_SPRITE/2020.2_SAM_latest --xp param:compiler.enablePerformanceTrace=true --log_dir vpp_log --report_dir reports  -t hw --platform /proj/xbuilds/2020.2_daily_latest/internal_platforms/xilinx_u30_gen3x4_1_202020_1/xilinx_u30_gen3x4_1_202020_1.xpfm -I. -c -o dummy.xo dummy.cpp -k dummy 

rm -f *.xclbin
v++ -s --jobs 4 --xp vivado_param:general.maxThreads=1 --remote_ip_cache /proj/fis/results/2020.2_SAM/SDX_GLOBAL_CACHE_FOR_SPRITE/2020.2_SAM_latest --xp param:compiler.enablePerformanceTrace=true --log_dir vpp_log --report_dir reports -t hw --platform /proj/xbuilds/2020.2_daily_latest/internal_platforms/xilinx_u30_gen3x4_1_202020_1/xilinx_u30_gen3x4_1_202020_1.xpfm -I. -l -o dummy.xclbin ./dummy.xo --config connectivity.cfg

xclbinutil --input ./dummy.xclbin --add-section SOFT_KERNEL[hello_world]-OBJ:RAW:../soft_kernel/hello_world.so --add-section SOFT_KERNEL[hello_world]-METADATA:JSON:hello_world.rtd -o soft_kernel_hello_world.xclbin

