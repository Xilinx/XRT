// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files

#include "TestArrayReconfOverhead.h"
#include "tools/common/XBUtilities.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

// System - Include Files
#include <fstream>
#include <filesystem>
#include <thread>
#include <iostream>
static constexpr size_t buffer_size_gb = 1;
static constexpr size_t buffer_size = buffer_size_gb * 1024 * 1024 * 1024; //1 GB
static constexpr size_t word_count = buffer_size/4;
static constexpr int itr_count = 500;

TestArrayReconfOverhead::TestArrayReconfOverhead()
  : TestRunner("aro", "Run end-to-end array reconfiguration overhead test")
{}

boost::property_tree::ptree
TestArrayReconfOverhead::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  std::string xclbin_name;
  try{
    xclbin_name = xrt_core::device_query<xrt_core::query::xclbin_name>(dev, xrt_core::query::xclbin_name::type::aro);
  }
  catch (const xrt_core::query::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    exit(EXIT_FAILURE);
  }
  auto xclbin_path = findPlatformFile(xclbin_name, ptree);
  if (!std::filesystem::exists(xclbin_path))
  {
    throw std::runtime_error(boost::str(boost::format("Invalid xclbin file path %s") % xclbin_path));
  }

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
  if(XBUtilities::getVerbose())
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

  const auto seq_name = xrt_core::device_query<xrt_core::query::sequence_name>(dev, xrt_core::query::sequence_name::type::aro);
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
  xrt::bo bo_ofm(working_dev, buffer_size, XRT_BO_FLAGS_HOST_ONLY, kernel.group_id(3));
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
  if(XBUtilities::getVerbose()) { 
    logger(ptree, "Details", boost::str(boost::format("Buffer size: '%f'GB") % buffer_size_gb));
    logger(ptree, "Details", boost::str(boost::format("No. of iterations: '%f'") % itr_count));
  }

  auto start = std::chrono::high_resolution_clock::now();
  try{
    auto run = kernel(1, bo_ifm, NULL, bo_ofm, NULL, bo_instr, instr_size, NULL);
    run.wait2();
  }
  catch (const std::exception& ex)
  {
    logger(ptree, "Error", ex.what());
    ptree.put("status", test_token_failed);
    return ptree;
  }
  auto end = std::chrono::high_resolution_clock::now();
  float elapsedSecs = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();

  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i< itr_count; i++)
  {
    try{
      auto run = kernel(1, bo_ifm, NULL, bo_ofm, NULL, bo_instr, instr_size, NULL);
      run.wait2();
    }
    catch (const std::exception& ex)
    {
      logger(ptree, "Error", ex.what());
      ptree.put("status", test_token_failed);
      return ptree;
    }
  }
  end = std::chrono::high_resolution_clock::now();
  float elapsedSecsAverage = std::chrono::duration_cast<std::chrono::duration<float>>(end-start).count();
  elapsedSecsAverage /= itr_count;
  logger(ptree, "Debug", boost::str(boost::format("ElapsedSec: '%.1f' us") % (elapsedSecs * 1000000)));
  logger(ptree, "Debug", boost::str(boost::format("ElapsedSecAverage: '%.1f' us") % (elapsedSecsAverage * 1000000)));
  float overhead = elapsedSecs - elapsedSecsAverage; 
  logger(ptree, "Details", boost::str(boost::format("Overhead: '%.1f' us") % (overhead * 1000000)));

  ptree.put("status", test_token_passed);
  return ptree;
}