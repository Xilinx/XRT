platform -name zcu104_revmin -desc "A reVISION platform targeting the ZCU104 evaluation board, which includes 2GB of DDR4 for the Processing System, video codec unit supports H.264/H.265, optimized to work with reVISION development environment. More information at https://www.xilinx.com/products/boards-and-kits/zcu104.html" -hw ./zcu104_revmin.dsa -out ./output  -prebuilt

system -name xrt -display-name "A53 OpenCL Linux" -boot ./src/boot  -readme ./src/generic.readme
domain -name xrt -proc psu_cortexa53 -os linux -image ./src/a53/xrt/image
sysconfig config -bif ./src/a53/xrt/linux.bif
domain -runtime opencl
domain -pmuqemu-args ./src/qemu/lnx/pmu_args.txt
domain -qemu-args ./src/qemu/lnx/qemu_args.txt
domain -qemu-data ./src/boot
domain -sysroot ./src/aarch64-xilinx-linux

platform -generate
