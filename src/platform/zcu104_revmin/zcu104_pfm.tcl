platform -name zcu104_revmin -desc "A basic platform targeting the zcu104 evaluation board, which includes 4GB of DDR4 for the Processing System, 512MB of DDR4 for the Programmable Logic, 2x64MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/ek-u1-zcu104-g.html" -hw ./dep/hw/zcu104_revmin.dsa -out ./output  -prebuilt

set pfm_xfcv_incdir ./dep/sw/inc/xfopencv
#system -name a53_linux -display-name "A53 Linux"  -boot ./dep/boot  -readme ./dep/generic.readme
#domain -name a53_linux -proc psu_cortexa53_0 -os linux -image ./dep/a53/linux/image
#domain -prebuilt-data ./dep/sw/prebuilt
#library -lib-path {} \
#	-inc-path ${pfm_xfcv_incdir}
#sysconfig config -bif ./dep/a53/linux/linux.bif
#domain -pmuqemu-args ./dep/qemu/lnx/pmu_args.txt
#domain -qemu-args ./dep/qemu/lnx/qemu_args.txt
#domain -qemu-data ./dep/boot

#system -name a53_standalone  -display-name "A53 Standalone" -boot ./dep/boot  -readme ./dep/generic.readme
#domain -name a53_standalone -proc psu_cortexa53_0 -os standalone
#domain -pmuqemu-args ./dep/qemu/std/pmu_args.txt
#domain -qemu-args ./dep/qemu/std/qemu_args.txt
#domain -qemu-data ./dep/boot
#domain -prebuilt-data ./dep/sw/prebuilt
#app -lscript ./dep/a53/standalone/lscript.ld
#sysconfig config -bif ./dep/a53/standalone/standalone.bif

system -name xrt -display-name "A53 OpenCL Linux" -boot ./dep/boot  -readme ./dep/generic.readme
domain -name xrt -proc psu_cortexa53 -os linux -image ./dep/a53/ocl/image
library -lib-path {} \
	-inc-path ${pfm_xfcv_incdir}
sysconfig config -bif ./dep/a53/ocl/linux.bif
domain -runtime opencl
domain -prebuilt-data ./dep/sw/prebuilt
domain -pmuqemu-args ./dep/qemu/lnx/pmu_args.txt
domain -qemu-args ./dep/qemu/lnx/qemu_args.txt
domain -qemu-data ./dep/boot

#system -name r5_standalone -display-name "R5 Standalone" -boot ./dep/boot  -readme ./dep/generic.readme
#domain -name r5_standalone -os standalone -proc psu_cortexr5_0
#domain -prebuilt-data ./dep/sw/prebuilt
#app -lscript ./dep/r5/standalone/lscript.ld
#sysconfig config -bif ./dep/r5/standalone/standalone.bif

platform -generate
