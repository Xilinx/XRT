platform -name zcu102_svm -desc "A basic platform targeting the ZCU102 evaluation board, which includes 4GB of DDR4 for the Processing System, 512MB of DDR4 for the Programmable Logic, 2x64MB Quad-SPI Flash and an SDIO card interface. More information at https://www.xilinx.com/products/boards-and-kits/ek-u1-zcu102-g.html" -hw ./zcu102_svm.dsa -out ./output  -prebuilt
platform -samples {./samples}

system -name ocl -display-name "A53 OpenCL Linux" -boot ./src/boot  -readme ./src/generic.readme
domain -name ocl -proc psu_cortexa53 -os linux -image ./src/a53/ocl/image
boot -bif ./src/a53/ocl/linux.bif
domain -runtime opencl

platform -generate
