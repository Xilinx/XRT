#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/mman.h>

#include "xrt/xrt_kernel.h"

constexpr unsigned long long int operator "" _Ki(unsigned long long int n) {
  return n * 1024;
}

constexpr unsigned long long int operator "" _Mi(unsigned long long int n) {
  return n * 1024 * 1024;
}

#ifdef __cplusplus
extern "C" {
#endif
#include "core/edge/include/sk_types.h"

// User private data structure container (context object) definition
class xrtHandles : public pscontext
{
public:
  xrt::device dhdl;
  xrt::kernel bandwidth_kernel;
  xrtHandles(xclDeviceHandle dhdl_in, const xuid_t xclbin_uuid)
    : dhdl(dhdl_in)
    , bandwidth_kernel(dhdl,xclbin_uuid,"bandwidth")
  {
  }
};

  __attribute__((visibility("default")))
  pscontext *bandwidth_kernel_init(xclDeviceHandle dhdl, const xuid_t xclbin_uuid) {
    xrtHandles *handles = new xrtHandles(dhdl, xclbin_uuid);

    return(handles);
  }
  
__attribute__((visibility("default")))
int bandwidth_kernel(int reps, double *max_throughput, xrtHandles *xrtHandle)
{
  double max_throughput_int = 0;

  // Starting at 4K and going up to 16M with increments of power of 2
  for (uint32_t i = 4_Ki; i <= 16_Mi; i *= 2) {
    unsigned int data_size = i;
    
    // These commands will allocate memory on the FPGA. The xrt::bo objects
    // can be used to reference the memory locations on the device.
    // Creating Buffers
    auto input_buffer = xrt::bo(xrtHandle->dhdl, data_size, xrtHandle->bandwidth_kernel.group_id(0));
    auto output_buffer = xrt::bo(xrtHandle->dhdl, data_size, xrtHandle->bandwidth_kernel.group_id(1));
    auto input_host = input_buffer.map<unsigned char*>();
    auto output_host = output_buffer.map<unsigned char*>();

    // Filling up memory with an incremental byte pattern
    for (uint32_t j = 0; j < data_size; j++) {
      input_host[j] = j % 256;
      // Initializing output vectors to zero
      output_host[j] = 0;
    }
    
    auto time_start = std::chrono::high_resolution_clock::now();
    auto run = xrtHandle->bandwidth_kernel(input_buffer, output_buffer, data_size, reps);
    run.wait();
    auto time_end = std::chrono::high_resolution_clock::now();

    // check
    for (uint32_t j = 0; j < data_size; j++) {
      if (output_host[j] != input_host[j]) {
        syslog(LOG_ERR,"%s: ERROR : kernel failed to copy entry %d, input %d output %d\n",__func__, j, input_host[j], output_host[j]);
        return EXIT_FAILURE;
      }
    }

    double usduration =
    (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);

    double dnsduration = (double)usduration;
    double dsduration = dnsduration / ((double)1000000000); // Convert the duration from nanoseconds to seconds
    double bpersec = (data_size * 1) / dsduration;
    double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // Convert b/sec to mb/sec
    
    if (mbpersec > max_throughput_int) max_throughput_int = mbpersec;
    syslog(LOG_INFO, "%s: Throughput : %f MB/s\n",__func__,mbpersec);
  }

  syslog(LOG_INFO, "%s: Throughput (Type: DDR) = %f MB/s\n",__func__,max_throughput_int);
  max_throughput[0] = max_throughput_int;

  return 0;
}


__attribute__((visibility("default")))
int bandwidth_kernel_fini(xrtHandles *handles) {
  delete handles;
  return 0;
}
  
#ifdef __cplusplus
}
#endif
