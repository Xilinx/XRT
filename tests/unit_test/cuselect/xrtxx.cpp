/**
 * Copyright (C) 2020 Xilinx, Inc
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

// % g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o xrtxx.exe xrtxx.cpp -lxrt_coreutil


#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"

#include <fstream>
#include <stdexcept>
#include <numeric>
#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <thread>

const size_t NUM_WORKGROUPS = 1;
const size_t WORKGROUP_SIZE = 16;
const size_t LENGTH = NUM_WORKGROUPS * WORKGROUP_SIZE;
const size_t data_size = LENGTH;

// vadd has 4 CUs each with 4 arguments connected as follows
//  vadd_1 (0,1,2,3)
//  vadd_2 (1,2,3,0)
//  vadd_3 (2,3,0,1)
//  vadd_4 (3,0,1,2)
// Purpose of this test is to execute kernel jobs with auto select
// of matching CUs based on the connectivity of the buffer arguments.

using data_type = int;
const size_t buffer_size = data_size*sizeof(data_type);

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = true;


struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  bool running = false;

  xrt::device device;
  xrt::kernel kernel;

  std::array<xrt::bo,4> args;

  std::array<int*,3> input_data;
  std::array<int*,1> output_data;

  job_type(const xrt::device& dev, const xrt::uuid& uuid, const std::array<int,4>& banks)
    : device(dev)
  {
    static size_t count = 0;
    id = count++;

    if (banks == std::array<int,4>{0,1,2,3})
      // Test manual explicit cu selection based on xclbin
      // introspection. This implies that at least one of the
      // specified CUs support the connectivity of the arguments per
      // banks arg.  This particular selection should warn about
      // vadd_2 being incompatible
      kernel = xrt::kernel(device, uuid, "vadd:{vadd_1,vadd_2}");
    else
      // Else just let XRT select the matching CUs, this should filter
      // out 3 CUs since only one is compatible
      kernel = xrt::kernel(device, uuid, "vadd");

    // Set kernel inputs
    for (size_t arg=0; arg<3; ++arg) {
      unsigned int bank = banks[arg];
      args[arg] = xrt::bo(device, buffer_size, bank);
      input_data[arg] = args[arg].map<int*>();
      std::iota(input_data[arg], input_data[arg] + (data_size / sizeof(int)), arg);
    }

    // Set kernel output
    unsigned int bank = banks[3];
    args[3] = xrt::bo(device, buffer_size, bank);
    output_data[0] = args[3].map<int*>();
    std::fill(output_data[0], output_data[0] + (data_size / sizeof(int)), 0);

    // Migrate all memory objects to device
    std::for_each(args.begin(), args.end(), [](auto& bo) { bo.sync(XCL_BO_SYNC_BO_TO_DEVICE); });
  }

  // Event callback for last job event
  void
  done()
  {
    verify_results();
    running = false;

    // Reschedule job unless stop is asserted.
    // This ties up the XRT thread that notifies host that event is done
    // Probably not too bad given that enqueue (run()) time should be very fast.
    if (!stop)
      run();
  }

  void
  run()
  {
    running = true;
    ++runs;

    auto run = xrt::run(kernel);

    const size_t global[1] = {NUM_WORKGROUPS * WORKGROUP_SIZE};
    const size_t local[1] = {WORKGROUP_SIZE};
    const size_t group_size = global[0] / local[0];

    ////////////////////////////////////////////////////////////////
    // OpenCL NDR is not supported by native APIs, this test has to modify
    // the register map manually to set NDR
    auto pkt = reinterpret_cast<ert_start_kernel_cmd*>(run.get_ert_packet());
    auto regmap = pkt->data + pkt->extra_cu_masks;
    ////////////////////////////////////////////////////////////////

    const size_t local_size_bytes = local[0] * sizeof(data_type);
    for (size_t id = 0; id < group_size; id++) {
      ////////////////////////////////////////////////////////////////
      // OpenCL NDR is not supported directly by native APIs
      regmap[0x10 / 4] = global[0];
      regmap[0x18 / 4] = 1;
      regmap[0x20 / 4] = 1;
      regmap[0x28 / 4] = local[0];
      regmap[0x30 / 4] = 1;
      regmap[0x38 / 4] = 1;
      regmap[0x40 / 4] = id;
      regmap[0x30 / 4] = 0;
      regmap[0x38 / 4] = 0;
      ////////////////////////////////////////////////////////////////
      auto arg0 = xrt::bo(args[0], local_size_bytes, local_size_bytes * id);
      auto arg1 = xrt::bo(args[1], local_size_bytes, local_size_bytes * id);
      auto arg2 = xrt::bo(args[2], local_size_bytes, local_size_bytes * id);
      auto arg3 = xrt::bo(args[3], local_size_bytes, local_size_bytes * id);
      run(arg0, arg1, arg2, arg3);
      run.wait();
    }

    done();
  }

  // Verify data of last stage addone output.  Note that last output
  // has been copied back to in[0] up job completion
  void
  verify_results()
  {
    // The addone kernel adds 1 to the first element in input a, since
    // the job has 4 stages, the resulting first element of a[0] will
    // be incremented by 4.
    auto dataA  = input_data[0];
    auto dataB  = input_data[1];
    auto dataC  = input_data[2];
    auto result = output_data[0];
    auto bytes = data_size*sizeof(data_type);
    args[3].sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    for (size_t idx=0; idx<data_size; ++idx) {
      unsigned long add = dataA[idx] + dataB[idx] + dataC[idx];
      if (result[idx] != add) {
        std::cout << "got result[" << idx << "] = " << result[idx] << " expected " << add << "\n";
        throw std::runtime_error("VERIFY FAILED");
      }
    }
  }
};

void
run_test(const xrt::device& device, const xrt::uuid& uuid)
{
  // create the jobs
  std::vector<job_type> jobs;
  jobs.reserve(5);

  // success jobs
  jobs.emplace_back(device, uuid, std::array<int,4>{0,1,2,3});
  jobs.emplace_back(device, uuid, std::array<int,4>{1,2,3,0});
  jobs.emplace_back(device, uuid, std::array<int,4>{2,3,0,1});
  jobs.emplace_back(device, uuid, std::array<int,4>{3,0,1,2});
  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});

  // impossile combination, will fail
  jobs.emplace_back(device, uuid, std::array<int,4>{3,0,1,0});
  try {
    jobs.back().run();
    throw 10;
  }
  catch (const std::exception& ex) {
    std::cout << "job execution failed as expected: " << ex.what() << "\n";
  }
  catch (int) {
    throw std::runtime_error("job execution succeeded unexpectedly");
  }
}

void
run(int argc, char** argv)
{
  if (argc < 2)
    throw std::runtime_error("usage: host.exe <xclbin>");

  xrt::device device{0};
  auto uuid = device.load_xclbin(argv[1]);

  run_test(device, uuid);
}

int
main(int argc, char* argv[])
{
  try {
    run(argc,argv);
    std::cout << "TEST SUCCESS\n";
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
