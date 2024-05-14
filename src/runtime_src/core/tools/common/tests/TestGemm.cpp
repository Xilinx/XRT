// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestGemm.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>
#include <thread>

static constexpr size_t host_app = 1; //opcode
static constexpr uint32_t num_of_cores = 32;
static constexpr uint32_t size_4K   = 0x1000;
static constexpr uint32_t offset_3K = 0x0C00;
/* Each record timer entry has 32bit ID and 32bit AIE Timer low value.
* The first 32 bit in the buffer is used to store total number 
* of record timer entries written so far. So, max_count_in_size_3K is 1 less 
* than total number of entries possible in 3K buffer section.
*/ 
static constexpr uint32_t max_count_in_size_3K = (offset_3K / (2 * sizeof(uint32_t))) - 1;
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

namespace {

// Copy values from text files into buff, expecting values are ascii encoded hex
static void 
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

static size_t 
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

} //anonymous namespace

boost::property_tree::ptree
TestGemm::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();

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
  xrt::hw_context hwctx{working_dev, xclbin.get_uuid()};
  xrt::kernel kernel{hwctx, kernelName};

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::gemm_int8);
  auto dpu_instr = findPlatformFile(seq_name, ptree);
  if (!std::filesystem::exists(dpu_instr))
    return ptree;

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

  //Create BOs
  xrt::bo bo_instr(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));
  xrt::bo bo_result(working_dev, size_4K, XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));

  init_instr_buf(bo_instr, dpu_instr);

  //Sync BOs
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  //get current performance mode
  const auto perf_mode = xrt_core::device_query<xrt_core::query::performance_mode>(dev);

  //set to performance mode
  xrt_core::device_update<xrt_core::query::performance_mode>(dev.get(), xrt_core::query::performance_mode::power_type::high);
  // 5 second delay gives the clocks time to reach the targeted frequency
  // remove this when VITIS-11934 is fixed
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));

  int ipu_hclock = 0;
  try {
    //get h-clock
    auto raw = xrt_core::device_query<xrt_core::query::clock_freq_topology_raw>(dev);
    auto clock_topology = reinterpret_cast<const clock_freq_topology*>(raw.data());
    for (int c = 0; c < clock_topology->m_count; c++) {
      if(boost::iequals(clock_topology->m_clock_freq[c].m_name, "H CLock"))
        ipu_hclock = clock_topology->m_clock_freq[c].m_freq_Mhz;
    }

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

  //Add time delay to wait till the device has transferred data back to the host
  //To-do: remove this delay after CR-1194803 is fixed
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  //Calculate TOPS
  if (ipu_hclock == 0) {
    logger(ptree, "Error", "IPU H-clock is 0");
    ptree.put("status", test_token_failed);
    return ptree;
  }
  double ipu_hclck_period = 1000000000.0 / (ipu_hclock * 1000000); // MHz to ns
  uint32_t* core_ptr = reinterpret_cast<uint32_t*>(bo_result_map+offset_3K);
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

  logger(ptree, "Details", boost::str(boost::format("Total Duration: '%.1f' ns") % (ipu_hclck_period * (total_cycle_count/num_of_cores))));
  logger(ptree, "Details", boost::str(boost::format("Average cycle count: '%.1f'") % (total_cycle_count/num_of_cores)));
  logger(ptree, "Details", boost::str(boost::format("IPU H-Clock: '%f' MHz") % ipu_hclock));
  logger(ptree, "Details", boost::str(boost::format("TOPS: '%.1f'") % TOPS));

  ptree.put("status", test_token_passed);
  return ptree;
}
