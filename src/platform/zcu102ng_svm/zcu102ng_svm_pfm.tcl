platform -name zcu102ng_svm -desc "A basic platform targeting the ZCU102 evaluation board, which includes 4GB of DDR4 for the Processing System, 512MB of DDR4 for the Programmable Logic, 2x64MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/ek-u1-zcu102-g.html" -hw ./zcu102ng_svm.dsa -out ./output  -prebuilt
platform -samples {./samples}

system -name xrt -display-name "A53 OpenCL Linux" -boot ./src/boot  -readme ./src/generic.readme
domain -name xrt -proc psu_cortexa53 -os linux -image ./src/a53/xrt/image
sysconfig config -bif ./src/a53/xrt/linux.bif
domain -runtime opencl

platform -generate
