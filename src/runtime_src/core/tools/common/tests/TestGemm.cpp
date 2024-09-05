// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestGemm.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "core/common/api/bo_int.h"

namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>
#include <thread>

static constexpr size_t host_app = 1; //opcode
static constexpr uint32_t num_of_cores = 32;

/*
* Essentially, we are doing 4 unrolled loop of 8x8_8x8 matmult.
* Each 8x8_8x8 matmult involves 8x8x8=512 MAC or 512*2 OP=1024 OPs.
* Total inner*outer loop count= 2*2*12*4 (4 for unrolled loop)=192.
* Total OPs= 192*1024= 192K OPs.
*/
static constexpr uint32_t total_ops = ((8*8*8)*2)*(2*2*12*4); //192K OPs


// ----- C L A S S   M E T H O D S -------------------------------------------
TestGemm::TestGemm()
  : TestRunner("gemm", "Measure the TOPS value of GEMM operations")
{}

boost::property_tree::ptree
TestGemm::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::gemm);
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path))
    return ptree;

  logger(ptree, "Xclbin", xclbin_path);

  xrt::xclbin xclbin;
  try {
    xclbin = xrt::xclbin(xclbin_path);
  }
  catch (const std::runtime_error& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  // Determine The DPU Kernel Name
  auto xkernels = xclbin.get_kernels();

  auto itr = std::find_if(xkernels.begin(), xkernels.end(), [](xrt::xclbin::kernel& k) {
    auto name = k.get_name();
    return name.rfind("DPU",0) == 0; // Starts with "DPU"
  });

  xrt::xclbin::kernel xkernel;
  if (itr!=xkernels.end())
    xkernel = *itr;
  else {
    logger(ptree, "Error", "No kernel with `DPU` found in the xclbin");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  auto kernelName = xkernel.get_name();
  logger(ptree, "Details", boost::str(boost::format("Kernel name is '%s'") % kernelName));

  auto working_dev = xrt::device(dev);
  working_dev.register_xclbin(xclbin);

  xrt::hw_context hwctx;
  xrt::kernel kernel;
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    kernel = xrt::kernel(hwctx, kernelName);
  } 
  catch (const std::exception& ex)
  {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::gemm_int8);
  auto dpu_instr = findPlatformFile(seq_name, ptree);
  if (!std::filesystem::exists(dpu_instr))
    return ptree;

  if(XBU::getVerbose())
    logger(ptree, "DPU-Sequence", dpu_instr);

  size_t instr_size = 0;
  try {
    instr_size = get_instr_size(dpu_instr); 
  }
  catch(const std::exception& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  //Create Instruction BO
  xrt::bo bo_instr(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  init_instr_buf(bo_instr, dpu_instr);
  //Sync Instruction BO
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  // Create 128KB Debug BO to capture TOPS data
  xrt::bo bo_result = xrt_core::bo_int::create_debug_bo(hwctx, 0x20000);

  //get current performance mode
  const auto perf_mode = xrt_core::device_query<xrt_core::query::performance_mode>(dev);

  //set to performance mode
  xrt_core::device_update<xrt_core::query::performance_mode>(dev.get(), xrt_core::query::performance_mode::power_type::high);

  // wait until clock reaches the targeted frequency
  auto const target_h_clock_freq = 1810;
  int ipu_hclock = 0;
  while (ipu_hclock < target_h_clock_freq) {
    //get h-clock
    auto raw = xrt_core::device_query<xrt_core::query::clock_freq_topology_raw>(dev);
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for (int c = 0; c < clock_topology->m_count; c++) {
      if(boost::iequals(clock_topology->m_clock_freq[c].m_name, "H CLock"))
        ipu_hclock = clock_topology->m_clock_freq[c].m_freq_Mhz;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  try {
    //run kernel
    auto run = kernel(host_app, NULL, NULL, NULL, NULL, bo_instr, instr_size, NULL);
    // Wait for kernel to be done
    run.wait2();
  }
  catch (const std::exception& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }

  //map ouput buffer
  bo_result.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  auto bo_result_map = bo_result.map<uint8_t*>();

  //Calculate TOPS
  if (ipu_hclock == 0) {
    logger(ptree, "Error", "IPU H-clock is 0");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  double ipu_hclck_period = 1000000000.0 / (ipu_hclock * 1000000); // MHz to ns

  uint32_t* core_ptr = reinterpret_cast<uint32_t*>(bo_result_map);
  double TOPS = 0.0;
  double total_cycle_count = 0.0;

  for (uint32_t n = 0 ; n < num_of_cores; n++) {
    auto cycle_count = *core_ptr;
    if(cycle_count == 0) {
      logger(ptree, "Error", "cycle count is 0");
      ptree.put("status", test_token_failed);
      return ptree;
    }
    auto temp_TOPS_per_core = total_ops/(ipu_hclck_period * cycle_count * 1000);
    total_cycle_count = total_cycle_count + cycle_count;
    TOPS = TOPS + temp_TOPS_per_core;
    core_ptr++;
  }

  //reset the performance mode
  xrt_core::device_update<xrt_core::query::performance_mode>(dev.get(), static_cast<xrt_core::query::performance_mode::power_type>(perf_mode));
  if(XBU::getVerbose()) {
    logger(ptree, "Details", boost::str(boost::format("Total Duration: '%.1f' ns") % (ipu_hclck_period * (total_cycle_count/num_of_cores))));
    logger(ptree, "Details", boost::str(boost::format("Average cycle count: '%.1f'") % (total_cycle_count/num_of_cores)));
    logger(ptree, "Details", boost::str(boost::format("NPU H-Clock: '%f' MHz") % ipu_hclock));
  }
  logger(ptree, "Details", boost::str(boost::format("TOPS: '%.1f'") % TOPS));

  ptree.put("status", test_token_passed);
  return ptree;
}
