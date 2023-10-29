// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsPlVerify.h"

#include "tools/common/BusyBar.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
namespace XBU = XBUtilities;

#include <thread>

static const int COUNT = 1024;
static std::chrono::seconds MAX_TEST_DURATION(60 * 5); //5 minutes

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPsPlVerify::TestPsPlVerify()
  : TestRunner("ps-pl-verify", 
                "Run PS controlled 'Hello World' PL kernel test", 
                "ps_bandwidth.xclbin",
                true){}

static void
runTestInternal(std::shared_ptr<xrt_core::device> dev,
                boost::property_tree::ptree& ptree,
                TestPsPlVerify* test,
                bool& is_thread_running)
{
  test->runTest(dev, ptree);
  is_thread_running = false;
}

boost::property_tree::ptree
TestPsPlVerify::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");

  XBUtilities::BusyBar busy_bar("Running Test", std::cout); 
  busy_bar.start(XBUtilities::is_escape_codes_disabled());
  bool is_thread_running = true;

  // Start the test process
  // std::thread test_thread(run_script, cmd, std::ref(os_stdout), std::ref(os_stderr), std::ref(is_thread_running));
  std::thread test_thread([&] { runTestInternal(dev, ptree, this, is_thread_running); });
  // Wait for the test process to finish
  while (is_thread_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    try {
      busy_bar.check_timeout(MAX_TEST_DURATION);
    } catch (const std::exception&) {
      test_thread.detach();
      throw;
    }
  }
  test_thread.join();
  busy_bar.finish();

  return ptree;
}

void
TestPsPlVerify::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  const std::string test_path = findPlatformPath(dev, ptree);
  const std::string b_file = findXclbinPath(dev, ptree);
  const std::vector<std::string> dependency_paths = findDependencies(test_path, m_xclbin);
  bool flag_s = false;

  xrt::device device(dev->get_device_id());

  // Load dependency xclbins onto device if any
  for (const auto& path : dependency_paths) {
      auto retVal = validate_binary_file(path);
      if (retVal == EOPNOTSUPP) {
        ptree.put("status", test_token_skipped);
        return;
      } else if (retVal != EXIT_SUCCESS) {
        logger(ptree, "Error", "Unknown error validating depedencies");
        ptree.put("status", test_token_failed);
        return;
      }

      device.load_xclbin(path);
  }

  // Load ps kernel onto device
  auto retVal = validate_binary_file(b_file);
  if (flag_s || retVal == EOPNOTSUPP) {
    ptree.put("status", test_token_skipped);
    return;
  } else if (retVal != EXIT_SUCCESS) {
    logger(ptree, "Error", "Unknown error validating ps kernel xclbin");
    ptree.put("status", test_token_failed);
    return;
  }

  auto uuid = device.load_xclbin(b_file);
  auto bandwidth_kernel = xrt::kernel(device, uuid, "bandwidth_kernel");

  auto max_throughput_bo = xrt::bo(device, 4096, bandwidth_kernel.group_id(1));
  auto max_throughput = max_throughput_bo.map<double*>();

  int reps = 10000;

  std::fill(max_throughput,max_throughput+(4096/sizeof(double)),0);

  max_throughput_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 4096, 0);

  auto run = bandwidth_kernel(reps,max_throughput_bo);
  run.wait();

  max_throughput_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 4096, 0);

  ptree.put("status", test_token_passed);
}
