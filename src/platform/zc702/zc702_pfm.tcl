# Adding a comment to check the wall-submit issue for platforms.

platform -name zc702 -desc "Basic platform targeting the ZC702 board, which includes 1GB of DDR3, 16MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/ek-z7-zc702-g.html" -hw ./zc702.dsa -out ./output  -prebuilt

system -name xrt -display-name "A9 OpenCL Linux"  -boot ./src/boot  -readme ./src/generic.readme
domain -name xrt -proc ps7_cortexa9_0 -os linux -image ./src/a9/xrt/image
sysconfig config -bif ./src/a9/xrt/linux.bif
domain -runtime opencl
domain -qemu-args ./src/qemu/lnx/qemu_args.txt
domain -qemu-data ./src/boot
domain -prebuilt-data ./src/prebuilt
domain -sysroot ./src/arm-xilinx-linux

platform -generate

