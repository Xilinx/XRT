// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include <fstream>
#include "TestValidateUtilities.h"

// Constructor for BO_set
// BO_set is a collection of all the buffer objects so that the operations on all buffers can be done from a single object
// Parameters:
// - device: Reference to the xrt::device object
// - kernel: Reference to the xrt::kernel object
BO_set::BO_set(const xrt::device& device, const xrt::kernel& kernel, const std::string& dpu_instr, size_t buffer_size) 
  : buffer_size(buffer_size), 
    bo_ifm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1)),
    bo_param (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(2)),
    bo_ofm   (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3)),
    bo_inter (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(4)),
    bo_mc    (device, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(7))
{
  if (dpu_instr.empty()) {
    // Create a no-op instruction if no instruction file is provided
    std::memset(bo_instr.map<char*>(), (uint8_t)0, buffer_size);
  } else {
    size_t instr_size = XrtSmi::Validate::get_instr_size(dpu_instr); 
    bo_instr = xrt::bo(device, instr_size, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  }
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
  hw_ctx = xrt::hw_context(params.device, params.xclbin.get_uuid());
  // Initialize kernels, buffer objects, and runs
  for (int j = 0; j < params.queue_len; j++) {
    auto kernel = xrt::kernel(hw_ctx, params.kernel_name);
    auto bos = BO_set(params.device, kernel, params.dpu_file, params.buffer_size);
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
  for (int i = 0; i < params.itr_count; i++) {
    // Start all runs in the queue so that they run in parallel
    for (int cnt = 0; cnt < params.queue_len; cnt++) {
      run_list[cnt].start();
    }
    // Wait for all runs in the queue to complete
    for (int cnt = 0; cnt < params.queue_len; cnt++) {
      run_list[cnt].wait2();
    }
  }
}

namespace XrtSmi{
namespace Validate{

// Copy values from text files into buff, expecting values are ascii encoded hex
void 
init_instr_buf(xrt::bo &bo_instr, const std::string& dpu_file) {
  std::ifstream dpu_stream(dpu_file);
  if (!dpu_stream.is_open()) {
    throw std::runtime_error(boost::str(boost::format("Failed to open %s for reading") % dpu_file));
  }

  auto instr = bo_instr.map<int*>();
  std::string line;
  while (std::getline(dpu_stream, line)) {
    if (line.at(0) == '#') {
      continue;
    }
    std::stringstream ss(line);
    unsigned int word = 0;
    ss >> std::hex >> word;
    *(instr++) = word;
  }
}

size_t 
get_instr_size(const std::string& dpu_file) {
  std::ifstream file(dpu_file);
  if (!file.is_open()) {
    throw std::runtime_error(boost::str(boost::format("Failed to open %s for reading") % dpu_file));
  }
  size_t size = 0;
  std::string line;
  while (std::getline(file, line)) {
    if (line.at(0) != '#') {
      size++;
    }
  }
  if (size == 0) {
    throw std::runtime_error("Invalid DPU instruction length");
  }
  return size;
}

void
wait_for_max_clock(int& ipu_hclock, std::shared_ptr<xrt_core::device> dev) {
  int ipu_hclock_pre = 0;
  auto hclock_steady_counter = 0;
  auto first_steady_state = -1, second_steady_state = -1;;

  for(int i=0; i<100;i++){
    auto raw = xrt_core::device_query<xrt_core::query::clock_freq_topology_raw>(dev);
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for (int c = 0; c < clock_topology->m_count; c++) {
      if(boost::iequals(clock_topology->m_clock_freq[c].m_name, "H CLock"))
        ipu_hclock = clock_topology->m_clock_freq[c].m_freq_Mhz;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //std::cout << "NPU clock: " << ipu_hclock <<std::endl;

    hclock_steady_counter = (ipu_hclock == ipu_hclock_pre) ? hclock_steady_counter + 1 : 0;
    if(hclock_steady_counter == 8 && first_steady_state == -1 && ipu_hclock >= 1810) {
      //break;
      first_steady_state = ipu_hclock_pre; 
      hclock_steady_counter = 0;
    }
    
    if(hclock_steady_counter == 8 && first_steady_state != -1 && second_steady_state == -1 && ipu_hclock > first_steady_state) {
      //break;
      second_steady_state = ipu_hclock; 
      hclock_steady_counter = 0;
    }
    
    if (hclock_steady_counter == 8 && second_steady_state != -1  && ipu_hclock > second_steady_state) {
      break;  
    }
    
    ipu_hclock_pre = ipu_hclock; // Update hclk with hclk_pre

  }
}

}// end of namespace Validate
}// end of namespace XrtSmi
