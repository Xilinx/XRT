/*
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************
Kernel to kernel streaming example consisting of three compute
units in a linear hardware pipeline.

1) Adder kernel that adds two input vectors from global memory
and streams result to output stream

2) Streaming kernel that increments input stream with a scalar adder
and streams result to output

3) Multiplier kernel that multiplies a global memory vector with the
stream output of the streaming kernel and writes result into global memory.

       _____________
      |add          |<----- in1 (global memory)
      | in1 + in2   |<----- in2 (global memory)
      |_____________|------+
       _____________       | (s1) AXI4 Stream
      |incr         |<-----+
      | s1 + adder  |<----- adder (scalar)
      |_____________|----->+
       _____________       | (s2) AXI4 Stream
      |mult         |<-----+
      | s2 * in3    |<----- in3 (global memory)
      |_____________|-----> out (global memory)

out = [in1 + in2 + adder] * in3

This example illustrates counted auto-restart on the incr streaming
kernel and the use of mailbox to change the adder value of incr.

Since incr is a streaming kernel, it is stalled while waiting for
input from first stage adder.  Since the mailbox values are not picked
up by the streaming kernel before it starts running actually
controlling the value of the adder for the purpose of comparing the
final result is not easy.

This example prints the difference between the expected 'adder' and
adder actually used by incr kernel and prints the value of expected
'adder' along with the value of the actual adder used by incr kernel.

This a POC example, while the xclbin is compiled with 'incr' using

config_interface -s_axilite_mailbox both
config_interface -s_axilite_auto_restart_counter 1

the xclbin contains no meta data to reflect mailbox and counter. As a
POC work-around, xrt.ini can be used to specify which kernels have
what features.

% cat xrt.ini
[Runtime]
mailbox_kernels="/krnl_stream_vdatamover/"
auto_restart_kernels="/krnl_stream_vdatamover/"

Syntax being "/kname1/kname2/.../" where knameN is the name of the
kernel (not the name of a compute unit).  Undefined behavior if this
convention is not followed or if the provided kernel names identifies
kernels without the specified features.

****************************************************************/

// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o mailbox.exe main.cpp -lxrt_coreutil -luuid -pthread
// % mailbox.exe -k <xclbin> [--iter <iterations>]
// The incr kernel scalar 'adder' argument is incremented by 1 in each iteration.
// See xclbin.mk for building xclbin for desired platform

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include "xrt/xrt_kernel.h"
#include "experimental/xrt_ip.h"
#include "experimental/xrt_ini.h"
#include "experimental/xrt_mailbox.h"

using value_type = std::uint32_t;

static size_t data_size = 8 * 1024 * 1024;
static size_t data_size_bytes = sizeof(int) * data_size;

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <bdf | device_index>\n";
  std::cout << "";
  std::cout << "  [--iter <number>]: number of counted restarts of streaming kernel\n";
  std::cout << "";
  std::cout << "* Program runs the pipeline [add]-[incr]-[mult] specified number of times\n";
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

static void
run(const xrt::device& device, const xrt::uuid& uuid, unsigned int iter)
{
  // add(add_in1, add_in2, nullptr, data_size)
  xrt::kernel add(device, uuid, "krnl_stream_vadd");
  xrt::bo add_in1(device, data_size_bytes, add.group_id(0));
  auto add_in1_data = add_in1.map<int*>();
  xrt::bo add_in2(device, data_size_bytes, add.group_id(1));
  auto add_in2_data = add_in2.map<int*>();

  // mult(mult_in1, nullptr, mult_out, data_size)
  xrt::kernel mult(device, uuid, "krnl_stream_vmult");
  xrt::bo mult_in1(device, data_size_bytes, mult.group_id(0));
  auto mult_in1_data = mult_in1.map<int*>();
  xrt::bo mult_out(device, data_size_bytes, mult.group_id(2));
  auto mult_out_data = mult_out.map<int*>();

  // incr(nullptr, nullptr, adder)
  xrt::kernel incr(device, uuid, "krnl_stream_vdatamover");
  unsigned int adder = 0;

  // create run objects for re-use in loop
  xrt::run add_run(add);
  xrt::run mult_run(mult);

  // start the incr kernel in auto restart mode with default adder
  // since it is a streaming kernel it will be stalled waiting for
  // input
  auto incr_run = incr(xrt::autostart{iter}, nullptr, nullptr, adder);

  // create mailbox to programatically update the incr kernel
  xrt::mailbox incr_mbox(incr_run);

  // while validation of pipeline output is not possible we still
  // compute the expected output if the adder had been synced up
  // accordingly by the incr kernel
  std::vector<int> sw_out_data(data_size);

  for (unsigned int cnt = 0; cnt < iter; ++cnt) {

    // Create the test data and software result
    for(size_t i = 0; i < data_size; ++i) {
      add_in1_data[i] = static_cast<int>(i);
      add_in2_data[i] = 2 * static_cast<int>(i);
      mult_in1_data[i] = static_cast<int>(i);
      mult_out_data[i] = 0;
      sw_out_data[i] = (add_in1_data[i] + add_in2_data[i] + adder) * mult_in1_data[i];
    }

    // sync test data to kernel
    add_in1.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    add_in2.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    mult_in1.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // start the pipeline
    add_run(add_in1, add_in2, nullptr, data_size);
    mult_run(mult_in1, nullptr, mult_out, data_size);

    // at any time before or after kernel execution it is
    // safe to update the mailbox part of incr, so update
    // it with the next value of adder
    incr_mbox.set_arg(2, ++adder); // update the mailbox

    // wait for at least the adder to complete, this should
    // ensure that the streaming incr kernel is either running
    // or also done
    add_run.wait();

    // now it is safe to write the mailbox content to hw, the
    // write will not be picked up until the next iteration
    // of the pipeline (incr), it will not modify adder while
    // incr is currently running.
    incr_mbox.write();  // requests sync of mailbox to hw

    // now wait for this iteration of the pipeline to complete
    // by waiting for the multiplier to be done
    mult_run.wait();

    // sync result from device to host
    mult_out.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    // compare with expected scalar adder
    auto prev = 0;  // expected difference
    for (size_t i = 0 ; i < data_size; i++) {
      if (mult_out_data[i] != sw_out_data[i]) {
        // check what the adder value actually was
        if (mult_in1_data[i] == 0)
          continue;  // don't divide by 0

        auto diff = (sw_out_data[i] - mult_out_data[i]) / mult_in1_data[i];
        auto sw_adder = adder - 1;        // the expected adder
        auto hw_adder = sw_adder - diff;  // the actual adder used
        if (prev != (sw_adder - hw_adder)) {
          std::cout << "iteration = " << cnt
                    << " diff = " << diff
                    << " sw_adder = " << sw_adder
                    << " hw_adder = " << hw_adder << '\n';
          prev = sw_adder - hw_adder;
        }
      }
    }
  }
}

static void
run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  std::string device_id = "0";
  unsigned int iter = 1;

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
    else if (cur == "--iter")
      iter = std::stoul(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  adjust_for_hw_emulation();

  // Disable ert to avoid scheduler arming interrupts on the xrt::ip controlled
  xrt::ini::set("Runtime.ert", false);
  xrt::xclbin xclbin{xclbin_fnm};
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin);

  run(device, uuid, iter);
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
