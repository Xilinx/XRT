// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestTCTAllColumn.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

// System - Include Files
#include <fstream>
#include <filesystem>

static constexpr size_t host_app = 1; //opcode
static constexpr size_t buffer_size = 4;
static constexpr size_t word_count = buffer_size/4;
static constexpr int itr_count = 20000;

// ----- C L A S S   M E T H O D S -------------------------------------------
TestTCTAllColumn::TestTCTAllColumn()
  : TestRunner("tct-all-col", "Measure average TCT processing time for all columns")
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
TestTCTAllColumn::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();

  const auto xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::validate);
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

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::tct_one_column);
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
  xrt::bo bo_ifm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(1));
  xrt::bo bo_ofm(working_dev, 4*buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
  xrt::bo bo_instr(working_dev, instr_size*sizeof(int), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(5));

  init_instr_buf(bo_instr, dpu_instr);

  // map input buffer
  auto ifm_mapped = bo_ifm.map<int*>();
	for (size_t i = 0; i < word_count; i++)
		ifm_mapped[i] = rand() % 4096;

  //Sync BOs
  bo_instr.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  bo_ifm.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  //Log
  logger(ptree, "Details", boost::str(boost::format("Buffer size: '%f' bytes") % buffer_size));
  logger(ptree, "Details", boost::str(boost::format("No. of iterations: '%f'") % itr_count));

  auto start = std::chrono::high_resolution_clock::now();
  try {
    auto run = kernel(host_app, bo_ifm, NULL, bo_ofm, NULL, bo_instr, instr_size, NULL);
    // Wait for kernel to be done
    run.wait2();
  }
  catch (const std::exception& ex) {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }
  auto end = std::chrono::high_resolution_clock::now();

  //map ouput buffer
  bo_ofm.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  auto ofm_mapped = bo_ofm.map<int*>();
  for (size_t i = 0; i < word_count; i++) {
    if (ofm_mapped[i] != ifm_mapped[i]) {
      auto msg = boost::str(boost::format("Data mismatch at out buffer[%d]") % i);
      logger(ptree, "Error", msg);
      return ptree;
    }
  }

  //Calculate throughput
  float elapsedSecs = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();
  float throughput = itr_count / elapsedSecs;
  float latency = (elapsedSecs / itr_count) * 1000000; //convert s to us
  logger(ptree, "Details", boost::str(boost::format("Average time for TCT: '%.1f' us") % latency));
  logger(ptree, "Details", boost::str(boost::format("Average TCT throughput: '%.1f' TCT/s") % throughput));

  ptree.put("status", test_token_passed);
  return ptree;
}
