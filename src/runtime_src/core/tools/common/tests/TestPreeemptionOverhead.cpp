// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPreemptionOverhead.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
#include "core/common/unistd.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

namespace XBU = XBValidateUtils;
namespace xq = xrt_core::query;

// System - Include Files
#include <filesystem>

static constexpr uint32_t num_of_preemptions = 500;
static constexpr int iterations = 100;

// ----- S T A T I C  M E T H O D S ------------------------------------------

// Helper function to map no_of_cols to elf_name::type
static xq::elf_name::type 
get_elf_type(int no_of_cols, const std::string& level) {
  if (level == "noop") {
    switch (no_of_cols) {
      case 4: return xq::elf_name::type::preemption_noop_4x4;
      case 8: return xq::elf_name::type::preemption_noop_4x8;
      default: throw std::invalid_argument("Invalid number of columns for 'noop'.");
    }
  } else if (level == "memtile") {
    switch (no_of_cols) {
      case 4: return xq::elf_name::type::preemption_memtile_4x4;
      case 8: return xq::elf_name::type::preemption_memtile_4x8;
      default: throw std::invalid_argument("Invalid number of columns for 'memtile'.");
    }
  } else {
    throw std::invalid_argument("Invalid level. Supported levels are 'noop' and 'memtile'.");
  }
}

// Helper function to map no_of_cols to xclbin_name::type
static xq::xclbin_name::type 
get_xclbin_type(int no_of_cols) {
  switch (no_of_cols) {
    case 4:
      return xq::xclbin_name::type::preemption_4x4;
    case 8:
      return xq::xclbin_name::type::preemption_4x8;
    default:
      throw std::invalid_argument("Invalid number of columns");
  }
}

// ----- C L A S S   M E T H O D S -------------------------------------------
double
TestPreemptionOverhead::run_preempt_test(const std::shared_ptr<xrt_core::device>& device, 
                                  boost::property_tree::ptree& ptree, int no_of_cols, const std::string& level)
{
  const auto xclbin_path = XBU::get_xclbin_path(device, get_xclbin_type(no_of_cols), ptree);

  if (!std::filesystem::exists(xclbin_path))
    throw xrt_core::error("The test is not supported on this device.");
  
  auto xclbin = xrt::xclbin(xclbin_path);

  // Determine The DPU Kernel Name
  auto kernelName = XBU::get_kernel_name(xclbin, ptree);

  auto working_dev = xrt::device(device);
  working_dev.register_xclbin(xclbin);

  const auto tmp = get_elf_type(no_of_cols, level);
  const auto elf_name = xrt_core::device_query<xq::elf_name>(device, tmp);
  auto elf_path = XBU::findPlatformFile(elf_name, ptree);

  if (!std::filesystem::exists(elf_path))
    throw xrt_core::error("The test is not supported on this device.");

  xrt::hw_context hwctx;
  xrt::kernel kernel;
  try {
    hwctx = xrt::hw_context(working_dev, xclbin.get_uuid());
    kernel = get_kernel(hwctx, kernelName, elf_path);
  } 
  catch (const std::exception& )
  {
    throw xrt_core::error("Not enough columns available. Please make sure no other workload is running on the device.");
  }

  auto run = xrt::run(kernel);
  // to-do: replace with XBU::get_opcode() when dpu sequence flow is taken out
  run.set_arg(0, 3);
  run.set_arg(1, 0);
  run.set_arg(2, 0);
  run.set_arg(3, 0);
  run.set_arg(4, 0);
  run.set_arg(5, 0);
  run.set_arg(6, 0);
  run.set_arg(7, 0);

  // Set kernel argument and trigger it to run
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; i ++) {
    run.start();
    run.wait2();
  }
  auto end = std::chrono::high_resolution_clock::now();

  auto elapsed_micro_secs = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
  return elapsed_micro_secs/iterations;
}

TestPreemptionOverhead::TestPreemptionOverhead()
  : TestRunner("preemption-overhead", "Measure preemption overhead at noop and memtile levels")
{}

boost::property_tree::ptree
TestPreemptionOverhead::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.erase("xclbin");
 
  // this test is only for privileged users as it requires enabling/disabling preemption
  if(!xrt_core::is_user_privileged()) {
    XBU::logger(ptree, "Details", "This test requires admin privileges");
    ptree.put("status", XBU::test_token_skipped);
    return ptree;
  }
  
  if(XBUtilities::getVerbose())
    XBU::logger(ptree, "Details", "Using ELF");

  const std::vector<int> columns = {4, 8};
  const std::vector<std::string> levels = {"noop", "memtile"};
  for (const auto& level : levels) {
    for (auto ncol : columns) {
      //disable force layer preemption
      xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(0));
      double noop_exec_time = 0;
      try {
      noop_exec_time = run_preempt_test(dev, ptree, ncol, level);
      } 
      catch (const std::exception& ex) {
        XBU::logger(ptree, "Error", ex.what());
        ptree.put("status", XBU::test_token_failed);
        return ptree;
      }

      //enable force layer preemption
      xrt_core::device_update<xq::preemption>(dev.get(), static_cast<uint32_t>(1));
      double noop_preempt_exec_time = 0;
      try {
        noop_preempt_exec_time = run_preempt_test(dev, ptree, ncol, level);
      } 
      catch (const std::exception& ex) {
        XBU::logger(ptree, "Error", ex.what());
        ptree.put("status", XBU::test_token_failed);
        return ptree;
      }

      auto overhead = (noop_preempt_exec_time - noop_exec_time) / num_of_preemptions;
      XBU::logger(ptree, "Details", boost::str(boost::format("Average %s preemption overhead for 4x%d is %.1f us") % level % ncol % overhead));
      ptree.put("status", XBU::test_token_passed);
    }
  }
  return ptree;
}
