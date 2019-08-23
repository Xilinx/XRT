platform -name zcu104_base -desc "A basic platform targeting the ZCU102 evaluation board, which includes 4GB of DDR4 for the Processing System, 512MB of DDR4 for the Programmable Logic, 2x64MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/zcu104.html" -hw ./zcu104_base.xsa -out ./output

domain -name xrt -display-name "A53 OpenCL Linux" -proc psu_cortexa53 -os linux -image ./src/a53/xrt/image
domain config -boot ./src/boot
domain config -bif ./src/a53/xrt/linux.bif
domain -runtime opencl
domain -pmuqemu-args ./src/qemu/lnx/pmu_args.txt
domain -qemu-args ./src/qemu/lnx/qemu_args.txt
domain -qemu-data ./src/boot
#domain -sysroot ./src/aarch64-xilinx-linux

platform -generate

