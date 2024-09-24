// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestHelper.h"

// Constructor for BO_set
// BO_set is a collection of all the buffer objects so that the operations on all buffers can be done from a single object
// Parameters:
// - device: Reference to the xrt::device object
// - kernel: Reference to the xrt::kernel object
BO_set::BO_set(const xrt::device& device, const xrt::kernel& kernel, size_t buffer_size) 
  : buffer_size(buffer_size), 
    bo_instr (device, buffer_size, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5)),
    bo_ifm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1)),
    bo_param (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(2)),
    bo_ofm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3)),
    bo_inter (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4)),
    bo_mc    (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(7))
{
  // no-op instruction buffer
  std::memset(bo_instr.map<char*>(), (uint8_t)0, buffer_size);
}

// Method to synchronize buffer objects to the device
void BO_set::sync_bos_to_device() {
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_param.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_mc.sync(XCL_BO_SYNC_BO_TO_DEVICE);
}

// Method to set kernel arguments
// Parameters:
// - run: Reference to the xrt::run object
void BO_set::set_kernel_args(xrt::run& run) const {
  uint64_t opcode = 1;
  run.set_arg(0, opcode);
  run.set_arg(1, bo_ifm);
  run.set_arg(2, bo_param);
  run.set_arg(3, bo_ofm);
  run.set_arg(4, bo_inter);
  run.set_arg(5, bo_instr);
  run.set_arg(6, buffer_size/sizeof(int));
  run.set_arg(7, bo_mc);
}

void 
TestCase::initialize()
{
  // Initialize kernels, buffer objects, and runs
  for (int j = 0; j < queue_len; j++) {
    auto kernel = xrt::kernel(hw_ctx, kernel_name);
    auto bos = BO_set(device, kernel, buffer_size);
    bos.sync_bos_to_device();
    auto run = xrt::run(kernel);
    bos.set_kernel_args(run);
    run.start();
    run.wait2();

    kernels.push_back(kernel);
    bo_set_list.push_back(bos);
    run_list.push_back(run);
  }
}

// Method to run the test case
void
TestCase::run()
{
  for (int i = 0; i < itr_count; i++) {
    // Start all runs in the queue so that they run in parallel
    for (int cnt = 0; cnt < queue_len; cnt++) {
      run_list[cnt].start();
    }
    // Wait for all runs in the queue to complete
    for (int cnt = 0; cnt < queue_len; cnt++) {
      run_list[cnt].wait2();
    }
  }
}
