This test is for hello world kernel, which should be included in the
platform's package.

For example, my system has U200 platform installed at /opt/xilinx/dsa/xilinx_u200_xdma_201830_2

## Compile
Source setup.sh after install XRT package.
``` bash
$ make
```

## Run test
``` bash
#Run xcl* API test:
$ ./xcl_api_iops -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin

#Run xrt* API test:
$ ./xrt_api_iops -k /opt/xilinx/dsa/xilinx_u200_xdma_201830_2/test/verify.xclbin
```
