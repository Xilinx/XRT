

// aarch64-linux-gnu-g++ -c -I/proj/xcohdstaff1/stephenr/github/XRT/WIP/build/Debug//opt/xilinx/xrt/include -o pskernel.o /proj/xcohdstaff1/stephenr/github/XRT/WIP/src/runtime_src/tools/xclbinutil/unittests/PSKernel/pskernel.cpp
// aarch64-linux-gnu-g++ pskernel.o -shared -o pskernel.so


#include "xrt.h"

class xrtHandles 
{
public:
};

__attribute__((visibility("default")))
xrtHandles *kernel0_init(xclDeviceHandle xcl_dhdl, const xuid_t xclbin_uuid) {
  xrtHandles *handles = new xrtHandles;
  return(handles);
}

__attribute__((visibility("default")))
int kernel0(float *inA, float *inB, float *out, const int input_size, const int output_size, float increment, float *increment_out, xrtHandles *handles) {
  return 0;
}

__attribute__((visibility("default")))
int kernel0_fini(xrtHandles *handles) {
  return 0;
}
