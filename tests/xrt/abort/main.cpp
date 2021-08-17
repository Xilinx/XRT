/*
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************
Sample test case to abort running kernel that is software
resetable.

The example uses a kernel that can be called to run in an infinite
loop, the kernel is synthesized with software reset and is aborted
through host code.

       _____________
      |add          |<----- in1 (global memory)
      | in1 + in2   |<----- in2  (global memory)
      |             |-----> out  (global memory)
      |             |<----- size (scalar)
      |_____________|<----- hang (scalar)

The kernel is built with a pre_tcl script that sets

   config_interface -s_axilite_sw_reset

If kernel argument 'hang' is non-zero, the kernel will enter an
infinite loop that can be broken only with a reset.

The test harness allows the user to specify if the kernel should
hang in a infinite loop and be abort after some time.

This example illustrates sw reset using xrt::run::abort.
****************************************************************/

// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o reset.exe main.cpp -lxrt_coreutil -luuid -pthread
// % reset.exe -k <xclbin> [--hang]

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <future>
#include <thread>
#include <vector>
#include "xrt/xrt_kernel.h"
#include "experimental/xrt_ini.h"

using value_type = std::uint32_t;

static size_t data_size = 8 * 1024 * 1024;
static size_t data_size_bytes = sizeof(int) * data_size;

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <bdf | device_index>\n";
  std::cout << "";
  std::cout << "  [--hang <val>]: specify to value != 0 to make kernel hang and test sw reset\n";
}

static bool
is_hw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem ? std::strcmp(xem,"hw_emu")==0 : false;
  return hwem;
}

static void
adjust_for_hw_emulation()
{
  if (!is_hw_emulation())
    return;
  
  data_size = 4096;
  data_size_bytes = data_size * sizeof(int);
}

static ert_cmd_state
abort_async(xrt::run run, int hang)
{
  return hang ? run.abort() : ert_cmd_state(0);
}

static void
run(const xrt::device& device, const xrt::uuid& uuid, int hang)
{
  // add(in1, in2, nullptr, data_size)
  xrt::kernel add(device, uuid, "loop_vadd");
  xrt::bo in1(device, data_size_bytes, add.group_id(0));
  auto in1_data = in1.map<int*>();
  xrt::bo in2(device, data_size_bytes, add.group_id(1));
  auto in2_data = in2.map<int*>();
  xrt::bo out(device, data_size_bytes, add.group_id(2));
  auto out_data = out.map<int*>();

  // computed expected result
  std::vector<int> sw_out_data(data_size);

  // Create the test data and software result
  for(size_t i = 0; i < data_size; ++i) {
    in1_data[i] = static_cast<int>(i);
    in2_data[i] = 2 * static_cast<int>(i);
    out_data[i] = 0;
    sw_out_data[i] = (in1_data[i] + in2_data[i] + hang);
  }

  // sync test data to kernel
  in1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  in2.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // start the kernel, if hang > 0, then kernel will hang
  auto run = add(in1, in2, out, data_size, hang);

  // asynchronous abort
  auto abort_ret = std::async(std::launch::async, abort_async, run, hang);

  // wait for run to complete
  auto state = run.wait();
  std::cout << "kernel completed with state (" << state << ")\n";

  if (hang) {
    if (abort_ret.get() != state)
      throw std::runtime_error("bad abort state or cmd state");
    return;
  }
      
  // sync result from device to host
  out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

  // compare with expected scalar adders
  for (size_t i = 0 ; i < data_size; i++) {
    if (out_data[i] != sw_out_data[i]) {
      std::cout << "error detected "
                << " expected output = " << sw_out_data[i]
                << " observed output = " << out_data[i]
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
  int hang = 0;

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
    else if (cur == "--hang")
      hang = std::stoi(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  if (hang && is_hw_emulation())
    throw std::runtime_error("abort is not yet supported in hw emulation");

  adjust_for_hw_emulation();

  // Disable ert to avoid scheduler arming interrupts on the xrt::ip controlled
  xrt::ini::set("Runtime.ert", false);
  xrt::xclbin xclbin{xclbin_fnm};
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin);

  run(device, uuid, hang);
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
