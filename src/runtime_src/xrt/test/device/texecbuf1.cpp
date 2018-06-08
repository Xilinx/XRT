/**
 * Copyright (C) 2018 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <boost/test/unit_test.hpp>
#include "../test_helpers.h"

#include "xrt/device/device.h"
#include "xrt/scheduler/command.h"
using namespace xrt::test;

namespace {

static void
configure_scheduler(xrt::device* device)
{
  // create command object
  xrt::command configure(device,xrt::command::opcode_type::configure);
  
  // create a configure command
  auto& packet = configure.get_packet();
  packet[1] = 0x20000/32; // slot_size;
  packet[2] = 1;          // num_cus;
  packet[3] = 16;         // cu_offset;
  packet[4] = 0x1800000;  // cu_base_addr;

  auto& features = packet[5];
  if (xrt::config::get_ert())
    features |= 0x1;
  if (xrt::config::get_ert_polling())
    features |= 0x2;
  if (xrt::config::get_ert_cudma())
    features |= 0x4;
  if (xrt::config::get_ert_cuisr())
    features |= 0x8;
  if (xrt::config::get_ert_cqint())
    features |= 0x10;
  if (xrt::config::get_timeline_trace())
    features |= 0x20;

  std::cout << "features: " << std::hex << features << std::dec << "\n";

  auto& header = packet[0];
  uint32_t mask = 127 << 12; // count mask: [19:12]
  header = (header & (~mask)) | (5 << 12);

  auto exec_bo = configure.get_exec_bo();
  device->exec_buf(exec_bo);
  while (device->exec_wait(1000)==0)
    ;
  std::cout << "configuration done\n";
}

static void
run_bin_hello_wg(xrt::device* device, size_t count)
{
  // create output buffer
  auto boh = device->alloc(20*sizeof(char));
  if (device->getDeviceAddr(boh)!=0x0)
    throw std::runtime_error("device memory address is not 0x0");

  std::vector<xrt::command> cmds;

  while (count--) {
    // create command object
    cmds.emplace_back(device,xrt::command::opcode_type::start_kernel);
    auto& start_kernel = cmds.back();

    auto& packet = start_kernel.get_packet();
    packet[0] = 0x13001; // [22:12] = 0x13 = 19 = payload size
    packet[1] = 0x1;     // cu mask
    // kernel args assuming buffer allocated at device addr 0x0
    packet[19] = 0;      // 2..19 = 0

    device->exec_buf(start_kernel.get_exec_bo());
  }

  int complete_count = 0;
  while (complete_count<cmds.size()) {
    // wait for at least one command to finish
    while (device->exec_wait(1000)==0) ;
    ++complete_count;
  }

  // assert that all commands are indeed in complete state
  for (auto& cmd : cmds) {
    auto header = cmd.get_header();
    if ((header & 0xFF)!=5)
      throw std::runtime_error("command not complete");
  }
}

static void
execwait(xrt::device* device)
{
  while (1) {
    std::cout << "calling exec_wait\n";
    auto x = device->exec_wait(3000);
    std::cout << "value: " << x << "\n";
  }
}

}


BOOST_AUTO_TEST_SUITE(test_execbuf1)

BOOST_AUTO_TEST_CASE(xbuf1)
{
  auto pred = [](const xrt::hal::device& hal) {
    return (hal.getDriverLibraryName().find("xclgemdrv")!=std::string::npos);
  };
  auto devices = xrt::test::loadDevices(pred);
  
  for (auto& device : devices) {
    device.open();
    device.setup(); // this creates the worker threads
    device.printDeviceInfo(std::cout) << "\n";
    std::string libraryName = device.getDriverLibraryName();
    std::cout << libraryName << "\n";

    try {
      configure_scheduler(&device);
      run_bin_hello_wg(&device,100);
    }
    catch (const std::exception& ex) {
      std::cout << ex.what() << "\n";
    }
    xrt::purge_command_freelist();
    device.close();
  }

}


BOOST_AUTO_TEST_SUITE_END()


