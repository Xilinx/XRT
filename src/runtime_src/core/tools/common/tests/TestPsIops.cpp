// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "TestPsIops.h"
#include "TestValidateUtilities.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include <boost/program_options.hpp>
#include <chrono>
#include <iomanip>
#include <thread>
#include <sstream>

#include "ps_iops_util/xilutil.hpp"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#ifdef _WIN32
#pragma warning(disable : 4702) //TODO remove when test is implemented properly
#endif

using ms_t = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

static const int COUNT = 1024;
static const size_t DATA_SIZE = COUNT * sizeof(int);

typedef struct task_args
{
  int thread_id;
  int queueLength;
  unsigned int total;
  Clock::time_point start;
  Clock::time_point end;
} arg_t;

struct krnl_info
{
  std::string name;
  bool new_style;
};

static bool verbose = false;
static barrier barrier;
static struct krnl_info krnl = { "hello_world", false };

// ----- C L A S S   M E T H O D S -------------------------------------------
TestPsIops::TestPsIops()
  : TestRunner("ps-iops", 
                "Run IOPS PS test", 
                "ps_validate.xclbin",
                true){}

boost::property_tree::ptree
TestPsIops::run(std::shared_ptr<xrt_core::device> dev)
{
  boost::property_tree::ptree ptree = get_test_header();
  ptree.put("xclbin_directory", "/lib/firmware/xilinx/ps_kernels/");
  runTest(dev, ptree);
  return ptree;
}

static double
runThread(std::vector<xrt::run>& cmds, unsigned int total, arg_t& arg)
{
  int i = 0;
  unsigned int issued = 0, completed = 0;
  arg.start = Clock::now();

  for (auto& cmd : cmds) {
    cmd.start();
    issued++;
    if (issued == total)
      break;
  }

  while (completed < total) {
    cmds[i].wait();

    completed++;
    if (issued < total) {
      cmds[i].start();
      issued++;
    }

    i++;
    if (static_cast<long unsigned int>(i) == cmds.size())
      i = 0;
  }

  arg.end = Clock::now();
  return static_cast<double>((std::chrono::duration_cast<ms_t>(arg.end - arg.start)).count());
}

static void
runTestThread(const xrt::device& device, const xrt::kernel& hello_world,
              arg_t& arg)
{
  std::vector<xrt::run> cmds;
  std::vector<xrt::bo> bos;

  for (int i = 0; i < arg.queueLength; i++) {
    auto run = xrt::run(hello_world);
    auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
    run.set_arg(0, bo0);
    bos.push_back(std::move(bo0));
    auto bo1 = xrt::bo(device, DATA_SIZE, hello_world.group_id(1));
    run.set_arg(1, bo1);
    bos.push_back(std::move(bo1));
    run.set_arg(2, COUNT);
    cmds.push_back(std::move(run));
  }
  barrier.wait();

  runThread(cmds, arg.total, arg);

  barrier.wait();
}

void
TestPsIops::testMultiThreads(const std::string& dev, const std::string& xclbin_fn,
                            int threadNumber, int queueLength, unsigned int total, boost::property_tree::ptree& ptree)
{
  std::vector<std::thread> threads(threadNumber);
  std::vector<arg_t> arg(threadNumber);

  xrt::device device(dev);
  auto uuid = device.load_xclbin(xclbin_fn);
  auto hello_world = xrt::kernel(device, uuid.get(), krnl.name);

  barrier.init(threadNumber + 1);

  for (int i = 0; i < threadNumber; i++) {
    arg[i].thread_id = i;
    arg[i].queueLength = queueLength;
    arg[i].total = total;
    threads[i] = std::thread([&](int i){ runTestThread(device, hello_world, arg[i]); }, i);
  }

  /* Wait threads to prepare to start */
  barrier.wait();
  auto start = Clock::now();

  /* Wait threads done */
  barrier.wait();
  auto end = Clock::now();

  for (int i = 0; i < threadNumber; i++)
    threads[i].join();

  /* calculate performance */
  int overallCommands = 0;
  double duration;
  for (int i = 0; i < threadNumber; i++) {
    if (verbose) {
      duration = static_cast<double>((std::chrono::duration_cast<ms_t>(arg[i].end - arg[i].start)).count());
      XBValidateUtils::logger(ptree, boost::str(boost::format("Details for Thread %d") % arg[i].thread_id),
                    boost::str(boost::format("Commands: %d IOPS: %f") % total % boost::io::group(std::setprecision(0), std::fixed, (total * 1000000.0 / duration))));
    }
    overallCommands += total;
  }

  duration = static_cast<double>((std::chrono::duration_cast<ms_t>(end - start)).count());
  XBValidateUtils::logger(ptree, "Details", boost::str(boost::format("Overall Commands: %d IOPS: %f (%s)")
                % total % boost::io::group(std::setprecision(0), std::fixed, (overallCommands * 1000000.0 / duration)) % krnl.name));
  ptree.put("status", XBValidateUtils::test_token_passed);
}

void
TestPsIops::runTest(std::shared_ptr<xrt_core::device> dev, boost::property_tree::ptree& ptree)
{
  XBValidateUtils::logger(ptree, "Details", "Test not supported.");
  ptree.put("status", XBValidateUtils::test_token_skipped);
  return;

  xrt::device device(dev);

  const std::string test_path = XBValidateUtils::findPlatformPath(dev, ptree);
  const std::vector<std::string> dependency_paths = findDependencies(test_path, m_xclbin);
  // Validate dependency xclbins onto device if any
  for (const auto& path : dependency_paths) {
    auto retVal = XBValidateUtils::validate_binary_file(path);
    if (retVal == EOPNOTSUPP) {
      ptree.put("status", XBValidateUtils::test_token_skipped);
      return;
    } else if (retVal != EXIT_SUCCESS) {
      XBValidateUtils::logger(ptree, "Error", "Unknown error validating depedencies");
      ptree.put("status", XBValidateUtils::test_token_failed);
      return;
    }
    // device.load_xclbin(path);
  }

  const std::string b_file = XBValidateUtils::findXclbinPath(dev, ptree); // "/lib/firmware/xilinx/ps_kernels/ps_bandwidth.xclbin"
  auto retVal = XBValidateUtils::validate_binary_file(b_file);
  if (retVal == EOPNOTSUPP) {
    ptree.put("status", XBValidateUtils::test_token_skipped);
    return;
  }

  krnl.new_style = true;
  const int threadNumber = 2;
  const int queueLength = 128;
  const int total = 50000;

  const auto bdf_tuple = xrt_core::device_query<xrt_core::query::pcie_bdf>(dev);
  const std::string bdf = xrt_core::query::pcie_bdf::to_string(bdf_tuple);
  try {
    // TODO need to add processing for dependency paths
    testMultiThreads(bdf, b_file, threadNumber, queueLength, total, ptree);
    return;
  } catch (const std::exception& ex) {
    XBValidateUtils::logger(ptree, "Error", ex.what());
  } catch (...) {
    // Test failed.
  }
  ptree.put("status", XBValidateUtils::test_token_failed);
}