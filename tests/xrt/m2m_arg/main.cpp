/**
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */
/****************************************************************
This test illustrates m2m copying to local buffer object of
incompatible kernel arguments.

The test consists of 3 kernels

1) vmult_factor(const int in*, factor, int* out, size): 
Multiply each element in a vector with a constant factor
All arguments are allocated in DDR bank0

2) vadd_factor(const int in*, factor, int* out, size):
Add each element in a vector with a constant factor
All arguments are allocated in DDR bank1

3) vadd(const int* in1, const int* in2, const int* in3, int* out, size):
Add 3 input vectors 
All arguments are allocated in DDR bank2

       _____________
      |vaddf: bank1 |<----- in   (global memory)
      | in[] + add  |<----- add  (scalar)
      |_____________|-----> ovaf (global memory)
       _____________ 
      |vmultf: bank0|<----- in   (global memory)
      | in[] * mult |<----- mult (scalar)
      |_____________|-----> ovmf (global memory)

      wait();
       _____________  
      |vadd: bank2  |<----- in   (global memory)
      |             |<----- ovaf (global memory)
      | in1+in2+in3 |<----- ovmf (global memory)
      |_____________|-----> out  (global memory)

The test allocates one buffer for the vector input 'in' to vaddf,
vmultf, and vadd.  The buffer object for input is created in bank
compatibile with the connectivity of 'vadd'.

in, out: bank2
ovaf: bank1
ovmf: bank0

Since 'in' is incompatible with vaddf and vmultf two local buffers
(one for each of these kernels) are created when 'in' is set as 
argument to these two kernels.

Since 'ovaf' is incompatible with vadd, a local buffer is created 
for the this input when set on vadd.  Ditto for ovmf.

In all 4 local buffers are created.  If the device supports m2m
then the local buffers will copy the DDR content of the src buffer.
****************************************************************/

// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o m2marg.exe main.cpp -lxrt_coreutil -luuid -pthread

#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

static size_t data_size = 4096;
static size_t data_size_bytes = data_size * sizeof(int);

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <bdf | device_index>\n";
  std::cout << "";
}

static bool
is_hw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem ? std::strcmp(xem,"hw_emu")==0 : false;
  return hwem;
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? std::strcmp(xem,"sw_emu")==0 : false;
  return swem;
}

static void
adjust_for_emulation()
{
  if (!is_hw_emulation() && !is_sw_emulation())
    return;

  data_size = 128;
  data_size_bytes = data_size * sizeof(int);
}

static void
run(const xrt::device& device, const xrt::uuid& uuid)
{
  // vmf(in1, factor, out, data_size)
  // out[] = in1[] * factor
  xrt::kernel vmf(device, uuid, "krnl_vmult_factor");

  // vaf(in1, factor, out, data_size)
  // out[] = in1[] + factor
  xrt::kernel vaf(device, uuid, "krnl_vadd_factor");

  // vadd(in1, in2, in3, out, data_size)
  // out[] = in1[] + in2[] + in3[]
  xrt::kernel vadd(device, uuid, "krnl_vadd");

  // const data input, first input to all 3 kernels, allocated
  // compatible with vadd kernel
  xrt::bo in(device, data_size_bytes, vadd.group_id(0));
  auto in_data = in.map<int*>();
  std::iota(in_data, in_data + data_size, 0);
  in.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // output of vmf, will be used as input to incompatible vadd kernel
  xrt::bo vmf_out(device, data_size_bytes, vmf.group_id(2));
  auto vmf_out_data = vmf_out.map<int*>();

  // output of vaf, will be used as input to incompatible vadd kernel
  xrt::bo vaf_out(device, data_size_bytes, vaf.group_id(2));
  auto vaf_out_data = vaf_out.map<int*>();

  // output of vadd
  xrt::bo out(device, data_size_bytes, vadd.group_id(3));
  auto out_data = out.map<int*>();
  std::fill(out_data, out_data + data_size, 0);
  out.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // start vaf and vmf and wait for completion
  // both these calls will allocated local compatible copies
  // of the input vector
  auto run_vmf = vmf(in, 2, vmf_out, data_size);
  auto run_vaf = vaf(in, 1, vaf_out, data_size);
  run_vmf.wait();
  run_vaf.wait();

  // start vadd and wait()
  // local copies of both vmf_out and vaf_out will be created
  auto run = vadd(in, vmf_out, vaf_out, out, data_size);
  run.wait();

  // sync output of vadd to host
  out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

  // compare with expected scalar adder
  for (size_t i = 0 ; i < data_size; i++) {
    auto expected = in_data[i] * 2 + in_data[i] + 1 + in_data[i];
    if (out_data[i] != expected) {
      std::cout << " expected output[" << i << "] = " << expected
                << " observed output[" << i << "] = " << out_data[i]
                << '\n';
      throw std::runtime_error("result mismatch");
    }
  }
}

static void
run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  std::string device_id = "0";

  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_id = arg;
    else if (cur == "-k")
      xclbin_fnm = arg;
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  adjust_for_emulation();

  xrt::xclbin xclbin{xclbin_fnm};
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin);

  run(device, uuid);
}


int
main(int argc, char* argv[])
{
  try {
    run(argc,argv);
    std::cout << "TEST PASSED\n";
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
