# Adding a comment to check the wall-submit issue for platforms.

platform -name zc702_base -desc "Basic platform targeting the ZC702 board, which includes 1GB of DDR3, 16MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/ek-z7-zc702-g.html" -hw ./zc702_base.xsa -out ./output  -prebuilt

domain -name xrt -proc ps7_cortexa9 -os linux -image ./src/a9/xrt/image
domain config -boot ./src/boot
domain config -bif ./src/a9/xrt/linux.bif
domain -runtime opencl
domain -qemu-args ./src/qemu/lnx/qemu_args.txt
domain -qemu-data ./src/boot
#domain -sysroot ./src/arm-xilinx-linux

platform -generate

