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
% reset -k <xclbin> --hang 1

The test also illustrates how to wait for kernel completion with a
time out.  The time out can be used along with testing hang, in which
case the kernel is aborted after expired timeout in milliseconds
% reset -k <xclbin> --hang 1 --timeout 1000

Without kernel hang, timed out wait will be repeated until kernel
actual completes.  The smaller the timeout the more calls to wait
will be observed.
% reset -k <xclbin> --timeout 10

To validate that the kernel runs without hang, simply call as
% reset -k <xclbin>

XRT supports to execution modes, one is unmanged execution, which by
far is the fastest and one is managed execution where XRT under the
hood is managing all running kernels and asynchronously completes the
kernels.  This mode is what OpenCL is using.  The test in this file
supports test of timeout and hang for managed kernel execution as well
as unmanaged execution.  To test managed execution just use the
optional [--managed argument]
% reset -k <xclbin> --managed ...

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
  std::cout << "usage: %s [options] \n\n"
            << "  -k <bitstream>\n"
            << "  -d <bdf | device_index>\n"
            << ""
            << "  [--hang <val>]: specify to value != 0 to make kernel hang and test sw reset\n"
            << "  [--timeout <ms>]: specify a timeout in millisecond to wait for completion\n"
            << "  [--managed]: use managed (monitored) kernel execution\n";

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

// callback for managed execution
static void
run_done(const void*, ert_cmd_state state, void* data)
{
  std::cout << "run_done\n";
}

// asynchronous abort function
static ert_cmd_state
abort_async(xrt::run run)
{
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return run.abort();
}

// Test abort with or without hanging kernel
static void
abort(const xrt::run& run)
{
  // asynchronous abort
  auto abort_ret = std::async(std::launch::async, abort_async, run);

  // wait for run to complete
  auto state = run.wait();
  std::cout << "abort: kernel completed with state (" << state << ")\n";

  if (abort_ret.get() != state)
      throw std::runtime_error("bad abort state or cmd state");
}

// Test wait with timeout with or without hanging kernel
static void
timeout(xrt::run run, int hang, int timeout_ms)
{
  auto state = run.wait(timeout_ms);
  std::cout << "timeout: wait completed with state (" << state << ")\n";

  // a timed out wait may have left the command running, it is the
  // responsibility of the caller to either continue to wait or abort
  // the run.  In this test, abort if kernel is hanging, or continue
  // waiting until kernel completes
  if (state == ERT_CMD_STATE_TIMEOUT && hang) {
    state = run.abort();
    std::cout << "timeout: kernel aborted with state (" << state << ")\n";
  }
  else {
    int waits = 1;
    while (run.wait(timeout_ms) == ERT_CMD_STATE_TIMEOUT)
      ++waits;
    
    std::cout << "timeout (" << waits << "): kernel completed with state (" << run.state() << ")\n";
  }
}

static void
run(const xrt::device& device, const xrt::uuid& uuid, int hang, int timeout_ms, bool managed)
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

  // create a kernel run
  auto run = xrt::run(add);

  // test managed execution (execution monitor)
  if (managed)
    run.add_callback(ERT_CMD_STATE_COMPLETED, run_done, nullptr);

  // start the run, if hang > 0, then kernel will hang
  run(in1, in2, out, data_size, hang);

  // call proper test
  if (!timeout_ms && hang)
    abort(run);
  else if (timeout_ms)
    timeout(run, hang, timeout_ms);
  else
    run.wait();
      
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
  bool managed = false;
  int hang = 0;
  int timeout_ms = 0;

  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;

      // No argument switches
      if (cur == "--managed")
        managed = true;

      continue;
    }

    if (cur == "-d")
      device_id = arg;
    else if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "--hang")
        hang = std::stoi(arg);
    else if (cur == "--timeout")
      timeout_ms = std::stoi(arg);
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

  run(device, uuid, hang, timeout_ms, managed);
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
